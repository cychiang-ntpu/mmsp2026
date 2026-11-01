/*============================================================================
 *  display_none.c  —  display.h 的「沒有視窗」替身
 *----------------------------------------------------------------------------
 *  Linux 上找不到 X11 的標頭（沒裝 libx11-dev）時 Makefile 會改編這個檔案：display_open 永遠回傳 -1，
 *  呼叫端就會退回「只寫檔」。mock 模式與評測 1–7 都不需要視窗，所以這樣也能把作業做完。
 *  要視窗：sudo apt install libx11-dev，再 make clean && make。
 *===========================================================================*/
#include <stdio.h>
#include "display.h"

int display_open(int w, int h, const char *title) {
    (void)w; (void)h; (void)title;
    fprintf(stderr, "這個版本沒有視窗支援（編譯時沒有 X11 標頭；sudo apt install libx11-dev 後重新 make）\n");
    return -1;
}
void display_show(const uint8_t *rgb) { (void)rgb; }
int  display_poll(void) { return 0; }
void display_close(void) {}
