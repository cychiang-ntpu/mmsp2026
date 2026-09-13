/* sticky_send.c — 對 chat.c 的 TCP server 連續 send 多則短訊息，觀察「黏包」
 * 用法：  macOS/Linux:  ./sticky_send <server IP> <port> [則數=5] [每則間隔毫秒=0]
 *         Windows:      .\sticky_send.exe 127.0.0.1 5000 5 0        （PowerShell 或 cmd）
 *   間隔 0    → 黏成一包或兩包，chat 的泡泡數少於則數（每次跑可能不同）
 *   間隔 500  → 每則分開送達，chat 畫出多顆泡泡
 * 結論：TCP 是位元組串流，send 幾次 ≠ recv 幾次。解法見 nettcpudp 作業 3（長度前綴）。 */
#ifdef _WIN32
  #define _WIN32_WINNT 0x0600          /* inet_pton 需要 Vista 以上（與 chat.c 相同） */
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #define SLEEP_MS(ms) Sleep(ms)
  #define CLOSESOCK closesocket
#else
  #define _POSIX_C_SOURCE 200809L      /* -std=c99 下 Linux 才會宣告 inet_pton、nanosleep */
  #include <sys/socket.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <time.h>
  static void SLEEP_MS(int ms) {
      struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
      nanosleep(&ts, NULL);
  }
  #define CLOSESOCK close
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 3) { fprintf(stderr, "用法: %s <IP> <port> [則數] [間隔ms]\n", argv[0]); return 1; }
    int count = argc > 3 ? atoi(argv[3]) : 5;
    int gap   = argc > 4 ? atoi(argv[4]) : 0;
#ifdef _WIN32
    WSADATA w; WSAStartup(MAKEWORD(2, 2), &w);
    SetConsoleOutputCP(65001);
#endif
    int s = (int)socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a; memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_port = htons((unsigned short)atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &a.sin_addr) != 1) { fprintf(stderr, "IP 格式錯誤\n"); return 1; }
    if (connect(s, (struct sockaddr *)&a, sizeof a) < 0) { perror("connect"); return 1; }

    for (int i = 1; i <= count; i++) {
        char msg[64];
        int n = snprintf(msg, sizeof msg, "第%d則", i);   /* 沒有分隔符號，也沒有長度欄位 */
        send(s, msg, n, 0);
        printf("send #%d: %d bytes\n", i, n);
        if (gap > 0) SLEEP_MS(gap);
    }
    SLEEP_MS(300);                    /* 讓對方來得及收完再關 */
    CLOSESOCK(s);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
