/*============================================================================
 *  voicelink_raw.c  —  最天真的 UDP 語音串流（Team 2 的起點；故意什麼都沒做）
 *----------------------------------------------------------------------------
 *  用法：
 *    voicelink_raw send <ip> <port> <in.wav>             讀 WAV，每 20 ms 送一包 640 bytes
 *    voicelink_raw recv <port> <out.wav> [--timeout ms]   收到什麼就照到達順序寫什麼，2 秒沒封包就結束
 *    voicelink_raw call <my_port> <peer_ip> <peer_port>   麥克風 → 對方喇叭、對方 → 我的喇叭；按 Enter 結束
 *    voicelink_raw devices                                列出麥克風與喇叭
 *
 *  【這支程式故意很笨】封包沒有 header、沒有 seq／timestamp、沒有 jitter buffer、沒有遺失補償（PLC）、
 *  沒有 STATS。網路正常（127.0.0.1）時它看起來能用；一經過 udp_impair.py 就會：
 *    掉包   → 聲音直接少一段（recv 寫出的 WAV 變短；call 時喇叭出現 20 ms 的空洞）
 *    亂序   → 兩段聲音對調（因為「到達順序」就是「播放順序」）
 *    重複   → 同一段聲音播兩次
 *    抖動   → call 時 ring 一下空、一下滿：忽快忽慢、破音
 *  這些就是你們要解決的問題。README（team2_voicelink/README.md）規定了封包格式與 STATS；要加的東西各放在哪裡：
 *    header（magic／type／seq／ts／ssrc）  → cmd_send 的迴圈裡「組封包」那幾行（用 udp.h 的 put_u16／put_u32），
 *                                             以及 call 的 mic_push_and_send（送端兩處）；
 *                                             recv 端在 cmd_recv 與 recv_thread 的「收到一包」之後先解析、檢查 magic 與長度
 *    seq／timestamp 的排序與去重、jitter buffer → 取代 cmd_recv 裡「直接 append」的那一行，
 *                                             以及 call 的 ring（ring_push／ring_pop 改成「依 seq 排進去、照 ts 拿出來」）
 *    播放時鐘（每 20 ms 一拍，不等網路）   → mock 模式：cmd_recv 要改成「每 20 ms 從 buffer 拿一個 frame 寫出去」，
 *                                             不再是「來一包寫一包」；live 模式：audio_cb 本身就是那個時鐘
 *    PLC（zero／repeat）                     → 播放時鐘那一拍 buffer 裡沒有該播的 seq 時（audio_cb 的 ring_pop 失敗那個分支，
 *                                             以及 mock 模式的對應位置）
 *    BYE 封包                               → cmd_send 結尾送 3 次；cmd_recv／recv_thread 收到就結束
 *    STATS                                  → 每個 cmd_* 的結尾印到 stderr
 *  wrapper/ 裡的四個標頭（wav.h、udp.h、clock.h、audio.h）都是完成的，可以直接用、也可以改。
 *
 *  結束碼：0 正常；1 網路或裝置錯誤；2 參數或檔案格式錯誤。
 *===========================================================================*/
#include "wrapper/udp.h"                 /* 一定要第一個：裡面有 _POSIX_C_SOURCE 與 winsock 的順序要求 */
#include "wrapper/clock.h"
#include "wrapper/wav.h"
#include "wrapper/audio.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 執行緒與鎖：和 Team 1 的 platform.h、第 2 週的 chat.c 同一套寫法 */
#ifdef _WIN32
  #include <process.h>
  typedef HANDLE thread_t;
  static int thread_create(thread_t *t, unsigned (__stdcall *fn)(void *), void *arg) {
      *t = (HANDLE)_beginthreadex(NULL, 0, fn, arg, 0, NULL);
      return (*t == NULL) ? -1 : 0;
  }
  static void thread_join(thread_t t) { WaitForSingleObject(t, INFINITE); CloseHandle(t); }
  #define THREAD_FN(name) unsigned __stdcall name(void *arg)
  #define THREAD_RETURN   return 0
  typedef CRITICAL_SECTION lock_t;
  static void lock_init(lock_t *l) { InitializeCriticalSection(l); }
  static void lock_take(lock_t *l) { EnterCriticalSection(l); }
  static void lock_give(lock_t *l) { LeaveCriticalSection(l); }
#else
  #include <pthread.h>
  typedef pthread_t thread_t;
  static int thread_create(thread_t *t, void *(*fn)(void *), void *arg) { return pthread_create(t, NULL, fn, arg); }
  static void thread_join(thread_t t) { pthread_join(t, NULL); }
  #define THREAD_FN(name) void *name(void *arg)
  #define THREAD_RETURN   return NULL
  typedef pthread_mutex_t lock_t;
  static void lock_init(lock_t *l) { pthread_mutex_init(l, NULL); }
  static void lock_take(lock_t *l) { pthread_mutex_lock(l); }
  static void lock_give(lock_t *l) { pthread_mutex_unlock(l); }
#endif

#define FRAME_SAMPLES 320                        /* 20 ms @ 16 kHz */
#define FRAME_BYTES   (FRAME_SAMPLES * 2)        /* 640 */
#define FRAME_MS      20
#define MAX_DGRAM     2048                       /* 收封包的緩衝區：比 640 大很多，壞封包也收得下 */

static int usage(void) {
    fprintf(stderr,
        "用法:\n"
        "  voicelink_raw send <ip> <port> <in.wav>             讀 16 kHz/16-bit/單聲道 WAV，每 20 ms 送一包 640 bytes\n"
        "  voicelink_raw recv <port> <out.wav> [--timeout ms]   照到達順序寫成 WAV；這麼久沒封包就結束（預設 2000）\n"
        "  voicelink_raw call <my_port> <peer_ip> <peer_port>   雙向通話（戴耳機）；按 Enter 結束\n"
        "  voicelink_raw devices                                列出麥克風與喇叭\n");
    return 2;
}

/* "5004" → 5004；不合法回傳 -1（和 Team 1 的 net_parse_port 一樣，不用 atoi） */
static int parse_port(const char *s) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v < 1 || v > 65535) return -1;
    return (int)v;
}

/*--------------------------------------------------------------------------
 * send：WAV → 每 20 ms 一個 640 bytes 的 datagram
 *-------------------------------------------------------------------------*/
static int cmd_send(const char *ip, int port, const char *path) {
    wav_t w;
    int rc = wav_read(path, &w);
    if (rc == -1) { fprintf(stderr, "錯誤: 讀不到 %s\n", path); return 2; }
    if (rc == -2) { fprintf(stderr, "錯誤: %s 不是 16-bit PCM 的 WAV\n", path); return 2; }
    if (w.sample_rate != 16000 || w.channels != 1 || w.bits != 16) {
        fprintf(stderr, "錯誤: %s 是 %u Hz／%u 聲道／%u-bit；只接受 16000 Hz／1 聲道／16-bit\n",
                path, (unsigned)w.sample_rate, (unsigned)w.channels, (unsigned)w.bits);
        wav_free(&w);
        return 2;
    }

    udp_t u;
    if (udp_open(&u, NULL, 0) != 0 || udp_set_peer(&u, ip, port) != 0) { wav_free(&w); return 1; }

    size_t n_frames = (w.n_samples + FRAME_SAMPLES - 1) / FRAME_SAMPLES;   /* 無條件進位：最後不足 320 也算一包 */
    printf("送 %s：%u 個 sample → %u 包，每 %d ms 一包，到 %s:%d\n",
           path, (unsigned)w.n_samples, (unsigned)n_frames, FRAME_MS, ip, port);
    fflush(stdout);

    uint8_t pkt[FRAME_BYTES];
    double t0 = clock_ms();
    size_t sent = 0;
    for (size_t k = 0; k < n_frames; k++) {
        /* ── 組封包 ──  這裡只有 PCM。要加 header 就在 pkt 前面留 12 bytes：put_u16(pkt+2, seq) 等等。
         * 最後一包不足 640 bytes 的部分補 0。int16_t 轉成 bytes 時照 WAV 的 little-endian。 */
        memset(pkt, 0, sizeof(pkt));
        size_t start = k * FRAME_SAMPLES;
        size_t n = w.n_samples - start;
        if (n > FRAME_SAMPLES) n = FRAME_SAMPLES;
        for (size_t i = 0; i < n; i++) {
            uint16_t v = (uint16_t)w.pcm[start + i];
            pkt[2 * i] = (uint8_t)(v & 0xff);
            pkt[2 * i + 1] = (uint8_t)(v >> 8);
        }
        if (udp_send(&u, pkt, sizeof(pkt)) < 0) { udp_close(&u); wav_free(&w); return 1; }
        sent++;
        /* 第 k 包送完，睡到「t0 + 20·(k+1)」：用絕對時間，誤差才不會一包一包累積起來 */
        sleep_until_ms(t0 + (double)FRAME_MS * (double)(k + 1));
    }
    double elapsed = clock_ms() - t0;
    printf("sent %u packets in %.0f ms\n", (unsigned)sent, elapsed);

    udp_close(&u);
    wav_free(&w);
    return 0;
}

/*--------------------------------------------------------------------------
 * recv：收到什麼、照到達順序寫什麼（這就是問題所在）
 *-------------------------------------------------------------------------*/
static int cmd_recv(int port, const char *path, int timeout_ms) {
    udp_t u;
    if (udp_open(&u, NULL, port) != 0) return 1;
    printf("聽 0.0.0.0:%d，寫到 %s；第一包之後 %d ms 沒有封包就結束（第一包最多等 30 秒）\n", port, path, timeout_ms);
    fflush(stdout);

    /* 可長大的 byte 陣列：收到的 bytes 全部接在後面 */
    uint8_t *out = NULL;
    size_t out_len = 0, out_cap = 0;
    size_t n_pkts = 0;
    uint8_t buf[MAX_DGRAM];
    int rc_exit = 0;

    for (;;) {
        int wait = (n_pkts == 0) ? 30000 : timeout_ms;
        int k = udp_recv(&u, buf, sizeof(buf), wait, NULL);
        if (k < 0) { rc_exit = 1; break; }
        if (k == 0) {                                     /* 逾時：送端送完了（或根本沒開） */
            if (n_pkts == 0) fprintf(stderr, "30 秒內一個封包都沒收到；送端開了嗎？port 對嗎？\n");
            break;
        }
        /* ── 收到一包 ──  這裡應該先檢查 header（magic、長度）、讀出 seq／ts、放進 jitter buffer。
         * 現在是：直接 append。亂序到的就亂序寫、掉的就少一段、重複的就寫兩次。 */
        if (out_len + (size_t)k > out_cap) {
            size_t nc = out_cap ? out_cap * 2 : 65536;
            while (nc < out_len + (size_t)k) nc *= 2;
            uint8_t *p = (uint8_t *)realloc(out, nc);
            if (!p) { fprintf(stderr, "錯誤: 記憶體不足\n"); rc_exit = 1; break; }
            out = p;
            out_cap = nc;
        }
        memcpy(out + out_len, buf, (size_t)k);
        out_len += (size_t)k;
        n_pkts++;
    }
    udp_close(&u);

    /* bytes → int16_t（little-endian），奇數 byte 的尾巴丟掉；再寫成 WAV */
    size_t n_samples = out_len / 2;
    int16_t *pcm = (int16_t *)malloc(n_samples ? n_samples * sizeof(int16_t) : 1);
    if (!pcm) { free(out); return 1; }
    for (size_t i = 0; i < n_samples; i++) pcm[i] = (int16_t)(out[2 * i] | (out[2 * i + 1] << 8));
    if (wav_write(path, pcm, n_samples, 16000) != 0) {
        fprintf(stderr, "錯誤: 寫不出 %s\n", path);
        rc_exit = 2;
    }
    printf("received %u packets, %u bytes\n", (unsigned)n_pkts, (unsigned)out_len);
    free(pcm);
    free(out);
    return rc_exit;
}

/*--------------------------------------------------------------------------
 * call：雙向。三條執行緒在跑：
 *   main         等 Enter
 *   recv_thread  udp_recv → ring_push（網路到達的 frame 排隊等播）
 *   audio 執行緒  audio_cb：mic 的 sample 湊滿 320 個就 sendto；spk 從 ring_pop 拿一個 frame、沒有就靜音
 * ring 是「8 個 frame 的環狀佇列」：recv_thread 放、audio_cb 拿，兩邊用鎖保護。
 * 它不是 jitter buffer：不看 seq、不排序、不等「先累積幾個再播」；滿了就把新的丟掉。
 *-------------------------------------------------------------------------*/
#define RING_FRAMES 8

typedef struct {
    int16_t frames[RING_FRAMES][FRAME_SAMPLES];
    int head, count;                         /* head = 下一個要拿的位置；count = 目前有幾個 */
    lock_t lock;
    size_t dropped;                          /* ring 滿了被丟掉的 frame 數（看得出網路「一陣一陣」到） */
} ring_t;

static void ring_push(ring_t *r, const int16_t *frame) {
    lock_take(&r->lock);
    if (r->count < RING_FRAMES) {
        memcpy(r->frames[(r->head + r->count) % RING_FRAMES], frame, FRAME_BYTES);
        r->count++;
    } else {
        r->dropped++;
    }
    lock_give(&r->lock);
}

/* 回傳 1 並把一個 frame 複製到 out；ring 空的回傳 0 */
static int ring_pop(ring_t *r, int16_t *out) {
    int ok = 0;
    lock_take(&r->lock);
    if (r->count > 0) {
        memcpy(out, r->frames[r->head], FRAME_BYTES);
        r->head = (r->head + 1) % RING_FRAMES;
        r->count--;
        ok = 1;
    }
    lock_give(&r->lock);
    return ok;
}

typedef struct {
    udp_t   u;
    ring_t  ring;
    volatile int running;                    /* main 把它設成 0，其他執行緒看到就收工 */
    /* mic：callback 給的 n 不一定剛好 320，所以先湊在 stage 裡，滿 320 才送 */
    int16_t mic_stage[FRAME_SAMPLES];
    size_t  mic_fill;
    /* spk：同理，一個 frame 可能分兩次 callback 播完，用 spk_cur／spk_pos 記著播到哪 */
    int16_t spk_cur[FRAME_SAMPLES];
    size_t  spk_pos;                         /* == FRAME_SAMPLES 表示 spk_cur 已經播完、要再拿一個 */
    size_t  sent, received, silence;
} call_t;

/* 把湊滿的 320 個 sample 送出去。要加 header（seq／ts／ssrc）就在這裡。 */
static void mic_push_and_send(call_t *c, const int16_t *mic, size_t n) {
    for (size_t i = 0; i < n; i++) {
        c->mic_stage[c->mic_fill++] = mic[i];
        if (c->mic_fill == FRAME_SAMPLES) {
            uint8_t pkt[FRAME_BYTES];
            for (size_t j = 0; j < FRAME_SAMPLES; j++) {
                uint16_t v = (uint16_t)c->mic_stage[j];
                pkt[2 * j] = (uint8_t)(v & 0xff);
                pkt[2 * j + 1] = (uint8_t)(v >> 8);
            }
            if (udp_send(&c->u, pkt, sizeof(pkt)) > 0) c->sent++;
            c->mic_fill = 0;
        }
    }
}

/* 填 n 個 sample 到喇叭：從 ring 拿；ring 空的就填 0（這裡就是 PLC 要放的地方：zero 或 repeat 上一個 frame） */
static void spk_fill(call_t *c, int16_t *spk, size_t n) {
    size_t done = 0;
    while (done < n) {
        if (c->spk_pos >= FRAME_SAMPLES) {               /* 上一個 frame 播完了，拿下一個 */
            if (!ring_pop(&c->ring, c->spk_cur)) {
                memset(c->spk_cur, 0, FRAME_BYTES);      /* ── 沒有東西可播：補靜音 ── */
                c->silence++;
            }
            c->spk_pos = 0;
        }
        size_t take = FRAME_SAMPLES - c->spk_pos;
        if (take > n - done) take = n - done;
        memcpy(spk + done, c->spk_cur + c->spk_pos, take * sizeof(int16_t));
        c->spk_pos += take;
        done += take;
    }
}

/* 音訊執行緒每 20 ms 叫一次。mic 或 spk 可能是 NULL（audio.h 第 5 點：退路模式會分開叫） */
static void audio_cb(const int16_t *mic, int16_t *spk, size_t n, void *user) {
    call_t *c = (call_t *)user;
    if (!c->running) { if (spk) memset(spk, 0, n * sizeof(int16_t)); return; }
    if (mic) mic_push_and_send(c, mic, n);
    if (spk) spk_fill(c, spk, n);
}

/* 網路執行緒：收一包 → 放進 ring。每 200 ms 醒來看一下 running，main 說停就停。 */
static THREAD_FN(recv_thread) {
    call_t *c = (call_t *)arg;
    uint8_t buf[MAX_DGRAM];
    int16_t frame[FRAME_SAMPLES];
    while (c->running) {
        int k = udp_recv(&c->u, buf, sizeof(buf), 200, NULL);
        if (k < 0) break;
        if (k == 0) continue;
        /* ── 收到一包 ──  應該先解析 header；現在是只要剛好 640 bytes 就當成一個 frame，其他丟掉 */
        if (k != FRAME_BYTES) continue;
        for (size_t i = 0; i < FRAME_SAMPLES; i++) frame[i] = (int16_t)(buf[2 * i] | (buf[2 * i + 1] << 8));
        ring_push(&c->ring, frame);
        c->received++;
    }
    THREAD_RETURN;
}

static int cmd_call(int my_port, const char *peer_ip, int peer_port) {
    static call_t c;                              /* static：很大（ring 有 8 個 frame），不放堆疊上 */
    memset(&c, 0, sizeof(c));
    lock_init(&c.ring.lock);
    c.spk_pos = FRAME_SAMPLES;                    /* 一開始沒有 frame 在播 */
    c.running = 1;

    if (udp_open(&c.u, NULL, my_port) != 0 || udp_set_peer(&c.u, peer_ip, peer_port) != 0) return 1;

    thread_t th;
    if (thread_create(&th, recv_thread, &c) != 0) { fprintf(stderr, "錯誤: 開不了接收執行緒\n"); return 1; }

    if (audio_start_duplex(audio_cb, &c) != 0) {
        c.running = 0;
        thread_join(th);
        udp_close(&c.u);
        return 1;
    }
    printf("通話中：我聽 port %d，送到 %s:%d。請戴耳機。按 Enter 結束。\n", my_port, peer_ip, peer_port);
    fflush(stdout);

    char line[16];
    if (fgets(line, sizeof(line), stdin) == NULL) { /* 等 Enter（stdin 被關掉時 fgets 立刻回傳 NULL，也算結束） */
        line[0] = ' ';
    }

    c.running = 0;
    audio_stop();                                 /* 之後 audio_cb 不會再被叫 */
    thread_join(th);
    udp_close(&c.u);
    printf("結束：送出 %u 包、收到 %u 包、喇叭補靜音 %u 次、ring 滿了丟掉 %u 包\n",
           (unsigned)c.sent, (unsigned)c.received, (unsigned)c.silence, (unsigned)c.ring.dropped);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (udp_global_init() != 0) return 1;
    int rc;

    if (strcmp(argv[1], "devices") == 0) {
        int n = audio_list_devices();
        printf("共 %d 個裝置\n", n);
        rc = 0;
    } else if (strcmp(argv[1], "send") == 0 && argc == 5) {
        int port = parse_port(argv[3]);
        if (port < 0) { fprintf(stderr, "錯誤: port 不合法：%s\n", argv[3]); rc = 2; }
        else rc = cmd_send(argv[2], port, argv[4]);
    } else if (strcmp(argv[1], "recv") == 0 && (argc == 4 || argc == 6)) {
        int port = parse_port(argv[2]);
        int timeout_ms = 2000;
        if (argc == 6) {
            if (strcmp(argv[4], "--timeout") != 0 || atoi(argv[5]) <= 0) { udp_global_cleanup(); return usage(); }
            timeout_ms = atoi(argv[5]);
        }
        if (port < 0) { fprintf(stderr, "錯誤: port 不合法：%s\n", argv[2]); rc = 2; }
        else rc = cmd_recv(port, argv[3], timeout_ms);
    } else if (strcmp(argv[1], "call") == 0 && argc == 5) {
        int my_port = parse_port(argv[2]);
        int peer_port = parse_port(argv[4]);
        if (my_port < 0 || peer_port < 0) { fprintf(stderr, "錯誤: port 不合法\n"); rc = 2; }
        else rc = cmd_call(my_port, argv[3], peer_port);
    } else {
        rc = usage();
    }

    udp_global_cleanup();
    return rc;
}
