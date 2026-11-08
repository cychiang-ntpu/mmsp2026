/*============================================================================
 *  m9_adpcm.c  —  模組 9：把一個 WAV 用 ADPCM 壓成 4 bits／sample 再解回來，算 SQNR 與 bytes 的比例
 *----------------------------------------------------------------------------
 *  用法：m9_adpcm <in.wav> [out.wav] [--frame N] [--trace K] [--fixed-step S]
 *        out.wav      解回來的聲音（預設 "adpcm_out.wav"），可以聽
 *        --frame N    每 N 個 sample 把預測器歸零一次（預設 320 = Team 2 一包；0 = 整個檔案連續編碼，不歸零）
 *        --trace K    印出前 K 個 sample 的每一步（預設 0）：原始、預測、殘差、step、4-bit 碼、重建、誤差
 *        --fixed-step S  關掉「自適應」：step 固定為 S（就變成普通的 DPCM），看看為什麼需要 A
 *  看到什麼：「N 個 sample：16-bit 是 2N bytes → ADPCM 是 N/2 bytes（比例 0.250）」與「SQNR 2x dB」（規格 ≥ 20）。
 *  試著改：--frame 0 看 SQNR 高多少（代價是掉一包以後全部失步）；--fixed-step 100 看小聲清楚、大聲爆掉；
 *          --fixed-step 2000 看大聲還行、小聲全是雜音——所以 step 要「自己調」。
 *
 *  ADPCM = Adaptive Differential PCM（自適應差分脈衝編碼調變）。三個字各對應一個想法（第 9 週 2.3、audio_coding.md）：
 *    D（差分）    語音相鄰兩個 sample 很像，所以不送 sample 本身，送「和預測值的差」d = s − pred，差比較小、用的 bits 比較少。
 *    PCM（量化）  d 用 4 bits 量化：1 bit 正負 ＋ 3 bits 大小（|d| 和 step、step/2、step/4 比三次，像用三把尺量長度）。
 *    A（自適應）  量化的刻度 step 不固定：剛才的差很大（碼的大小 4–7）→ 下一步 step 放大；差很小（0–3）→ step 縮小。
 *                 兩邊用同一張表（ADPCM_STEP、ADPCM_INDEX）算，所以 step 不用送過去。
 *  最重要的一句話：**預測值要用「重建出來的」sample，不是原始的**。解碼端手上只有重建值；編碼端如果偷用原始值，
 *  兩邊的 pred 就不一樣，量化誤差會一路累積（--trace 看「重建」那一欄就是兩邊共同知道的東西）。
 *  Team 2 的規定：每一包（320 個 sample）開頭把 pred、index 歸零（--frame 320），一包只靠自己就能解，掉一包不會害到下一包。
 *  這支程式和 python_ref/voicelink.py 的 adpcm_encode／adpcm_decode 一模一樣（同一張表、同樣的初值），所以搬進 step7 之後
 *  可以和 python_ref 互通、過測試 9。
 *===========================================================================*/
/* ---- 下面這些 .h 是課程自己寫的 wrapper，不是系統函式庫：都在 ../../baseline/wrapper/（Makefile 用 -I 指過去）。
 *   udp.h 一定要是第一個 #include：它在任何系統標頭之前定義 _POSIX_C_SOURCE（macOS／Linux 才看得到 select／clock_gettime），
 *   而且 Windows 的 winsock2.h 必須排在 windows.h 前面；放第一行兩邊都安全（和 Team 1 的 platform.h 同一個道理）。
 *   udp.h    UDP socket 的薄包裝（Windows 底下是 Winsock、macOS／Linux 是 POSIX socket）＋ put_u16／put_u32／get_u16／get_u32（big-endian）
 *   wav.h    讀寫 WAV：wav_read／wav_write（wav_t）
 */
#include "udp.h"                 /* 一定排第一（原因見上面） */
#include "wav.h"

#include <math.h>                /* log10（Linux 要 -lm） */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- 兩張表（IMA ADPCM 的表，和 python_ref 一樣）----
 * ADPCM_STEP：89 個可能的 step，每一格約是前一格的 1.1 倍（7 → 32767）。index 就是「現在用第幾格」。
 * ADPCM_INDEX：看剛才碼的「大小」（0–7）決定 index 要加多少：小的差 → −1（step 慢慢變小）；大的差 → +2/+4/+6/+8（step 快速變大）。*/
static const int ADPCM_STEP[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060,
    1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
    7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767 };
static const int ADPCM_INDEX[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };
#define ADPCM_INDEX_FIRST 48     /* 每一包的第一個 sample：step = 724，從 0 一步就能跳到訊號附近 */
#define ADPCM_INDEX0      32     /* 第二個 sample 起：step = 157，之後讓它自己調 */

/* 編碼端與解碼端共用的狀態：上一個「重建值」與目前的 step index。兩邊永遠一模一樣。 */
typedef struct { int pred; int index; } adpcm_state_t;

static int fixed_step = 0;       /* --fixed-step S：S > 0 時關掉自適應（教學用） */
static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static int cur_step(const adpcm_state_t *st) { return fixed_step > 0 ? fixed_step : ADPCM_STEP[st->index]; }

/* 解碼一個 4-bit 碼 nibble：把 pred 往前推、調 index。編碼端也呼叫它——這就是「用重建值預測」的意思。 */
static void adpcm_reconstruct(adpcm_state_t *st, int nibble) {
    int step  = cur_step(st);
    int delta = step >> 3;                      /* 半階修正：落在量化區間的中點，而不是下緣 */
    if (nibble & 4) delta += step;              /* 3 bits 的大小 = 三把尺：step、step/2、step/4 */
    if (nibble & 2) delta += step >> 1;
    if (nibble & 1) delta += step >> 2;
    st->pred  = clampi((nibble & 8) ? st->pred - delta : st->pred + delta, -32768, 32767);   /* bit 3 是正負 */
    st->index = clampi(st->index + ADPCM_INDEX[nibble & 7], 0, 88);                          /* 自適應：調 step */
}

/* 編碼一個 sample s：算殘差、量化成 4 bits，然後呼叫 reconstruct 讓自己的 pred 和解碼端同步。 */
static int adpcm_encode_one(adpcm_state_t *st, int s) {
    int d = s - st->pred;                       /* D：差分 */
    int step = cur_step(st), nibble = 0;
    if (d < 0) { nibble = 8; d = -d; }          /* bit 3：正負 */
    if (d >= step)      { nibble |= 4; d -= step; }        /* 三把尺量 |d| */
    if (d >= step >> 1) { nibble |= 2; d -= step >> 1; }
    if (d >= step >> 2) { nibble |= 1; }
    adpcm_reconstruct(st, nibble);              /* ★ 用重建值更新 pred，不是用 s */
    return nibble;
}

static void state_reset(adpcm_state_t *st) { st->pred = 0; st->index = ADPCM_INDEX_FIRST; }

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "用法: m9_adpcm <in.wav> [out.wav] [--frame N] [--trace K] [--fixed-step S]\n"); return 2; }
    const char *in = argv[1], *out = "adpcm_out.wav";
    long frame = 320, trace = 0;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--frame") && i + 1 < argc)           frame = atol(argv[++i]);
        else if (!strcmp(argv[i], "--trace") && i + 1 < argc)      trace = atol(argv[++i]);
        else if (!strcmp(argv[i], "--fixed-step") && i + 1 < argc) fixed_step = atoi(argv[++i]);
        else if (argv[i][0] != '-')                                 out = argv[i];
        else { fprintf(stderr, "錯誤: 不認識的選項 %s\n", argv[i]); return 2; }
    }
    wav_t w;
    if (wav_read(in, &w) != 0 || w.channels != 1) { fprintf(stderr, "錯誤: 讀不到或不是單聲道 16-bit：%s\n", in); return 2; }

    size_t n = w.n_samples;
    uint8_t *code = (uint8_t *)calloc((n + 1) / 2, 1);              /* 兩個 sample 一個 byte */
    int16_t *back = (int16_t *)malloc(n * sizeof(int16_t));
    if (!code || !back) { fprintf(stderr, "錯誤: 記憶體不足\n"); return 1; }

    /* ---- 編碼：每個 sample → 4 bits；先到的放高 4 bits（規格：高 4 bits 先）---- */
    adpcm_state_t enc; state_reset(&enc);
    if (trace) printf("%6s %7s %7s %7s %6s %5s %7s %6s\n", "n", "原始 s", "預測", "殘差 d", "step", "碼", "重建", "誤差");
    for (size_t i = 0; i < n; i++) {
        if (frame > 0 && (long)(i % frame) == 0) state_reset(&enc);          /* 每一包開頭歸零（Team 2 的規定） */
        else if (frame > 0 && (long)(i % frame) == 1) enc.index = ADPCM_INDEX0;   /* 第二個 sample 起換成中等 step */
        else if (frame == 0 && i == 1) enc.index = ADPCM_INDEX0;
        int s = w.pcm[i], pred_before = enc.pred, step_before = cur_step(&enc);
        int nib = adpcm_encode_one(&enc, s);
        if (i % 2 == 0) code[i / 2] |= (uint8_t)(nib << 4); else code[i / 2] |= (uint8_t)nib;
        if ((long)i < trace)
            printf("%6u %7d %7d %7d %6d  %c%d%d%d %7d %6d\n", (unsigned)i, s, pred_before, s - pred_before, step_before,
                   (nib & 8) ? '-' : '+', (nib >> 2) & 1, (nib >> 1) & 1, nib & 1, enc.pred, s - enc.pred);
    }

    /* ---- 解碼：和編碼端用同一個 reconstruct、同樣的歸零規則，所以 pred 走的路一模一樣 ---- */
    adpcm_state_t dec; state_reset(&dec);
    for (size_t i = 0; i < n; i++) {
        if (frame > 0 && (long)(i % frame) == 0) state_reset(&dec);
        else if (frame > 0 && (long)(i % frame) == 1) dec.index = ADPCM_INDEX0;
        else if (frame == 0 && i == 1) dec.index = ADPCM_INDEX0;
        int nib = (i % 2 == 0) ? (code[i / 2] >> 4) : (code[i / 2] & 0x0F);
        adpcm_reconstruct(&dec, nib);
        back[i] = (int16_t)dec.pred;
    }

    /* ---- SQNR = 10·log10(訊號功率 / 誤差功率) ---- */
    double ps = 0, pn = 0;
    for (size_t i = 0; i < n; i++) { double x = w.pcm[i], e = (double)w.pcm[i] - back[i]; ps += x * x; pn += e * e; }
    double sqnr = pn > 0 ? 10.0 * log10(ps / pn) : 999.0;
    printf("%u 個 sample：16-bit 是 %u bytes → ADPCM 是 %u bytes（比例 %.3f）\n",
           (unsigned)n, (unsigned)(n * 2), (unsigned)((n + 1) / 2), (double)((n + 1) / 2) / (double)(n * 2));
    if (frame > 0) printf("每 %ld 個 sample 把預測器歸零一次（= Team 2 的一包）；", frame); else printf("整個檔案連續編碼（不歸零）；");
    if (fixed_step > 0) printf("step 固定 %d（沒有自適應 = 普通 DPCM）\n", fixed_step); else printf("step 自適應（89 格的表）\n");
    printf("前 5 個 sample：原始 %6d %6d %6d %6d %6d\n", w.pcm[0], w.pcm[1], w.pcm[2], w.pcm[3], w.pcm[4]);
    printf("               解回 %6d %6d %6d %6d %6d\n", back[0], back[1], back[2], back[3], back[4]);
    printf("SQNR %.1f dB（規格 ≥ 20）%s\n", sqnr, sqnr >= 20.0 ? "OK" : "FAIL");
    if (wav_write(out, back, n, w.sample_rate) == 0) printf("解回來的聲音存到 %s\n", out);
    free(code); free(back); wav_free(&w);
    return sqnr >= 20.0 ? 0 : 1;
}
