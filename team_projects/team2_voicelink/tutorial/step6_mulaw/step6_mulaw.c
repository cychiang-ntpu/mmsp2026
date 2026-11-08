/*============================================================================
 *  step6_mulaw.c  —  第 6 步：功能 4 語音壓縮：--codec mulaw（16-bit → 8-bit，每包 320 bytes，位元率減半）
 *----------------------------------------------------------------------------
 *  用法：
 *    step6_mulaw send <ip> <port> <in.wav> [--burst] [--seq0 <n>] [--codec pcm|mulaw]
 *    step6_mulaw recv <port> <out.wav> [--bind <ip>] [--buffer <ms>] [--plc zero|repeat] [--timeout <ms>]
 *        recv 不用指定 codec：以收到的第一個合法 AUDIO 封包的 type 為準
 *
 *  和第 5 步的差別（diff step5_robust.c step6_mulaw.c）：
 *    1. μ-law（G.711 的想法，μ = 255；第 9 週 2.2 與 examples/mulaw_demo.py 的那條曲線）：
 *         壓縮  y = sign(x)·ln(1 + μ|x|)／ln(1 + μ)        x 是正規化到 [-1, 1] 的 sample（x = s / 32768）
 *         量化  q = clamp(round(y × 128), -128, 127)       256 階均勻量化；存成 1 byte = q + 128（0…255）
 *         展開  x' = sign(q)·((1 + μ)^(|q|/128) − 1)／μ    s' = round(x' × 32768)
 *       小訊號的階距小、大訊號的階距大，所以 SQNR 幾乎不隨音量變（線性 8-bit 小聲時會很糟）。
 *       程式一開始把兩張表建好：enc[65536]（每個 16-bit sample → 1 byte）、dec[256]（每個 byte → 16-bit）；
 *       之後每個 sample 只是查一次表，比呼叫 log／pow 快幾百倍。
 *    2. 封包 type 0x04 AUDIO_MULAW，payload 320 bytes（一個 sample 一個 byte），封包總長 332。
 *    3. send --codec mulaw：組封包時先把 640 bytes 的 PCM 壓成 320 bytes；STATS 的 codec 與 bytes 跟著變（bytes 約一半）。
 *    4. recv：第一個合法 AUDIO 的 type 決定這次串流的 codec；之後 type 不同的當壞封包。收到的包「先解碼成 16-bit，再進
 *       jitter buffer」，所以 buffer 與 PLC 一行都不用改——repeat 重複的是解碼後的 frame。
 *    5. 輸出不會逐 byte 相同（有失真），驗收看 SQNR：老師的語音檔實測約 37 dB（規格 ≥ 30）。
 *===========================================================================*/
/* ---- 下面這些 .h 是課程自己寫的 wrapper，不是系統函式庫：都在 ../../baseline/wrapper/（Makefile 用 -I 指過去）。
 *   udp.h 一定要是第一個 #include：它在任何系統標頭之前定義 _POSIX_C_SOURCE（macOS／Linux 才看得到 select／clock_gettime），
 *   而且 Windows 的 winsock2.h 必須排在 windows.h 前面；放第一行兩邊都安全（和 Team 1 的 platform.h 同一個道理）。
 *   udp.h    UDP socket 的薄包裝（Windows 底下是 Winsock、macOS／Linux 是 POSIX socket）＋ put_u16／put_u32／get_u16／get_u32（big-endian）
 *   clock.h  單調時鐘：clock_ms（毫秒）、sleep_until_ms（睡到某個絕對時刻）
 *   wav.h    讀寫 WAV：wav_read／wav_write（wav_t）
 */
#include "udp.h"                 /* 一定排第一（原因見上面） */
#include "clock.h"
#include "wav.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>                /* log、pow、lrint：建 μ-law 表用（Linux 要連結 -lm） */

#define FRAME_SAMPLES 320
#define FRAME_BYTES   (FRAME_SAMPLES * 2)
#define FRAME_MS      20
#define MAX_DGRAM     2048

#define MAGIC      0x56
#define T_AUDIO    0x01
#define T_BYE      0x02
#define T_MULAW    0x04                          /* type：μ-law 壓縮的 frame，320 bytes */
#define HDR_LEN    12
#define PKT_AUDIO  (HDR_LEN + FRAME_BYTES)       /* 652 */
#define PKT_MULAW  (HDR_LEN + FRAME_SAMPLES)     /* 332 */
#define MU         255.0                         /* μ-law 的 μ */
#define BYE_REPEAT 3

#define RING_SLOTS  64                           /* jitter buffer 的格數（要 ≥ cap）；第 seq 包放在第 seq % 64 格 */
#define FIRST_WAIT_MIN 30000                     /* 第一包最多等 max(10 × timeout, 30 秒)（不是永久等） */

typedef struct {
    uint8_t  type;
    uint16_t seq;
    uint32_t ts;
    uint32_t ssrc;
} hdr_t;

static int usage(void) {
    fprintf(stderr,
        "用法:\n"
        "  step6_mulaw send <ip> <port> <in.wav> [--burst] [--seq0 <n>] [--codec pcm|mulaw]\n"
        "  step6_mulaw recv <port> <out.wav> [--bind <ip>] [--buffer <ms>] [--plc zero|repeat] [--timeout <ms>]\n"
        "      --codec    pcm：16-bit 原樣 640 bytes（預設）；mulaw：8-bit 320 bytes（recv 自動依封包 type 判斷）\n"
        "      --burst    不等 20 ms，一口氣全送（看 overflow 用）\n"
        "      --seq0     第一包的 seq（預設 0；給 65500 可以看 16-bit 繞回）\n"
        "      --bind     只收這個本機 IP 的封包（預設 0.0.0.0 = 全部）\n"
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
    if (h->type == T_MULAW && len == PKT_MULAW) return 1;
    if (h->type == T_BYE && len == HDR_LEN) return 1;
    return 0;
}

/* type → 名字（STATS 用） */
static const char *codec_name(uint8_t type) { return type == T_MULAW ? "mulaw" : "pcm"; }

/*--------------------------------------------------------------------------
 * μ-law：兩張查表
 *-------------------------------------------------------------------------*/
static uint8_t mulaw_enc[65536];                 /* 索引 = sample + 32768（把 -32768…32767 平移成 0…65535） */
static int16_t mulaw_dec[256];                   /* 索引 = 收到的 byte */

/* 夾在 [lo, hi] 之間 */
static long clampl(long v, long lo, long hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* 程式開始時叫一次：用公式把兩張表算好。lrint = 四捨五入到最接近的整數（.5 時取偶數，和 Python 的 round 一樣）。 */
static void mulaw_init(void) {
    for (long sv = -32768; sv <= 32767; sv++) {
        double x = (double)sv / 32768.0;                                    /* 正規化到 [-1, 1) */
        double y = log(1.0 + MU * fabs(x)) / log(1.0 + MU);                 /* 壓縮：log 曲線，結果在 [0, 1] */
        if (x < 0) y = -y;                                                  /* 符號另外處理（copysign） */
        long q = clampl(lrint(y * 128.0), -128, 127);                       /* 256 階均勻量化 */
        mulaw_enc[sv + 32768] = (uint8_t)(q + 128);                         /* 存成 0…255 */
    }
    for (int b = 0; b < 256; b++) {
        double q = (double)(b - 128) / 128.0;                               /* 還原成 [-1, 1) 的 y */
        double x = (pow(1.0 + MU, fabs(q)) - 1.0) / MU;                     /* 展開：log 的反函數 */
        if (q < 0) x = -x;
        mulaw_dec[b] = (int16_t)clampl(lrint(x * 32768.0), -32768, 32767);
    }
}

/* 320 個 16-bit sample → 320 bytes */
static void mulaw_encode(const int16_t *pcm, uint8_t *out) {
    for (int i = 0; i < FRAME_SAMPLES; i++) out[i] = mulaw_enc[pcm[i] + 32768];
}

/* 320 bytes → 320 個 16-bit sample */
static void mulaw_decode(const uint8_t *in, int16_t *pcm) {
    for (int i = 0; i < FRAME_SAMPLES; i++) pcm[i] = mulaw_dec[in[i]];
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
static void seen_clear(jb_t *jb, uint16_t seq)    { jb->seen[seq >> 3] &= (uint8_t)~(1u << (seq & 7)); }

/* 換了 ssrc（對方重啟、新的串流）：buffer、序號、播放進度全部從頭來；計數器（received、lost ...）不清。
 * 不能用 memset 整個清掉：cap、plc_zero 與計數器要留著，所以一個欄位一個欄位處理。 */
static void jb_reset(jb_t *jb) {
    for (int i = 0; i < RING_SLOTS; i++) jb->slot[i].valid = 0;
    memset(jb->seen, 0, sizeof(jb->seen));
    jb->count = 0;
    jb->started = 0;
    jb->have_first = jb->have_highest = jb->have_end = 0;
    jb->trailing_concealed = 0;
}

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
    /* 6. BYE 已經到了、但這包比 BYE 算出的結尾還後面（BYE 被亂序超車）：把結尾往後推，不然它永遠不會被播 */
    if (jb->have_end && seq_diff(seq, jb->end_seq) >= 0) jb->end_seq = (uint16_t)(seq + 1);
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
    /* seq 繞回的準備：把「正對面」（半圈之外）那個 seq 的 seen 位元清掉。每一拍清一個，拍子連續，
     * 所以當 seq 走完一圈（65536 包 ≈ 22 分鐘）回來時，那些 bit 早就是 0，不會被誤判成 dup。 */
    seen_clear(jb, (uint16_t)(jb->expected + 32768));
}

/* 知道串流到哪裡結束了（BYE 算出來的、或逾時時看過的最大 seq + 1）。之後一直 tick 到 expected == end_seq。 */
static void jb_finish_at(jb_t *jb, uint16_t end) {
    if (jb->have_highest && seq_diff((uint16_t)(jb->highest + 1), end) > 0) end = (uint16_t)(jb->highest + 1);
    /* BYE 的 seq 不合理（比看過最大的多超過 cap 包：對方的 BYE 沒照規格給 seq）：只信看過最大的 + 1 */
    if (jb->have_highest && seq_diff(end, jb->highest) > jb->cap + 1) end = (uint16_t)(jb->highest + 1);
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
static int cmd_send(const char *ip, int port, const char *path, int burst, int seq0, int use_mulaw) {
    wav_t w;
    if (load_wav_16k_mono(path, &w) != 0) return 2;

    udp_t u;
    if (udp_open(&u, NULL, 0) != 0 || udp_set_peer(&u, ip, port) != 0) { wav_free(&w); return 1; }

    size_t n_frames = (w.n_samples + FRAME_SAMPLES - 1) / FRAME_SAMPLES;
    srand((unsigned)time(NULL) ^ (unsigned)clock_ms());
    uint32_t ssrc = ((uint32_t)rand() << 16) ^ (uint32_t)rand();
    uint8_t ptype = use_mulaw ? T_MULAW : T_AUDIO;                     /* --codec 決定 type 與每包的長度 */
    size_t pkt_len = use_mulaw ? PKT_MULAW : PKT_AUDIO;
    printf("send：%s（%u samples → %u 包）→ %s:%d，ssrc=0x%08X，codec=%s，每 %d ms 一包 %u bytes\n",
           path, (unsigned)w.n_samples, (unsigned)n_frames, ip, port, (unsigned)ssrc, codec_name(ptype), FRAME_MS,
           (unsigned)pkt_len);
    fflush(stdout);

    uint8_t pkt[PKT_AUDIO];
    int16_t frame[FRAME_SAMPLES];
    uint16_t seq = (uint16_t)seq0;               /* 平常從 0 開始；--seq0 65500 可以看 65535 → 0 的繞回 */
    size_t sent_bytes = 0;
    double t0 = clock_ms();
    for (size_t k = 0; k < n_frames; k++) {
        memset(pkt, 0, sizeof(pkt));
        pack_header(pkt, ptype, seq, (uint32_t)(k * FRAME_SAMPLES), ssrc);
        /* 先把這一包的 320 個 sample 湊齊（最後一包不足的補 0），再依 codec 填進 payload */
        size_t start = k * FRAME_SAMPLES;
        size_t n = w.n_samples - start;
        if (n > FRAME_SAMPLES) n = FRAME_SAMPLES;
        memset(frame, 0, sizeof(frame));
        memcpy(frame, w.pcm + start, n * sizeof(int16_t));
        if (use_mulaw) mulaw_encode(frame, pkt + HDR_LEN);                 /* 320 bytes */
        else pcm_to_bytes(frame, FRAME_SAMPLES, pkt + HDR_LEN);            /* 640 bytes */
        if (udp_send(&u, pkt, pkt_len) < 0) { udp_close(&u); wav_free(&w); return 1; }
        sent_bytes += pkt_len;
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
    fprintf(stderr, "STATS role=send codec=%s frames=%u bytes=%u duration_ms=%.0f pace=%d\n",
            codec_name(ptype), (unsigned)n_frames, (unsigned)sent_bytes, duration, burst ? 0 : FRAME_MS);

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

static int cmd_recv(const char *bind_ip, int port, const char *path, int buffer_ms, int plc_zero, int timeout_ms) {
    if (buffer_ms % FRAME_MS != 0) { fprintf(stderr, "錯誤: --buffer 要是 %d 的倍數\n", FRAME_MS); return 2; }
    int buffer_frames = buffer_ms / FRAME_MS;
    if (buffer_frames * 4 > RING_SLOTS) {
        fprintf(stderr, "錯誤: --buffer 最多 %d ms\n", RING_SLOTS / 4 * FRAME_MS);
        return 2;
    }

    udp_t u;
    if (udp_open(&u, bind_ip, port) != 0) return 1;        /* bind_ip NULL = 0.0.0.0 */
    printf("recv：聽 %s:%d，buffer=%d ms（%d 包，上限 %d），plc=%s，timeout=%d ms → %s\n", bind_ip ? bind_ip : "0.0.0.0",
           port, buffer_ms, buffer_frames, buffer_frames * 4, plc_zero ? "zero" : "repeat", timeout_ms, path);
    int first_wait = timeout_ms * 10 > FIRST_WAIT_MIN ? timeout_ms * 10 : FIRST_WAIT_MIN;
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
    uint32_t ssrc = 0;   int have_ssrc = 0;      /* 這次串流的 ssrc；換了就 jb_reset */
    uint8_t codec_type = T_AUDIO; int have_codec = 0;   /* 這次串流的 codec（封包的 type）：第一個合法 AUDIO 決定 */
    unsigned n_bad = 0;                          /* 壞封包數 */

    for (;;) {
        double now = clock_ms();
        if (jb_done(&jb)) break;

        /* 1. 算「下一件事」的時刻：下一拍、或逾時——看哪個先到 */
        double deadline = have_last_rx ? last_rx + timeout_ms : t_start + first_wait;
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
            if (!parse_header(buf, k, &h)) {     /* 壞封包：計數、丟掉、繼續（絕不當掉） */
                n_bad++;
                if (n_bad <= 3) printf("壞封包 %d bytes（magic／長度／type 不對），丟掉\n", k);
            } else {
                last_rx = now; have_last_rx = 1;
                if (h.type == T_AUDIO || h.type == T_MULAW) {
                    if (!have_codec) {           /* 第一個合法 AUDIO 決定這次串流的 codec */
                        codec_type = h.type; have_codec = 1;
                        printf("codec=%s（type 0x%02X）\n", codec_name(h.type), h.type);
                    } else if (h.type != codec_type) {   /* 同一次串流 codec 不會換：type 不同的當壞封包 */
                        n_bad++;
                        continue;
                    }
                    if (have_ssrc && h.ssrc != ssrc) {   /* 對方重啟了（新的 ssrc）：buffer 清掉重來 */
                        printf("ssrc 從 0x%08X 變成 0x%08X：重置 buffer\n", (unsigned)ssrc, (unsigned)h.ssrc);
                        jb_reset(&jb);
                        have_bye = 0;
                    }
                    ssrc = h.ssrc; have_ssrc = 1;
                    if (!have_play_t0) {         /* 第一包：buffer ms 之後開播 */
                        play_t0 = now + buffer_ms;
                        have_play_t0 = 1;
                        printf("第一包到了（seq=%u），%d ms 後開播\n", (unsigned)h.seq, buffer_ms);
                    }
                    /* 先解碼成 16-bit，再進 jitter buffer（buffer 與 PLC 不用知道 codec） */
                    if (h.type == T_MULAW) mulaw_decode(buf + HDR_LEN, frame);
                    else bytes_to_pcm(buf + HDR_LEN, FRAME_SAMPLES, frame);
                    jb_put(&jb, h.seq, frame, now);
                } else if (!jb.have_end && (!have_ssrc || h.ssrc == ssrc)) {   /* BYE（同一個 ssrc 的才算）：知道結尾了 */
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
        if (!have_last_rx && now - t_start >= first_wait) {
            fprintf(stderr, "%d 秒內一個封包都沒收到；送端開了嗎？port 對嗎？\n", first_wait / 1000);
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
    if (n_bad) printf("丟掉了 %u 個壞封包（magic、長度或 type 不對）\n", n_bad);
    printf("已存檔：%s（%u samples，%u frames）\n", path, (unsigned)out.len, jb.frames_out);
    double avg = jb.played_real ? jb.delay_sum / jb.played_real : 0.0;
    /* 規格的 STATS：欄位順序與名稱不可改（評測用正規表示式抓） */
    fprintf(stderr, "STATS role=recv buffer_ms=%d plc=%s codec=%s frames_out=%u received=%u lost=%u "
            "late=%u dup=%u reordered=%u overflow=%u concealed=%u avg_delay_ms=%.1f max_delay_ms=%.1f\n",
            buffer_ms, plc_zero ? "zero" : "repeat", codec_name(codec_type), jb.frames_out, jb.received, jb_lost(&jb),
            jb.late, jb.dup, jb.reordered, jb.overflow, jb.concealed, avg, jb.delay_max);
    free(out.pcm);
    return rc_exit;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (udp_global_init() != 0) return 1;
    mulaw_init();                                 /* 建 μ-law 的兩張表（幾毫秒） */
    int rc;

    if (strcmp(argv[1], "send") == 0 && argc >= 5) {
        int port = parse_port(argv[3]);
        int burst = 0, seq0 = 0, use_mulaw = 0;
        int ok = (port >= 0);
        if (!ok) fprintf(stderr, "錯誤: port 不合法：%s\n", argv[3]);
        for (int i = 5; ok && i < argc; i++) {
            if (strcmp(argv[i], "--burst") == 0) burst = 1;
            else if (strcmp(argv[i], "--codec") == 0 && i + 1 < argc && strcmp(argv[i + 1], "pcm") == 0) {
                use_mulaw = 0; i++;
            } else if (strcmp(argv[i], "--codec") == 0 && i + 1 < argc && strcmp(argv[i + 1], "mulaw") == 0) {
                use_mulaw = 1; i++;
            }
            else if (strcmp(argv[i], "--seq0") == 0 && i + 1 < argc && parse_int(argv[i + 1]) >= 0)
                seq0 = parse_int(argv[++i]) & 0xFFFF;
            else { fprintf(stderr, "錯誤: 不認識的選項或值不對：%s\n", argv[i]); ok = 0; }
        }
        rc = ok ? cmd_send(argv[2], port, argv[4], burst, seq0, use_mulaw) : 2;
    } else if (strcmp(argv[1], "recv") == 0 && argc >= 4) {
        int port = parse_port(argv[2]);
        int buffer_ms = 60, timeout_ms = 2000, plc_zero = 0;
        const char *bind_ip = NULL;
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
            else if (strcmp(name, "--bind") == 0 && val)                              bind_ip = val;
            else { fprintf(stderr, "錯誤: 不認識的選項或值不對：%s\n", name); ok = 0; }
        }
        rc = ok ? cmd_recv(bind_ip, port, argv[3], buffer_ms, plc_zero, timeout_ms) : 2;
    } else {
        rc = usage();
    }

    udp_global_cleanup();
    return rc;
}
