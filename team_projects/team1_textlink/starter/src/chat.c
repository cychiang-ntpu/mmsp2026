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
 *
 *  ── 這個檔案的地圖 ─────────────────────────────────────────────────────────
 *  上面那張圖：左邊是「我這台」的主執行緒，右邊是「對方那台」的接收執行緒。每一台電腦上兩條都有，
 *  所以兩邊可以同時打字、同時收訊息。
 *
 *  為什麼要兩條執行緒（thread）：程式有兩件事都會「卡住等」：等鍵盤（read_line）、等網路（frame_recv）。
 *  只有一條執行路線的話，停在等鍵盤的時候，對方的訊息到了也沒辦法顯示。所以開第二條執行緒專門等網路，
 *  兩條同時跑、共用同一份全域變數。共用就會搶，於是需要「鎖」（見下面 g_lock 的說明）。
 *
 *  一則文字的資料流：
 *    送：鍵盤 → read_line（一律轉成 UTF-8）→ utf8_validate → send_text：(huff_encode) → frame_send → socket
 *    收：socket → frame_recv → (huff_decode) → handle_text：utf8_validate → add_message：濾控制字元 → 螢幕
 *
 *  建議的閱讀順序（跟著程式執行的順序，不是檔案裡的順序）：
 *    1. chat_run（檔案最後）   入口：開接收執行緒，然後進入「讀一行 → 判斷指令 → 送出」的主迴圈
 *    2. read_line、send_text    主執行緒：讀鍵盤、送一則文字
 *    3. recv_thread、handle_text 接收執行緒：收一個 frame、依 type 分派、顯示文字
 *    4. add_message、redraw_screen  兩條執行緒怎麼共用聊天紀錄與畫面（鎖在這裡）
 *    5. send_file_cmd、handle_file_frame  聊天中傳檔；真正的傳輸流程在 src/transfer.c，這裡只接上畫面與最後的回覆
 *
 *  可以跳過：draw_bubble／draw_caption 的排版計算、list_files 裡兩個平台各自「列出資料夾內容」的 API、has_ext。
 *===========================================================================*/
#include "textlink.h"
#include <inttypes.h>             /* PRIu64：印 uint64_t 用的格式巨集 */
#include <stdarg.h>               /* va_list：讓 add_sysf 像 printf 一樣接受不定個數的參數 */
#ifndef _WIN32
  #include <dirent.h>             /* opendir／readdir：POSIX 列出資料夾內容（Windows 用的 API 在 windows.h 裡） */
#endif

/*========================= 常數與全域狀態 ==================================*/
#define MAX_MSG      100      /* 最多保留幾則訊息               */
#define SHOW_MSG     16       /* 畫面上顯示最近幾則（避免標題列被捲走） */
#define RECV_DIR     "received"   /* 聊天中收到的檔案存在這個資料夾 */
#define MAX_PICK     12       /* /files 最多列出幾個檔案 */
#define BUBBLE_WIDTH 36       /* 泡泡每行最多幾個 bytes          */
#define SCREEN_WIDTH 78       /* 畫面總寬度                     */

/* 一則訊息是誰的：對方（白色泡泡，靠左）、我（綠色泡泡，靠右）、系統（灰色小字，不畫泡泡） */
typedef enum { WHO_PEER = 0, WHO_ME = 1, WHO_SYS = 2 } who_t;

/* struct：把「屬於同一則訊息」的幾個變數綁成一個型別。Message m; 之後用 m.text、m.who 取用各欄位。 */
typedef struct {
    char      text[TL_MAX_TEXT];
    who_t     who;
    size_t    raw_bytes;      /* 訊息本身的 UTF-8 bytes 數                 */
    size_t    wire_bytes;     /* 實際上線的 bytes 數（含 frame 標頭與 codebook） */
    tl_mode_t mode;
} Message;

/* 聊天紀錄：固定 100 格的陣列，g_msg_count 是目前用了幾格。滿了就丟掉最舊的一則（見 add_message）。
 * 這裡的全域變數都加 static：只有這個 .c 檔看得到，不會和別的檔案的同名變數衝突。g_ 開頭是「global」的習慣寫法。 */
static Message  g_history[MAX_MSG];
static int      g_msg_count = 0;

static socket_t g_sock = SOCK_INVALID;
static char     g_peer[64] = "?";
/* volatile：告訴編譯器「這個變數可能被另一條執行緒改掉，每次用到都要重新從記憶體讀」。
 * 沒有它，編譯器看到 while (g_running) 的迴圈裡沒有人改 g_running，可能就只讀一次，迴圈永遠停不下來。
 * 注意 volatile 不是鎖：它只適合這種「值只有 0／1、誰先誰後差一點也無妨」的簡單旗標。
 * 像聊天紀錄那樣「好幾個欄位要一起改」的資料，就一定要用下面的互斥鎖。
 *   g_running  0 = 該結束了。兩條執行緒都會讀，也都可能把它設成 0。
 *   g_mode     目前是 RAW 還是 HUFF。主執行緒在 /raw、/huff 時改；兩條執行緒畫標題列時讀。 */
static volatile int g_running = 1;
static volatile int g_mode = MODE_HUFF;

static int g_todo_frame = 0, g_todo_utf8 = 0, g_todo_huff = 0;   /* 啟動時自我檢查的結果 */

static char g_status[200] = "";            /* 輸入列上方的狀態列：檔案傳輸進度 */
static char g_pick[MAX_PICK][300];         /* /files 列出的檔案路徑，/send <編號> 用 */
static int  g_pick_n = 0;
static char g_tx_name[128] = "";           /* 正在傳送的檔名 */
/* 下面三個變數用來處理「我送出檔案之後，等對方回覆存檔結果」這件事：主執行緒負責送（send_file_cmd），
 * 回覆卻是接收執行緒收到的（handle_file_frame），兩條執行緒靠這幾個變數互相通知。 */
static volatile int g_wait_ack = 0;        /* 1 = 檔案已送完，等對方回覆存檔結果 */
static volatile int g_tx_posted = 0;       /* 1 = 「已送出」那則系統訊息已經顯示 */
static double g_ack_t0 = 0.0;              /* 開始傳檔的時刻（now_ms），收到回覆時用來算總共花了多久 */

/*--- 互斥鎖：兩條執行緒都會寫聊天紀錄、重畫畫面，一次只准一條進去 ---*/
/* 不上鎖會怎樣：主執行緒正在把我的訊息放進 g_history、g_msg_count 加到一半，接收執行緒同時也在放對方的訊息，
 * 兩則可能寫進同一格，或是畫面畫到一半被另一條執行緒清掉重畫，escape code 與文字交錯成亂碼。
 * 規則：碰 g_history、g_msg_count、g_status 或呼叫 redraw_screen 之前先 lock_take()，做完立刻 lock_give()。
 * 拿不到鎖的那一條會停下來等，所以鎖住的時間要短；也絕對不要「拿了鎖還沒還、又去拿同一把」，會永遠等下去（deadlock）。
 * 這把鎖（g_lock，管畫面與紀錄）和 src/net.c 的送出鎖（管 socket 的送出）是兩把不同的鎖，各管各的。
 * Windows 的 CRITICAL_SECTION 使用前要初始化，所以有 lock_init；三個函式名稱兩個平台相同，下面的程式就不用再分平台。 */
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
/* ANSI escape sequence：終端機的「控制指令」。終端機收到 ESC（ASCII 27，C 字串裡寫成 \x1b）後面接 '[' 的一串字，
 * 不會把它顯示出來，而是照著執行：
 *   ESC [ 數字;數字;... m   設定之後文字的樣式。0 = 全部還原、1 = 粗體、30–37 = 文字顏色、40–47 = 背景顏色、
 *                           90–97 = 亮色文字。例：\x1b[1;97;42m = 粗體＋亮白字＋綠底。
 *   ESC [ 2J                清除整個畫面          ESC [ H   游標移到左上角
 * 用法是「先印顏色碼 → 印文字 → 印 CLR_RESET」；忘了還原，後面所有文字都會帶著那個顏色。
 * C 會把相鄰的字串常數自動接起來，所以可以寫 printf(CLR_DIM "文字" CLR_RESET "\n")。
 * Windows 的主控台要先開啟這個功能才認得這些指令，src/main.c 一開始已經做了。
 * 這也是為什麼 add_message 要把對方送來的控制字元濾掉：不濾的話，對方可以送 ESC 指令來清我們的畫面、改顏色。 */
#define CLR_RESET    "\x1b[0m"
#define CLR_GREEN    "\x1b[1;97;42m"   /* 亮白字、綠底 → 自己的泡泡 */
#define CLR_WHITE    "\x1b[30;47m"     /* 黑字、白底  → 對方的泡泡  */
#define CLR_TITLE    "\x1b[1;97;44m"   /* 亮白字、藍底 → 標題列     */
#define CLR_DIM      "\x1b[90m"        /* 灰色 → 提示文字           */
#define CLR_WARN     "\x1b[33m"        /* 黃色 → 尚未實作的提醒      */
#define CLEAR_SCREEN "\x1b[2J\x1b[H"

/* 畫一顆泡泡。折行時往回退到 UTF-8 字元邊界（續位元組都是 10xxxxxx）。
 * 註：和 baseline 一樣用 bytes 數估寬度，中文（3 bytes、顯示 2 格）會有誤差。 */
/* text 是訊息內容，is_me 非 0 表示是自己的（綠色、靠右）。呼叫者：redraw_screen（呼叫時已經持有 g_lock）。
 * 每一圈處理一行：從 start 開始最多取 BUBBLE_WIDTH 個 bytes。
 * 為什麼要「往回退」：一個中文字是 3 bytes，如果剛好從中間切開，這一行的結尾和下一行的開頭都會是亂碼。
 * text[start + take] 是「下一行的第一個 byte」；(byte & 0xC0) == 0x80 表示它的最高兩個 bits 是 10，也就是續位元組，
 * 代表切在一個字的中間，所以 take 減 1 再看，直到下一行從一個字的開頭開始。
 * take 退到 0（一整行都是續位元組；合法的 UTF-8 不會這樣）時改回硬切，否則 start 不前進，迴圈不會結束。 */
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

        /* 自己的泡泡靠右：先印 pad 個空白把它推過去。"%*s" 的 * 表示「寬度由參數給」，
         * 所以 printf("%*s", pad, "") 就是印 pad 個空白。4 是泡泡左右各 2 格的留白。 */
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
/* 例："HUFF  12 B -> 41 B on wire (342%)"。百分比 = wire_bytes ÷ raw_bytes × 100，和檔案傳輸的壓縮率是同一個定義；
 * 短訊息加上 5 bytes 標頭與 codebook 之後常常遠大於 100%，這是正常的，也是報告要討論的現象。
 * raw_bytes 為 0 時不能除，直接顯示 0。呼叫者：redraw_screen。 */
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

/* 整個畫面清掉重畫：標題列 → 模式與指令提示 →（尚未實作的黃字）→ 最近 16 則訊息 →（狀態列）→ 分隔線 → 輸入提示。
 * 這是最簡單的「終端機 GUI」做法：不去算哪裡要更新，任何東西變了就全部重畫一次。
 * 這個函式自己「沒有」上鎖：呼叫它的人要先拿著 g_lock（add_message、set_status、chat_run 處理空行時都是這樣做）。
 * 副作用：使用者打到一半還沒按 Enter 的字會從畫面上消失，但它們仍在終端機的輸入緩衝區裡，按 Enter 照樣送出。 */
static void redraw_screen(void) {
    char title[160];
    printf(CLEAR_SCREEN);

    snprintf(title, sizeof(title), "TextLink  |  對方 %s", g_peer);
    printf(CLR_TITLE "  %-*s" CLR_RESET "\n", SCREEN_WIDTH - 2, title);    /* %-*s：靠左、補空白到指定寬度，藍底才會延伸成一整條 */
    printf(CLR_DIM "  模式: %s   指令: /files  /send  /raw  /huff  /help  /quit" CLR_RESET "\n",
           g_mode == MODE_HUFF ? "HUFF（Huffman 壓縮）" : "RAW（不壓縮）");
    if (g_todo_frame || g_todo_utf8 || g_todo_huff)
        printf(CLR_WARN "  尚未實作:%s%s%s   → 見 starter/README.md 的 TODO 清單" CLR_RESET "\n",
               g_todo_frame ? " [frame 標頭]" : "",
               g_todo_utf8  ? " [UTF-8 檢查]" : "",
               g_todo_huff  ? " [Huffman]" : "");
    printf("\n");

    /* 只畫最後 SHOW_MSG 則：first 是要畫的第一則在陣列裡的位置 */
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
    if (g_status[0]) printf(CLR_WARN "  %s" CLR_RESET "\n", g_status);      /* g_status[0] 不是 '\0' ＝ 狀態列有內容 */
    printf(CLR_DIM "──────────────────────────────────────────" CLR_RESET "\n");
    printf("訊息> ");
    fflush(stdout);                      /* "訊息> " 後面沒有換行，不 fflush 可能不會立刻顯示 */
}

/* 把一則訊息加入紀錄並重畫（有上鎖，兩條執行緒都可安全呼叫）。
 * 控制字元（含 ESC）一律換成空白：對方送來的文字不可以有機會操控我們的終端機。 */
/*   text        訊息內容（以 '\0' 結尾的 UTF-8）
 *   who         誰的訊息
 *   raw／wire   原始 bytes 數／實際上線 bytes 數，只用來畫泡泡下面那行小字；系統訊息傳 0
 *   mode        這則訊息是用 RAW 還是 HUFF 送的
 * 呼叫者：主執行緒（自己送出的訊息）、接收執行緒（對方的訊息）、add_system／add_sysf（系統訊息）。
 * 從 lock_take 到 lock_give 之間的一整段是一個「不可被打斷的動作」：放進紀錄＋重畫，另一條執行緒要等這段做完。 */
static void add_message(const char *text, who_t who, size_t raw, size_t wire, tl_mode_t mode) {
    lock_take();
    /* 滿了：memmove 把第 1–99 則整批往前搬一格（第 0 則被蓋掉），空出最後一格 */
    if (g_msg_count == MAX_MSG) {
        memmove(&g_history[0], &g_history[1], sizeof(Message) * (MAX_MSG - 1));
        g_msg_count--;
    }
    Message *m = &g_history[g_msg_count++];
    snprintf(m->text, sizeof(m->text), "%s", text);       /* 複製一份進紀錄；snprintf 保證不會寫超出 m->text */
    /* 過濾控制字元：ASCII 0x00–0x1F（含 ESC = 0x1B、換行、Tab）與 0x7F（DEL）。
     * 先轉成 unsigned char 再比：char 在多數平台是有號的，UTF-8 的 0x80 以上會變成負數，直接比 < 0x20 會誤殺所有中文。
     * UTF-8 多 byte 字元的每個 byte 都 ≥ 0x80，所以這個過濾不會傷到任何正常文字。 */
    for (char *p = m->text; *p; p++)
        if ((unsigned char)*p < 0x20 || *p == 0x7F) *p = ' ';
    m->who = who;
    m->raw_bytes = raw;
    m->wire_bytes = wire;
    m->mode = mode;
    redraw_screen();
    lock_give();
}

/* 加一則系統訊息（灰色小字）。fmt 裡要剛好有一個 %s，由 detail 填入；沒有東西要填時，
 * 呼叫端在 fmt 最後放一個 %s、detail 傳空字串 ""（本檔很多地方這樣寫）。 */
static void add_system(const char *fmt, const char *detail) {
    char buf[256];
    snprintf(buf, sizeof(buf), fmt, detail);
    add_message(buf, WHO_SYS, 0, 0, MODE_RAW);
}

/* 同上，但和 printf 一樣可以接任意個數的參數（... 的部分）。va_list／va_start／va_end 是 C 取用「不定個數參數」的
 * 標準寫法；vsnprintf 是 snprintf 的「吃 va_list」版本。照抄這個樣板就能寫出自己的 printf 風格函式。 */
static void add_sysf(const char *fmt, ...) {
    char buf[400];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    add_message(buf, WHO_SYS, 0, 0, MODE_RAW);
}

/* 設定輸入列上方的狀態列並重畫；text 傳 "" 就是清掉。兩條執行緒都會呼叫，所以一樣要上鎖。 */
static void set_status(const char *text) {
    lock_take();
    snprintf(g_status, sizeof(g_status), "%s", text);
    redraw_screen();
    lock_give();
}

/* 檔案傳輸進度：每前進 10% 更新一次狀態列（每個 chunk 都整個重畫會閃）*/
/* 型別符合 tl_progress_fn，由 src/transfer.c 的 file_send_frames（label = "傳送"，主執行緒）
 * 與本檔的 handle_file_frame（label = "接收"，接收執行緒）呼叫。done／total 是目前與全部的 bytes 數。
 * static 區域變數：函式返回後值還留著，下次呼叫接著用（一般區域變數每次呼叫都重來）。這裡用它記「上次顯示到第幾格」。
 * 傳送與接收可能同時進行，所以各記各的（last_tx／last_rx），用 label 決定這次用哪一個。
 * step = 目前在第幾個 10%（0–10）；和上次相同就直接返回、不重畫。done == total（完成）時一定顯示，
 * 並把紀錄重設成 -1，下一個檔案才會從頭開始顯示。 */
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
static file_rx_t g_rx;                      /* 全域變數沒寫初值時自動全部是 0，正好是 file_rx_t 要求的「還沒開始」狀態 */
static int g_rx_discard = 0;                /* 1 = 這個檔案已經出錯，剩下的 FILE_DATA 直接丟掉 */

/* 處理一個 FILE_BEGIN／FILE_DATA／FILE_END frame。只在接收執行緒裡執行（由 recv_thread 呼叫）。
 * p／len 是 payload；p 由 recv_thread 負責 free，這裡不用管。
 *
 * 聊天與命令列 recv 的差別：命令列收到壞的檔案就結束程式；聊天不能因為一個檔案壞了就斷線，
 * 所以出錯時設 g_rx_discard = 1，把這個檔案剩下的 frame 默默丟掉，直到對方的 FILE_END 才回覆「失敗」並恢復正常。
 *
 * FILE_END 在聊天裡有兩種意思，因為兩邊都可以送檔案，而且共用同一條連線：
 *   (a) payload 為空            ：對方送完「他的」檔案了 → 我們檢查、解碼、存檔，然後回覆 1 byte 的結果
 *   (b) payload 1 byte（0 或 1）：這是對方對「我們送的」檔案的回覆（ack）
 * 下面第三個分支用「長度是 1，而且我們目前沒有在收檔案」來認出 (b)，其餘的 FILE_END 都當成 (a)。 */
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
            file_rx_reset(&g_rx);                 /* 已經收到的部分沒有用了，先把記憶體還掉 */
            add_sysf("[檔案] 接收失敗（%s），這個檔案會被丟棄", tl_strerror(rc));
        } else {
            chat_progress("接收", g_rx.got, g_rx.data_size);
        }
    } else if (len == 1 && !g_rx.begun && !g_rx_discard) {
        /* FILE_END 帶 1 byte：這是對方對「我們送的檔案」的回覆 */
        double ms = now_ms() - g_ack_t0;          /* 從 send_file_cmd 開始傳，到現在收到回覆，總共多久 */
        if (!g_wait_ack) return;                  /* 我們根本沒有在等回覆：忽略這個 frame */
        /* 兩條執行緒的先後順序問題：小檔案時，對方的回覆可能在主執行緒印出「已送出…」之前就到了，
         * 畫面上會變成先看到「對方已成功存檔」、才看到「已送出」，順序顛倒。
         * 做法：每 5 ms 看一次 g_tx_posted，等主執行緒把那一則印出來；最多等 100 次（約 0.5 秒）就不等了。
         * 這種「睡一下再看一次」叫 polling，寫起來最簡單；更正規的做法是 condition variable。
         * Sleep 的單位是毫秒；nanosleep 吃的是 {秒, 奈秒}，5 ms = 5 × 1000 × 1000 奈秒。 */
        for (int i = 0; i < 100 && !g_tx_posted; i++) {     /* 小檔案時回覆可能比「已送出」還早到：等它先顯示 */
#ifdef _WIN32
            Sleep(5);
#else
            struct timespec ts = {0, 5 * 1000 * 1000};
            nanosleep(&ts, NULL);
#endif
        }
        g_wait_ack = 0;                           /* 回覆到了：主執行緒可以再送下一個檔案 */
        set_status("");
        if (p[0] == 0) add_sysf("[檔案] 對方已成功還原並存檔 %s（從開始傳到收到回覆 %.1f ms）", g_tx_name, ms);
        else           add_sysf("[檔案] 對方回報 %s 還原失敗（解碼或寫檔沒成功）", g_tx_name);
    } else {
        /* FILE_END（空）：對方送完了 → 檢查、解碼、存檔、回覆結果 */
        /* 前面已經出錯（g_rx_discard）就不必 finish，直接當成失敗。不論成敗都要回覆：對方正在等這 1 byte。
         * 這個 frame_send 是從「接收執行緒」送出的，主執行緒同一時間可能也在送訊息或檔案；
         * frame_send 裡的送出鎖（src/net.c 的 net_send_lock）保證兩個 frame 不會互相插進對方的中間。
         * 注意：解碼與寫檔都在接收執行緒裡做，大檔案解碼期間這條執行緒不會去收新的 frame，
         * 對方這段時間送來的訊息要等解碼完才會顯示。 */
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
        else if (!g_rx_discard)                   /* discard 的情況之前已經顯示過錯誤訊息，不重複 */
            add_sysf("[檔案] 收到 %s 但無法還原：%s", g_rx.name, tl_strerror(rc));
        file_rx_reset(&g_rx);                     /* 回到「還沒開始」，準備收下一個檔案 */
        g_rx_discard = 0;
    }
}

/* 顯示一則對方送來的文字。只在接收執行緒裡執行。
 *   text／len  已經是「解壓縮之後」的文字 bytes（不保證有 '\0' 結尾，所以一律用 len）
 *   wire       這則訊息實際上線的 bytes 數（payload + 5 bytes 標頭），畫小字用
 *   mode       對方用 RAW 還是 HUFF 送的
 * 原則：網路上來的資料一律先當成「不可信」，檢查過才顯示：
 *   1. 太長（放不進 Message.text）→ 丟棄
 *   2. 中間夾著 '\0'：UTF-8 檢查會放行 U+0000，但 C 字串遇到 '\0' 就結束，後半段會無聲地消失 → 直接拒收
 *   3. utf8_validate 說不合法 → 丟棄。亂碼不只是難看：不合法的 byte 序列可能讓終端機或之後的處理出錯。
 * 這裡特別寫成「== TL_ERR_DATA 才丟」，而不是「!= TL_OK 就丟」，理由見函式裡的註解。 */
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
    buf[len] = '\0';                      /* 補上結尾，變成 C 字串之後才能交給 add_message */
    add_message(buf, WHO_PEER, len, wire, mode);
}

/* 接收執行緒的本體。THREAD_FN(recv_thread) 會展開成這個平台要求的執行緒函式寫法（見 include/platform.h）。
 * 它從 chat_run 開出來之後就一直在這個迴圈裡：等一個完整的 frame → 看 type → 交給對應的處理函式 → free → 再等下一個。
 * 大部分時間它都停在 frame_recv 裡面等資料；這不會耗 CPU，也不影響主執行緒讀鍵盤。 */
THREAD_FN(recv_thread) {
    (void)arg;                            /* 參數沒用到；這樣寫是告訴編譯器「我知道」，避免 unused parameter 警告 */
    while (g_running) {
        uint8_t type = 0, *payload = NULL;
        size_t len = 0;
        int rc = frame_recv(g_sock, &type, &payload, &len);
        if (rc != TL_OK) {
            /* frame_recv 失敗有兩種來源：(1) 是我們自己要結束，主執行緒把 socket 關了（這時 g_running 已經是 0，什麼都不用說）；
             * (2) 對方離線或送來壞的標頭。第二種要通知使用者。主執行緒這時還停在 read_line 等鍵盤，
             * 我們沒辦法從這裡把它叫醒，所以訊息寫「按 Enter 離開」：使用者按了 Enter，主迴圈看到 g_running == 0 就會結束。 */
            if (g_running && (rc == TL_ERR_CLOSED || rc == TL_ERR_NET))
                add_system("[系統] 對方已離線，連線結束（按 Enter 離開）%s", "");
            else if (g_running)
                add_system("[系統] 連線結束：%s（按 Enter 離開）", tl_strerror(rc));
            g_running = 0;
            break;
        }

        if (type == T_TEXT_RAW) {
            handle_text(payload, len, len + TL_HDR_LEN, MODE_RAW);      /* 上線 bytes = payload + 5 bytes 標頭 */
        } else if (type == T_TEXT_HUFF) {
            /* 先解壓縮再當成文字處理。第三個參數是「最多允許解出幾 bytes」：一則訊息不該超過 TL_MAX_TEXT - 1，
             * 對方就算送來一個宣稱會解出幾 GB 的壞資料，huff_decode 也必須在這個上限內停下來回報錯誤。
             * 解不開只顯示一則系統訊息，連線照常繼續：frame 的邊界沒有壞，下一個 frame 還是讀得到。 */
            uint8_t *dec = NULL;
            size_t dlen = 0;
            rc = huff_decode(payload, len, TL_MAX_TEXT - 1, &dec, &dlen);
            if (rc == TL_OK) {
                handle_text(dec, dlen, len + TL_HDR_LEN, MODE_HUFF);
                free(dec);                /* huff_decode 成功時配置的輸出，由呼叫端 free */
            } else {
                add_system("[系統] 收到 Huffman 訊息但解不開：%s", tl_strerror(rc));
            }
        } else if (type == T_FILE_BEGIN || type == T_FILE_DATA || type == T_FILE_END) {
            handle_file_frame(type, payload, len);
        } else {
            /* 不認得的 type：對方和我們講的不是同一套規格，後面的資料也無從解讀，結束比硬撐安全 */
            add_system("[系統] 收到不認得的封包 type，關閉連線（按 Enter 離開）%s", "");
            g_running = 0;
        }
        free(payload);                    /* frame_recv 配置的 payload，每一圈用完都要還 */
    }
    THREAD_RETURN;
}

/*========================= 主執行緒：讀鍵盤、送出 ==========================*/

/* 讀一行輸入，結果一律是 UTF-8。
 * Windows 主控台用 ReadConsoleW 讀 UTF-16 再轉 UTF-8：fgets 在主控台上讀中文、emoji 不可靠。
 * 輸入被導向（測試腳本用 < 或 | 餵資料）時，則照 baseline 用 fgets，假設來源已是 UTF-8。 */
/*   buf／cap  輸出緩衝區與它的容量；結果以 '\0' 結尾，行尾的換行已經去掉
 *   回傳      1 = 讀到一行；0 = 輸入結束（Ctrl+Z／Ctrl+D、或導向的檔案讀完了）
 *   呼叫者    chat_run 的主迴圈。這個函式會阻塞到使用者按 Enter。
 *
 * 為什麼 Windows 要特別處理：Windows 主控台內部用 UTF-16 存文字（wchar_t，每個單位 2 bytes）。fgets 拿到的是
 * 主控台「依目前字碼頁轉換過」的 bytes，這條路對非 ASCII 字元長年有問題，中文可能變成 0 或亂碼。
 * ReadConsoleW（W = wide）直接拿 UTF-16，再用 WideCharToMultiByte(CP_UTF8, ...) 自己轉成 UTF-8 最穩；
 * emoji 在 UTF-16 裡佔兩個單位（surrogate pair），這個函式會正確轉成 4 bytes 的 UTF-8。
 * 我們的 frame、utf8_validate、Huffman（SYM_CHAR）全都假設文字是 UTF-8，所以在入口就統一。
 *
 * GetConsoleMode 成功 ＝ 標準輸入真的是一個主控台視窗；失敗 ＝ 輸入被導向到檔案或管線，這時 ReadConsoleW 不能用。
 *
 * 讀這段程式的方法：#ifdef 把一個 if ... else 拆在兩邊。
 *   Windows：if (是主控台) { ReadConsoleW ... } else { fgets ... }
 *   POSIX  ：#ifdef 裡面整段不存在，只剩下 { fgets ... } 這個區塊，一定會執行。 */
static int read_line(char *buf, size_t cap) {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD cmode;
    if (GetConsoleMode(h, &cmode)) {
        static wchar_t w[TL_MAX_TEXT];                    /* static：8 KB 的緩衝區不放在堆疊上 */
        DWORD got = 0;                                    /* 實際讀到幾個 UTF-16 單位（含行尾的 \r\n） */
        if (!ReadConsoleW(h, w, TL_MAX_TEXT - 1, &got, NULL) || got == 0) return 0;
        if (w[0] == 0x1A) return 0;                       /* Ctrl+Z = 結束輸入 */
        /* 回傳值 n 是轉出來的 UTF-8 bytes 數；0 表示失敗（例如 buf 放不下），這時 buf 變成空字串，等於這一行被忽略。
         * 傳進去的長度是 got，不含 '\0'，所以轉出來的結果也沒有 '\0'，要自己補。 */
        int n = WideCharToMultiByte(CP_UTF8, 0, w, (int)got, buf, (int)cap - 1, NULL, NULL);
        if (n < 0) n = 0;
        buf[n] = '\0';
    } else
#endif
    {
        if (fgets(buf, (int)cap, stdin) == NULL) return 0;
    }
    /* strcspn(buf, "\r\n") 回傳「第一個 \r 或 \n 出現在第幾個位置」（都沒有就回傳字串長度）；
     * 在那裡放 '\0' 等於把行尾的換行切掉。Windows 的行尾是 \r\n，所以兩個字元都要列進去。 */
    buf[strcspn(buf, "\r\n")] = '\0';
    return 1;
}

/* 送一則文字。回傳 TL_OK 時 *wire 是實際上線的 bytes 數。 */
/*   text／n   要送的 UTF-8 文字與它的 bytes 數（不含 '\0'；'\0' 不會被送出去）
 *   mode      MODE_RAW：payload 就是文字本身，type = TEXT_RAW
 *             MODE_HUFF：payload 是 huff_encode 的輸出（裡面含 codebook），type = TEXT_HUFF
 *   wire      輸出：payload + TL_HDR_LEN（5 bytes 標頭），給泡泡下面的小字用
 *   回傳      TL_OK 或 TL_ERR_*（TL_ERR_TODO 表示 frame 或 Huffman 還沒實作）
 *   呼叫者    chat_run。只在主執行緒裡執行。
 * enc 是 huff_encode 配置的，送完就 free；注意 rc 先存起來、free 之後才 return，順序反了就會漏掉 free。 */
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
/* name 是否以 ext 結尾（不分大小寫；ext 要傳小寫）。n <= e：檔名只有 ".txt" 四個字、前面沒有主檔名的，不算。 */
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

/* list_files 每找到一個檔案就呼叫一次：符合條件就把 "資料夾/檔名" 記進 g_pick、大小記進 sizes（同一個編號）。
 * 不符合（清單滿了、不是 .txt／.wav、檔名含非 ASCII）就什麼都不做。 */
static void pick_add(const char *dir, const char *name, double *sizes, double size) {
    if (g_pick_n >= MAX_PICK) return;
    if (!has_ext(name, ".txt") && !has_ext(name, ".wav")) return;
    for (const char *p = name; *p; p++)
        if ((unsigned char)*p >= 0x80) return;     /* 非 ASCII 檔名先略過（Windows 的開檔 API 要另外處理）*/
    snprintf(g_pick[g_pick_n], sizeof(g_pick[0]), "%s/%s", dir, name);
    sizes[g_pick_n++] = size;
}

/* /files [資料夾]：列出 .txt 與 .wav，給 /send <編號> 用 */
/* 「列出資料夾裡有哪些檔案」標準 C 沒有提供，兩個平台各用各的 API，但形狀一樣：開始 → 一次拿一筆 → 沒有了 → 關閉。
 *   Windows：FindFirstFileA("資料夾\\*") 拿第一筆 → FindNextFileA 拿下一筆 → FindClose。
 *            檔案大小被拆成高、低兩個 32-bit 欄位，合起來是 High × 2^32 + Low（4294967296 = 2^32）。
 *   POSIX  ：opendir → readdir 一次一筆 → closedir。readdir 只給名字，大小與「是不是一般檔案」要再用 stat 問；
 *            S_ISREG = 是一般檔案（排除資料夾等）。
 * 兩邊都把子資料夾排除。結果用系統訊息一行一行顯示在聊天畫面裡；編號從 1 開始，對應 g_pick[編號 - 1]。 */
static void list_files(const char *dir) {
    double sizes[MAX_PICK];
    g_pick_n = 0;                         /* 每次 /files 都重新編號，舊的清單作廢 */
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
    /* 資料夾不存在、打不開，和「裡面沒有符合的檔案」走同一條路：g_pick_n 都是 0 */
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
/*   arg     "/send" 後面的那一段字串（可能以空白開頭）。這個函式會直接修改它（去掉尾端的空白與引號）。
 *   回傳    0 = 連線還能用（包含「檔案打不開」這種與連線無關的失敗）；-1 = 網路斷了，chat_run 應該結束
 *   呼叫者  chat_run。在主執行緒裡執行：傳檔期間主執行緒不讀鍵盤，送完才回到輸入；接收執行緒不受影響。
 *
 * 和命令列 transfer_send 的差別：那邊送完就自己呼叫 frame_recv 等回覆；這裡不行，因為這條連線的「收」
 * 全部歸接收執行緒管（兩條執行緒同時 recv 同一個 socket，資料會被隨機分走）。
 * 所以這裡只送，回覆由接收執行緒的 handle_file_frame 處理，兩邊用 g_wait_ack、g_tx_posted、g_ack_t0、g_tx_name 傳話。 */
static int send_file_cmd(char *arg) {
    /* 整理參數：跳過開頭的空白；去掉尾端的空白與 "；開頭有 " 也跳過（從檔案總管拖曳含空白的路徑進來時，路徑會帶引號）。
     * 什麼都沒給：當成使用者想看有哪些檔案。 */
    while (*arg == ' ') arg++;
    size_t n = strlen(arg);
    while (n > 0 && (arg[n - 1] == ' ' || arg[n - 1] == '"')) arg[--n] = '\0';
    if (*arg == '"') arg++;
    if (*arg == '\0') { list_files("."); return 0; }

    /* 參數「整串都是數字」（*end == '\0'）而且落在上次 /files 的編號範圍內 → 當成編號；否則整串當成路徑 */
    const char *path = arg;
    char *end = NULL;
    long idx = strtol(arg, &end, 10);
    if (*end == '\0' && idx >= 1 && idx <= g_pick_n) path = g_pick[idx - 1];

    /* 一次只等一個回覆：g_tx_name 等變數只有一份，而且回覆的 frame 裡沒有寫它是針對哪個檔案 */
    if (g_wait_ack) { add_sysf("[檔案] 上一個檔案還在等對方回覆，請稍候"); return 0; }

    tl_stats_t st;
    tl_mode_t m = (tl_mode_t)g_mode;        /* 先把模式複製一份：這次傳輸從頭到尾、連同顯示的文字都用同一個值 */
    g_ack_t0 = now_ms();
    g_wait_ack = 1;                         /* 先設好：對方的回覆可能比這個函式返回還早到 */
    g_tx_posted = 0;
    int rc = file_send_frames(g_sock, path, m, &st, chat_progress);
    snprintf(g_tx_name, sizeof(g_tx_name), "%s", st.name);       /* st.name 在失敗時也已經填好（消毒過的檔名） */
    if (rc == TL_OK) {
        if (g_wait_ack) set_status("已送完，等對方還原與存檔…");
        /* 壓縮率 = 上線 bytes ÷ 原始 bytes × 100%（tl_ratio），分子含所有 frame 標頭與 codebook；和 STATS 的 ratio 同一個定義 */
        add_sysf("[檔案] 已送出 %s（%s%s）：原始 %" PRIu64 " B，上線 %" PRIu64 " B，壓縮率 %.2f%%，encode %.1f ms，send %.1f ms",
                 st.name, m == MODE_HUFF ? "HUFF 符號=" : "RAW", m == MODE_HUFF ? tl_sym_name(st.sym) : "",
                 st.file_bytes, st.wire_bytes,
                 100.0 * tl_ratio(st.wire_bytes, st.file_bytes), st.encode_ms, st.send_ms);
        g_tx_posted = 1;                    /* 通知接收執行緒：「已送出」印好了，可以顯示對方的回覆了 */
        return 0;
    }
    /* 失敗：不會有回覆了，把等待狀態收掉，再依原因給不同的提示 */
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
/* 結果存在 g_todo_*，只用來決定畫面上方的黃字與錯誤訊息的措辭；不檢查實作「對不對」（那是 make test 的工作）。
 * huff_encode 如果已經實作而且成功，會配置輸出，所以要 free。 */
static void selftest(void) {
    uint8_t hdr[TL_HDR_LEN], *out = NULL;
    size_t out_len = 0;
    g_todo_frame = (frame_pack_header(hdr, T_TEXT_RAW, 1) == TL_ERR_TODO);
    g_todo_utf8  = (utf8_validate((const uint8_t *)"a", 1) == TL_ERR_TODO);
    int rc = huff_encode((const uint8_t *)"a", 1, SYM_BYTE, &out, &out_len);
    g_todo_huff  = (rc == TL_ERR_TODO);
    if (rc == TL_OK) free(out);
}

/* 聊天的入口。呼叫者：src/main.c，在 net_listen_accept 或 net_connect 成功之後。
 *   s      已接通的 socket。接通之後 server 與 client 的行為完全相同，所以只需要這一個函式
 *   peer   對方的 "IP:port"，顯示在標題列
 *   mode   一開始用 RAW 還是 HUFF（命令列的 --raw／--huff），之後可以用 /raw、/huff 切換
 *   回傳   程式的結束碼：0 正常結束、1 開不了接收執行緒 */
int chat_run(socket_t s, const char *peer, tl_mode_t mode) {
    g_sock = s;
    g_mode = mode;
    snprintf(g_peer, sizeof(g_peer), "%s", peer);
    lock_init();                          /* 一定要在開第二條執行緒之前：那之後就可能有人要用鎖 */
    selftest();

    /* 開接收執行緒：從這一行之後，recv_thread 和下面的主迴圈「同時」在跑 */
    thread_t th;
    if (thread_create(&th, recv_thread, NULL) != 0) {
        fprintf(stderr, "錯誤: 建立接收執行緒失敗\n");
        return 1;
    }

    redraw_screen();
    static char input[TL_MAX_TEXT];       /* static：4 KB 的緩衝區不放在堆疊上 */
    /* 主迴圈：讀一行 → 是指令就執行 → 不是指令就當成訊息送出。
     * continue = 這一行處理完了，回去讀下一行；break = 離開聊天。read_line 回傳 0（輸入結束）也會離開。 */
    while (g_running && read_line(input, sizeof(input))) {
        if (!g_running) break;            /* 等鍵盤的這段時間連線可能已經結束（接收執行緒把 g_running 設成 0） */
        if (input[0] == '\0') { lock_take(); redraw_screen(); lock_give(); continue; }   /* 空行：不送，只重畫 */

        /* 指令：strcmp 回傳 0 表示兩個字串完全相同；strncmp(..., 7) 只比前 7 個字元，用來認「/files 」這種後面還有參數的 */
        if (strcmp(input, "/quit") == 0) break;
        if (strcmp(input, "/raw") == 0)  { g_mode = MODE_RAW;  add_system("[系統] 已切換為 RAW：之後送出的訊息不壓縮%s", ""); continue; }
        if (strcmp(input, "/huff") == 0) { g_mode = MODE_HUFF; add_system("[系統] 已切換為 HUFF：之後送出的訊息先經 Huffman 編碼%s", ""); continue; }
        if (strcmp(input, "/help") == 0) {
            add_sysf("[系統] /files [資料夾] 列出 .txt 與 .wav   /send <編號或路徑> 傳檔給對方（收到的存在 %s/）", RECV_DIR);
            add_sysf("[系統] /raw 不壓縮   /huff Huffman 壓縮   /quit 離開；泡泡下的小字是「原始 bytes -> 上線 bytes」");
            continue;
        }
        if (strcmp(input, "/files") == 0)        { list_files("."); continue; }
        if (strncmp(input, "/files ", 7) == 0)   { list_files(input + 7); continue; }   /* input + 7：跳過 "/files " 這 7 個字元 */
        if (strcmp(input, "/send") == 0 || strncmp(input, "/send ", 6) == 0) {
            if (send_file_cmd(input + 5) != 0) break;
            continue;
        }
        if (input[0] == '/') { add_sysf("[系統] 不認得的指令 %s，輸入 /help 看說明", input); continue; }

        /* 一般訊息。送出之前先檢查自己的輸入是不是合法 UTF-8：不要把壞資料送給對方。
         * 和 handle_text 一樣寫成「== TL_ERR_DATA 才擋」：utf8_validate 還是 TODO 時照樣放行。 */
        size_t n = strlen(input), wire = 0;
        if (utf8_validate((const uint8_t *)input, n) == TL_ERR_DATA) {
            add_system("[系統] 輸入不是合法 UTF-8，沒有送出（終端機編碼設定？Windows 請先 chcp 65001）%s", "");
            continue;
        }
        tl_mode_t m = (tl_mode_t)g_mode;
        int rc = send_text(input, n, m, &wire);
        /* TL_ERR_TODO：place holder 還沒寫，不是連線的問題，留在聊天裡；其他錯誤代表連線壞了，離開。
         * 送成功之後才把自己的訊息畫成泡泡，畫面上有的就是真的送出去的。 */
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
    /* 先把 g_running 設成 0 再關：接收執行緒看到 frame_recv 失敗時，g_running 已經是 0，就知道是我們自己要結束，
     * 不會印「對方已離線」。關掉 socket 之後，對方的 recv 通常會回傳 0（見 src/net.c 的 recv_all），對方畫面顯示我們離線了。
     * 這裡沒有等接收執行緒結束（沒有 join）：return 之後 main 隨即結束，整個行程連同那條執行緒一起被作業系統收掉。 */
    g_running = 0;
    CLOSESOCK(g_sock);
    printf(CLR_RESET "\n再見！\n");
    return 0;
}
