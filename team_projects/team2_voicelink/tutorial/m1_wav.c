/*============================================================================
 *  m1_wav.c  —  模組 1：用 wav.h 讀一個 WAV、印出它的格式、把前 1 秒寫成另一個 WAV
 *----------------------------------------------------------------------------
 *  用法：m1_wav <in.wav> [out.wav]        （out.wav 預設 "out.wav"）
 *  看到什麼：取樣率、聲道、bits、sample 數、長度（秒），以及「寫出 16000 個 sample」。
 *  試著改：把 16000 改成 8000（寫半秒）、或印出第 1000 個 sample 的值 w.pcm[1000]。
 *
 *  wav_t 是一個 struct（把幾個相關的變數綁成一包）：
 *      sample_rate  每秒幾個 sample（我們只收 16000）
 *      channels     1 = 單聲道
 *      bits         每個 sample 幾個 bit（16）
 *      pcm          指向 sample 陣列的指標（int16_t，由 wav_read 用 malloc 配置，用完要 wav_free）
 *      n_samples    陣列裡有幾個 int16_t
 *===========================================================================*/
#include "udp.h"                 /* 這個模組用不到網路，但 wrapper 要求它排第一（平台的 #define 在裡面） */
#include "wav.h"

#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "用法: m1_wav <in.wav> [out.wav]\n"); return 2; }
    const char *in  = argv[1];
    const char *out = (argc >= 3) ? argv[2] : "out.wav";

    /* wav_read(path, &w)：把結果填進 w（&w = w 的位址，這叫「輸出參數」）；回傳 0 成功、-1 讀不到、-2 格式不支援 */
    wav_t w;
    int rc = wav_read(in, &w);
    if (rc == -1) { fprintf(stderr, "錯誤: 讀不到 %s\n", in); return 2; }
    if (rc == -2) { fprintf(stderr, "錯誤: %s 不是 16-bit PCM 的 WAV\n", in); return 2; }

    double seconds = (double)w.n_samples / w.channels / w.sample_rate;   /* 立體聲時 n_samples 是左右加起來的 */
    printf("%s\n", in);
    printf("  取樣率   %u Hz\n", (unsigned)w.sample_rate);
    printf("  聲道     %u\n", (unsigned)w.channels);
    printf("  bits     %u\n", (unsigned)w.bits);
    printf("  sample   %u 個（每個 int16_t，%u bytes）\n", (unsigned)w.n_samples, (unsigned)(w.n_samples * 2));
    printf("  長度     %.3f 秒 = %u 個 20 ms 的 frame\n", seconds, (unsigned)((w.n_samples + 319) / 320));
    printf("  前 5 個 sample：%d %d %d %d %d\n", w.pcm[0], w.pcm[1], w.pcm[2], w.pcm[3], w.pcm[4]);

    /* Team 2 只收 16 kHz／單聲道／16-bit；不是的話規格要印錯誤、結束碼 2 */
    if (w.sample_rate != 16000 || w.channels != 1 || w.bits != 16) {
        fprintf(stderr, "錯誤: 只接受 16000 Hz／1 聲道／16-bit\n");
        wav_free(&w);
        return 2;
    }

    /* 寫出前 1 秒（16000 個 sample）；檔案不到 1 秒就全部寫 */
    size_t n = w.n_samples < 16000 ? w.n_samples : 16000;
    if (wav_write(out, w.pcm, n, w.sample_rate) != 0) { fprintf(stderr, "錯誤: 寫不出 %s\n", out); wav_free(&w); return 2; }
    printf("寫出 %u 個 sample（%.3f 秒）到 %s\n", (unsigned)n, (double)n / w.sample_rate, out);

    wav_free(&w);                /* 釋放 wav_read 配置的記憶體（和 malloc／free 成對一樣） */
    return 0;
}
