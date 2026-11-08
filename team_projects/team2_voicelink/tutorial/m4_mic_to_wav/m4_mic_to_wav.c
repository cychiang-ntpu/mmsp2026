/*============================================================================
 *  m4_mic_to_wav.c  —  模組 4：用 audio.h 錄 3 秒麥克風，存成 mic.wav
 *----------------------------------------------------------------------------
 *  用法：m4_mic_to_wav [mic.wav]
 *  看到什麼：「錄音中…」3 秒後印「錄了 48000 個 sample，callback 被叫了 N 次」，mic.wav 可以用播放器聽。
 *           沒有麥克風／喇叭時印「沒有裝置」、結束碼 1，不會當掉。
 *  試著改：把 3 秒改成 1 秒；印出每次 callback 的 n（通常是 320，但不保證——所以 Team 2 的程式要「湊」）。
 *
 *  callback（回呼）：你不是去「要」聲音，而是把一個函式交給音訊系統，它每 20 ms 在「它自己的執行緒」上呼叫你一次，
 *  給你剛錄到的 n 個 sample（mic），並要你把接下來要播的 n 個 sample 填進 spk。
 *  規則：spk 一定要填（沒東西就填 0）；mic 或 spk 可能是 NULL（退路模式分開叫）；callback 裡不做慢的事（不開檔、不 printf 很多）。
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
#include "audio.h"               /* audio_start_duplex／audio_stop；實作在 ../../baseline/wrapper/audio.c（要連結 audio.o） */

#include <stdio.h>
#include <string.h>

#define SECONDS 3
#define MAX_SAMPLES (AUDIO_RATE * SECONDS)        /* 16000 × 3 = 48000 */

/* 錄音的狀態：callback 和 main 共用。這個模組只有 callback 寫、main 在 audio_stop 之後才讀，所以不用鎖。 */
typedef struct {
    int16_t  pcm[MAX_SAMPLES];
    size_t   filled;                              /* 已經錄了幾個 sample */
    unsigned calls;                               /* callback 被叫了幾次 */
} rec_t;

static void cb(const int16_t *mic, int16_t *spk, size_t n, void *user) {
    rec_t *r = (rec_t *)user;                     /* user 就是我們交給 audio_start_duplex 的 rec_t 位址（C 沒有閉包，都用這招） */
    r->calls++;
    if (mic) {                                    /* 錄：把 n 個 sample 接在後面，滿了就不收 */
        size_t take = MAX_SAMPLES - r->filled;
        if (take > n) take = n;
        memcpy(r->pcm + r->filled, mic, take * sizeof(int16_t));
        r->filled += take;
    }
    if (spk) memset(spk, 0, n * sizeof(int16_t)); /* 放：這個模組不播東西，填 0（不填會播出垃圾） */
}

int main(int argc, char **argv) {
    const char *out = (argc >= 2) ? argv[1] : "mic.wav";
    static rec_t r;                               /* static：96 KB，不放堆疊上；static 也保證一開始全是 0 */

    if (audio_start_duplex(cb, &r) != 0) { fprintf(stderr, "沒有裝置（或開不了麥克風／喇叭）\n"); return 1; }
    printf("錄音中… %d 秒（對著麥克風講話）\n", SECONDS);
    fflush(stdout);
    double t0 = clock_ms();
    while (r.filled < MAX_SAMPLES && clock_ms() - t0 < SECONDS * 1000.0 + 500.0) sleep_ms(50);   /* main 只是等 */
    audio_stop();                                 /* 之後 callback 不會再被叫，現在讀 r 是安全的 */

    if (wav_write(out, r.pcm, r.filled, AUDIO_RATE) != 0) { fprintf(stderr, "錯誤: 寫不出 %s\n", out); return 2; }
    printf("錄了 %u 個 sample（%.2f 秒），callback 被叫了 %u 次（平均每次 %.0f 個 sample），存到 %s\n",
           (unsigned)r.filled, (double)r.filled / AUDIO_RATE, r.calls, r.calls ? (double)r.filled / r.calls : 0.0, out);
    return 0;
}
