/*============================================================================
 *  net.c  —  TCP 連線、send_all／recv_all、計時（殼：已完成，可以直接用）
 *----------------------------------------------------------------------------
 *  與 baseline/chat.c 第 5 節的差別：
 *    1. 監聽端可以用 --bind 指定只聽哪一個本機 IP；預設 0.0.0.0（所有網路介面），
 *       別台電腦才連得進來。
 *    2. 連線端有逾時（預設 10 秒），連不上不會無限等待。
 *    3. 兩端都會印出對方的 IP 與 port，跨機器展示時看得到連到誰。
 *    4. 出錯不再直接 exit，而是回傳錯誤，讓 main 決定結束碼。
 *    5. 多了 send_all／recv_all：TCP 是位元組串流，一次 send／recv 不保證處理完。
 *===========================================================================*/
#include "textlink.h"

uint64_t g_tx_bytes = 0;
uint64_t g_rx_bytes = 0;

/* 送出鎖（說明見 textlink.h）*/
#ifdef _WIN32
  static CRITICAL_SECTION g_send_lock;
  static int g_send_lock_ready = 0;
  void net_send_lock(void)   { if (g_send_lock_ready) EnterCriticalSection(&g_send_lock); }
  void net_send_unlock(void) { if (g_send_lock_ready) LeaveCriticalSection(&g_send_lock); }
#else
  static pthread_mutex_t g_send_lock = PTHREAD_MUTEX_INITIALIZER;
  void net_send_lock(void)   { pthread_mutex_lock(&g_send_lock); }
  void net_send_unlock(void) { pthread_mutex_unlock(&g_send_lock); }
#endif

const char *tl_sym_name(tl_sym_t sym) {
    return sym == SYM_CHAR ? "char" : sym == SYM_S16 ? "s16" : "byte";
}

const char *tl_strerror(int rc) {
    switch (rc) {
    case TL_OK:         return "成功";
    case TL_ERR_TODO:   return "這個功能的 place holder 尚未實作";
    case TL_ERR_NET:    return "網路錯誤";
    case TL_ERR_CLOSED: return "對方已關閉連線";
    case TL_ERR_PROTO:  return "收到不合規格的封包";
    case TL_ERR_NOMEM:  return "記憶體不足";
    case TL_ERR_IO:     return "檔案讀寫失敗";
    case TL_ERR_DATA:   return "資料內容不合法";
    default:            return "未知的錯誤";
    }
}

static void net_perror(const char *what) {
#ifdef _WIN32
    fprintf(stderr, "錯誤: %s (WSA error %d)\n", what, WSAGetLastError());
#else
    fprintf(stderr, "錯誤: %s (%s)\n", what, strerror(errno));
#endif
}

int net_init(void) {
#ifdef _WIN32
    /* Windows 使用網路前必須先「開機」Winsock 函式庫（POSIX 不用） */
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { net_perror("WSAStartup"); return TL_ERR_NET; }
    InitializeCriticalSection(&g_send_lock);
    g_send_lock_ready = 1;
#else
    /* 對方先關線時，send 預設會用 SIGPIPE 把整支程式殺掉；改成讓 send 回傳錯誤 */
    signal(SIGPIPE, SIG_IGN);
#endif
    return TL_OK;
}

void net_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

int net_parse_port(const char *s) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v < 1 || v > 65535) return -1;
    return (int)v;
}

/* 關掉 Nagle 演算法：frame_send 會先送 5 bytes 標頭、再送 payload，兩次小的 send 連在一起時，
 * Nagle 會讓第二次等到第一次被 ACK 才送（對方又可能延遲 ACK 數十毫秒），量到的時間就不準了。 */
static void set_nodelay(socket_t s) {
    int yes = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&yes, sizeof(yes));
}

static void format_peer(const struct sockaddr_in *a, char *out, size_t cap) {
    char ip[INET_ADDRSTRLEN] = "?";
    inet_ntop(AF_INET, (void *)&a->sin_addr, ip, sizeof(ip));
    snprintf(out, cap, "%s:%d", ip, (int)ntohs(a->sin_port));
}

/*--------------------------------------------------------------------------
 * 監聽端：socket → bind → listen → accept
 *-------------------------------------------------------------------------*/
socket_t net_listen_accept(const char *bind_ip, int port, char *peer, size_t peer_cap) {
    socket_t ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls == SOCK_INVALID) { net_perror("socket"); return SOCK_INVALID; }

    /* SO_REUSEADDR：程式重開時允許立刻重綁同一個 port */
    int yes = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((unsigned short)port);
    if (bind_ip == NULL) {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);           /* 0.0.0.0：聽本機所有網卡 */
    } else if (inet_pton(AF_INET, bind_ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "錯誤: --bind 的 IP 格式不對：%s\n", bind_ip);
        CLOSESOCK(ls);
        return SOCK_INVALID;
    }

    if (bind(ls, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        net_perror("bind（port 被占用？--bind 的 IP 不是這台電腦的？）");
        CLOSESOCK(ls);
        return SOCK_INVALID;
    }
    if (listen(ls, 1) < 0) { net_perror("listen"); CLOSESOCK(ls); return SOCK_INVALID; }

    printf("監聽中：%s:%d%s，等待對方連線...\n",
           bind_ip ? bind_ip : "0.0.0.0", port, bind_ip ? "" : "（本機所有網路介面）");
    printf("（對方要連的是這台電腦的 IP；查 IP：Windows `ipconfig`、macOS `ipconfig getifaddr en0`、Linux `hostname -I`）\n");
    fflush(stdout);

    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    socket_t cs = accept(ls, (struct sockaddr *)&from, &fromlen);   /* 阻塞直到有人連進來 */
    CLOSESOCK(ls);                       /* 一對一，不再收新連線 */
    if (cs == SOCK_INVALID) { net_perror("accept"); return SOCK_INVALID; }

    set_nodelay(cs);
    format_peer(&from, peer, peer_cap);
    printf("對方已連線：%s\n", peer);
    fflush(stdout);
    return cs;
}

/*--------------------------------------------------------------------------
 * 連線端：socket → connect（加上逾時）
 * 做法：先把 socket 設成「非阻塞」，connect 會立刻返回；再用 select 等它
 * 變成可寫（＝連上了）或逾時；最後把 socket 改回阻塞模式。
 *-------------------------------------------------------------------------*/
static int set_nonblocking(socket_t s, int on) {
#ifdef _WIN32
    u_long v = on ? 1u : 0u;
    return ioctlsocket(s, FIONBIO, &v);
#else
    int fl = fcntl(s, F_GETFL, 0);
    if (fl < 0) return -1;
    return fcntl(s, F_SETFL, on ? (fl | O_NONBLOCK) : (fl & ~O_NONBLOCK));
#endif
}

socket_t net_connect(const char *ip, int port, int timeout_ms, char *peer, size_t peer_cap) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((unsigned short)port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "錯誤: IP 格式不對：%s（要像 192.168.1.23 這樣的 IPv4 位址）\n", ip);
        return SOCK_INVALID;
    }

    socket_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == SOCK_INVALID) { net_perror("socket"); return SOCK_INVALID; }

    printf("連線到 %s:%d ...（最多等 %d 秒）\n", ip, port, timeout_ms / 1000);
    fflush(stdout);

    set_nonblocking(s, 1);
    int rc = connect(s, (struct sockaddr *)&addr, sizeof(addr));
    if (rc < 0) {
#ifdef _WIN32
        int in_progress = (WSAGetLastError() == WSAEWOULDBLOCK);
#else
        int in_progress = (errno == EINPROGRESS);
#endif
        if (!in_progress) { net_perror("connect"); CLOSESOCK(s); return SOCK_INVALID; }

        fd_set wfds, efds;
        FD_ZERO(&wfds); FD_SET(s, &wfds);
        FD_ZERO(&efds); FD_SET(s, &efds);             /* Windows 把連線失敗報在 exceptfds */
        struct timeval tv;
        tv.tv_sec  = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        rc = select((int)s + 1, NULL, &wfds, &efds, &tv);
        if (rc == 0) {
            fprintf(stderr, "錯誤: 連線逾時。對方程式開了嗎？IP 對嗎？防火牆允許了嗎？兩台在同一個網路嗎？\n");
            CLOSESOCK(s);
            return SOCK_INVALID;
        }
        int so_err = 0;
        socklen_t so_len = sizeof(so_err);
        getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&so_err, &so_len);
        if (rc < 0 || so_err != 0) {
            fprintf(stderr, "錯誤: 連不上 %s:%d（對方沒有程式在聽這個 port，或被防火牆擋下）\n", ip, port);
            CLOSESOCK(s);
            return SOCK_INVALID;
        }
    }
    set_nonblocking(s, 0);
    set_nodelay(s);

    format_peer(&addr, peer, peer_cap);
    printf("連線成功：對方 %s\n", peer);
    fflush(stdout);
    return s;
}

/*--------------------------------------------------------------------------
 * send_all／recv_all
 *
 * send(s, buf, 1000) 可能只送出 300 bytes；recv(s, buf, 1000) 可能只拿到 7 bytes
 * （半包），也可能一次拿到兩則訊息（黏包）。所以「我要剛好 n bytes」必須自己迴圈。
 * frame 層就是靠 recv_all 先拿滿 5 bytes 標頭、再拿滿 payload，訊息邊界才切得出來。
 *-------------------------------------------------------------------------*/
int send_all(socket_t s, const void *buf, size_t n) {
    const char *p = (const char *)buf;
    while (n > 0) {
        int chunk = (n > (1u << 20)) ? (1 << 20) : (int)n;
        int k = (int)send(s, p, chunk, 0);
        if (k <= 0) return TL_ERR_NET;
        p += k;
        n -= (size_t)k;
        g_tx_bytes += (uint64_t)k;
    }
    return TL_OK;
}

int recv_all(socket_t s, void *buf, size_t n) {
    char *p = (char *)buf;
    while (n > 0) {
        int chunk = (n > (1u << 20)) ? (1 << 20) : (int)n;
        int k = (int)recv(s, p, chunk, 0);
        if (k == 0) return TL_ERR_CLOSED;      /* 對方正常關閉 */
        if (k < 0)  return TL_ERR_NET;
        p += k;
        n -= (size_t)k;
        g_rx_bytes += (uint64_t)k;
    }
    return TL_OK;
}

/* 單調時鐘：不受系統時間調整影響，量測經過時間要用這個，不要用 time() 或 clock() */
double now_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
#endif
}
