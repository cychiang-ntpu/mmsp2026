/*============================================================================
 *  m2_udp_recv.c  —  模組 2（收端）：recvfrom 收 datagram，印出「幾個 bytes、誰寄的、內容」
 *----------------------------------------------------------------------------
 *  用法：m2_udp_recv <port>               （另一個視窗跑 m2_udp_send 127.0.0.1 <port>）
 *  看到什麼：每收到一個 datagram 印一行 "N bytes from 127.0.0.1:xxxxx: 文字"。收到空字串、或 10 秒沒東西就結束。
 *  試著改：把 timeout 改成 2000（Team 2 的 recv 預設就是 2 秒沒封包結束）；或把 buf 改成 8 bytes 看長訊息被截掉
 *         （UDP 的規則：一個 datagram 一次收完，裝不下的部分作業系統直接丟掉）。
 *===========================================================================*/
/* ---- 下面這些 .h 是課程自己寫的 wrapper，不是系統函式庫：都在 ../../baseline/wrapper/（Makefile 用 -I 指過去）。
 *   udp.h 一定要是第一個 #include：它在任何系統標頭之前定義 _POSIX_C_SOURCE（macOS／Linux 才看得到 select／clock_gettime），
 *   而且 Windows 的 winsock2.h 必須排在 windows.h 前面；放第一行兩邊都安全（和 Team 1 的 platform.h 同一個道理）。
 *   udp.h    UDP socket 的薄包裝（Windows 底下是 Winsock、macOS／Linux 是 POSIX socket）＋ put_u16／put_u32／get_u16／get_u32（big-endian）
 */
#include "udp.h"                 /* 一定排第一（原因見上面） */

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "用法: m2_udp_recv <port>\n"); return 2; }
    int port = atoi(argv[1]);
    if (port < 1 || port > 65535) { fprintf(stderr, "錯誤: port 要是 1–65535\n"); return 2; }

    if (udp_global_init() != 0) return 1;

    udp_t u;
    if (udp_open(&u, NULL, port) != 0) return 1;  /* bind 到 0.0.0.0:port：這台電腦所有網卡送到這個 port 的都收 */
    printf("聽 0.0.0.0:%d；收到空行或 10 秒沒東西就結束\n", port);

    char buf[2048];
    size_t count = 0;
    for (;;) {
        struct sockaddr_in from;                  /* 寄件人的位址（udp_recv 會填） */
        /* udp_recv：等最多 10000 ms。回傳 >0 收到幾個 bytes；0 逾時；-1 錯誤。
         * 一次回來就是「一整個 datagram」——對方 sendto 幾個 bytes，這裡就收到幾個（裝得下的話） */
        int k = udp_recv(&u, buf, sizeof(buf) - 1, 10000, &from);
        if (k < 0) break;
        if (k == 0) { printf("10 秒沒有東西，結束\n"); break; }

        buf[k] = '\0';                            /* 收到的是純 bytes，自己補字串結尾才能用 %s 印 */
        char who[64];
        udp_format_addr(&from, who, sizeof(who)); /* 把位址變成 "127.0.0.1:54321" 這樣的字串 */
        if (buf[0] == '\0') { printf("收到空字串（對方結束了）\n"); break; }
        printf("%d bytes from %s: %s\n", k, who, buf);
        count++;
    }
    printf("共收到 %u 個 datagram\n", (unsigned)count);

    udp_close(&u);
    udp_global_cleanup();
    return 0;
}
