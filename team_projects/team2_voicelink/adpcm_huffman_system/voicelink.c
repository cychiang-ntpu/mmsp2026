/*============================================================================
 *  voicelink.c  —  ADPCM／Huffman 系統的框架（adpcm_huffman_system/）：和 mulaw_system 相同的起點
 *----------------------------------------------------------------------------
 *  建議：先把你們做好的 mulaw_system/voicelink.c 複製過來覆蓋這個檔（這樣 μ-law 已經在表裡，Huffman 才有東西可以疊），
 *  再照 README.md 加 ADPCM（9a）或／和 Huffman（9b）。這個框架檔只是讓這個資料夾一開始就編得過。
 *    搜尋 "★ 第 9 步" 就是要動手的地方：
 *      ★ 第 9 步 (a)  ADPCM：搬 m9 的表與三個函式 → 寫 adpcm_encode_frame／adpcm_decode_frame（每包獨立、開頭歸零）
 *                     → CODECS 加一列 { "adpcm", 0x05, 160, … } → 過測試 9
 *      ★ 第 9 步 (b)  Huffman：固定表寫死在程式裡 → encode 要能回傳實際長度（介面改成 size_t）→ type 0x06 的 payload
 *                     （1 byte 底層 type ＋ 2 bytes 有效位元數 ＋ 位元串）→ 過測試 10（你們對你們）
 *  用法同 mulaw_system/voicelink.c。
 *===========================================================================*/
/* ---- 下面這些 .h 是課程自己寫的 wrapper，不是系統函式庫：都在 ../baseline/wrapper/（Makefile 用 -I 指過去）。
 *   udp.h 一定要是第一個 #include：它在任何系統標頭之前定義 _POSIX_C_SOURCE（macOS／Linux 才看得到 select／clock_gettime），
 *   而且 Windows 的 winsock2.h 必須排在 windows.h 前面；放第一行兩邊都安全（和 Team 1 的 platform.h 同一個道理）。
 *   udp.h    UDP socket 的薄包裝（Windows 底下是 Winsock、macOS／Linux 是 POSIX socket）＋ put_u16／put_u32／get_u16／get_u32（big-endian）
 *   clock.h  單調時鐘：clock_ms（毫秒）、sleep_until_ms（睡到某個絕對時刻）
 *   wav.h    讀寫 WAV：wav_read／wav_write（wav_t）
 *   audio.h  麥克風與喇叭：audio_start_duplex／audio_stop／audio_list_devices（實作在 audio.c，用 miniaudio；要連結 ../audio.o）
 */
#include "udp.h"                 /* 一定排第一（原因見上面） */
#include "clock.h"
#include "wav.h"
#include "audio.h"               /* audio_start_duplex／audio_stop／audio_list_devices（實作在 ../baseline/wrapper/audio.c） */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>                /* log、pow、lrint：建 μ-law 表用（Linux 要連結 -lm） */

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

#define FRAME_SAMPLES 320
#define FRAME_BYTES   (FRAME_SAMPLES * 2)
#define FRAME_MS      20
#define MAX_DGRAM     2048

#define MAGIC      0x56
#define T_AUDIO    0x01
#define T_BYE      0x02
#define HDR_LEN    12
#define PKT_AUDIO  (HDR_LEN + FRAME_BYTES)       /* 652 */
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
        "  voicelink send <ip> <port> <in.wav> [--burst] [--seq0 <n>] [--codec <名字>]\n"
        "  voicelink recv <port> <out.wav> [--bind <ip>] [--buffer <ms>] [--plc zero|repeat] [--timeout <ms>]\n"
        "  voicelink call <my_port> <peer_ip> <peer_port> [--buffer <ms>] [--plc zero|repeat] [--codec <名字>]\n"
        "      雙向通話（戴耳機）；按 Enter 結束\n"
        "  voicelink devices                      列出麥克風與喇叭\n"
        "      --codec    送端用哪種 codec（CODECS 表裡的名字；這一步只有 pcm = 16-bit 原樣 640 bytes）；recv 依封包 type 自動判斷\n"
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

/*--------------------------------------------------------------------------
 * codec 介面：換 codec 只動這一區（第 8、9 步就是在 CODECS 表裡各加一列，並寫出那一列的 encode／decode）
 *-------------------------------------------------------------------------*/
typedef struct {
    const char *name;                            /* --codec 的名字 = STATS 印的名字 */
    uint8_t     type;                            /* 封包 header 的 type（規格的表） */
    size_t      payload;                         /* 每包聲音資料的 bytes（規格：pcm 640、mulaw 320、adpcm 160） */
    void (*encode)(const int16_t *pcm, uint8_t *out);   /* 320 個 16-bit sample → payload bytes */
    void (*decode)(const uint8_t *in, int16_t *pcm);    /* payload bytes → 320 個 16-bit sample */
} codec_t;

/* ★ 第 9 步 (a)：m9 的 ADPCM_STEP／ADPCM_INDEX、adpcm_state_t、adpcm_reconstruct、adpcm_encode_one 搬到這裡，
 * 再寫 adpcm_encode_frame／adpcm_decode_frame（每包 320 個 sample 獨立：state_reset、第 2 個 sample 換 ADPCM_INDEX0、兩個 nibble 一個 byte、高 4 bits 先）。
 * ★ 第 9 步 (b)：Huffman 的 huff_code[256]／huff_len[256]（離線統計、寫死）與 bit writer／reader 也放這裡。 */

/* pcm：不壓縮，只是 16-bit 轉成 little-endian 的 bytes（和第 1 步一樣） */
static void pcm_encode(const int16_t *pcm, uint8_t *out) { pcm_to_bytes(pcm, FRAME_SAMPLES, out); }
static void pcm_decode(const uint8_t *in, int16_t *pcm)  { bytes_to_pcm(in, FRAME_SAMPLES, pcm); }

static const codec_t CODECS[] = {
    { "pcm", T_AUDIO, FRAME_BYTES, pcm_encode, pcm_decode },
    /* （先把 mulaw_system 做好的那一列 mulaw 複製過來）
     * ★ 第 9 步 (a)：加一列 { "adpcm", 0x05, FRAME_SAMPLES / 2, adpcm_encode_frame, adpcm_decode_frame }（160 bytes）
     * ★ 第 9 步 (b)：Huffman 的 payload 長度不固定——把 encode 改成回傳 size_t（實際長度）、payload 欄改成「最大長度」、
     *                parse_header 的 == 改成 <=，再加一列 { "huff-mulaw", 0x06, 3 + 400, huff_encode, huff_decode } */
};
#define N_CODECS (sizeof(CODECS) / sizeof(CODECS[0]))

/* 名字 → 表的那一列（--codec 用）；找不到回傳 NULL */
static const codec_t *codec_by_name(const char *name) {
    for (size_t i = 0; i < N_CODECS; i++) if (strcmp(CODECS[i].name, name) == 0) return &CODECS[i];
    return NULL;
}
/* 封包 type → 表的那一列（recv 用）；找不到回傳 NULL（＝不是 AUDIO 類的 type） */
static const codec_t *codec_by_type(uint8_t type) {
    for (size_t i = 0; i < N_CODECS; i++) if (CODECS[i].type == type) return &CODECS[i];
    return NULL;
}

/* --codec 給了表裡沒有的名字：把表裡有的名字列出來，順便提醒該去哪裡加（學生最常撞到的錯誤） */
static void codec_unknown(const char *name) {
    fprintf(stderr, "錯誤: 不認識的 codec「%s」。CODECS 表裡目前有：", name);
    for (size_t i = 0; i < N_CODECS; i++) fprintf(stderr, "%s%s", i ? "、" : "", CODECS[i].name);
    fprintf(stderr, "\n      要加新的 codec：在 voicelink.c 搜尋「★」，照 README.md 的步驟在 CODECS 表加一列並寫出它的 encode／decode。\n");
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
    if (h->type == T_BYE && len == HDR_LEN) return 1;
    const codec_t *c = codec_by_type(h->type);                        /* AUDIO 類：type 在表裡、長度剛好 = header + payload */
    return c != NULL && len == (int)(HDR_LEN + c->payload);
}

/* type → 名字（STATS 用）；不在表裡就當 pcm（還沒收到任何包時 STATS 也要印得出來） */
static const char *codec_name(uint8_t type) { const codec_t *c = codec_by_type(type); return c ? c->name : "pcm"; }

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
static int cmd_send(const char *ip, int port, const char *path, int burst, int seq0, const codec_t *codec) {
    wav_t w;
    if (load_wav_16k_mono(path, &w) != 0) return 2;

    udp_t u;
    if (udp_open(&u, NULL, 0) != 0 || udp_set_peer(&u, ip, port) != 0) { wav_free(&w); return 1; }

    size_t n_frames = (w.n_samples + FRAME_SAMPLES - 1) / FRAME_SAMPLES;
    srand((unsigned)time(NULL) ^ (unsigned)clock_ms());
    uint32_t ssrc = ((uint32_t)rand() << 16) ^ (uint32_t)rand();
    uint8_t ptype = codec->type;                                       /* --codec 查表：type 與每包的長度都在表裡 */
    size_t pkt_len = HDR_LEN + codec->payload;
    printf("send：%s（%u samples → %u 包）→ %s:%d，ssrc=0x%08X，codec=%s，每 %d ms 一包 %u bytes\n",
           path, (unsigned)w.n_samples, (unsigned)n_frames, ip, port, (unsigned)ssrc, codec_name(ptype), FRAME_MS,
           (unsigned)pkt_len);
    fflush(stdout);

    uint8_t pkt[PKT_AUDIO];                      /* 最大的一種（pcm）就夠放任何 codec */
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
        codec->encode(frame, pkt + HDR_LEN);                               /* 表裡那一列的 encode：pcm 是 640 bytes */
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
                if (codec_by_type(h.type) != NULL) {   /* AUDIO 類（表裡有的 type） */
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
                    codec_by_type(h.type)->decode(buf + HDR_LEN, frame);
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

/*--------------------------------------------------------------------------
 * call：雙向通話。三條執行緒在跑：
 *   main          等 Enter
 *   recv_thread   udp_recv → 解析 → 解碼 → jb_put（網路到達的 frame 排進 jitter buffer）
 *   音訊執行緒     audio_cb：mic 湊滿 320 個 → 編碼、加 header、sendto；spk 每 320 個向 jb 要一個 frame（= 播放時鐘）
 * recv_thread 與 audio_cb 都碰 jb，用 lock 保護。
 *-------------------------------------------------------------------------*/
typedef struct {
    udp_t    u;
    jb_t     jb;                                 /* 和 mock 模式同一個 jitter buffer */
    lock_t   lock;                               /* 保護 jb 與下面「收端狀態」的鎖 */
    volatile int running;                        /* main 把它設成 0，其他執行緒看到就收工 */
    int      buffer_ms;
    const codec_t *codec;                        /* 我送出去用的 codec（--codec）；對方用什麼看封包的 type */
    /* 送端狀態（只有音訊執行緒碰，不用鎖） */
    uint32_t ssrc;                               /* 我這次串流的識別碼 */
    uint16_t seq;                                /* 下一包的 seq */
    uint32_t ts;                                 /* 下一包的 ts（每包 +320） */
    int16_t  mic_stage[FRAME_SAMPLES];           /* callback 給的 n 不一定剛好 320，先湊在這裡，滿 320 才送 */
    size_t   mic_fill;
    int16_t  spk_cur[FRAME_SAMPLES];             /* 正在播的 frame；一個 frame 可能分兩次 callback 播完 */
    size_t   spk_pos;                            /* == FRAME_SAMPLES 表示播完了，要再向 jb 要一個 */
    unsigned sent, sent_bytes;
    /* 收端狀態（recv_thread 寫、audio_cb 讀：在鎖裡） */
    int      have_peer_ssrc; uint32_t peer_ssrc; /* 對方這次串流的 ssrc */
    int      have_codec;     uint8_t  codec_type;
    int      have_first;     double   first_arrival;   /* 第一包到達時刻：+ buffer ms 就開播 */
    unsigned bad;
} call_t;

/* 把湊滿的 320 個 sample 編碼、加 header、送出去（和 cmd_send 的迴圈做的事一樣） */
static void mic_push_and_send(call_t *c, const int16_t *mic, size_t n) {
    for (size_t i = 0; i < n; i++) {
        c->mic_stage[c->mic_fill++] = mic[i];
        if (c->mic_fill == FRAME_SAMPLES) {
            uint8_t pkt[PKT_AUDIO];
            size_t pkt_len = HDR_LEN + c->codec->payload;
            pack_header(pkt, c->codec->type, c->seq, c->ts, c->ssrc);
            c->codec->encode(c->mic_stage, pkt + HDR_LEN);
            if (udp_send(&c->u, pkt, pkt_len) > 0) { c->sent++; c->sent_bytes += (unsigned)pkt_len; }
            c->seq++;
            c->ts += FRAME_SAMPLES;
            c->mic_fill = 0;
        }
    }
}

/* 填 n 個 sample 到喇叭。每播完一個 frame 就向 jb 要下一個——這裡就是 live 模式的播放時鐘 */
static void spk_fill(call_t *c, int16_t *spk, size_t n) {
    size_t done = 0;
    while (done < n) {
        if (c->spk_pos >= FRAME_SAMPLES) {       /* 上一個 frame 播完了，拿下一個 */
            double now = clock_ms();
            lock_take(&c->lock);
            if (!c->jb.started && c->have_first && now >= c->first_arrival + c->buffer_ms) jb_start(&c->jb);
            if (c->jb.started) jb_tick(&c->jb, c->spk_cur, now);     /* 有就播、沒有就 PLC；拍子一定前進 */
            else memset(c->spk_cur, 0, FRAME_BYTES);                  /* 還沒開播（還在累積 buffer）：靜音 */
            lock_give(&c->lock);
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
    call_t *c = (call_t *)user;                  /* user 就是我們交給 audio_start_duplex 的那個 call_t 的位址 */
    if (!c->running) { if (spk) memset(spk, 0, n * sizeof(int16_t)); return; }
    if (mic) mic_push_and_send(c, mic, n);
    if (spk) spk_fill(c, spk, n);
}

/* 網路執行緒：收一包 → 解析 → 解碼 → jb_put。每 200 ms 醒來看一下 running，main 說停就停。 */
static THREAD_FN(recv_thread) {
    call_t *c = (call_t *)arg;
    uint8_t buf[MAX_DGRAM];
    int16_t frame[FRAME_SAMPLES];
    while (c->running) {
        int k = udp_recv(&c->u, buf, sizeof(buf), 200, NULL);
        if (k < 0) break;
        if (k == 0) continue;
        double now = clock_ms();
        hdr_t h;
        if (!parse_header(buf, k, &h)) { c->bad++; continue; }
        if (h.type == T_BYE) {                   /* 對方掛了：buffer 清掉，等下一次開播 */
            lock_take(&c->lock);
            jb_reset(&c->jb);
            c->have_first = 0;
            c->have_peer_ssrc = 0;
            lock_give(&c->lock);
            continue;
        }
        if (!c->have_codec) { c->codec_type = h.type; c->have_codec = 1; }
        else if (h.type != c->codec_type) { c->bad++; continue; }
        codec_by_type(h.type)->decode(buf + HDR_LEN, frame);         /* 解碼在鎖外面做（鎖裡不做慢的事）；parse_header 已保證 type 在表裡 */
        lock_take(&c->lock);
        if (c->have_peer_ssrc && h.ssrc != c->peer_ssrc) { jb_reset(&c->jb); c->have_first = 0; }
        c->peer_ssrc = h.ssrc; c->have_peer_ssrc = 1;
        if (!c->have_first) { c->first_arrival = now; c->have_first = 1; }
        jb_put(&c->jb, h.seq, frame, now);
        lock_give(&c->lock);
    }
    THREAD_RETURN;
}

static int cmd_call(int my_port, const char *peer_ip, int peer_port, int buffer_ms, int plc_zero, const codec_t *codec) {
    if (buffer_ms % FRAME_MS != 0) { fprintf(stderr, "錯誤: --buffer 要是 %d 的倍數\n", FRAME_MS); return 2; }
    int buffer_frames = buffer_ms / FRAME_MS;
    if (buffer_frames * 4 > RING_SLOTS) {
        fprintf(stderr, "錯誤: --buffer 最多 %d ms\n", RING_SLOTS / 4 * FRAME_MS);
        return 2;
    }

    static call_t c;                              /* static：很大（jb 有 64 個 frame），不放堆疊上 */
    memset(&c, 0, sizeof(c));
    lock_init(&c.lock);
    c.jb.cap = buffer_frames * 4;
    c.jb.plc_zero = plc_zero;
    c.buffer_ms = buffer_ms;
    c.codec = codec;
    c.spk_pos = FRAME_SAMPLES;                    /* 一開始沒有 frame 在播 */
    srand((unsigned)time(NULL) ^ (unsigned)clock_ms());
    c.ssrc = ((uint32_t)rand() << 16) ^ (uint32_t)rand();
    c.running = 1;

    if (udp_open(&c.u, NULL, my_port) != 0 || udp_set_peer(&c.u, peer_ip, peer_port) != 0) return 1;

    thread_t th;
    if (thread_create(&th, recv_thread, &c) != 0) { fprintf(stderr, "錯誤: 開不了接收執行緒\n"); udp_close(&c.u); return 1; }

    if (audio_start_duplex(audio_cb, &c) != 0) {  /* 開麥克風與喇叭；失敗（沒有裝置）→ 結束碼 1 */
        c.running = 0;
        thread_join(th);
        udp_close(&c.u);
        return 1;
    }
    printf("通話中：我聽 port %d，送到 %s:%d，buffer=%d ms，plc=%s，codec=%s。請戴耳機。按 Enter 結束。\n",
           my_port, peer_ip, peer_port, buffer_ms, plc_zero ? "zero" : "repeat", codec->name);
    fflush(stdout);

    char line[16];
    if (fgets(line, sizeof(line), stdin) == NULL) line[0] = 0;   /* 等 Enter（stdin 被關掉時立刻回傳 NULL，也算結束） */

    c.running = 0;
    audio_stop();                                 /* 之後 audio_cb 不會再被叫 */
    for (int i = 0; i < BYE_REPEAT; i++) {        /* 告訴對方我掛了 */
        uint8_t bye[HDR_LEN];
        pack_header(bye, T_BYE, c.seq, c.ts, c.ssrc);
        udp_send(&c.u, bye, sizeof(bye));
        c.seq++;
    }
    thread_join(th);
    udp_close(&c.u);

    /* STATS：收端那一行（和 recv 相同的欄位）＋ 送端那一行 */
    if (c.jb.have_highest) jb_finish_at(&c.jb, (uint16_t)(c.jb.highest + 1));   /* 算 lost 用：結尾 = 看過最大的 + 1 */
    double avg = c.jb.played_real ? c.jb.delay_sum / c.jb.played_real : 0.0;
    printf("結束：送出 %u 包、收到 %u 包、補了 %u 個 frame、壞封包 %u\n", c.sent, c.jb.received, c.jb.concealed, c.bad);
    fprintf(stderr, "STATS role=recv buffer_ms=%d plc=%s codec=%s frames_out=%u received=%u lost=%u "
            "late=%u dup=%u reordered=%u overflow=%u concealed=%u avg_delay_ms=%.1f max_delay_ms=%.1f\n",
            buffer_ms, plc_zero ? "zero" : "repeat", codec_name(c.have_codec ? c.codec_type : T_AUDIO),
            c.jb.frames_out, c.jb.received, jb_lost(&c.jb), c.jb.late, c.jb.dup, c.jb.reordered, c.jb.overflow,
            c.jb.concealed, avg, c.jb.delay_max);
    fprintf(stderr, "STATS role=send codec=%s frames=%u bytes=%u duration_ms=%u pace=%d\n",
            codec->name, c.sent, c.sent_bytes, c.sent * FRAME_MS, FRAME_MS);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (udp_global_init() != 0) return 1;
    int rc;

    if (strcmp(argv[1], "send") == 0 && argc >= 5) {
        int port = parse_port(argv[3]);
        int burst = 0, seq0 = 0;
        const codec_t *codec = &CODECS[0];       /* 預設 pcm */
        int ok = (port >= 0);
        if (!ok) fprintf(stderr, "錯誤: port 不合法：%s\n", argv[3]);
        for (int i = 5; ok && i < argc; i++) {
            if (strcmp(argv[i], "--burst") == 0) burst = 1;
            else if (strcmp(argv[i], "--codec") == 0 && i + 1 < argc) {
                codec = codec_by_name(argv[i + 1]);
                if (!codec) { codec_unknown(argv[i + 1]); ok = 0; }
                i++;
            }
            else if (strcmp(argv[i], "--seq0") == 0 && i + 1 < argc && parse_int(argv[i + 1]) >= 0)
                seq0 = parse_int(argv[++i]) & 0xFFFF;
            else { fprintf(stderr, "錯誤: 不認識的選項或值不對：%s\n", argv[i]); ok = 0; }
        }
        rc = ok ? cmd_send(argv[2], port, argv[4], burst, seq0, codec) : 2;
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
    } else if (strcmp(argv[1], "call") == 0 && argc >= 5) {
        int my_port = parse_port(argv[2]);
        int peer_port = parse_port(argv[4]);
        int buffer_ms = 60, plc_zero = 0;
        const codec_t *codec = &CODECS[0];
        int ok = (my_port >= 0 && peer_port >= 0);
        if (!ok) fprintf(stderr, "錯誤: port 不合法\n");
        for (int i = 5; ok && i < argc; i += 2) {
            const char *name = argv[i];
            const char *val = (i + 1 < argc) ? argv[i + 1] : NULL;
            if (strcmp(name, "--buffer") == 0 && val && parse_int(val) > 0)          buffer_ms = parse_int(val);
            else if (strcmp(name, "--plc") == 0 && val && strcmp(val, "zero") == 0)   plc_zero = 1;
            else if (strcmp(name, "--plc") == 0 && val && strcmp(val, "repeat") == 0) plc_zero = 0;
            else if (strcmp(name, "--codec") == 0 && val) {
                codec = codec_by_name(val);
                if (!codec) { codec_unknown(val); ok = 0; }
            }
            else { fprintf(stderr, "錯誤: 不認識的選項或值不對：%s\n", name); ok = 0; }
        }
        rc = ok ? cmd_call(my_port, argv[3], peer_port, buffer_ms, plc_zero, codec) : 2;
    } else if (strcmp(argv[1], "devices") == 0) {
        int n = audio_list_devices();
        printf("共 %d 個裝置\n", n);
        rc = 0;
    } else {
        rc = usage();
    }

    udp_global_cleanup();
    return rc;
}
