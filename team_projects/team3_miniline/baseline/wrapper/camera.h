/*============================================================================
 *  camera.h  —  攝影機（一次只開一台；輸出固定 w×h 的 RGB）
 *----------------------------------------------------------------------------
 *  實作依作業系統分三個檔案（Makefile 會自己挑一個）：
 *    camera_win.c    Windows   Media Foundation（IMFSourceReader）
 *    camera_mac.m    macOS     AVFoundation（AVCaptureSession）
 *    camera_linux.c  Linux     V4L2（/dev/video0，mmap）
 *
 *  camera_count()                 有幾台攝影機（0 = 沒有，不算錯誤）
 *  camera_name(i, buf, cap)       第 i 台的名字（UTF-8）寫進 buf；回傳 0 成功、-1 沒有這一台
 *  camera_open(index, w, h)       打開第 index 台並開始擷取，之後 camera_read 給的每張都是 w×h 的 RGB
 *                                 （向裝置要 w×h；裝置給別的尺寸就在軟體裡裁切＋縮放）。回傳 0 成功、-1 失敗
 *                                 （沒有攝影機、被別的程式占用、沒有權限……錯誤訊息已印到 stderr）
 *  camera_read(rgb, timeout_ms)   等「下一張」最多 timeout_ms 毫秒；拿到就把 w×h×3 bytes 的 RGB 寫進 rgb。
 *                                 回傳 1 有新的一張、0 逾時（這段時間沒有新畫面）、-1 錯誤（裝置被拔掉）
 *                                 攝影機通常 30 fps，我們只要 5 fps：每 200 ms 來拿一次，拿到的是「最新」那張，
 *                                 中間沒拿的會被丟掉（不會排隊、不會越來越慢）。
 *  camera_close()                 停止並關掉；重複呼叫也安全
 *
 *  RGB 的排法和 jpeg.h 一樣：每個像素 3 bytes（R、G、B），由左到右、由上到下，沒有補位。
 *  camera_read 可以從任何執行緒呼叫，但 open／close 請在同一條執行緒（通常是 main）。
 *===========================================================================*/
#ifndef ML_CAMERA_H
#define ML_CAMERA_H

#include <stdint.h>
#include <stddef.h>

int  camera_count(void);
int  camera_name(int i, char *buf, size_t cap);
int  camera_open(int index, int w, int h);
int  camera_read(uint8_t *rgb, int timeout_ms);
void camera_close(void);

#endif /* ML_CAMERA_H */
