/*============================================================================
 *  display.h  —  開一個視窗把 RGB 畫出來（一次只開一個視窗）
 *----------------------------------------------------------------------------
 *  實作依作業系統分三個檔案（Makefile 會自己挑一個）：
 *    display_win.c    Windows   GDI（StretchDIBits）
 *    display_mac.m    macOS     Cocoa（NSWindow ＋ 自己畫的 NSView）
 *    display_linux.c  Linux     Xlib（XPutImage；要裝 libx11-dev）
 *    display_none.c   沒有 X11 標頭時的替身：display_open 永遠回傳 -1
 *
 *  display_open(w, h, title)   開一個 w×h 的視窗。回傳 0 成功、-1 失敗（沒有桌面環境、SSH 進來的 headless 機器）；
 *                              失敗時呼叫端應該退回「只寫檔」。
 *  display_show(rgb)           把 w×h×3 bytes 的 RGB 畫到視窗上（立刻重畫，不等下一次 poll）
 *  display_poll()              處理視窗事件（移動、重畫、關閉鈕……）。回傳 1 視窗還在、0 使用者把視窗關了。
 *                              **要定期呼叫**（例如每次迴圈、至少每 100 ms 一次），不然作業系統會覺得程式沒回應。
 *  display_close()             關掉視窗；重複呼叫也安全
 *
 *  四個函式都要在同一條執行緒上呼叫（視窗系統的規定），通常就是 main。
 *===========================================================================*/
#ifndef ML_DISPLAY_H
#define ML_DISPLAY_H

#include <stdint.h>

int  display_open(int w, int h, const char *title);
void display_show(const uint8_t *rgb);
int  display_poll(void);
void display_close(void);

#endif /* ML_DISPLAY_H */
