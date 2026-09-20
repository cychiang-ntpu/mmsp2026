/*============================================================================
 *  chat.c  —  聊天畫面：功能 1 文字聊天，以及在畫面中選檔傳送（功能 2、3）（殼：已完成）
 *----------------------------------------------------------------------------
 *  從 baseline/chat.c 延伸而來，畫面與執行緒架構相同，差別在於：
 *
 *    baseline                          這裡
 *    ───────────────────────────      ─────────────────────────────────────
 *    send(input) 直接丟 bytes          frame_send(type, payload)：有長度前綴
 *    recv(buf, 511) 拿到多少算多少      frame_recv()：一次剛好一則訊息
 *    收到什麼就顯示什麼                 先 utf8_validate、再濾掉控制字元才顯示
 *    沒有壓縮                          --huff：送出前 huff_encode、收到後 huff_decode
 *                                      （文字以 UTF-8 字元為符號、WAV 以 16-bit sample 為符號）
 *    只有 TCP／UDP 兩種畫面資訊         每顆泡泡下面顯示「原始 bytes → 實際上線 bytes」
 *    只能傳文字                        /files 列出 .txt／.wav、/send 選一個傳給對方；
 *                                      對方的聊天畫面自動接收、解碼、存到 received/
 *
 *   ┌────────────────────┐                       ┌─────────────────────────┐
 *   │ 主執行緒            │                       │ 接收執行緒               │
 *   │  read_line()       │                       │  frame_recv()           │
 *   │   ↓                │                       │   ↓                     │
 *   │ (huff_encode)      │ ── TEXT_RAW/HUFF ──►  │ (huff_decode)           │
 *   │   ↓                │                       │   ↓                     │
 *   │  frame_send()      │                       │  utf8_validate()        │
 *   │   ↓                │                       │   ↓                     │
 *   │  add_message(我)   │                       │  add_message(對方)       │
 *   └────────────────────┘                       └─────────────────────────┘
 *
 *  這個檔案裡沒有 TODO。frame、UTF-8、Huffman 還沒實作時，畫面上方會用黃字列出來，
 *  相關的動作會變成一則 [系統] 訊息，不會當掉。
 *===========================================================================*/
#include "textlink.h"
#include <inttypes.h>
#include <stdarg.h>
#ifndef _WIN32
  #include <dirent.h>
#endif

/*========================= 常數與全域狀態 ==================================*/
#define MAX_MSG      100      /* 最多保留幾則訊息               */
#define SHOW_MSG     16       /* 畫面上顯示最近幾則（避免標題列被捲走） */
#define RECV_DIR     "received"   /* 聊天中收到的檔案存在這個資料夾 */
#define MAX_PICK     12       /* /files 最多列出幾個檔案 */
#define BUBBLE_WIDTH 36       /* 泡泡每行最多幾個 bytes          */
#define SCREEN_WIDTH 78       /* 畫面總寬度                     */

typedef enum { WHO_PEER = 0, WHO_ME = 1, WHO_SYS = 2 } who_t;

typedef struct {
    char      text[TL_MAX_TEXT];
    who_t     who;
    size_t    raw_bytes;      /* 訊息本身的 UTF-8 bytes 數                 */
    size_t    wire_bytes;     /* 實際上線的 bytes 數（含 frame 標頭與 codebook） */
    tl_mode_t mode;
} Message;

static Message  g_history[MAX_MSG];
static int      g_msg_count = 0;

static socket_t g_sock = SOCK_INVALID;
static char     g_peer[64] = "?";
static volatile int g_running = 1;
static volatile int g_mode = MODE_HUFF;

static int g_todo_frame = 0, g_todo_utf8 = 0, g_todo_huff = 0;   /* 啟動時自我檢查的結果 */

static char g_status[200] = "";            /* 輸入列上方的狀態列：檔案傳輸進度 */
static char g_pick[MAX_PICK][300];         /* /files 列出的檔案路徑，/send <編號> 用 */
static int  g_pick_n = 0;
static char g_tx_name[128] = "";           /* 正在傳送的檔名 */
static volatile int g_wait_ack = 0;        /* 1 = 檔案已送完，等對方回覆存檔結果 */
static volatile int g_tx_posted = 0;       /* 1 = 「已送出」那則系統訊息已經顯示 */
static double g_ack_t0 = 0.0;

/*--- 互斥鎖：兩條執行緒都會寫聊天紀錄、重畫畫面，一次只准一條進去 ---*/
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

/*========================= 終端機「GUI」繪圖 ===============================*/
#define CLR_RESET    "\x1b[0m"
#define CLR_GREEN    "\x1b[1;97;42m"   /* 亮白字、綠底 → 自己的泡泡 */
#define CLR_WHITE    "\x1b[30;47m"     /* 黑字、白底  → 對方的泡泡  */
#define CLR_TITLE    "\x1b[1;97;44m"   /* 亮白字、藍底 → 標題列     */
#define CLR_DIM      "\x1b[90m"        /* 灰色 → 提示文字           */
#define CLR_WARN     "\x1b[33m"        /* 黃色 → 尚未實作的提醒      */
#define CLEAR_SCREEN "\x1b[2J\x1b[H"

/* 畫一顆泡泡。折行時往回退到 UTF-8 字元邊界（續位元組都是 10xxxxxx）。
 * 註：和 baseline 一樣用 bytes 數估寬度，中文（3 bytes、顯示 2 格）會有誤差。 */
static void draw_bubble(const char *text, int is_me) {
    const char *color = is_me ? CLR_GREEN : CLR_WHITE;
    int len = (int)strlen(text);
    int start = 0;
    while (start < len) {
        int take = len - start;
        if (take > BUBBLE_WIDTH) {
            take = BUBBLE_WIDTH;
            while (take > 0 && ((unsigned char)text[start + take] & 0xC0) == 0x80)
                take--;
            if (take == 0) take = BUBBLE_WIDTH;
        }
        char line[BUBBLE_WIDTH + 1];
        memcpy(line, text + start, (size_t)take);
        line[take] = '\0';

        if (is_me) {
            int pad = SCREEN_WIDTH - take - 4;
            if (pad < 0) pad = 0;
            printf("%*s%s  %s  %s\n", pad, "", color, line, CLR_RESET);
        } else {
            printf("  %s  %s  %s\n", color, line, CLR_RESET);
        }
        start += take;
    }
}

/* 泡泡下方的小字：這則訊息原本幾 bytes、實際送上網路幾 bytes（功能 1 的報告要用） */
static void draw_caption(const Message *m) {
    char cap[96];
    double pct = m->raw_bytes ? 100.0 * (double)m->wire_bytes / (double)m->raw_bytes : 0.0;
    snprintf(cap, sizeof(cap), "%s  %u B -> %u B on wire (%.0f%%)",
             m->mode == MODE_HUFF ? "HUFF" : "RAW",
             (unsigned)m->raw_bytes, (unsigned)m->wire_bytes, pct);
    if (m->who == WHO_ME) {
        int pad = SCREEN_WIDTH - (int)strlen(cap);
        if (pad < 0) pad = 0;
        printf("%*s" CLR_DIM "%s" CLR_RESET "\n", pad, "", cap);
    } else {
        printf("  " CLR_DIM "%s" CLR_RESET "\n", cap);
    }
}

static void redraw_screen(void) {
    char title[160];
    printf(CLEAR_SCREEN);

    snprintf(title, sizeof(title), "TextLink  |  對方 %s", g_peer);
    printf(CLR_TITLE "  %-*s" CLR_RESET "\n", SCREEN_WIDTH - 2, title);
    printf(CLR_DIM "  模式: %s   指令: /files  /send  /raw  /huff  /help  /quit" CLR_RESET "\n",
           g_mode == MODE_HUFF ? "HUFF（Huffman 壓縮）" : "RAW（不壓縮）");
    if (g_todo_frame || g_todo_utf8 || g_todo_huff)
        printf(CLR_WARN "  尚未實作:%s%s%s   → 見 starter/README.md 的 TODO 清單" CLR_RESET "\n",
               g_todo_frame ? " [frame 標頭]" : "",
               g_todo_utf8  ? " [UTF-8 檢查]" : "",
               g_todo_huff  ? " [Huffman]" : "");
    printf("\n");

    int first = g_msg_count > SHOW_MSG ? g_msg_count - SHOW_MSG : 0;
    for (int i = first; i < g_msg_count; i++) {
        const Message *m = &g_history[i];
        if (m->who == WHO_SYS) {
            printf(CLR_DIM "  %s" CLR_RESET "\n", m->text);
        } else {
            draw_bubble(m->text, m->who == WHO_ME);
            draw_caption(m);
        }
    }

    printf("\n");
    if (g_status[0]) printf(CLR_WARN "  %s" CLR_RESET "\n", g_status);
    printf(CLR_DIM "──────────────────────────────────────────" CLR_RESET "\n");
    printf("訊息> ");
    fflush(stdout);
}

/* 把一則訊息加入紀錄並重畫（有上鎖，兩條執行緒都可安全呼叫）。
 * 控制字元（含 ESC）一律換成空白：對方送來的文字不可以有機會操控我們的終端機。 */
static void add_message(const char *text, who_t who, size_t raw, size_t wire, tl_mode_t mode) {
    lock_take();
    if (g_msg_count == MAX_MSG) {
        memmove(&g_history[0], &g_history[1], sizeof(Message) * (MAX_MSG - 1));
        g_msg_count--;
    }
    Message *m = &g_history[g_msg_count++];
    snprintf(m->text, sizeof(m->text), "%s", text);
    for (char *p = m->text; *p; p++)
        if ((unsigned char)*p < 0x20 || *p == 0x7F) *p = ' ';
    m->who = who;
    m->raw_bytes = raw;
    m->wire_bytes = wire;
    m->mode = mode;
    redraw_screen();
    lock_give();
}

static void add_system(const char *fmt, const char *detail) {
    char buf[256];
    snprintf(buf, sizeof(buf), fmt, detail);
    add_message(buf, WHO_SYS, 0, 0, MODE_RAW);
}

static void add_sysf(const char *fmt, ...) {
    char buf[400];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    add_message(buf, WHO_SYS, 0, 0, MODE_RAW);
}

static void set_status(const char *text) {
    lock_take();
    snprintf(g_status, sizeof(g_status), "%s", text);
    redraw_screen();
    lock_give();
}

/* 檔案傳輸進度：每前進 10% 更新一次狀態列（每個 chunk 都整個重畫會閃）*/
static void chat_progress(const char *label, uint64_t done, uint64_t total) {
    static int last_tx = -1, last_rx = -1;
    int *last = (strcmp(label, "傳送") == 0) ? &last_tx : &last_rx;
    int step = total ? (int)(done * 10 / total) : 10;
    if (step == *last && done != total) return;
    *last = (done == total) ? -1 : step;

    char buf[200];
    snprintf(buf, sizeof(buf), "%s中 %3d%%  %.2f / %.2f MB", label, step * 10,
             (double)done / 1048576.0, (double)total / 1048576.0);
    set_status(buf);
}

/*========================= 接收執行緒 ======================================*/
/* 對方在聊天中送檔案過來：把 FILE_* frame 餵給 transfer.c 的接收狀態機 */
static file_rx_t g_rx;
static int g_rx_discard = 0;                /* 1 = 這個檔案已經出錯，剩下的 FILE_DATA 直接丟掉 */

static void handle_file_frame(uint8_t type, const uint8_t *p, size_t len) {
    if (type == T_FILE_BEGIN) {
        int rc = file_rx_begin(&g_rx, p, len);
        if (rc == TL_OK) {
            add_sysf("[檔案] 對方開始傳 %s（原始 %" PRIu64 " B，%s）", g_rx.name, g_rx.orig_size,
                     g_rx.mode == MODE_HUFF ? "HUFF" : "RAW");
        } else {
            g_rx_discard = 1;
            add_sysf("[檔案] 對方要傳檔案，但 FILE_BEGIN 不被接受（%s），這個檔案會被丟棄", tl_strerror(rc));
        }
    } else if (type == T_FILE_DATA) {
        if (g_rx_discard) return;
        int rc = file_rx_data(&g_rx, p, len);
        if (rc != TL_OK) {
            g_rx_discard = 1;
            file_rx_reset(&g_rx);
            add_sysf("[檔案] 接收失敗（%s），這個檔案會被丟棄", tl_strerror(rc));
        } else {
            chat_progress("接收", g_rx.got, g_rx.data_size);
        }
    } else if (len == 1 && !g_rx.begun && !g_rx_discard) {
        /* FILE_END 帶 1 byte：這是對方對「我們送的檔案」的回覆 */
        double ms = now_ms() - g_ack_t0;
        if (!g_wait_ack) return;
        for (int i = 0; i < 100 && !g_tx_posted; i++) {     /* 小檔案時回覆可能比「已送出」還早到：等它先顯示 */
#ifdef _WIN32
            Sleep(5);
#else
            struct timespec ts = {0, 5 * 1000 * 1000};
            nanosleep(&ts, NULL);
#endif
        }
        g_wait_ack = 0;
        set_status("");
        if (p[0] == 0) add_sysf("[檔案] 對方已成功還原並存檔 %s（從開始傳到收到回覆 %.1f ms）", g_tx_name, ms);
        else           add_sysf("[檔案] 對方回報 %s 還原失敗（解碼或寫檔沒成功）", g_tx_name);
    } else {
        /* FILE_END（空）：對方送完了 → 檢查、解碼、存檔、回覆結果 */
        tl_stats_t st;
        char saved[512] = "";
        int rc = g_rx_discard ? TL_ERR_PROTO : file_rx_finish(&g_rx, RECV_DIR, &st, saved, sizeof(saved));
        uint8_t status = (rc == TL_OK) ? 0 : 1;
        frame_send(g_sock, T_FILE_END, &status, 1);
        set_status("");
        if (rc == TL_OK)
            add_sysf("[檔案] 已存檔 %s：原始 %" PRIu64 " B，上線 %" PRIu64 " B，壓縮率 %.2f%%，decode %.1f ms，共 %.1f ms",
                     saved, st.file_bytes, st.wire_bytes, 100.0 * tl_ratio(st.wire_bytes, st.file_bytes),
                     st.decode_ms, st.total_ms);
        else if (!g_rx_discard)
            add_sysf("[檔案] 收到 %s 但無法還原：%s", g_rx.name, tl_strerror(rc));
        file_rx_reset(&g_rx);
        g_rx_discard = 0;
    }
}

static void handle_text(const uint8_t *text, size_t len, size_t wire, tl_mode_t mode) {
    if (len >= TL_MAX_TEXT) {
        add_system("[系統] 收到過長的訊息，已丟棄%s", "");
        return;
    }
    if (memchr(text, '\0', len) != NULL || utf8_validate(text, len) == TL_ERR_DATA) {
        add_system("[系統] 收到不是合法 UTF-8 的訊息，已丟棄%s", "");
        return;
    }
    /* utf8_validate 還是 TODO 時會走到這裡：照樣顯示，但標題列會提醒尚未實作 */
    char buf[TL_MAX_TEXT];
    memcpy(buf, text, len);
    buf[len] = '\0';
    add_message(buf, WHO_PEER, len, wire, mode);
}

THREAD_FN(recv_thread) {
    (void)arg;
    while (g_running) {
        uint8_t type = 0, *payload = NULL;
        size_t len = 0;
        int rc = frame_recv(g_sock, &type, &payload, &len);
        if (rc != TL_OK) {
            if (g_running && (rc == TL_ERR_CLOSED || rc == TL_ERR_NET))
                add_system("[系統] 對方已離線，連線結束（按 Enter 離開）%s", "");
            else if (g_running)
                add_system("[系統] 連線結束：%s（按 Enter 離開）", tl_strerror(rc));
            g_running = 0;
            break;
        }

        if (type == T_TEXT_RAW) {
            handle_text(payload, len, len + TL_HDR_LEN, MODE_RAW);
        } else if (type == T_TEXT_HUFF) {
            uint8_t *dec = NULL;
            size_t dlen = 0;
            rc = huff_decode(payload, len, TL_MAX_TEXT - 1, &dec, &dlen);
            if (rc == TL_OK) {
                handle_text(dec, dlen, len + TL_HDR_LEN, MODE_HUFF);
                free(dec);
            } else {
                add_system("[系統] 收到 Huffman 訊息但解不開：%s", tl_strerror(rc));
            }
        } else if (type == T_FILE_BEGIN || type == T_FILE_DATA || type == T_FILE_END) {
            handle_file_frame(type, payload, len);
        } else {
            add_system("[系統] 收到不認得的封包 type，關閉連線（按 Enter 離開）%s", "");
            g_running = 0;
        }
        free(payload);
    }
    THREAD_RETURN;
}

/*========================= 主執行緒：讀鍵盤、送出 ==========================*/

/* 讀一行輸入，結果一律是 UTF-8。
 * Windows 主控台用 ReadConsoleW 讀 UTF-16 再轉 UTF-8：fgets 在主控台上讀中文、emoji 不可靠。
 * 輸入被導向（測試腳本用 < 或 | 餵資料）時，則照 baseline 用 fgets，假設來源已是 UTF-8。 */
static int read_line(char *buf, size_t cap) {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD cmode;
    if (GetConsoleMode(h, &cmode)) {
        static wchar_t w[TL_MAX_TEXT];
        DWORD got = 0;
        if (!ReadConsoleW(h, w, TL_MAX_TEXT - 1, &got, NULL) || got == 0) return 0;
        if (w[0] == 0x1A) return 0;                       /* Ctrl+Z = 結束輸入 */
        int n = WideCharToMultiByte(CP_UTF8, 0, w, (int)got, buf, (int)cap - 1, NULL, NULL);
        if (n < 0) n = 0;
        buf[n] = '\0';
    } else
#endif
    {
        if (fgets(buf, (int)cap, stdin) == NULL) return 0;
    }
    buf[strcspn(buf, "\r\n")] = '\0';
    return 1;
}

/* 送一則文字。回傳 TL_OK 時 *wire 是實際上線的 bytes 數。 */
static int send_text(const char *text, size_t n, tl_mode_t mode, size_t *wire) {
    if (mode == MODE_RAW) {
        *wire = n + TL_HDR_LEN;
        return frame_send(g_sock, T_TEXT_RAW, (const uint8_t *)text, n);
    }
    uint8_t *enc = NULL;
    size_t enc_len = 0;
    int rc = huff_encode((const uint8_t *)text, n, SYM_CHAR, &enc, &enc_len);   /* 文字：符號 = UTF-8 字元 */
    if (rc != TL_OK) return rc;
    /* 設計問題留給你們：enc_len 比 n 還大的時候（短訊息幾乎一定如此），要照送、
     * 改送 TEXT_RAW、還是改用兩端內建的固定 codebook？見 ../README.md 功能 1。 */
    *wire = enc_len + TL_HDR_LEN;
    rc = frame_send(g_sock, T_TEXT_HUFF, enc, enc_len);
    free(enc);
    return rc;
}

/*========================= 選檔案、送檔案 ==================================*/
static int has_ext(const char *name, const char *ext) {
    size_t n = strlen(name), e = strlen(ext);
    if (n <= e) return 0;
    for (size_t i = 0; i < e; i++) {
        char a = name[n - e + i], b = ext[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

static void pick_add(const char *dir, const char *name, double *sizes, double size) {
    if (g_pick_n >= MAX_PICK) return;
    if (!has_ext(name, ".txt") && !has_ext(name, ".wav")) return;
    for (const char *p = name; *p; p++)
        if ((unsigned char)*p >= 0x80) return;     /* 非 ASCII 檔名先略過（Windows 的開檔 API 要另外處理）*/
    snprintf(g_pick[g_pick_n], sizeof(g_pick[0]), "%s/%s", dir, name);
    sizes[g_pick_n++] = size;
}

/* /files [資料夾]：列出 .txt 與 .wav，給 /send <編號> 用 */
static void list_files(const char *dir) {
    double sizes[MAX_PICK];
    g_pick_n = 0;
#ifdef _WIN32
    char pattern[300];
    WIN32_FIND_DATAA fd;
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                pick_add(dir, fd.cFileName, sizes,
                         (double)fd.nFileSizeHigh * 4294967296.0 + (double)fd.nFileSizeLow);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    DIR *d = opendir(dir);
    if (d != NULL) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            char full[600];
            struct stat sb;
            snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
            if (stat(full, &sb) == 0 && S_ISREG(sb.st_mode)) pick_add(dir, e->d_name, sizes, (double)sb.st_size);
        }
        closedir(d);
    }
#endif
    if (g_pick_n == 0) {
        add_sysf("[檔案] %s 裡沒有 .txt 或 .wav。可以用 /files <資料夾>，或直接 /send <檔案路徑>", dir);
        return;
    }
    for (int i = 0; i < g_pick_n; i++)
        add_sysf("[檔案] %2d) %-40s %8.2f MB", i + 1, g_pick[i], sizes[i] / 1048576.0);
    add_sysf("[檔案] 輸入 /send <編號> 傳送（目前模式 %s；先打 /raw 或 /huff 可切換）",
             g_mode == MODE_HUFF ? "HUFF" : "RAW");
}

/* /send <編號|路徑>：在同一條連線上把檔案傳給對方。回傳 0 表示連線還能用。 */
static int send_file_cmd(char *arg) {
    while (*arg == ' ') arg++;
    size_t n = strlen(arg);
    while (n > 0 && (arg[n - 1] == ' ' || arg[n - 1] == '"')) arg[--n] = '\0';
    if (*arg == '"') arg++;
    if (*arg == '\0') { list_files("."); return 0; }

    const char *path = arg;
    char *end = NULL;
    long idx = strtol(arg, &end, 10);
    if (*end == '\0' && idx >= 1 && idx <= g_pick_n) path = g_pick[idx - 1];

    if (g_wait_ack) { add_sysf("[檔案] 上一個檔案還在等對方回覆，請稍候"); return 0; }

    tl_stats_t st;
    tl_mode_t m = (tl_mode_t)g_mode;
    g_ack_t0 = now_ms();
    g_wait_ack = 1;                         /* 先設好：對方的回覆可能比這個函式返回還早到 */
    g_tx_posted = 0;
    int rc = file_send_frames(g_sock, path, m, &st, chat_progress);
    snprintf(g_tx_name, sizeof(g_tx_name), "%s", st.name);
    if (rc == TL_OK) {
        if (g_wait_ack) set_status("已送完，等對方還原與存檔…");
        add_sysf("[檔案] 已送出 %s（%s%s）：原始 %" PRIu64 " B，上線 %" PRIu64 " B，壓縮率 %.2f%%，encode %.1f ms，send %.1f ms",
                 st.name, m == MODE_HUFF ? "HUFF 符號=" : "RAW", m == MODE_HUFF ? tl_sym_name(st.sym) : "",
                 st.file_bytes, st.wire_bytes,
                 100.0 * tl_ratio(st.wire_bytes, st.file_bytes), st.encode_ms, st.send_ms);
        g_tx_posted = 1;
        return 0;
    }
    g_wait_ack = 0;
    set_status("");
    if (rc == TL_ERR_IO)
        add_sysf("[檔案] 開不了 %s（路徑打錯？超過 64 MiB？）。用 /files 看有哪些檔案", path);
    else if (rc == TL_ERR_TODO)
        add_sysf("[檔案] 沒有送出：%s 尚未實作%s", g_todo_frame ? "frame 標頭（src/frame.c）" : "Huffman（src/huffman.c）",
                 g_todo_frame ? "" : "，可先 /raw 再傳");
    else
        add_sysf("[檔案] 傳送失敗：%s", tl_strerror(rc));
    return (rc == TL_ERR_NET || rc == TL_ERR_CLOSED) ? -1 : 0;
}

/* 啟動時各呼叫一次 place holder，看它是不是還回傳 TL_ERR_TODO */
static void selftest(void) {
    uint8_t hdr[TL_HDR_LEN], *out = NULL;
    size_t out_len = 0;
    g_todo_frame = (frame_pack_header(hdr, T_TEXT_RAW, 1) == TL_ERR_TODO);
    g_todo_utf8  = (utf8_validate((const uint8_t *)"a", 1) == TL_ERR_TODO);
    int rc = huff_encode((const uint8_t *)"a", 1, SYM_BYTE, &out, &out_len);
    g_todo_huff  = (rc == TL_ERR_TODO);
    if (rc == TL_OK) free(out);
}

int chat_run(socket_t s, const char *peer, tl_mode_t mode) {
    g_sock = s;
    g_mode = mode;
    snprintf(g_peer, sizeof(g_peer), "%s", peer);
    lock_init();
    selftest();

    thread_t th;
    if (thread_create(&th, recv_thread, NULL) != 0) {
        fprintf(stderr, "錯誤: 建立接收執行緒失敗\n");
        return 1;
    }

    redraw_screen();
    static char input[TL_MAX_TEXT];
    while (g_running && read_line(input, sizeof(input))) {
        if (!g_running) break;
        if (input[0] == '\0') { lock_take(); redraw_screen(); lock_give(); continue; }

        if (strcmp(input, "/quit") == 0) break;
        if (strcmp(input, "/raw") == 0)  { g_mode = MODE_RAW;  add_system("[系統] 已切換為 RAW：之後送出的訊息不壓縮%s", ""); continue; }
        if (strcmp(input, "/huff") == 0) { g_mode = MODE_HUFF; add_system("[系統] 已切換為 HUFF：之後送出的訊息先經 Huffman 編碼%s", ""); continue; }
        if (strcmp(input, "/help") == 0) {
            add_sysf("[系統] /files [資料夾] 列出 .txt 與 .wav   /send <編號或路徑> 傳檔給對方（收到的存在 %s/）", RECV_DIR);
            add_sysf("[系統] /raw 不壓縮   /huff Huffman 壓縮   /quit 離開；泡泡下的小字是「原始 bytes -> 上線 bytes」");
            continue;
        }
        if (strcmp(input, "/files") == 0)        { list_files("."); continue; }
        if (strncmp(input, "/files ", 7) == 0)   { list_files(input + 7); continue; }
        if (strcmp(input, "/send") == 0 || strncmp(input, "/send ", 6) == 0) {
            if (send_file_cmd(input + 5) != 0) break;
            continue;
        }
        if (input[0] == '/') { add_sysf("[系統] 不認得的指令 %s，輸入 /help 看說明", input); continue; }

        size_t n = strlen(input), wire = 0;
        if (utf8_validate((const uint8_t *)input, n) == TL_ERR_DATA) {
            add_system("[系統] 輸入不是合法 UTF-8，沒有送出（終端機編碼設定？Windows 請先 chcp 65001）%s", "");
            continue;
        }
        tl_mode_t m = (tl_mode_t)g_mode;
        int rc = send_text(input, n, m, &wire);
        if (rc == TL_ERR_TODO) {
            if (g_todo_frame)
                add_system("[系統] 沒有送出：frame 標頭尚未實作（src/frame.c 的 TODO 1、2）%s", "");
            else
                add_system("[系統] 沒有送出：Huffman 尚未實作（src/huffman.c）。可先輸入 /raw 改用不壓縮模式%s", "");
            continue;
        }
        if (rc != TL_OK) { add_system("[系統] 傳送失敗：%s", tl_strerror(rc)); break; }
        add_message(input, WHO_ME, n, wire, m);
    }

    /* 收尾：關 socket，接收執行緒的 recv 會因此返回而結束 */
    g_running = 0;
    CLOSESOCK(g_sock);
    printf(CLR_RESET "\n再見！\n");
    return 0;
}
