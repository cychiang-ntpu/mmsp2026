/*============================================================================
 *  m5_play_wav.c  —  模組 5：把一個 WAV 透過 audio callback 播出來（每次 callback 餵 n 個 sample）
 *----------------------------------------------------------------------------
 *  用法：m5_play_wav <in.wav>             （16 kHz／單聲道／16-bit）
 *  看到什麼：喇叭播出聲音；結束印「播了 N 個 sample，callback 被叫了 M 次，檔案結束後補了 K 次 0」。
 *  試著改：在 callback 裡每次只給一半（spk 前半填聲音、後半填 0）聽聽看「欠載」是什麼聲音；
 *         或把 pos 每次多跳 320（跳過一個 frame）聽聽「掉包」的聲音。
 *
 *  和 Team 2 的關係：live 模式的喇叭就是這樣餵的——每次 callback 向 jitter buffer 要一個 frame；
 *  這裡「要 frame」的來源是檔案的下一段，檔案播完就填 0（Team 2 裡是 PLC）。
 *===========================================================================*/
/* ---- 下面這些 .h 是課程自己寫的 wrapper，不是系統函式庫：都在 ../../baseline/wrapper/（Makefile 用 -I 指過去）。
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
#include "audio.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    wav_t    w;                                   /* 整個檔案在記憶體裡 */
    size_t   pos;                                 /* 播到第幾個 sample */
    unsigned calls, zeros;                        /* callback 次數；檔案結束後補 0 的次數 */
} play_t;

static void cb(const int16_t *mic, int16_t *spk, size_t n, void *user) {
    play_t *p = (play_t *)user;
    (void)mic;                                    /* 這個模組不錄音；(void) 告訴編譯器「我知道沒用到」免得警告 */
    if (!spk) return;
    p->calls++;
    size_t left = p->w.n_samples - p->pos;        /* 檔案還剩幾個 sample */
    size_t take = left < n ? left : n;            /* 這次給得出幾個 */
    memcpy(spk, p->w.pcm + p->pos, take * sizeof(int16_t));
    if (take < n) {                               /* 不夠 n 個：剩下的填 0（spk 一定要填滿） */
        memset(spk + take, 0, (n - take) * sizeof(int16_t));
        p->zeros++;
    }
    p->pos += take;
}

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "用法: m5_play_wav <in.wav>\n"); return 2; }
    static play_t p;
    int rc = wav_read(argv[1], &p.w);
    if (rc != 0) { fprintf(stderr, "錯誤: 讀不到或不是 16-bit PCM：%s\n", argv[1]); return 2; }
    if (p.w.sample_rate != AUDIO_RATE || p.w.channels != 1) {
        fprintf(stderr, "錯誤: 只接受 16000 Hz／1 聲道\n"); wav_free(&p.w); return 2;
    }
    double seconds = (double)p.w.n_samples / AUDIO_RATE;

    if (audio_start_duplex(cb, &p) != 0) { fprintf(stderr, "沒有裝置（或開不了喇叭）\n"); wav_free(&p.w); return 1; }
    printf("播放 %s（%.2f 秒）…\n", argv[1], seconds);
    fflush(stdout);
    double t0 = clock_ms();
    while (p.pos < p.w.n_samples && clock_ms() - t0 < seconds * 1000.0 + 2000.0) sleep_ms(50);   /* main 只是等播完 */
    sleep_ms(300);                                /* 讓喇叭的輸出 buffer 把最後一點播掉 */
    audio_stop();

    printf("播了 %u 個 sample，callback 被叫了 %u 次（平均每次 %.0f 個），檔案結束後補了 %u 次 0\n",
           (unsigned)p.pos, p.calls, p.calls ? (double)p.pos / p.calls : 0.0, p.zeros);
    wav_free(&p.w);
    return 0;
}
