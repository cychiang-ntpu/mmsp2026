/*============================================================================
 *  step2_header.c  —  第 2 步：每包加 12 bytes 的 header（magic／type／seq／ts／ssrc）＋ BYE
 *----------------------------------------------------------------------------
 *  用法：
 *    step2_header send <ip> <port> <in.wav>      每 20 ms 送一包 652 bytes（12 header + 640 PCM），最後送 3 次 BYE
 *    step2_header recv <port> <out.wav>          解析 header、每包印一行 seq／ts，仍照到達順序寫 WAV；BYE 或 2 秒沒封包就結束
 *
 *  和第 1 步的差別（diff step1_raw.c step2_header.c）：
 *    送端  組封包時先填 12 bytes 的 header，PCM 往後挪 12 bytes；每包 seq +1、ts +320；送完再送 3 次 BYE；印 STATS role=send
 *    收端  收到一包先檢查 magic 與長度（不對就丟）、讀出 type／seq／ts／ssrc 印出來；收到 BYE 就結束
 *  這一步還是「到達順序 = 播放順序」：經過 udp_impair.py --loss 0.05 時，看印出來的 seq 會跳號，但 WAV 還是會短一截。
 *
 *  封包格式（規格「封包格式」；所有多 byte 欄位 big-endian）：
 *    offset 0   1 byte   magic  0x56（'V'）
 *    offset 1   1 byte   type   0x01 AUDIO、0x02 BYE
 *    offset 2   2 bytes  seq    每包 +1，16-bit 會繞回
 *    offset 4   4 bytes  ts     這包第一個 sample 是整段的第幾個 sample（第 k 包 = 320k）；BYE 的 ts = 真正的 sample 總數
 *    offset 8   4 bytes  ssrc   這次串流的隨機識別碼
 *    offset 12  640      PCM    320 個 16-bit little-endian sample（BYE 沒有這段）
 *===========================================================================*/
#include "udp.h"                 /* 一定要第一個 include */
#include "clock.h"
#include "wav.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>                /* time()：給亂數種子用 */

#define FRAME_SAMPLES 320
#define FRAME_BYTES   (FRAME_SAMPLES * 2)
#define FRAME_MS      20
#define MAX_DGRAM     2048

/* ---- 封包格式的常數 ---- */
#define MAGIC      0x56                          /* 'V'：第 0 個 byte 一定是它，不是就不是我們的封包 */
#define T_AUDIO    0x01                          /* type：一個 20 ms 的 PCM frame */
#define T_BYE      0x02                          /* type：串流結束 */
#define HDR_LEN    12                            /* header 的長度 */
#define PKT_AUDIO  (HDR_LEN + FRAME_BYTES)       /* AUDIO 封包的總長：652 */
#define BYE_REPEAT 3                             /* BYE 送幾次（UDP 可能掉） */

/* 一個解析好的 header：收端用 parse_header 把 bytes 拆成這幾個欄位。
 * struct 就是「把幾個相關的變數綁成一包」，之後用 h.seq、h.ts 取用。 */
typedef struct {
    uint8_t  type;
    uint16_t seq;
    uint32_t ts;
    uint32_t ssrc;
} hdr_t;

static int usage(void) {
    fprintf(stderr,
        "用法:\n"
        "  step2_header send <ip> <port> <in.wav>    每 20 ms 送一包 652 bytes，最後 3 次 BYE\n"
        "  step2_header recv <port> <out.wav>        解析 header、印 seq/ts，照到達順序寫 WAV\n");
    return 2;
}

static int parse_port(const char *s) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v < 1 || v > 65535) return -1;
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

/*--------------------------------------------------------------------------
 * header 的打包與解析
 *-------------------------------------------------------------------------*/
/* 把 12 bytes 的 header 填進 p[0..11]。
 * 不可以宣告一個 struct 直接 memcpy 進封包：C 的 struct 會有對齊的空隙，而且 CPU 是 little-endian，
 * 網路慣例是 big-endian（高位 byte 在前）。所以用 udp.h 的 put_u16／put_u32 一個 byte 一個 byte放：
 *   put_u16(p + 2, 0x1234) → p[2] = 0x12, p[3] = 0x34   （就是 v >> 8 與 v & 0xff） */
static void pack_header(uint8_t *p, uint8_t type, uint16_t seq, uint32_t ts, uint32_t ssrc) {
    p[0] = MAGIC;
    p[1] = type;
    put_u16(p + 2, seq);
    put_u32(p + 4, ts);
    put_u32(p + 8, ssrc);
}

/* 檢查一個收到的 datagram 是不是合法的封包；是就把欄位填進 *h、回傳 1，不是回傳 0（呼叫端丟掉它）。
 * 順序很重要：先看長度夠不夠 12，才能去讀 header（不然會讀到陣列外面）；再看 magic；最後看長度和 type 配不配。 */
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

/*--------------------------------------------------------------------------
 * send：WAV → 每 20 ms 一個 652 bytes 的 datagram，最後 3 次 BYE
 *-------------------------------------------------------------------------*/
static int cmd_send(const char *ip, int port, const char *path) {
    wav_t w;
    if (load_wav_16k_mono(path, &w) != 0) return 2;

    udp_t u;
    if (udp_open(&u, NULL, 0) != 0 || udp_set_peer(&u, ip, port) != 0) { wav_free(&w); return 1; }

    size_t n_frames = (w.n_samples + FRAME_SAMPLES - 1) / FRAME_SAMPLES;
    /* ssrc：這次串流的隨機識別碼。rand() 只有 15 bits 保證，所以拼兩次；種子用時間，每次執行才不一樣 */
    srand((unsigned)time(NULL) ^ (unsigned)clock_ms());
    uint32_t ssrc = ((uint32_t)rand() << 16) ^ (uint32_t)rand();
    printf("send：%s（%u samples → %u 包）→ %s:%d，ssrc=0x%08X，每 %d ms 一包 %d bytes\n",
           path, (unsigned)w.n_samples, (unsigned)n_frames, ip, port, (unsigned)ssrc, FRAME_MS, PKT_AUDIO);
    fflush(stdout);

    uint8_t pkt[PKT_AUDIO];
    uint16_t seq = 0;                             /* 每送一包 +1；uint16_t 到 65535 之後自動繞回 0，正好是規格要的 */
    size_t sent_bytes = 0;
    double t0 = clock_ms();
    for (size_t k = 0; k < n_frames; k++) {
        memset(pkt, 0, sizeof(pkt));
        /* header 在前 12 bytes：第 k 包的 ts = 320k（它的第一個 sample 是整段的第幾個） */
        pack_header(pkt, T_AUDIO, seq, (uint32_t)(k * FRAME_SAMPLES), ssrc);
        /* PCM 從 offset 12 開始 */
        size_t start = k * FRAME_SAMPLES;
        size_t n = w.n_samples - start;
        if (n > FRAME_SAMPLES) n = FRAME_SAMPLES;
        pcm_to_bytes(w.pcm + start, n, pkt + HDR_LEN);

        if (udp_send(&u, pkt, sizeof(pkt)) < 0) { udp_close(&u); wav_free(&w); return 1; }
        sent_bytes += sizeof(pkt);
        seq++;
        sleep_until_ms(t0 + (double)FRAME_MS * (double)(k + 1));
    }
    /* BYE × 3：seq 接著 +1；ts = 真正的 sample 總數（最後一包補的 0 不算），收端用它把輸出截到正確長度 */
    for (int i = 0; i < BYE_REPEAT; i++) {
        uint8_t bye[HDR_LEN];
        pack_header(bye, T_BYE, seq, (uint32_t)w.n_samples, ssrc);
        udp_send(&u, bye, sizeof(bye));
        seq++;
    }
    double duration = clock_ms() - t0;
    printf("送完 %u 包 + %d 次 BYE，花了 %.0f ms\n", (unsigned)n_frames, BYE_REPEAT, duration);
    /* 規格要求的一行（印到 stderr；bytes 只算 AUDIO 封包） */
    fprintf(stderr, "STATS role=send codec=pcm frames=%u bytes=%u duration_ms=%.0f pace=%d\n",
            (unsigned)n_frames, (unsigned)sent_bytes, duration, FRAME_MS);

    udp_close(&u);
    wav_free(&w);
    return 0;
}

/*--------------------------------------------------------------------------
 * recv：解析 header、印出來，仍然照到達順序寫
 *-------------------------------------------------------------------------*/
static int cmd_recv(int port, const char *path) {
    udp_t u;
    if (udp_open(&u, NULL, port) != 0) return 1;
    printf("recv：聽 0.0.0.0:%d → %s；BYE 或 2000 ms 沒封包就結束\n", port, path);
    fflush(stdout);

    int16_t *out = NULL;
    size_t out_len = 0, out_cap = 0;
    size_t n_pkts = 0, n_bad = 0;
    uint8_t buf[MAX_DGRAM];
    int rc_exit = 0;

    for (;;) {
        int wait_ms = (n_pkts == 0) ? 30000 : 2000;
        int k = udp_recv(&u, buf, sizeof(buf), wait_ms, NULL);
        if (k < 0) { rc_exit = 1; break; }
        if (k == 0) {
            if (n_pkts == 0) fprintf(stderr, "30 秒內一個封包都沒收到；送端開了嗎？port 對嗎？\n");
            else printf("2000 ms 沒有封包，當作結束\n");
            break;
        }
        /* ── 收到一包：先解析 header ── */
        hdr_t h;
        if (!parse_header(buf, k, &h)) {          /* magic 不對或長度不對：不是我們的封包，丟掉 */
            n_bad++;
            continue;
        }
        if (h.type == T_BYE) {                    /* 送端說結束了 */
            printf("BYE  seq=%u ts=%u（真正的 sample 數）\n", (unsigned)h.seq, (unsigned)h.ts);
            break;
        }
        printf("seq=%u ts=%u ssrc=0x%08X\n", (unsigned)h.seq, (unsigned)h.ts, (unsigned)h.ssrc);
        /* PCM 從 offset 12 開始，接在 out 後面（還是到達順序！） */
        size_t n = FRAME_SAMPLES;
        if (out_len + n > out_cap) {
            size_t nc = out_cap ? out_cap * 2 : 65536;
            while (nc < out_len + n) nc *= 2;
            int16_t *p = (int16_t *)realloc(out, nc * sizeof(int16_t));
            if (!p) { fprintf(stderr, "錯誤: 記憶體不足\n"); rc_exit = 1; break; }
            out = p;
            out_cap = nc;
        }
        bytes_to_pcm(buf + HDR_LEN, n, out + out_len);
        out_len += n;
        n_pkts++;
    }
    udp_close(&u);

    if (wav_write(path, out, out_len, 16000) != 0) {
        fprintf(stderr, "錯誤: 寫不出 %s\n", path);
        rc_exit = 2;
    }
    printf("收到 %u 包（丟掉 %u 個壞封包），%u samples，寫到 %s\n",
           (unsigned)n_pkts, (unsigned)n_bad, (unsigned)out_len, path);
    free(out);
    return rc_exit;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (udp_global_init() != 0) return 1;
    int rc;

    if (strcmp(argv[1], "send") == 0 && argc == 5) {
        int port = parse_port(argv[3]);
        if (port < 0) { fprintf(stderr, "錯誤: port 不合法：%s\n", argv[3]); rc = 2; }
        else rc = cmd_send(argv[2], port, argv[4]);
    } else if (strcmp(argv[1], "recv") == 0 && argc == 4) {
        int port = parse_port(argv[2]);
        if (port < 0) { fprintf(stderr, "錯誤: port 不合法：%s\n", argv[2]); rc = 2; }
        else rc = cmd_recv(port, argv[3]);
    } else {
        rc = usage();
    }

    udp_global_cleanup();
    return rc;
}
