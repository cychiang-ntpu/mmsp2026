/*============================================================================
 *  udp.h  —  UDP socket 的薄包裝（開、設定對方、送、收＋逾時、關）；header-only
 *----------------------------------------------------------------------------
 *  【要放在 .c 檔的第一個 #include】：POSIX 那邊要在任何系統標頭之前定義 _POSIX_C_SOURCE，
 *  select／clock_gettime 等函式才會出現（和 Team 1 的 platform.h 同一個道理）。
 *
 *  UDP 和 Team 1 用的 TCP 差在哪（第 2 週 3.2 節）：
 *    TCP 像打電話：先接通，之後講的每個字保證按順序到、掉了會重講，但是要「等」。
 *    UDP 像寄明信片：每一張（datagram）各自寄，不保證會到、不保證順序、可能寄到兩張一樣的，
 *                   但是不用等。即時語音要的正是「不等」——遲到的聲音沒有意義。
 *  所以 UDP 沒有 connect／accept：兩邊各自 bind 自己的 port、用 sendto 指定對方位址寄出、用 recvfrom 收。
 *
 *  提供的東西：
 *    udp_global_init / udp_global_cleanup   程式開頭、結尾各叫一次（Windows 要「開機」Winsock）
 *    udp_open(&u, bind_ip, port)            開一個 UDP socket 並 bind（bind_ip NULL = 0.0.0.0、port 0 = 系統隨便挑）
 *    udp_set_peer(&u, ip, port)             記下「寄給誰」
 *    udp_send(&u, buf, n)                   寄一張明信片（一個 datagram）給 peer
 *    udp_recv(&u, buf, cap, timeout_ms, &from)  等最多 timeout_ms 毫秒收一張；回傳 bytes 數、0 逾時、-1 錯誤
 *    udp_close(&u)
 *    put_u16／put_u32／get_u16／get_u32    big-endian 的寫入／讀出（封包標頭用；之後你們加 seq／ts 會用到）
 *
 *  這個標頭同時提供跨平台的共用名字：socket_t、SOCK_INVALID、CLOSESOCK（和 Team 1 的 platform.h 相同）。
 *===========================================================================*/
#ifndef VL_UDP_H
#define VL_UDP_H

#ifdef _WIN32
  /* ---- Windows：Winsock ---- */
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600          /* Vista 以上的 API（inet_pton、WSAIoctl）*/
  #endif
  #include <winsock2.h>                  /* 一定要排在 windows.h 前面 */
  #include <ws2tcpip.h>
  #include <windows.h>
  #ifdef _MSC_VER
    #pragma comment(lib, "ws2_32.lib")
  #endif
  typedef SOCKET socket_t;
  #define CLOSESOCK(s)  closesocket(s)
  #define SOCK_INVALID  INVALID_SOCKET
  /* UDP 專用的怪事：對一個「沒人在聽」的 port 做 sendto，對方的作業系統會回一個 ICMP「port 不可達」，
   * Windows 把它記在我們的 socket 上，下一次 recvfrom 就報 WSAECONNRESET（明明 UDP 沒有連線可以 reset）。
   * 用 WSAIoctl 把 SIO_UDP_CONNRESET 關掉，Windows 就不再這樣做。有些 MinGW 的標頭沒有定義這個常數，自己補。 */
  #ifndef SIO_UDP_CONNRESET
    #define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
  #endif
#else
  /* ---- macOS / Linux (POSIX) ---- */
  #ifndef _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 200809L
  #endif
  #if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
    #define _DARWIN_C_SOURCE
  #endif
  #include <sys/types.h>
  #include <sys/socket.h>                /* socket、bind、sendto、recvfrom */
  #include <sys/select.h>                /* select：等 socket 有動靜，可以設逾時 */
  #include <sys/time.h>                  /* struct timeval */
  #include <netinet/in.h>                /* struct sockaddr_in、htons */
  #include <arpa/inet.h>                 /* inet_pton、inet_ntop */
  #include <unistd.h>                    /* close */
  #include <errno.h>
  typedef int socket_t;
  #define CLOSESOCK(s)  close(s)
  #define SOCK_INVALID  (-1)
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    socket_t fd;                  /* 作業系統給的 socket 代號 */
    struct sockaddr_in peer;      /* 「寄給誰」：對方的 IP 與 port（udp_set_peer 填） */
    int has_peer;                 /* 1 = peer 已經設定 */
} udp_t;

/* ---- big-endian 讀寫 ----
 * 網路上的多 byte 整數約定一律 big-endian（高位的 byte 先寫）。CPU 本身多半是 little-endian，
 * 所以不能直接 memcpy 一個 uint16_t 進封包；要一個 byte 一個 byte 拆開放。
 *   put_u16(p, 0x1234)  →  p[0] = 0x12, p[1] = 0x34
 *   get_u32(p)          →  (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]        */
static inline void put_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)(v & 0xff);
}
static inline void put_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)((v >> 16) & 0xff);
    p[2] = (uint8_t)((v >> 8) & 0xff); p[3] = (uint8_t)(v & 0xff);
}
static inline uint16_t get_u16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static inline uint32_t get_u32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* 印出「哪個網路函式失敗、作業系統說的原因」 */
static inline void udp_perror(const char *what) {
#ifdef _WIN32
    fprintf(stderr, "錯誤: %s (WSA error %d)\n", what, WSAGetLastError());
#else
    fprintf(stderr, "錯誤: %s (%s)\n", what, strerror(errno));
#endif
}

/* 程式一開始叫一次。回傳 0 成功、-1 失敗。Windows 要先 WSAStartup；POSIX 沒事做。 */
static inline int udp_global_init(void) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { udp_perror("WSAStartup"); return -1; }
#endif
    return 0;
}

/* 程式結束前叫一次，與 udp_global_init 成對 */
static inline void udp_global_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

/* 把 "192.168.1.23" 這樣的文字與 port 填進位址結構。回傳 0 成功、-1 IP 格式不對。 */
static inline int udp_make_addr(struct sockaddr_in *a, const char *ip, int port) {
    memset(a, 0, sizeof(*a));
    a->sin_family = AF_INET;
    a->sin_port   = htons((unsigned short)port);          /* port 在結構裡要用網路位元組順序（big-endian） */
    if (ip == NULL) {
        a->sin_addr.s_addr = htonl(INADDR_ANY);           /* 0.0.0.0：本機所有網卡 */
        return 0;
    }
    return inet_pton(AF_INET, ip, &a->sin_addr) == 1 ? 0 : -1;   /* inet_pton：IP 文字 → 4 bytes */
}

/* udp_open：開一個 UDP socket 並 bind 到 bind_ip:bind_port。
 *   bind_ip    只收哪一個本機 IP 的封包；NULL = 0.0.0.0（全部都收）
 *   bind_port  要聽的 port；0 = 讓作業系統隨便挑一個（純送出端用這個就好）
 *   回傳       0 成功、-1 失敗（錯誤訊息已印出） */
static inline int udp_open(udp_t *u, const char *bind_ip, int bind_port) {
    memset(u, 0, sizeof(*u));
    u->fd = SOCK_INVALID;

    struct sockaddr_in addr;
    if (udp_make_addr(&addr, bind_ip, bind_port) != 0) {
        fprintf(stderr, "錯誤: --bind 的 IP 格式不對：%s\n", bind_ip);
        return -1;
    }
    /* SOCK_DGRAM = UDP（SOCK_STREAM 才是 TCP） */
    socket_t s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == SOCK_INVALID) { udp_perror("socket"); return -1; }

    int yes = 1;                                           /* 程式重開時允許立刻重綁同一個 port */
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
    /* 收發緩衝區放大到 1 MB：macOS 預設的送出緩衝只有 9 216 bytes，一個大於它的 datagram 會直接 sendto 失敗
     * （EMSGSIZE「Message too long」）；Linux／Windows 預設比較大。放大之後三個平台都能送到 UDP 的上限 65 507 bytes。 */
    int bufsz = 1 << 20;
    setsockopt(s, SOL_SOCKET, SO_SNDBUF, (const char *)&bufsz, sizeof(bufsz));
    setsockopt(s, SOL_SOCKET, SO_RCVBUF, (const char *)&bufsz, sizeof(bufsz));

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        udp_perror("bind（port 被占用？--bind 的 IP 不是這台電腦的？）");
        CLOSESOCK(s);
        return -1;
    }
#ifdef _WIN32
    {   /* 關掉上面說的 WSAECONNRESET 行為 */
        BOOL  off = FALSE;
        DWORD got = 0;
        WSAIoctl(s, SIO_UDP_CONNRESET, &off, sizeof(off), NULL, 0, &got, NULL, NULL);
    }
#endif
    u->fd = s;
    return 0;
}

/* udp_set_peer：之後 udp_send 都寄到 ip:port。回傳 0 成功、-1 IP 格式不對。 */
static inline int udp_set_peer(udp_t *u, const char *ip, int port) {
    if (udp_make_addr(&u->peer, ip, port) != 0) {
        fprintf(stderr, "錯誤: IP 格式不對：%s（要像 192.168.1.23 這樣的 IPv4 位址）\n", ip);
        return -1;
    }
    u->has_peer = 1;
    return 0;
}

/* udp_send：把 buf 的 n 個 bytes 當成「一個」datagram 寄給 peer。
 *   回傳 寄出的 bytes 數（= n），-1 失敗。
 * 和 TCP 不同，一次 sendto 就是一個完整的封包：對方一次 recvfrom 就會拿到整包（或整包都沒有），不會「半包」。
 * 也和 TCP 不同，sendto 成功不代表對方收到——它只代表作業系統收下了這張明信片。 */
static inline int udp_send(udp_t *u, const void *buf, size_t n) {
    if (!u->has_peer) { fprintf(stderr, "錯誤: udp_send 之前要先 udp_set_peer\n"); return -1; }
    int k = (int)sendto(u->fd, (const char *)buf, (int)n, 0, (struct sockaddr *)&u->peer, sizeof(u->peer));
    if (k < 0) { udp_perror("sendto"); return -1; }
    return k;
}

/* udp_recv：等最多 timeout_ms 毫秒，收一個 datagram 到 buf（容量 cap bytes）。
 *   timeout_ms  < 0 表示一直等；0 表示看一下有沒有、沒有就立刻回來
 *   from        不是 NULL 的話，填入寄件人的 IP:port（可以拿來自動設定 peer）
 *   回傳        > 0 收到的 bytes 數；0 逾時（這段時間一個封包都沒有）；-1 錯誤
 * 做法：select 把 socket 交給作業系統說「有東西到、或時間到就叫醒我」，醒來後才 recvfrom，這樣 recvfrom 不會卡住。
 * 封包比 cap 大時，多出來的部分會被作業系統丟掉（UDP 的規則：一個 datagram 不能分兩次收）。 */
static inline int udp_recv(udp_t *u, void *buf, size_t cap, int timeout_ms, struct sockaddr_in *from) {
    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(u->fd, &rfds);
        struct timeval tv, *ptv = NULL;
        if (timeout_ms >= 0) {
            tv.tv_sec  = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            ptv = &tv;
        }
        int rc = select((int)u->fd + 1, &rfds, NULL, NULL, ptv);
        if (rc == 0) return 0;                                /* 逾時 */
        if (rc < 0) {
#ifndef _WIN32
            if (errno == EINTR) continue;                     /* 被訊號打斷（例如視窗大小改變）：再等一次 */
#endif
            udp_perror("select");
            return -1;
        }
        struct sockaddr_in src;
        socklen_t srclen = sizeof(src);
        int k = (int)recvfrom(u->fd, (char *)buf, (int)cap, 0, (struct sockaddr *)&src, &srclen);
        if (k < 0) {
#ifdef _WIN32
            int e = WSAGetLastError();
            if (e == WSAECONNRESET || e == WSAEMSGSIZE) continue;   /* 保險：上面已經關掉了；封包太大被截掉也不當錯誤 */
#else
            if (errno == EINTR || errno == ECONNREFUSED) continue;
#endif
            udp_perror("recvfrom");
            return -1;
        }
        if (from) *from = src;
        return k;
    }
}

/* 關掉 socket；重複呼叫也安全 */
static inline void udp_close(udp_t *u) {
    if (u->fd != SOCK_INVALID) CLOSESOCK(u->fd);
    u->fd = SOCK_INVALID;
    u->has_peer = 0;
}

/* 把位址寫成 "192.168.1.23:5004" 這樣的字串（印訊息用） */
static inline void udp_format_addr(const struct sockaddr_in *a, char *out, size_t cap) {
    char ip[INET_ADDRSTRLEN] = "?";
    inet_ntop(AF_INET, (void *)&a->sin_addr, ip, sizeof(ip));
    snprintf(out, cap, "%s:%d", ip, (int)ntohs(a->sin_port));
}

#endif /* VL_UDP_H */
