/*============================================================================
 *  chat.c  —  跨平台純 C 教學版「迷你 LINE」文字聊天程式
 *----------------------------------------------------------------------------
 *  課程：多媒體訊號處理（Department of Communication Engineering）
 *  目標：讓同學理解「應用程式 → Socket API → TCP/UDP → IP 網路」的完整流程，
 *        並體驗一個長得像 LINE 的聊天介面（綠色泡泡在右邊是自己，
 *        白色泡泡在左邊是對方）。
 *
 *  特色：
 *    1. 純 C（C99），零外部函式庫，Windows / macOS / Linux 皆可編譯。
 *    2. 可用命令列參數選擇 TCP 或 UDP 協定，方便比較兩者差異。
 *    3. 使用 ANSI 逃逸序列 (escape sequence) 在終端機畫出彩色聊天泡泡，
 *       模仿 LINE 的視覺風格（這也是最早期 BBS / 終端機軟體的做法）。
 *    4. 使用「執行緒 (thread)」同時處理【收訊息】與【打字送出】，
 *       這是所有即時通訊軟體的核心架構。
 *
 *  編譯方式：
 *    Linux / macOS :  gcc chat.c -o chat -lpthread
 *    Windows(MinGW):  gcc chat.c -o chat.exe -lws2_32
 *    Windows(MSVC) :  cl /utf-8 chat.c ws2_32.lib   (/utf-8 讓中文註解不亂碼)
 *
 *  使用方式（兩台電腦，或同一台開兩個終端機）：
 *    TCP 模式：一方當 server（先開），一方當 client（後連）
 *      電腦A:  ./chat tcp server 5000
 *      電腦B:  ./chat tcp client 192.168.1.10 5000    (A 的 IP)
 *
 *    UDP 模式：沒有連線概念，雙方各自綁定一個 port，互相指定對方位址
 *      電腦A:  ./chat udp 5000 192.168.1.20 6000      (自己聽5000, 傳給B的6000)
 *      電腦B:  ./chat udp 6000 192.168.1.10 5000      (自己聽6000, 傳給A的5000)
 *
 *    同一台電腦測試時，IP 一律填 127.0.0.1（本機回送位址 loopback）。
 *
 *----------------------------------------------------------------------------
 *  整體程式流程圖 (ASCII flowchart)
 *
 *                 ┌─────────────────────┐
 *                 │  main() 解析命令列   │
 *                 └──────────┬──────────┘
 *                            │
 *              ┌─────────────┴──────────────┐
 *              │ 選 TCP？          選 UDP？  │
 *              ▼                            ▼
 *   ┌──────────────────────┐   ┌───────────────────────────┐
 *   │ server:              │   │ socket(SOCK_DGRAM)        │
 *   │  socket→bind→listen  │   │ bind(自己的port)           │
 *   │  →accept 等人連線     │   │ 記下對方 IP:port           │
 *   │ client:              │   │ （UDP 不需要 connect，     │
 *   │  socket→connect      │   │   每個封包都獨立寄送）      │
 *   └──────────┬───────────┘   └────────────┬──────────────┘
 *              └─────────────┬──────────────┘
 *                            ▼
 *              ┌──────────────────────────┐
 *              │ 建立「接收執行緒」          │
 *              │ (recv_thread)            │
 *              └────────────┬─────────────┘
 *                           │
 *          ┌────────────────┴────────────────┐
 *          │  兩條執行緒【同時】進行：          │
 *          ▼                                 ▼
 *  ┌────────────────┐              ┌──────────────────┐
 *  │ 主執行緒 (UI)   │              │ 接收執行緒         │
 *  │ ┌────────────┐ │              │ ┌──────────────┐ │
 *  │ │等使用者打字  │ │              │ │recv()/       │ │
 *  │ │  fgets()   │ │              │ │recvfrom() 等封包│ │
 *  │ └─────┬──────┘ │              │ └──────┬───────┘ │
 *  │       ▼        │              │        ▼         │
 *  │ send()/sendto()│              │  存進聊天紀錄      │
 *  │       ▼        │              │        ▼         │
 *  │ 存紀錄+重畫畫面  │              │   重畫畫面        │
 *  │       │        │              │        │         │
 *  │       └──迴圈───│              │        └──迴圈── │
 *  └────────────────┘              └──────────────────┘
 *                           │
 *                  輸入 /quit 離開
 *                           ▼
 *              ┌──────────────────────┐
 *              │ closesocket / close  │
 *              │ (Windows: WSACleanup)│
 *              └──────────────────────┘
 *
 *----------------------------------------------------------------------------
 *  TCP 與 UDP 的差別（教學重點）
 *
 *   TCP (Transmission Control Protocol)      UDP (User Datagram Protocol)
 *   ─────────────────────────────────       ────────────────────────────
 *   ‧連線導向：要先「三方交握」建立連線        ‧無連線：直接把封包丟出去
 *   ‧可靠：掉封包會自動重傳、保證順序          ‧不可靠：掉了就掉了、可能亂序
 *   ‧位元組串流：訊息邊界要自己切              ‧保留訊息邊界：一次一包
 *   ‧適合：聊天文字、檔案傳輸                 ‧適合：即時語音/視訊、遊戲
 *   （LINE 的文字訊息走 TCP/TLS；             （LINE 的語音通話底層
 *     通話則用類似 UDP 的機制）                 用的就是 UDP/RTP 家族）
 *===========================================================================*/

/*========================= 1. 跨平台前置處理 ================================
 * C 語言本身沒有內建網路功能，網路 API 由作業系統提供：
 *   - Windows 用 Winsock  (winsock2.h, 需連結 ws2_32.lib)
 *   - macOS/Linux 用 BSD socket (sys/socket.h 等)
 * 兩者「長得 87% 像」，剩下的差異我們用巨集 (macro) 抹平，
 * 這就是「條件式編譯」— 跨平台程式最常見的技巧。
 *===========================================================================*/
#ifdef _WIN32
  /* ---- Windows 專用 ---- */
  #define _WIN32_WINNT 0x0600          /* 需要 Vista 以上的 API (inet_pton) */
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #include <process.h>                 /* _beginthreadex（要在使用前 include）*/
  #pragma comment(lib, "ws2_32.lib")   /* MSVC 自動連結 winsock 函式庫 */

  typedef SOCKET socket_t;             /* Windows 的 socket 型別是 SOCKET */
  #define CLOSESOCK(s)  closesocket(s)
  #define SOCK_INVALID  INVALID_SOCKET

  /* Windows 的執行緒 API 與 pthread 不同，這裡做一層薄薄的包裝 */
  typedef HANDLE thread_t;
  static int thread_create(thread_t *t, unsigned (__stdcall *fn)(void*), void *arg) {
      *t = (HANDLE)_beginthreadex(NULL, 0, fn, arg, 0, NULL);
      return (*t == NULL) ? -1 : 0;
  }
  #define THREAD_FN(name) unsigned __stdcall name(void *arg)
  #define THREAD_RETURN   return 0
#else
  /* ---- macOS / Linux (POSIX) 專用 ---- */
  #include <sys/socket.h>              /* socket(), bind(), ...            */
  #include <netinet/in.h>              /* struct sockaddr_in               */
  #include <arpa/inet.h>               /* inet_pton(), htons()             */
  #include <unistd.h>                  /* close()                          */
  #include <pthread.h>                 /* POSIX 執行緒                      */

  typedef int socket_t;                /* POSIX 的 socket 就是檔案描述子(int)*/
  #define CLOSESOCK(s)  close(s)
  #define SOCK_INVALID  (-1)

  typedef pthread_t thread_t;
  static int thread_create(thread_t *t, void *(*fn)(void*), void *arg) {
      return pthread_create(t, NULL, fn, arg);
  }
  #define THREAD_FN(name) void *name(void *arg)
  #define THREAD_RETURN   return NULL
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*========================= 2. 常數與全域狀態 ================================*/
#define MAX_MSG      100      /* 聊天視窗最多保留幾則訊息               */
#define MSG_LEN      512      /* 每則訊息最大位元組數                   */
#define BUBBLE_WIDTH 36       /* 聊天泡泡的最大寬度（字元數）            */
#define SCREEN_WIDTH 78       /* 畫面總寬度                            */

/* 一則訊息 = 內容 + 是誰說的 */
typedef struct {
    char text[MSG_LEN];
    int  is_me;               /* 1 = 自己(綠色靠右), 0 = 對方(白色靠左) */
} Message;

/* 聊天紀錄（兩條執行緒都會存取 → 需要「互斥鎖」保護，見下方說明） */
static Message g_history[MAX_MSG];
static int     g_msg_count = 0;

static socket_t g_sock = SOCK_INVALID;   /* 通訊用的 socket             */
static int      g_use_tcp = 1;           /* 1=TCP, 0=UDP                */
static struct sockaddr_in g_peer;        /* UDP 模式下對方的位址         */
static char     g_peer_name[64] = "Friend";  /* 對方顯示名稱             */
static volatile int g_running = 1;       /* 程式是否繼續執行的旗標        */

/*--------------------------------------------------------------------------
 * 【互斥鎖 mutex】
 * 兩條執行緒（打字的、收訊的）都會「寫入聊天紀錄」和「重畫畫面」，
 * 若同時進行會把資料或畫面弄亂（race condition，競態條件）。
 * mutex 就像廁所門鎖：一次只准一條執行緒進去。
 *-------------------------------------------------------------------------*/
#ifdef _WIN32
  static CRITICAL_SECTION g_lock;
  static void lock_init(void)   { InitializeCriticalSection(&g_lock); }
  static void lock_take(void)   { EnterCriticalSection(&g_lock); }
  static void lock_give(void)   { LeaveCriticalSection(&g_lock); }
#else
  static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
  static void lock_init(void)   { /* POSIX 用靜態初始化即可 */ }
  static void lock_take(void)   { pthread_mutex_lock(&g_lock); }
  static void lock_give(void)   { pthread_mutex_unlock(&g_lock); }
#endif

/*========================= 3. 終端機「GUI」繪圖 =============================
 * ANSI 逃逸序列：以 "\x1b[" 開頭的特殊字串，終端機看到後不會印出來，
 * 而是執行動作（換顏色、移動游標、清畫面…）。例如：
 *   "\x1b[2J"   清除整個畫面
 *   "\x1b[32m"  之後的文字變綠色
 *   "\x1b[0m"   顏色還原
 * macOS/Linux 的終端機原生支援；Windows 10 之後也支援，
 * 但要先呼叫 SetConsoleMode 打開 VT 模式（見 console_init）。
 *===========================================================================*/
#define CLR_RESET   "\x1b[0m"
#define CLR_GREEN   "\x1b[1;97;42m"   /* 亮白字、綠底 → 模仿 LINE 自己的泡泡 */
#define CLR_WHITE   "\x1b[30;47m"     /* 黑字、白底  → 對方的泡泡            */
#define CLR_TITLE   "\x1b[1;97;44m"   /* 亮白字、藍底 → 標題列               */
#define CLR_DIM     "\x1b[90m"        /* 灰色 → 提示文字                     */
#define CLEAR_SCREEN "\x1b[2J\x1b[H"  /* 清畫面 + 游標回左上角               */

/* 讓 Windows 的 cmd / PowerShell 也認得 ANSI 色碼 */
static void console_init(void) {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(h, &mode);
    SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    SetConsoleOutputCP(65001);   /* 輸出改用 UTF-8，中文才不會變亂碼 */
    SetConsoleCP(65001);
#endif
}

/*--------------------------------------------------------------------------
 * 畫出一則「聊天泡泡」。
 * 泡泡由三行組成（上框、內容、下框），內容太長會自動折行：
 *
 *   自己 (靠右, 綠色)                對方 (靠左, 白色)
 *                ┌──────────┐      ┌──────────┐
 *                │ 哈囉!     │      │ 嗨你好    │
 *                └──────────┘      └──────────┘
 *
 * 註：這裡用位元組數估寬度，中文(UTF-8 佔 3 bytes、顯示佔 2 格)會有些
 *     誤差，教學版先不處理全形寬度，同學可以當作進階作業改良它！
 *-------------------------------------------------------------------------*/
static void draw_bubble(const char *text, int is_me) {
    const char *color = is_me ? CLR_GREEN : CLR_WHITE;
    int len = (int)strlen(text);

    /* 逐段切割：每 BUBBLE_WIDTH 個位元組折一行（避免切在 UTF-8 中間，
       往回找到字元開頭 — UTF-8 的接續位元組都是 10xxxxxx 開頭）      */
    int start = 0;
    while (start < len) {
        int take = len - start;
        if (take > BUBBLE_WIDTH) {
            take = BUBBLE_WIDTH;
            while (take > 0 && ((unsigned char)text[start + take] & 0xC0) == 0x80)
                take--;               /* 退到 UTF-8 字元邊界 */
            if (take == 0) take = BUBBLE_WIDTH;  /* 保險 */
        }

        char line[MSG_LEN];
        memcpy(line, text + start, (size_t)take);
        line[take] = '\0';

        if (is_me) {
            /* 靠右：先印空白把泡泡推到右邊 */
            int pad = SCREEN_WIDTH - take - 4;
            if (pad < 0) pad = 0;
            printf("%*s%s  %s  %s\n", pad, "", color, line, CLR_RESET);
        } else {
            printf("  %s  %s  %s\n", color, line, CLR_RESET);
        }
        start += take;
    }
}

/*--------------------------------------------------------------------------
 * 重畫整個畫面：標題列 → 所有訊息泡泡 → 輸入提示。
 * 簡單起見，每次有新訊息就整個畫面重畫一次（就像影像的 frame refresh）。
 *-------------------------------------------------------------------------*/
static void redraw_screen(void) {
    printf(CLEAR_SCREEN);

    /* 標題列（模仿 LINE 上方的好友名稱列） */
    printf(CLR_TITLE "  %-*s" CLR_RESET "\n",
           SCREEN_WIDTH - 2, g_peer_name);
    printf(CLR_DIM "  protocol: %s   (/quit 離開)" CLR_RESET "\n\n",
           g_use_tcp ? "TCP" : "UDP");

    /* 聊天內容 */
    for (int i = 0; i < g_msg_count; i++)
        draw_bubble(g_history[i].text, g_history[i].is_me);

    /* 輸入提示列 */
    printf("\n" CLR_DIM "──────────────────────────────────────────" CLR_RESET "\n");
    printf("訊息> ");
    fflush(stdout);   /* 沒有換行的輸出要手動 flush 才會顯示 */
}

/* 把一則訊息加入紀錄並重畫（有上鎖，兩條執行緒都可安全呼叫） */
static void add_message(const char *text, int is_me) {
    lock_take();
    if (g_msg_count == MAX_MSG) {           /* 滿了就整體往前搬，丟掉最舊的 */
        memmove(&g_history[0], &g_history[1], sizeof(Message) * (MAX_MSG - 1));
        g_msg_count--;
    }
    snprintf(g_history[g_msg_count].text, MSG_LEN, "%s", text);
    g_history[g_msg_count].is_me = is_me;
    g_msg_count++;
    redraw_screen();
    lock_give();
}

/*========================= 4. 接收執行緒 ====================================
 * 這條執行緒的一生：不斷呼叫 recv()/recvfrom() 「阻塞等待」封包到來。
 * 「阻塞 (blocking)」= 沒資料時函式不會返回，執行緒睡著等待，不耗 CPU。
 * 這正是需要第二條執行緒的原因：若在主執行緒等封包，就沒辦法同時打字了。
 *
 *      ┌────────────┐
 *      │ 等待封包    │◄─────────┐
 *      │ recv/      │          │
 *      │ recvfrom   │          │
 *      └─────┬──────┘          │
 *            ▼                 │
 *      收到 n bytes?           │
 *        n>0 ──► 加入紀錄+重畫──┘
 *        n<=0 ─► 對方離線/錯誤 → 結束執行緒
 *===========================================================================*/
THREAD_FN(recv_thread) {
    (void)arg;
    char buf[MSG_LEN];

    while (g_running) {
        int n;
        if (g_use_tcp) {
            /* TCP：資料從「已連線」的串流讀出 */
            n = (int)recv(g_sock, buf, MSG_LEN - 1, 0);
        } else {
            /* UDP：每次收「一整個封包」，同時得知寄件人位址。
               我們順便把寄件人記成對方位址（這樣 client 先開也沒關係） */
            struct sockaddr_in from;
            socklen_t fromlen = sizeof(from);
            n = (int)recvfrom(g_sock, buf, MSG_LEN - 1, 0,
                              (struct sockaddr *)&from, &fromlen);
            if (n > 0) g_peer = from;
        }

        if (n <= 0) {   /* 0=TCP對方正常關閉, <0=發生錯誤 */
            if (g_running)
                add_message("[系統] 連線已中斷", 0);
            g_running = 0;
            break;
        }
        buf[n] = '\0';                /* 補字串結尾（網路傳來的沒有 \0） */
        add_message(buf, 0);          /* 0 = 對方說的 → 白色靠左 */
    }
    THREAD_RETURN;
}

/*========================= 5. 建立連線（TCP / UDP） =========================*/

/* 印出致命錯誤並離開 */
static void die(const char *msg) {
#ifdef _WIN32
    fprintf(stderr, "錯誤: %s (WSA error %d)\n", msg, WSAGetLastError());
#else
    perror(msg);
#endif
    exit(1);
}

/*--------------------------------------------------------------------------
 * TCP Server：socket → bind → listen → accept
 *
 *   socket()  跟 OS 要一個通訊端點（想像成申請一支電話）
 *   bind()    綁定自己的 port（決定電話號碼）
 *   listen()  開始待機（電話開機等人打來）
 *   accept()  接起一通來電 → 回傳一個「專屬這通電話」的新 socket
 *-------------------------------------------------------------------------*/
static socket_t tcp_server(int port) {
    socket_t ls = socket(AF_INET, SOCK_STREAM, 0);  /* SOCK_STREAM = TCP */
    if (ls == SOCK_INVALID) die("socket");

    /* SO_REUSEADDR：程式重開時允許立刻重綁同一個 port（教學實驗必備） */
    int yes = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;                 /* IPv4               */
    addr.sin_port        = htons((unsigned short)port); /* 主機→網路位元序 */
    addr.sin_addr.s_addr = htonl(INADDR_ANY);       /* 聽本機所有網卡      */

    if (bind(ls, (struct sockaddr *)&addr, sizeof(addr)) < 0) die("bind");
    if (listen(ls, 1) < 0) die("listen");

    printf("等待對方連線中 (port %d)...\n", port);
    socket_t cs = accept(ls, NULL, NULL);   /* 阻塞直到有人 connect 進來 */
    if (cs == SOCK_INVALID) die("accept");

    CLOSESOCK(ls);       /* 一對一聊天，不再收新連線，關掉監聽 socket */
    printf("對方已連線！\n");
    return cs;
}

/*--------------------------------------------------------------------------
 * TCP Client：socket → connect
 * connect() 會觸發著名的「TCP 三方交握」：
 *     Client ── SYN ──────► Server
 *     Client ◄─ SYN+ACK ── Server
 *     Client ── ACK ──────► Server      (連線建立！)
 *-------------------------------------------------------------------------*/
static socket_t tcp_client(const char *ip, int port) {
    socket_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == SOCK_INVALID) die("socket");

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((unsigned short)port);
    /* inet_pton：把 "192.168.1.10" 這種文字轉成 32-bit 二進位 IP */
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) die("IP 格式錯誤");

    printf("連線到 %s:%d ...\n", ip, port);
    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) die("connect");
    printf("連線成功！\n");
    return s;
}

/*--------------------------------------------------------------------------
 * UDP：只需 socket → bind 自己的 port，並記下對方位址供 sendto() 使用。
 * 沒有 listen/accept/connect —— 這就是「無連線」協定。
 *-------------------------------------------------------------------------*/
static socket_t udp_setup(int my_port, const char *peer_ip, int peer_port) {
    socket_t s = socket(AF_INET, SOCK_DGRAM, 0);    /* SOCK_DGRAM = UDP */
    if (s == SOCK_INVALID) die("socket");

    struct sockaddr_in me;
    memset(&me, 0, sizeof(me));
    me.sin_family      = AF_INET;
    me.sin_port        = htons((unsigned short)my_port);
    me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *)&me, sizeof(me)) < 0) die("bind");

    memset(&g_peer, 0, sizeof(g_peer));
    g_peer.sin_family = AF_INET;
    g_peer.sin_port   = htons((unsigned short)peer_port);
    if (inet_pton(AF_INET, peer_ip, &g_peer.sin_addr) != 1) die("IP 格式錯誤");

    printf("UDP 就緒：本機 port %d ↔ 對方 %s:%d\n", my_port, peer_ip, peer_port);
    return s;
}

/*========================= 6. 主程式 =======================================*/
static void usage(const char *prog) {
    printf("用法：\n");
    printf("  TCP server:  %s tcp server <port>\n", prog);
    printf("  TCP client:  %s tcp client <對方IP> <port>\n", prog);
    printf("  UDP:         %s udp <自己port> <對方IP> <對方port>\n", prog);
    printf("範例（本機測試用 127.0.0.1）：\n");
    printf("  %s tcp server 5000\n", prog);
    printf("  %s tcp client 127.0.0.1 5000\n", prog);
    exit(1);
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    /* Windows 使用網路前必須先「開機」Winsock 函式庫（POSIX 不用） */
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) die("WSAStartup");
#endif
    console_init();
    lock_init();

    /* ---- 解析命令列參數，建立連線 ---- */
    if (argc >= 2 && strcmp(argv[1], "tcp") == 0) {
        g_use_tcp = 1;
        if (argc == 4 && strcmp(argv[2], "server") == 0) {
            g_sock = tcp_server(atoi(argv[3]));
            snprintf(g_peer_name, sizeof(g_peer_name), "Friend (TCP client)");
        } else if (argc == 5 && strcmp(argv[2], "client") == 0) {
            g_sock = tcp_client(argv[3], atoi(argv[4]));
            snprintf(g_peer_name, sizeof(g_peer_name), "Friend (%s)", argv[3]);
        } else usage(argv[0]);
    } else if (argc == 5 && strcmp(argv[1], "udp") == 0) {
        g_use_tcp = 0;
        g_sock = udp_setup(atoi(argv[2]), argv[3], atoi(argv[4]));
        snprintf(g_peer_name, sizeof(g_peer_name), "Friend (%s)", argv[3]);
    } else {
        usage(argv[0]);
    }

    /* ---- 啟動接收執行緒 ---- */
    thread_t th;
    if (thread_create(&th, recv_thread, NULL) != 0) die("thread_create");

    /* ---- 主迴圈：等使用者打字 → 送出 ---- */
    redraw_screen();
    char input[MSG_LEN];
    while (g_running && fgets(input, sizeof(input), stdin) != NULL) {
        input[strcspn(input, "\r\n")] = '\0';   /* 去掉結尾換行字元 */

        if (input[0] == '\0') { redraw_screen(); continue; }  /* 空行忽略 */
        if (strcmp(input, "/quit") == 0) break;

        /* 送出訊息：TCP 用 send()（走已建立的連線），
                     UDP 用 sendto()（每包都要寫上收件人地址） */
        int n;
        if (g_use_tcp)
            n = (int)send(g_sock, input, (int)strlen(input), 0);
        else
            n = (int)sendto(g_sock, input, (int)strlen(input), 0,
                            (struct sockaddr *)&g_peer, sizeof(g_peer));

        if (n <= 0) { add_message("[系統] 傳送失敗", 0); break; }
        add_message(input, 1);   /* 1 = 自己說的 → 綠色靠右 */
    }

    /* ---- 收尾：關 socket，接收執行緒的 recv 會因此返回而結束 ---- */
    g_running = 0;
    CLOSESOCK(g_sock);
#ifdef _WIN32
    WSACleanup();
#endif
    printf(CLR_RESET "\n再見！\n");
    return 0;
}
