/*============================================================================
 *  m2_udp_send.c  —  模組 2（送端）：打一行字、按 Enter，就用 sendto 寄出一個 datagram
 *----------------------------------------------------------------------------
 *  用法：m2_udp_send <ip> <port>          （先開另一個視窗跑 m2_udp_recv <port>）
 *  看到什麼：每打一行，對面印一行「N bytes from ip:port: 文字」。一次 sendto = 一個 datagram（一張明信片），
 *           對方一次 recvfrom 就拿到整行，不會「半行」、也不會兩行黏在一起（TCP 才會）。打空行結束兩邊。
 *  試著改：連打兩行很長的字（超過 1400 bytes）看看；或把 ip 改成沒人在聽的位址——sendto 照樣「成功」（UDP 不知道對方在不在）。
 *===========================================================================*/
#include "udp.h"                 /* 一定排第一 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc != 3) { fprintf(stderr, "用法: m2_udp_send <ip> <port>\n"); return 2; }
    int port = atoi(argv[2]);
    if (port < 1 || port > 65535) { fprintf(stderr, "錯誤: port 要是 1–65535\n"); return 2; }

    if (udp_global_init() != 0) return 1;        /* Windows 要先「開機」Winsock */

    udp_t u;
    /* udp_open(&u, NULL, 0)：bind 到 0.0.0.0 的「隨便一個 port」——純送出端不需要固定 port。
     * udp_set_peer：記下之後 udp_send 都寄給誰（UDP 沒有 connect，只是記住位址） */
    if (udp_open(&u, NULL, 0) != 0 || udp_set_peer(&u, argv[1], port) != 0) return 1;
    printf("寄到 %s:%d。打一行字按 Enter 就寄出一個 datagram；打空行結束。\n", argv[1], port);

    char line[2048];
    size_t count = 0;
    while (fgets(line, sizeof(line), stdin) != NULL) {        /* 讀一行（含結尾的 '\n'） */
        size_t n = strlen(line);
        if (n > 0 && line[n - 1] == '\n') line[--n] = '\0';    /* 把 '\n' 去掉，寄的是純文字 */
        /* 寄出：一次 udp_send 就是一個完整的 datagram。空行也寄（1 個 byte 的空字串才不會變成 0 bytes），對面看到空字串就結束 */
        int k = udp_send(&u, line, n == 0 ? 1 : n);
        if (k < 0) break;
        count++;
        printf("  已寄出 %d bytes\n", k);
        if (n == 0) break;
    }
    printf("共寄出 %u 個 datagram\n", (unsigned)count);

    udp_close(&u);
    udp_global_cleanup();
    return 0;
}
