/*============================================================================
 *  m3_clock.c  —  模組 3：每 20 ms 醒來一次、連續 2 秒，量「醒來的時間」離「該醒的時間」差多少
 *----------------------------------------------------------------------------
 *  用法：m3_clock
 *  看到什麼：100 行「第 k 拍：該在 +xxx.0 ms，實際 +xxx.x ms，差 +0.0x ms」，最後印最大誤差。
 *           這就是 Team 2 送端「每 20 ms 一包」與收端「播放時鐘」依賴的東西：誤差要在 1 ms 以內，而且不會累積。
 *  試著改：把 sleep_until_ms(t0 + 20.0 * k) 換成 sleep_ms(20)，看誤差怎麼一拍一拍累積（Windows 上 100 拍會慢 1 秒以上）。
 *
 *  重點：第 k 拍的「截止時刻」是 t0 + 20k（絕對時刻），不是「上一拍 + 20」。
 *        這樣某一拍晚了 3 ms，下一拍還是對準格子，誤差不會累積。
 *===========================================================================*/
/* ---- 下面這些 .h 是課程自己寫的 wrapper，不是系統函式庫：都在 ../../baseline/wrapper/（Makefile 用 -I 指過去）。
 *   udp.h 一定要是第一個 #include：它在任何系統標頭之前定義 _POSIX_C_SOURCE（macOS／Linux 才看得到 select／clock_gettime），
 *   而且 Windows 的 winsock2.h 必須排在 windows.h 前面；放第一行兩邊都安全（和 Team 1 的 platform.h 同一個道理）。
 *   udp.h    UDP socket 的薄包裝（Windows 底下是 Winsock、macOS／Linux 是 POSIX socket）＋ put_u16／put_u32／get_u16／get_u32（big-endian）
 *   clock.h  單調時鐘：clock_ms（毫秒）、sleep_until_ms（睡到某個絕對時刻）
 */
#include "udp.h"                 /* 一定排第一（原因見上面） */
#include "clock.h"               /* clock_ms：單調時鐘（毫秒，double）；sleep_until_ms：睡到某個絕對時刻 */

#include <stdio.h>

#define TICK_MS 20.0
#define N_TICKS 100              /* 100 × 20 ms = 2 秒 */

int main(void) {
    printf("每 %.0f ms 一拍、共 %d 拍；用 sleep_until_ms(t0 + 20k) 的絕對時刻\n", TICK_MS, N_TICKS);
    double t0 = clock_ms();                       /* 起點。clock_ms 的數字本身沒有意義，只拿來相減 */
    double worst = 0.0;                           /* 最大誤差（絕對值） */

    for (int k = 1; k <= N_TICKS; k++) {
        double due = t0 + TICK_MS * k;            /* 第 k 拍該醒的絕對時刻 */
        sleep_until_ms(due);                      /* 大段用 sleep，最後 2 ms 忙等（spin）→ 準到 0.1 ms 左右 */
        double now = clock_ms();
        double err = now - due;                   /* 正 = 醒晚了 */
        if (err < 0) err = -err;
        if (err > worst) worst = err;
        printf("第 %3d 拍：該在 +%7.1f ms，實際 +%7.2f ms，差 %+.2f ms\n", k, due - t0, now - t0, now - due);
    }
    double total = clock_ms() - t0;
    printf("總共 %.1f ms（理想 %.0f），最大誤差 %.2f ms\n", total, TICK_MS * N_TICKS, worst);
    printf("%s\n", worst < 5.0 ? "OK：誤差在 5 ms 以內，可以當播放時鐘" : "注意：誤差太大（機器很忙？）");
    return 0;                                     /* 誤差大只是提醒（機器很忙時會這樣），不算失敗 */
}
