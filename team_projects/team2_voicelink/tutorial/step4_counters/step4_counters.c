/*============================================================================
 *  step4_counters.c  —  第 4 步：完整的計數器（late／dup／reordered／overflow／延遲）、--plc、--burst
 *----------------------------------------------------------------------------
 *  用法：
 *    step4_counters send <ip> <port> <in.wav> [--burst]
 *    step4_counters recv <port> <out.wav> [--buffer <ms>] [--plc zero|repeat] [--timeout <ms>]
 *
 *  和第 3 步的差別（diff step3_buffer.c step4_counters.c）：
 *    1. 每個收到的 AUDIO 封包都被分到「剛好一類」：dup（同一個 seq 之前收過）、late（它的拍子已經過了）、
 *       放進 buffer（亂序到的另外計 reordered）、overflow（buffer 滿了被丟掉）。
 *       這樣 frames_out = (received − late − dup − overflow) + concealed 一定成立——測試 7 就是驗這條式子。
 *    2. 「之前收過沒」用一張 65536 bits 的表（seen）記：seq 第 n 個 bit = 1 表示收過。8 KB 而已，查一次是 O(1)。
 *    3. buffer 有上限 cap = 4 × buffer 的 frame 數（--burst 一口氣全送時才不會塞爆）；環狀陣列放大到 64 格。
 *    4. 每個真的播出去的 frame 記「到達 → 播放」的延遲，印 avg_delay_ms／max_delay_ms。
 *    5. --plc zero：補 320 個 0；repeat（預設）：重複上一個播出的 frame。
 *    6. send --burst：不等 20 ms，一口氣全送——拿來看 overflow 怎麼發生。
 *===========================================================================*/
#include "udp.h"
#include "clock.h"
#include "wav.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define FRAME_SAMPLES 320
#define FRAME_BYTES   (FRAME_SAMPLES * 2)
#define FRAME_MS      20
#define MAX_DGRAM     2048

#define MAGIC      0x56
#define T_AUDIO    0x01
#define T_BYE      0x02
#define HDR_LEN    12
#define PKT_AUDIO  (HDR_LEN + FRAME_BYTES)
#define BYE_REPEAT 3

#define RING_SLOTS  64                           /* jitter buffer 的格數（要 ≥ cap）；第 seq 包放在第 seq % 64 格 */
#define FIRST_WAIT  30000                        /* 第一包最多等 30 秒（不是永久等） */

typedef struct {
    uint8_t  type;
    uint16_t seq;
    uint32_t ts;
    uint32_t ssrc;
} hdr_t;

static int usage(void) {
    fprintf(stderr,
        "用法:\n"
        "  step4_counters send <ip> <port> <in.wav> [--burst]\n"
        "  step4_counters recv <port> <out.wav> [--buffer <ms>] [--plc zero|repeat] [--timeout <ms>]\n"
        "      --burst    不等 20 ms，一口氣全送（看 overflow 用）\n"
        "      --buffer   jitter buffer 的目標延遲，20 的倍數（預設 60 = 先存 3 包再播；上限 = 4 倍）\n"
        "      --plc      掉包時補什麼：zero 補靜音、repeat 重複上一個 frame（預設）\n"
        "      --timeout  這麼久沒有封包就當作結束（預設 2000）\n");
    return 2;
}

static int parse_port(const char *s) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v < 1 || v > 65535) return -1;
    return (int)v;
}

/* "60" → 60；不是正整數回傳 -1（--buffer／--timeout 用） */
static int parse_int(const char *s) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v < 1 || v > 1000000) return -1;
    return (int)v;
}

static void pcm_to_bytes(const int16_t *pcm, size_t n, uint8_t *out) {
    for (size_t i = 0; i < n; i++) {
        uint16_t v = (uint16_t)pcm[i];
        out[2 * i]     = (uint8_t)(v & 0xff);
        out[2 * i + 1] = (uint8_t)(v >> 8);
    }
}

static void bytes_to_pcm(const uint8_t *in, size_t n, int16_t *pcm) {
    for (size_t i = 0; i < n; i++) pcm[i] = (int16_t)(in[2 * i] | (in[2 * i + 1] << 8));
}

static int load_wav_16k_mono(const char *path, wav_t *w) {
    int rc = wav_read(path, w);
    if (rc == -1) { fprintf(stderr, "錯誤: 讀不到 %s\n", path); return 2; }
    if (rc == -2) { fprintf(stderr, "錯誤: %s 不是 16-bit PCM 的 WAV\n", path); return 2; }
    if (w->sample_rate != 16000 || w->channels != 1 || w->bits != 16) {
        fprintf(stderr, "錯誤: %s 是 %u Hz／%u 聲道／%u-bit；只接受 16000 Hz／1 聲道／16-bit\n",
                path, (unsigned)w->sample_rate, (unsigned)w->channels, (unsigned)w->bits);
        wav_free(w);
        return 2;
    }
    return 0;
}

static void pack_header(uint8_t *p, uint8_t type, uint16_t seq, uint32_t ts, uint32_t ssrc) {
    p[0] = MAGIC;
    p[1] = type;
    put_u16(p + 2, seq);
    put_u32(p + 4, ts);
    put_u32(p + 8, ssrc);
}

static int parse_header(const uint8_t *p, int len, hdr_t *h) {
    if (len < HDR_LEN || p[0] != MAGIC) return 0;
    h->type = p[1];
    h->seq  = get_u16(p + 2);
    h->ts   = get_u32(p + 4);
    h->ssrc = get_u32(p + 8);
    if (h->type == T_AUDIO && len == PKT_AUDIO) return 1;
    if (h->type == T_BYE && len == HDR_LEN) return 1;
    return 0;
}

/* 16-bit seq 的比較：a 比 b 新幾個？回傳 a − b（正 = a 比較新、負 = a 比較舊、0 = 同一包）。
 * seq 是 uint16_t，65535 的下一個是 0；直接比大小會以為 0 < 65535 是「很舊」。
 * 做法：先用無號數相減（會自動 mod 65536），再把結果當成有號的 16-bit 看：
 *   a = 0, b = 65535 → (uint16_t)(0 − 65535) = 1   → (int16_t)1 = +1       a 比 b 新 1 包 ✓
 *   a = 65534, b = 2 → (uint16_t)(65534 − 2) = 65532 → (int16_t)65532 = −4   a 比 b 舊 4 包 ✓
 * 只要兩個 seq 的距離小於 32768（約 11 分鐘的封包）就對。RTP 也是這樣比的。 */
static int seq_diff(uint16_t a, uint16_t b) {
    return (int16_t)(uint16_t)(a - b);
}

/*--------------------------------------------------------------------------
 * jitter buffer
 *-------------------------------------------------------------------------*/
/* 一格：有沒有東西、是哪個 seq、什麼時候到的、320 個 sample */
typedef struct {
    int      valid;
    uint16_t seq;
    double   arrived;                            /* 到達時刻（clock_ms），算 avg_delay 用 */
    int16_t  pcm[FRAME_SAMPLES];
} slot_t;

/* 整個 jitter buffer 的狀態都放在一個 struct 裡；函式用指標 jb_t *jb 操作它（「輸出參數」的寫法）。
 *
 *   環狀陣列（ring）：16 格，seq 的包放在第 seq % 16 格。
 *       seq:   16 17 18 19  4  5  6  7  8  9 10 11 12 13 14 15      ← 每格放的 seq（例）
 *       slot:   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
 *   「下一個該播的在不在」只要看第 expected % 16 格裡的 seq 是不是 expected，不用搜尋、不用排序。
 *   亂序到的包自動排在正確的位置（因為位置由 seq 決定，不是由到達順序決定）。 */
typedef struct {
    slot_t   slot[RING_SLOTS];
    int      count;                              /* 目前有幾格是 valid */
    int      cap;                                /* 上限：最多存幾格（4 × buffer 的 frame 數），超過就丟最舊的 */
    int      plc_zero;                           /* 1 = --plc zero；0 = repeat */
    uint8_t  seen[65536 / 8];                    /* 位元表：seq 收過沒（第 seq 個 bit）。dup 與 lost 都靠它 */
    int      started;                            /* 播放時鐘開始走了沒 */
    uint16_t expected;                           /* 下一個該播的 seq（開播後才有意義） */
    int      have_first;  uint16_t first_seq;    /* 開播前看過最小的 seq：開播就從它開始 */
    int      have_highest; uint16_t highest;     /* 看過最大的 seq */
    int      have_end;    uint16_t end_seq;      /* 串流到哪裡結束（最後一個 AUDIO 的 seq + 1）；BYE 或逾時時才知道 */
    int16_t  last[FRAME_SAMPLES];                /* 上一個播出去的真 frame（PLC repeat 用）；一開始是 0 */
    /* 計數器（STATS） */
    unsigned received;                           /* 合法的 AUDIO 封包數 */
    unsigned unique;                             /* 不同 seq 的封包數（算 lost 用：遺失 = 應該有的 − 真的到的） */
    unsigned frames_out;                         /* 交出去的 frame 數（真的 + 補的） */
    unsigned concealed;                          /* PLC 補的 frame 數 */
    unsigned late, dup, reordered, overflow;     /* 規格的四個計數器 */
    unsigned trailing_concealed;                 /* 「連續」補了幾個（逾時收尾時要去掉） */
    unsigned played_real;                        /* 用真封包播的 frame 數 */
    double   delay_sum, delay_max;               /* 到達 → 播放 的延遲（ms）總和與最大值 */
} jb_t;

/* 位元表：65536 個 seq 各佔 1 bit。第 seq 個 bit 在第 seq/8 個 byte 的第 seq%8 位。 */
static int seen_get(const jb_t *jb, uint16_t seq) { return (jb->seen[seq >> 3] >> (seq & 7)) & 1; }
static void seen_set(jb_t *jb, uint16_t seq)      { jb->seen[seq >> 3] |= (uint8_t)(1u << (seq & 7)); }

/* buffer 滿了：找出最舊的（seq 最小的）那一格丟掉 */
static void jb_drop_oldest(jb_t *jb) {
    int oldest = -1;
    for (int i = 0; i < RING_SLOTS; i++) {
        if (!jb->slot[i].valid) continue;
        if (oldest < 0 || seq_diff(jb->slot[i].seq, jb->slot[oldest].seq) < 0) oldest = i;
    }
    if (oldest >= 0) { jb->slot[oldest].valid = 0; jb->count--; jb->overflow++; }
}

/* 收到一個合法的 AUDIO：分類（dup／late／reordered／overflow）後放進 buffer 或丟掉。now = 到達時刻。
 * 每個封包剛好落在一類，所以 received = dup + late + overflow + 真的播出去的（buffer 最後會排空）。 */
static void jb_put(jb_t *jb, uint16_t seq, const int16_t *pcm, double now) {
    jb->received++;
    /* 1. 重複：這個 seq 之前收過（還在 buffer 裡、或已經播過、或遲到過）→ 丟掉 */
    if (seen_get(jb, seq)) { jb->dup++; return; }
    seen_set(jb, seq);
    jb->unique++;                                /* 第一次看到這個 seq：它有到（算 lost 時要扣掉） */
    /* 2. 遲到：它的拍子已經過了（seq 比 expected 舊）→ 丟掉 */
    if (jb->started && seq_diff(seq, jb->expected) < 0) { jb->late++; return; }
    /* 3. 亂序：比看過最大的小，但還來得及 → 計 reordered，照樣放進去（位置由 seq 決定，自動排對） */
    if (jb->have_highest && seq_diff(seq, jb->highest) < 0) jb->reordered++;
    else { jb->highest = seq; jb->have_highest = 1; }
    if (!jb->started && (!jb->have_first || seq_diff(seq, jb->first_seq) < 0)) {
        jb->first_seq = seq;                     /* 開播前看到更小的 seq（第一包可能亂序到）：從它開始播 */
        jb->have_first = 1;
    }
    /* 4. 放進第 seq % 64 格 */
    slot_t *s = &jb->slot[seq % RING_SLOTS];
    if (s->valid) {                              /* 格子被別的 seq 占著（距離 ≥ 64 包：buffer 太小或 --burst） */
        if (seq_diff(seq, s->seq) < 0) { jb->overflow++; return; }   /* 占著的比較新：丟新來的這個舊包 */
        jb->overflow++;                          /* 占著的比較舊：丟它、放新的（count 不變） */
    } else {
        jb->count++;
    }
    s->valid = 1;
    s->seq = seq;
    s->arrived = now;
    memcpy(s->pcm, pcm, FRAME_BYTES);
    /* 5. 超過上限：丟最舊的 */
    if (jb->count > jb->cap) jb_drop_oldest(jb);
}

/* 開播：下一個該播的 = 開播前看過最小的 seq */
static void jb_start(jb_t *jb) {
    jb->expected = jb->have_first ? jb->first_seq : 0;
    jb->started = 1;
}

/* 播放時鐘的一拍：把要播的 320 個 sample 填進 out。有真的就播真的，沒有就 PLC 補。 */
static void jb_tick(jb_t *jb, int16_t *out, double now) {
    slot_t *s = &jb->slot[jb->expected % RING_SLOTS];
    if (s->valid && s->seq == jb->expected) {    /* ── 有：播它 ── */
        memcpy(out, s->pcm, FRAME_BYTES);
        memcpy(jb->last, s->pcm, FRAME_BYTES);   /* 記下來，下次掉包時 repeat 用 */
        s->valid = 0;
        jb->count--;
        jb->played_real++;
        double d = now - s->arrived;             /* 這包在 buffer 裡等了多久 */
        jb->delay_sum += d;
        if (d > jb->delay_max) jb->delay_max = d;
        jb->trailing_concealed = 0;
    } else {                                     /* ── 沒有：PLC 補一個 ── */
        if (jb->plc_zero) memset(out, 0, FRAME_BYTES);      /* zero：補靜音（波形跳到 0，會有「喀」聲） */
        else memcpy(out, jb->last, FRAME_BYTES);            /* repeat：重複上一個播出的 frame（連續掉包會變「嗡」聲） */
        jb->concealed++;
        jb->trailing_concealed++;
    }
    jb->expected++;                              /* 不管有沒有，拍子都前進（uint16_t 自動繞回） */
    jb->frames_out++;
}

/* 知道串流到哪裡結束了（BYE 算出來的、或逾時時看過的最大 seq + 1）。之後一直 tick 到 expected == end_seq。 */
static void jb_finish_at(jb_t *jb, uint16_t end) {
    if (jb->have_highest && seq_diff((uint16_t)(jb->highest + 1), end) > 0) end = (uint16_t)(jb->highest + 1);
    jb->end_seq = end;
    jb->have_end = 1;
}

/* 播完了沒：知道結尾，而且 expected 已經走到結尾 */
static int jb_done(const jb_t *jb) {
    return jb->have_end && jb->started && seq_diff(jb->expected, jb->end_seq) >= 0;
}

/* lost：從第一個播的 seq 到結尾，應該有幾包 − 真的到過幾包（含遲到的） */
static unsigned jb_lost(const jb_t *jb) {
    if (!jb->have_first || !jb->have_end) return 0;
    unsigned span = (uint16_t)(jb->end_seq - jb->first_seq);
    return span > jb->unique ? span - jb->unique : 0;
}

/*--------------------------------------------------------------------------
 * send（和第 2 步相同）
 *-------------------------------------------------------------------------*/
static int cmd_send(const char *ip, int port, const char *path, int burst) {
    wav_t w;
    if (load_wav_16k_mono(path, &w) != 0) return 2;

    udp_t u;
    if (udp_open(&u, NULL, 0) != 0 || udp_set_peer(&u, ip, port) != 0) { wav_free(&w); return 1; }

    size_t n_frames = (w.n_samples + FRAME_SAMPLES - 1) / FRAME_SAMPLES;
    srand((unsigned)time(NULL) ^ (unsigned)clock_ms());
    uint32_t ssrc = ((uint32_t)rand() << 16) ^ (uint32_t)rand();
    printf("send：%s（%u samples → %u 包）→ %s:%d，ssrc=0x%08X，每 %d ms 一包 %d bytes\n",
           path, (unsigned)w.n_samples, (unsigned)n_frames, ip, port, (unsigned)ssrc, FRAME_MS, PKT_AUDIO);
    fflush(stdout);

    uint8_t pkt[PKT_AUDIO];
    uint16_t seq = 0;
    size_t sent_bytes = 0;
    double t0 = clock_ms();
    for (size_t k = 0; k < n_frames; k++) {
        memset(pkt, 0, sizeof(pkt));
        pack_header(pkt, T_AUDIO, seq, (uint32_t)(k * FRAME_SAMPLES), ssrc);
        size_t start = k * FRAME_SAMPLES;
        size_t n = w.n_samples - start;
        if (n > FRAME_SAMPLES) n = FRAME_SAMPLES;
        pcm_to_bytes(w.pcm + start, n, pkt + HDR_LEN);
        if (udp_send(&u, pkt, sizeof(pkt)) < 0) { udp_close(&u); wav_free(&w); return 1; }
        sent_bytes += sizeof(pkt);
        seq++;
        if (!burst) sleep_until_ms(t0 + (double)FRAME_MS * (double)(k + 1));   /* --burst：不等，馬上送下一包 */
    }
    for (int i = 0; i < BYE_REPEAT; i++) {
        uint8_t bye[HDR_LEN];
        pack_header(bye, T_BYE, seq, (uint32_t)w.n_samples, ssrc);
        udp_send(&u, bye, sizeof(bye));
        seq++;
    }
    double duration = clock_ms() - t0;
    printf("送完 %u 包 + %d 次 BYE，花了 %.0f ms\n", (unsigned)n_frames, BYE_REPEAT, duration);
    fprintf(stderr, "STATS role=send codec=pcm frames=%u bytes=%u duration_ms=%.0f pace=%d\n",
            (unsigned)n_frames, (unsigned)sent_bytes, duration, burst ? 0 : FRAME_MS);

    udp_close(&u);
    wav_free(&w);
    return 0;
}

/*--------------------------------------------------------------------------
 * recv：收封包 → jitter buffer → 每 20 ms 一拍播出 → WAV ＋ STATS
 *-------------------------------------------------------------------------*/
/* 輸出緩衝（可長大的 sample 陣列）也包成一個 struct，append 一個 frame 就是一個函式 */
typedef struct {
    int16_t *pcm;
    size_t   len, cap;
} outbuf_t;

static int outbuf_append(outbuf_t *o, const int16_t *frame) {
    if (o->len + FRAME_SAMPLES > o->cap) {
        size_t nc = o->cap ? o->cap * 2 : 65536;
        int16_t *p = (int16_t *)realloc(o->pcm, nc * sizeof(int16_t));
        if (!p) { fprintf(stderr, "錯誤: 記憶體不足\n"); return -1; }
        o->pcm = p;
        o->cap = nc;
    }
    memcpy(o->pcm + o->len, frame, FRAME_BYTES);
    o->len += FRAME_SAMPLES;
    return 0;
}

static int cmd_recv(int port, const char *path, int buffer_ms, int plc_zero, int timeout_ms) {
    if (buffer_ms % FRAME_MS != 0) { fprintf(stderr, "錯誤: --buffer 要是 %d 的倍數\n", FRAME_MS); return 2; }
    int buffer_frames = buffer_ms / FRAME_MS;
    if (buffer_frames * 4 > RING_SLOTS) {
        fprintf(stderr, "錯誤: --buffer 最多 %d ms\n", RING_SLOTS / 4 * FRAME_MS);
        return 2;
    }

    udp_t u;
    if (udp_open(&u, NULL, port) != 0) return 1;
    printf("recv：聽 0.0.0.0:%d，buffer=%d ms（%d 包，上限 %d），plc=%s，timeout=%d ms → %s\n",
           port, buffer_ms, buffer_frames, buffer_frames * 4, plc_zero ? "zero" : "repeat", timeout_ms, path);
    fflush(stdout);

    static jb_t jb;                              /* static：很大（16 × 640 bytes），不放堆疊上；static 也保證一開始全是 0 */
    memset(&jb, 0, sizeof(jb));
    jb.cap = buffer_frames * 4;
    jb.plc_zero = plc_zero;
    outbuf_t out = { NULL, 0, 0 };
    uint8_t buf[MAX_DGRAM];
    int16_t frame[FRAME_SAMPLES];
    int rc_exit = 0;

    /* 時鐘：
     *   play_t0   開播時刻 = 第一包到達 + buffer ms；第 i 拍在 play_t0 + 20×i（絕對時刻，誤差不累積）
     *   last_rx   最後一次收到合法封包的時刻；超過 timeout 沒封包就當作送端死了
     *   bye_ts    BYE 的 ts（真正的 sample 數），結束時用來截掉最後一包補的 0 */
    double t_start = clock_ms();
    double play_t0 = 0;  int have_play_t0 = 0;
    double last_rx = 0;  int have_last_rx = 0;
    uint32_t bye_ts = 0; int have_bye = 0;

    for (;;) {
        double now = clock_ms();
        if (jb_done(&jb)) break;

        /* 1. 算「下一件事」的時刻：下一拍、或逾時——看哪個先到 */
        double deadline = have_last_rx ? last_rx + timeout_ms : t_start + FIRST_WAIT;
        if (have_play_t0) {
            double next_tick = play_t0 + (double)FRAME_MS * jb.frames_out;   /* 開播前 frames_out = 0：next_tick 就是開播時刻 */
            if (next_tick < deadline) deadline = next_tick;
        }
        /* 2. 等封包，最多等到 deadline（這就是「不用執行緒」的關鍵：等待的時間剛好是離下一拍的時間） */
        double wait = deadline - now;
        int wait_ms = wait <= 0 ? 0 : (int)(wait + 0.999);                   /* 無條件進位到整數毫秒 */
        int k = udp_recv(&u, buf, sizeof(buf), wait_ms, NULL);
        if (k < 0) { rc_exit = 1; break; }
        now = clock_ms();

        /* 3. 收到一包 */
        if (k > 0) {
            hdr_t h;
            if (parse_header(buf, k, &h)) {
                last_rx = now; have_last_rx = 1;
                if (h.type == T_AUDIO) {
                    if (!have_play_t0) {         /* 第一包：buffer ms 之後開播 */
                        play_t0 = now + buffer_ms;
                        have_play_t0 = 1;
                        printf("第一包到了（seq=%u），%d ms 後開播\n", (unsigned)h.seq, buffer_ms);
                    }
                    bytes_to_pcm(buf + HDR_LEN, FRAME_SAMPLES, frame);
                    jb_put(&jb, h.seq, frame, now);
                } else if (!jb.have_end) {       /* BYE：知道結尾了。BYE 的 seq = 最後一個 AUDIO 的 seq + 1 */
                    /* 三個 BYE 的 seq 各 +1，第一個掉了就會多算；用 ts（真正的 sample 數）交叉比對：
                     * ceil(ts/320) 就是 frame 數 = 結尾的 seq；和 BYE 的 seq 差 0、1、2 都合理，採用 ts 算的 */
                    uint16_t end = h.seq;
                    uint16_t end_ts = (uint16_t)((h.ts + FRAME_SAMPLES - 1) / FRAME_SAMPLES);
                    int d = seq_diff(end, end_ts);
                    if (h.ts > 0 && d >= 0 && d < BYE_REPEAT) end = end_ts;
                    jb_finish_at(&jb, end);
                    bye_ts = h.ts; have_bye = 1;
                    if (!have_play_t0) { play_t0 = now; have_play_t0 = 1; }   /* 還沒開播就收到 BYE（極短的檔）：馬上開播 */
                    printf("BYE（seq=%u ts=%u）：結尾 = seq %u，把 buffer 播完再結束\n",
                           (unsigned)h.seq, (unsigned)h.ts, (unsigned)jb.end_seq);
                }
            }
        }

        /* 4. 逾時：送端不見了。看過的最大 seq + 1 就是結尾 */
        if (have_last_rx && !jb.have_end && now - last_rx >= timeout_ms) {
            printf("%d ms 沒有封包，當作結束\n", timeout_ms);
            if (!have_play_t0) { play_t0 = now; have_play_t0 = 1; }
            if (!jb.started) jb_start(&jb);
            jb_finish_at(&jb, jb.have_highest ? (uint16_t)(jb.highest + 1) : jb.expected);
            /* 等待的這 timeout ms 裡，時鐘一直在補 frame；那些是「送端死掉之後」的拍子，不是遺失補償，從輸出去掉 */
            int extra = seq_diff(jb.expected, jb.end_seq);
            if (extra < 0) extra = 0;
            if ((unsigned)extra > jb.trailing_concealed) extra = (int)jb.trailing_concealed;
            jb.expected = (uint16_t)(jb.expected - extra);
            jb.frames_out -= (unsigned)extra;
            jb.concealed -= (unsigned)extra;
            out.len -= (size_t)extra * FRAME_SAMPLES;
            if (extra) printf("最後 %d 個是送端不見之後補的 frame，從輸出去掉\n", extra);
        }
        if (!have_last_rx && now - t_start >= FIRST_WAIT) {
            fprintf(stderr, "%d 秒內一個封包都沒收到；送端開了嗎？port 對嗎？\n", FIRST_WAIT / 1000);
            break;
        }

        /* 5. 播放時鐘：到了拍子就播。while：程式停頓太久的話一次補齊好幾拍，frames_out 才會和時間一致 */
        if (have_play_t0) {
            if (!jb.started) {
                if (now < play_t0) continue;
                jb_start(&jb);
                printf("開播：expected=%u，buffer 裡有 %d 包\n", (unsigned)jb.expected, jb.count);
            }
            while (now >= play_t0 + (double)FRAME_MS * jb.frames_out && !jb_done(&jb)) {
                jb_tick(&jb, frame, now);
                if (outbuf_append(&out, frame) != 0) { rc_exit = 1; break; }
            }
            if (rc_exit) break;
        }
    }
    udp_close(&u);

    /* BYE 的 ts 是真正的 sample 數：只有當它落在最後一包裡（去掉的是補的 0、不是真的聲音）才採用 */
    if (have_bye && out.len >= FRAME_SAMPLES && bye_ts > out.len - FRAME_SAMPLES && bye_ts <= out.len) out.len = bye_ts;

    if (rc_exit == 0 && wav_write(path, out.pcm, out.len, 16000) != 0) {
        fprintf(stderr, "錯誤: 寫不出 %s\n", path);
        rc_exit = 2;
    }
    printf("已存檔：%s（%u samples，%u frames）\n", path, (unsigned)out.len, jb.frames_out);
    double avg = jb.played_real ? jb.delay_sum / jb.played_real : 0.0;
    /* 規格的 STATS：欄位順序與名稱不可改（評測用正規表示式抓） */
    fprintf(stderr, "STATS role=recv buffer_ms=%d plc=%s codec=pcm frames_out=%u received=%u lost=%u "
            "late=%u dup=%u reordered=%u overflow=%u concealed=%u avg_delay_ms=%.1f max_delay_ms=%.1f\n",
            buffer_ms, plc_zero ? "zero" : "repeat", jb.frames_out, jb.received, jb_lost(&jb),
            jb.late, jb.dup, jb.reordered, jb.overflow, jb.concealed, avg, jb.delay_max);
    free(out.pcm);
    return rc_exit;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (udp_global_init() != 0) return 1;
    int rc;

    if (strcmp(argv[1], "send") == 0 && (argc == 5 || argc == 6)) {
        int port = parse_port(argv[3]);
        int burst = (argc == 6 && strcmp(argv[5], "--burst") == 0);
        if (port < 0) { fprintf(stderr, "錯誤: port 不合法：%s\n", argv[3]); rc = 2; }
        else if (argc == 6 && !burst) rc = usage();
        else rc = cmd_send(argv[2], port, argv[4], burst);
    } else if (strcmp(argv[1], "recv") == 0 && argc >= 4) {
        int port = parse_port(argv[2]);
        int buffer_ms = 60, timeout_ms = 2000, plc_zero = 0;
        int ok = (port >= 0);
        if (!ok) fprintf(stderr, "錯誤: port 不合法：%s\n", argv[2]);
        /* 選項：成對出現（名字、值），從 argv[4] 開始兩個兩個看 */
        for (int i = 4; ok && i < argc; i += 2) {
            const char *name = argv[i];
            const char *val = (i + 1 < argc) ? argv[i + 1] : NULL;
            if (strcmp(name, "--buffer") == 0 && val && parse_int(val) > 0)       buffer_ms = parse_int(val);
            else if (strcmp(name, "--timeout") == 0 && val && parse_int(val) > 0) timeout_ms = parse_int(val);
            else if (strcmp(name, "--plc") == 0 && val && strcmp(val, "zero") == 0)   plc_zero = 1;
            else if (strcmp(name, "--plc") == 0 && val && strcmp(val, "repeat") == 0) plc_zero = 0;
            else { fprintf(stderr, "錯誤: 不認識的選項或值不對：%s\n", name); ok = 0; }
        }
        rc = ok ? cmd_recv(port, argv[3], buffer_ms, plc_zero, timeout_ms) : 2;
    } else {
        rc = usage();
    }

    udp_global_cleanup();
    return rc;
}
