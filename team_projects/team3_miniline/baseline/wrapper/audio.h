/*============================================================================
 *  audio.h  —  麥克風與喇叭（16 kHz、單聲道、16-bit）；底下是 miniaudio，實作在 audio.c
 *----------------------------------------------------------------------------
 *  audio_start_duplex(cb, user)   打開麥克風與喇叭，開始「同時錄、同時放」。回傳 0 成功、-1 失敗。
 *  audio_stop()                   停止並關掉裝置。
 *  audio_list_devices()           印出所有喇叭與麥克風的名字；回傳裝置總數（沒有裝置時回傳 0，不算錯誤）。
 *
 *  callback（回呼）是什麼：你不是去「要」聲音，而是把一個函式交給音訊系統，它每 20 ms 在「它自己的執行緒」
 *  上呼叫你一次，給你剛錄到的 n 個 sample（mic），並要你把接下來要播的 n 個 sample 填進 spk。
 *    typedef void (*audio_cb_t)(const int16_t *mic, int16_t *spk, size_t n_samples, void *user);
 *  規則：
 *    1. n_samples 通常是 320（20 ms），但作業系統偶爾會給不同的數目；你的程式要能處理任意 n。
 *    2. spk 一定要填滿（沒東西播就填 0），不然喇叭會播出垃圾。
 *    3. callback 裡不能做慢的事（開檔、printf 很多行、等網路）：時間超過 20 ms 聲音就會斷。sendto 很快，可以。
 *    4. 它在另一條執行緒上跑：和 main 共用的變數要用鎖保護（見 voicelink_raw.c 的 ring）。
 *    5. 退路：有些機器沒辦法把麥克風與喇叭開成同一個 duplex 裝置，audio.c 會改開兩個裝置，
 *       這時 callback 會被分開呼叫：錄音那次 spk == NULL、播放那次 mic == NULL。所以兩個指標都要先檢查。
 *  user 是你自己的任何資料的位址，原封不動傳回給 callback（C 沒有閉包，都用這招）。
 *===========================================================================*/
#ifndef VL_AUDIO_H
#define VL_AUDIO_H

#include <stdint.h>
#include <stddef.h>

#define AUDIO_RATE          16000     /* Hz */
#define AUDIO_FRAME_SAMPLES 320       /* 20 ms */

typedef void (*audio_cb_t)(const int16_t *mic, int16_t *spk, size_t n_samples, void *user);

int  audio_start_duplex(audio_cb_t cb, void *user);
void audio_stop(void);
int  audio_list_devices(void);

#endif /* VL_AUDIO_H */
