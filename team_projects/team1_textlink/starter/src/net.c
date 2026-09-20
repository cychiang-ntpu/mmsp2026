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
 *
 *  ── 這個檔案的地圖 ─────────────────────────────────────────────────────────
 *  負責的事：整支程式裡「最底層、直接碰作業系統網路功能」的部分。這一層只管 bytes 怎麼送、怎麼收，
 *  完全不知道 frame、UTF-8、Huffman 是什麼。
 *
 *  先建立一個概念（第 2 週 3.2 節看過）：
 *    socket ≈ 一支電話。要通話，兩邊各要有一支。
 *    IP     ≈ 大樓的總機號碼（哪一台電腦）；port ≈ 分機（那台電腦上的哪一支程式）。
 *    監聽端（server）：裝好電話、公布分機、等鈴響、接起來  → socket、bind、listen、accept
 *    連線端（client）：拿起電話撥號                      → socket、connect
 *    接通之後兩邊地位完全相同，都用 send／recv 講話。
 *
 *  建議的閱讀順序：
 *    1. net_init                  程式一開始做一次的準備
 *    2. net_listen_accept         監聽端怎麼等到一條連線
 *    3. net_connect               連線端怎麼連出去；逾時那一段第一次讀可以只看函式前面的說明
 *    4. send_all／recv_all  ★     全檔最重要的兩個函式，口試會問「為什麼要迴圈」
 *    5. now_ms                    量時間用的時鐘
 *    6. net_send_lock／unlock     送出鎖（讀到 src/chat.c 的兩條執行緒時再回來看）
 *
 *  資料流中的位置（→ 是送出的方向，收的方向反過來走）：
 *    鍵盤／檔案 → (huff_encode) → frame_send〔src/frame.c〕→ send_all〔這裡〕→ 作業系統 → 網路
 *    網路 → 作業系統 → recv_all〔這裡〕→ frame_recv〔src/frame.c〕→ (huff_decode) → 螢幕／檔案
 *
 *  可以跳過：tl_sym_name、tl_strerror、net_perror、format_peer 只是把代號轉成給人看的文字。
 *===========================================================================*/
#include "textlink.h"

/* 全域計數器：這支程式到目前為止總共送出／收到幾個 bytes，由 send_all／recv_all 累加。
 * 殼本身沒有用到它們（STATS 的 wire_bytes 是 src/transfer.c 逐個 frame 加出來的），留給你們做量測或除錯時對帳用。 */
uint64_t g_tx_bytes = 0;
uint64_t g_rx_bytes = 0;

/* 送出鎖（說明見 textlink.h）
 *
 * 為什麼需要「鎖」：聊天時有兩條執行緒同時在跑（見 src/chat.c），兩條都可能送 frame。
 * 一個 frame 是分兩次 send_all 送出的（先 5 bytes 標頭、再 payload）。如果 A 執行緒剛送完標頭，
 * B 執行緒就插進來送它的標頭，對方收到的會是「A 標頭、B 標頭、A payload…」，整條串流從此對不上。
 * 互斥鎖（mutex）就像只有一把鑰匙的房間：net_send_lock 拿鑰匙（別人拿走了就在門口等），
 * net_send_unlock 還鑰匙。frame_send 在送標頭之前上鎖、送完 payload 才解鎖，所以一個 frame 一定是連續的。
 *
 * 兩個平台的鎖不同：Windows 用 CRITICAL_SECTION，使用前必須先初始化（在 net_init 裡做）；
 * g_send_lock_ready 記錄「初始化過了沒」，還沒初始化就不上鎖（只有一條執行緒時本來就不需要鎖），不會因此當掉。
 * POSIX 的 pthread_mutex_t 可以在宣告時直接用 PTHREAD_MUTEX_INITIALIZER 初始化，所以不需要那個旗標。
 * static：這些變數只有這個 .c 檔看得到，別的檔案只能透過下面兩個函式使用這把鎖。 */
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

/* 符號種類 → 要印在 STATS 與畫面上的名字。a ? b : c 是「a 成立就取 b，否則取 c」；這裡連用兩次。 */
const char *tl_sym_name(tl_sym_t sym) {
    return sym == SYM_CHAR ? "char" : sym == SYM_S16 ? "s16" : "byte";
}

/* 錯誤碼（textlink.h 裡的 TL_ERR_*）→ 中文說明。回傳的是字串常數，呼叫端不需要、也不可以 free。 */
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

/* 印出「哪個網路函式失敗、作業系統說的原因」。what 是自己取的說明文字。只給這個檔案內部用。
 * 系統函式失敗時只回傳 -1 之類的值，真正的原因另外放：POSIX 放在全域變數 errno（strerror 把它轉成英文說明），
 * Windows 的網路函式則要呼叫 WSAGetLastError() 拿錯誤編號。 */
static void net_perror(const char *what) {
#ifdef _WIN32
    fprintf(stderr, "錯誤: %s (WSA error %d)\n", what, WSAGetLastError());
#else
    fprintf(stderr, "錯誤: %s (%s)\n", what, strerror(errno));
#endif
}

/* 程式一開始呼叫一次（src/main.c），之後才能用任何網路功能。回傳 TL_OK 或 TL_ERR_NET。 */
int net_init(void) {
#ifdef _WIN32
    /* Windows 使用網路前必須先「開機」Winsock 函式庫（POSIX 不用） */
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { net_perror("WSAStartup"); return TL_ERR_NET; }
    InitializeCriticalSection(&g_send_lock);         /* 順便把送出鎖準備好 */
    g_send_lock_ready = 1;
#else
    /* 對方先關線時，send 預設會用 SIGPIPE 把整支程式殺掉；改成讓 send 回傳錯誤 */
    /* signal（訊號）是作業系統通知程式「出事了」的方式，多數訊號的預設處理就是結束程式，而且不會印任何訊息。
     * SIG_IGN = 忽略這個訊號。忽略之後，對已經斷掉的連線 send 會回傳 -1，send_all 就能回報 TL_ERR_NET，
     * 由我們自己決定怎麼收尾。Windows 沒有 SIGPIPE，所以不需要這一行。 */
    signal(SIGPIPE, SIG_IGN);
#endif
    return TL_OK;
}

/* 程式結束前呼叫一次（src/main.c）：把 Winsock「關機」，與 net_init 的 WSAStartup 成對。POSIX 沒有事要做。 */
void net_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

/* 把命令列上的字串（例如 "5000"）轉成 port 號碼。合法回傳 1–65535，不合法回傳 -1。呼叫者：src/main.c。
 * 不用 atoi 的原因：atoi("50a0") 會回傳 50、atoi("abc") 會回傳 0，分不出使用者打錯了。
 * strtol 會把「轉換停在哪個字元」寫進 end：
 *   end == s      一個數字都沒讀到
 *   *end != '\0'  數字後面還黏著別的字
 * port 在 TCP 標頭裡是 16 bits，所以最大 65535；0 有特殊意義，不能拿來用。 */
int net_parse_port(const char *s) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v < 1 || v > 65535) return -1;
    return (int)v;
}

/* 關掉 Nagle 演算法：frame_send 會先送 5 bytes 標頭、再送 payload，兩次小的 send 連在一起時，
 * Nagle 會讓第二次等到第一次被 ACK 才送（對方又可能延遲 ACK 數十毫秒），量到的時間就不準了。 */
/* 補充：Nagle 演算法是 TCP 預設開啟的省頻寬機制：把很小的資料先留著，湊多一點再一起送，避免網路上充滿
 * 「為了幾個 bytes 就送一個封包」的浪費。對大量傳輸有好處，對我們這種「小標頭＋資料」的送法則會多出延遲。
 * setsockopt 是「調整這個 socket 的選項」：第 2、3 個參數指定哪一層（TCP）的哪個選項（TCP_NODELAY），
 * 第 4、5 個參數是選項值的位址與大小（這裡是 int 的 1，代表打開）。
 * (const char *) 轉型是因為 Windows 版的 setsockopt 規定這個參數的型別是 const char *。
 * 沒有檢查回傳值：設定失敗只是慢一點，功能不受影響。 */
static void set_nodelay(socket_t s) {
    int yes = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&yes, sizeof(yes));
}

/* 把位址結構 a 裡的 IP 與 port 寫成 "192.168.1.23:5000" 這樣的字串，放進 out（容量 cap bytes）。
 * inet_ntop：4 bytes 的二進位 IP → 文字。ntohs：port 在結構裡是「網路位元組順序」（big-endian），
 * 轉回這台電腦的順序才能當一般整數印出來（n = network、h = host、s = short 16 bits）。 */
static void format_peer(const struct sockaddr_in *a, char *out, size_t cap) {
    char ip[INET_ADDRSTRLEN] = "?";
    inet_ntop(AF_INET, (void *)&a->sin_addr, ip, sizeof(ip));
    snprintf(out, cap, "%s:%d", ip, (int)ntohs(a->sin_port));
}

/*--------------------------------------------------------------------------
 * 監聽端：socket → bind → listen → accept
 *
 * net_listen_accept：在 bind_ip:port 等一個人連進來，回傳「已經接通」的那個 socket。
 *   bind_ip   只聽哪一個本機 IP（文字，例如 "192.168.1.23"）；NULL 表示全部都聽
 *   port      要聽的 port
 *   peer      輸出：對方的 "IP:port" 字串；peer_cap 是 peer 這個陣列的容量
 *   回傳      成功：接通的 socket；失敗：SOCK_INVALID（錯誤訊息已經印出）
 *   呼叫者    src/main.c（chat server）、src/transfer.c 的 transfer_recv
 * 注意：這個函式會「阻塞」（block）：停在 accept 那一行不往下走，直到有人連進來為止。
 *-------------------------------------------------------------------------*/
socket_t net_listen_accept(const char *bind_ip, int port, char *peer, size_t peer_cap) {
    /* 1. socket：跟作業系統要一支「電話」。AF_INET = IPv4，SOCK_STREAM = TCP（可靠、照順序的位元組串流）。
     *    這一支（ls = listening socket）只用來等人打進來，不拿來講話。 */
    socket_t ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls == SOCK_INVALID) { net_perror("socket"); return SOCK_INVALID; }

    /* SO_REUSEADDR：程式重開時允許立刻重綁同一個 port */
    /* （沒有設的話，上一條連線剛關掉的一、兩分鐘內，作業系統可能還保留著那個 port，bind 會失敗；
     *   開發時一直重開程式，這會很煩。） */
    int yes = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

    /* 2. 填「位址卡」struct sockaddr_in：IPv4、哪個 IP、哪個 port。先用 memset 整個清成 0，沒填到的欄位才不會是垃圾值。
     *    htons／htonl：把整數轉成「網路位元組順序」（big-endian）。IP 與 port 是要給網路上其他機器看的，
     *    大家約定一律用 big-endian，不管自己的 CPU 是哪一種（h = host、n = network、s = 16 bits、l = 32 bits）。 */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((unsigned short)port);
    if (bind_ip == NULL) {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);           /* 0.0.0.0：聽本機所有網卡 */
    } else if (inet_pton(AF_INET, bind_ip, &addr.sin_addr) != 1) {     /* inet_pton：IP 文字 → 4 bytes；回傳 1 才是成功 */
        fprintf(stderr, "錯誤: --bind 的 IP 格式不對：%s\n", bind_ip);
        CLOSESOCK(ls);                                      /* 每一條失敗的路都要記得把已經開的 socket 關掉 */
        return SOCK_INVALID;
    }

    /* 3. bind：把這支電話登記到「這個 IP 的這個分機（port）」。同一個 port 同時只能有一支程式登記。
     *    (struct sockaddr *) 轉型：bind 是各種網路通用的函式，參數型別是通用的 sockaddr；我們填的是 IPv4 專用的
     *    sockaddr_in，傳進去時要轉型，並用第三個參數告訴它實際的大小。 */
    if (bind(ls, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        net_perror("bind（port 被占用？--bind 的 IP 不是這台電腦的？）");
        CLOSESOCK(ls);
        return SOCK_INVALID;
    }
    /* 4. listen：開始接受來電。第二個參數是「還沒被 accept 的來電最多排幾通」；我們是一對一，1 就夠了。 */
    if (listen(ls, 1) < 0) { net_perror("listen"); CLOSESOCK(ls); return SOCK_INVALID; }

    printf("監聽中：%s:%d%s，等待對方連線...\n",
           bind_ip ? bind_ip : "0.0.0.0", port, bind_ip ? "" : "（本機所有網路介面）");
    printf("（對方要連的是這台電腦的 IP；查 IP：Windows `ipconfig`、macOS `ipconfig getifaddr en0`、Linux `hostname -I`）\n");
    fflush(stdout);                      /* printf 的文字可能先留在緩衝區；接下來要停很久，先強迫它顯示出來 */

    /* 5. accept：接起電話。回傳的是一個「新的」socket（cs = connected socket），之後的 send／recv 都用它；
     *    from 會被填入對方的 IP 與 port。fromlen 要先放進 from 的大小，accept 會改寫成實際填了多少。 */
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
 *
 * 為什麼這麼麻煩：一般的（阻塞的）connect 會一直等到連上或作業系統放棄為止。IP 打錯、或被防火牆
 * 默默丟掉時，那可能是幾十秒到一、兩分鐘，而且程式沒辦法自己決定要等多久。
 *   阻塞（blocking）    ：呼叫下去，事情沒做完就不回來。
 *   非阻塞（non-blocking）：呼叫下去立刻回來；事情還沒做完就回傳「錯誤」，錯誤碼的意思是「進行中」。
 *   select               ：把一組 socket 交給作業系統，「其中任何一個有動靜、或過了這麼久，就叫醒我」。
 *                          逾時長度由我們給，這就是 10 秒上限的來源。
 *-------------------------------------------------------------------------*/

/* 把 socket 切成非阻塞（on = 1）或切回阻塞（on = 0）。回傳 0 成功。
 * POSIX 的做法是「先讀出目前的旗標 → 把 O_NONBLOCK 這個 bit 打開（|）或關掉（& ~）→ 寫回去」，其他旗標不受影響。 */
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

/* net_connect：連到 ip:port，最多等 timeout_ms 毫秒。
 *   ip        對方的 IPv4 位址（文字）；只接受數字形式，不接受主機名稱
 *   peer      輸出：對方的 "IP:port" 字串；peer_cap 是它的容量
 *   回傳      成功：接通的 socket（已切回阻塞模式）；失敗：SOCK_INVALID（錯誤訊息已經印出）
 *   呼叫者    src/main.c（chat client）、src/transfer.c 的 transfer_send */
socket_t net_connect(const char *ip, int port, int timeout_ms, char *peer, size_t peer_cap) {
    /* 填對方的位址卡（欄位意義同 net_listen_accept）。先檢查 IP 格式，格式不對就不必開 socket 了。 */
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

    /* connect = 撥號。TCP 連線要雙方來回交換三個封包（三方交握）才算接通。
     * 非阻塞模式下 connect 通常立刻回傳 -1，但那不一定是失敗：要看錯誤碼。
     * 「進行中」在 POSIX 叫 EINPROGRESS、在 Windows 叫 WSAEWOULDBLOCK；其他錯誤碼才是真的失敗。
     * （rc == 0 代表當場就連上了，連到本機時有可能發生；這時整個 if 區塊都跳過。） */
    set_nonblocking(s, 1);
    int rc = connect(s, (struct sockaddr *)&addr, sizeof(addr));
    if (rc < 0) {
#ifdef _WIN32
        int in_progress = (WSAGetLastError() == WSAEWOULDBLOCK);
#else
        int in_progress = (errno == EINPROGRESS);
#endif
        if (!in_progress) { net_perror("connect"); CLOSESOCK(s); return SOCK_INVALID; }

        /* 用 select 等結果。fd_set 是「一組 socket」：FD_ZERO 清空、FD_SET 把 s 放進去。
         *   wfds（可寫）   ：連線完成時，socket 會變成「可以寫入」。
         *   efds（例外）   ：Windows 把「連線失敗」報在這一組；POSIX 則是報成可寫，再用下面的 SO_ERROR 分辨成敗。
         * 第一個參數是「最大的 socket 編號 + 1」（POSIX 需要；Windows 會忽略它）。
         * struct timeval 把逾時拆成「秒」與「微秒」兩個欄位：毫秒 ÷ 1000 得到秒，餘數 × 1000 得到微秒。 */
        fd_set wfds, efds;
        FD_ZERO(&wfds); FD_SET(s, &wfds);
        FD_ZERO(&efds); FD_SET(s, &efds);             /* Windows 把連線失敗報在 exceptfds */
        struct timeval tv;
        tv.tv_sec  = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        rc = select((int)s + 1, NULL, &wfds, &efds, &tv);
        /* select 的回傳值：0 = 時間到了都沒有動靜；負值 = select 本身出錯；正值 = 有幾個 socket 有動靜 */
        if (rc == 0) {
            fprintf(stderr, "錯誤: 連線逾時。對方程式開了嗎？IP 對嗎？防火牆允許了嗎？兩台在同一個網路嗎？\n");
            CLOSESOCK(s);
            return SOCK_INVALID;
        }
        /* 有動靜不等於連上了：對方那個 port 沒有程式在聽時，對方的作業系統會立刻拒絕，這也算「有結果」。
         * 用 getsockopt 讀出 SO_ERROR（這個 socket 最近一次的錯誤碼）：0 才是真的連上。 */
        int so_err = 0;
        socklen_t so_len = sizeof(so_err);
        getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&so_err, &so_len);
        if (rc < 0 || so_err != 0) {
            fprintf(stderr, "錯誤: 連不上 %s:%d（對方沒有程式在聽這個 port，或被防火牆擋下）\n", ip, port);
            CLOSESOCK(s);
            return SOCK_INVALID;
        }
    }
    /* 連上了。切回阻塞模式：之後的 send／recv 都假設「呼叫下去會等到有結果」，程式比較好寫。 */
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
 *
 * 為什麼 send 會只送一部分：send 做的事其實只是「把資料複製進作業系統的送出緩衝區」，真正送上網路是
 * 作業系統之後的事。緩衝區快滿的時候（對方收得慢、網路慢），它只收得下一部分，就回傳「這次收了 k bytes」，
 * 剩下的要我們自己再送一次。所以：p 往後移 k、n 減掉 k，直到 n 變成 0。
 * recv 也一樣：回傳的是「這次拿到幾個 bytes」，1 到你要求的數量之間都有可能（第 2 週的黏包實驗）。
 *
 * 兩個函式的參數相同：s 是已接通的 socket，buf 是資料（或要放資料的地方），n 是要處理的 bytes 數。
 * 回傳 TL_OK 表示 n bytes 全部處理完了。呼叫者：src/frame.c 的 frame_send／frame_recv。
 *-------------------------------------------------------------------------*/
int send_all(socket_t s, const void *buf, size_t n) {
    const char *p = (const char *)buf;              /* void * 不能做加法；轉成 char * 之後，p += k 就是往後移 k 個 bytes */
    while (n > 0) {
        /* send 的長度參數在 Windows 上是 int，而 n 是 size_t（64 位元系統上可以比 int 大很多）。
         * 每次最多交出 1 MiB（1 << 20 = 2 的 20 次方），轉型成 int 就一定安全。 */
        int chunk = (n > (1u << 20)) ? (1 << 20) : (int)n;
        int k = (int)send(s, p, chunk, 0);
        if (k <= 0) return TL_ERR_NET;              /* 送不出去：連線斷了（net_init 已忽略 SIGPIPE，所以會走到這裡） */
        p += k;
        n -= (size_t)k;
        g_tx_bytes += (uint64_t)k;
    }
    return TL_OK;
}

/* recv 的回傳值有三種，意思完全不同，要分開處理：
 *   > 0   這次拿到的 bytes 數（可能比要求的少）
 *   = 0   對方把連線正常關掉了，而且他送的資料我們都讀完了；再 recv 也不會有東西
 *   < 0   出錯（例如連線被重設）
 * 在收滿 n bytes 之前遇到後兩種，一律回報失敗：已經拿到的那一部分不完整，上層不應該使用。 */
int recv_all(socket_t s, void *buf, size_t n) {
    char *p = (char *)buf;
    while (n > 0) {
        int chunk = (n > (1u << 20)) ? (1 << 20) : (int)n;
        int k = (int)recv(s, p, chunk, 0);          /* 阻塞：一個 byte 都還沒到的時候，會停在這裡等 */
        if (k == 0) return TL_ERR_CLOSED;      /* 對方正常關閉 */
        if (k < 0)  return TL_ERR_NET;
        p += k;
        n -= (size_t)k;
        g_rx_bytes += (uint64_t)k;
    }
    return TL_OK;
}

/* 單調時鐘：不受系統時間調整影響，量測經過時間要用這個，不要用 time() 或 clock() */
/* 回傳「從某個固定起點到現在」經過的毫秒數。起點是哪裡不重要，也不保證是什麼；用法永遠是相減：
 *     double t0 = now_ms();  ...做事...  double elapsed = now_ms() - t0;
 * 為什麼不用 time()：它是「牆上的時鐘」，只有秒的解析度，而且電腦自動對時的時候會跳，量到的時間可能是負的。
 * 為什麼不用 clock()：它量的是「這支程式用掉的 CPU 時間」；程式停在 recv 等網路時 CPU 沒在做事，
 * 這段等待就不會被算進去，而我們要量的正是包含等待的實際經過時間。
 * Windows：計數器目前的讀數 ÷ 計數器每秒跳幾下 = 秒；× 1000 = 毫秒。
 * POSIX ：clock_gettime 給的是「秒」與「奈秒」兩個欄位；1 毫秒 = 1e6 奈秒。
 * 呼叫者：src/transfer.c（encode_ms、send_ms、decode_ms、total_ms）、src/chat.c。 */
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
