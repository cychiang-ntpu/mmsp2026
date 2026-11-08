/*============================================================================
 *  m8_mulaw.c  —  模組 8：把一個 WAV 用 μ-law 壓成 8 bits 再解回來，算 SQNR 與 bytes 的比例
 *----------------------------------------------------------------------------
 *  用法：m8_mulaw <in.wav> [out.wav]       （out.wav 預設 "mulaw_out.wav"：解回來的聲音，可以聽）
 *  看到什麼：「N 個 sample：16-bit 是 2N bytes → μ-law 是 N bytes（比例 0.500）」與「SQNR 37.x dB」（規格 ≥ 30）。
 *  試著改：把 MU 改成 1（幾乎是線性量化）看 SQNR 掉到 20 dB 以下；或把音量除以 10（x = sv / 327680.0 那種）看 μ-law 幾乎不變。
 *
 *  μ-law（G.711 的想法，μ = 255；第 9 週 2.2 與 examples/mulaw_demo.py）：
 *    壓縮  y = sign(x)·ln(1 + μ|x|) / ln(1 + μ)      x = s / 32768 在 [-1, 1)
 *    量化  q = clamp(round(y × 128), -128, 127)       256 階均勻量化 → 存成 1 byte = q + 128
 *    展開  x' = sign(q)·((1 + μ)^(|q|/128) − 1) / μ   s' = round(x' × 32768)
 *  查表：65536 個可能的 sample 各算一次 → enc[65536]；256 個 byte 各算一次 → dec[256]。之後每個 sample 查一次表。
 *===========================================================================*/
/* ---- 下面這些 .h 是課程自己寫的 wrapper，不是系統函式庫：都在 ../../baseline/wrapper/（Makefile 用 -I 指過去）。
 *   udp.h 一定要是第一個 #include：它在任何系統標頭之前定義 _POSIX_C_SOURCE（macOS／Linux 才看得到 select／clock_gettime），
 *   而且 Windows 的 winsock2.h 必須排在 windows.h 前面；放第一行兩邊都安全（和 Team 1 的 platform.h 同一個道理）。
 *   udp.h    UDP socket 的薄包裝（Windows 底下是 Winsock、macOS／Linux 是 POSIX socket）＋ put_u16／put_u32／get_u16／get_u32（big-endian）
 *   wav.h    讀寫 WAV：wav_read／wav_write（wav_t）
 */
#include "udp.h"                 /* 一定排第一（原因見上面） */
#include "wav.h"

#include <math.h>                /* log、pow、lrint（Linux 要 -lm） */
#include <stdio.h>
#include <stdlib.h>

#define MU 255.0
static uint8_t enc[65536];       /* 索引 = sample + 32768 */
static int16_t dec[256];

static long clampl(long v, long lo, long hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void mulaw_init(void) {
    for (long sv = -32768; sv <= 32767; sv++) {
        double x = (double)sv / 32768.0;
        double y = log(1.0 + MU * fabs(x)) / log(1.0 + MU);          /* 壓縮 */
        if (x < 0) y = -y;
        enc[sv + 32768] = (uint8_t)(clampl(lrint(y * 128.0), -128, 127) + 128);   /* 量化、存成 0…255 */
    }
    for (int b = 0; b < 256; b++) {
        double q = (double)(b - 128) / 128.0;
        double x = (pow(1.0 + MU, fabs(q)) - 1.0) / MU;              /* 展開 */
        if (q < 0) x = -x;
        dec[b] = (int16_t)clampl(lrint(x * 32768.0), -32768, 32767);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "用法: m8_mulaw <in.wav> [out.wav]\n"); return 2; }
    const char *out = (argc >= 3) ? argv[2] : "mulaw_out.wav";
    wav_t w;
    if (wav_read(argv[1], &w) != 0 || w.channels != 1) {
        fprintf(stderr, "錯誤: 讀不到或不是單聲道 16-bit：%s\n", argv[1]);
        return 2;
    }
    mulaw_init();

    uint8_t *bytes = (uint8_t *)malloc(w.n_samples);                 /* 壓縮後：每個 sample 1 byte */
    int16_t *back  = (int16_t *)malloc(w.n_samples * sizeof(int16_t));
    if (!bytes || !back) { fprintf(stderr, "錯誤: 記憶體不足\n"); return 1; }
    for (size_t i = 0; i < w.n_samples; i++) bytes[i] = enc[w.pcm[i] + 32768];   /* 編碼：查表 */
    for (size_t i = 0; i < w.n_samples; i++) back[i]  = dec[bytes[i]];           /* 解碼：查表 */

    /* SQNR = 10·log10(訊號功率 / 誤差功率)；用 double 累加才不會溢位 */
    double ps = 0, pn = 0;
    for (size_t i = 0; i < w.n_samples; i++) {
        double x = w.pcm[i], e = (double)w.pcm[i] - back[i];
        ps += x * x;
        pn += e * e;
    }
    double sqnr = pn > 0 ? 10.0 * log10(ps / pn) : 999.0;
    printf("%u 個 sample：16-bit 是 %u bytes → μ-law 是 %u bytes（比例 %.3f）\n",
           (unsigned)w.n_samples, (unsigned)(w.n_samples * 2), (unsigned)w.n_samples, 0.5);
    printf("前 5 個 sample：原始 %6d %6d %6d %6d %6d\n", w.pcm[0], w.pcm[1], w.pcm[2], w.pcm[3], w.pcm[4]);
    printf("               μ-law %6u %6u %6u %6u %6u（byte）\n", bytes[0], bytes[1], bytes[2], bytes[3], bytes[4]);
    printf("               解回  %6d %6d %6d %6d %6d\n", back[0], back[1], back[2], back[3], back[4]);
    printf("SQNR %.1f dB（規格 ≥ 30）%s\n", sqnr, sqnr >= 30.0 ? "OK" : "FAIL");
    if (wav_write(out, back, w.n_samples, w.sample_rate) == 0) printf("解回來的聲音存到 %s\n", out);
    free(bytes); free(back); wav_free(&w);
    return sqnr >= 30.0 ? 0 : 1;
}
