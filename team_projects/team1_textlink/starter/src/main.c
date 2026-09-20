/*============================================================================
 *  main.c  —  TextLink 進入點：解析命令列 → 分派（殼：已完成）
 *----------------------------------------------------------------------------
 *  命令列介面是規格固定的（見 ../README.md），評測腳本會照這個格式呼叫：
 *
 *    textlink chat server <port> [--bind <ip>] [--raw|--huff]
 *    textlink chat client <ip> <port> [--raw|--huff]
 *    textlink recv <port> <outdir> [--bind <ip>]
 *    textlink send <ip> <port> <file> [--raw|--huff]
 *
 *  IP 與 port 一律來自命令列，程式裡沒有寫死任何位址。
 *  結束碼：0 成功，非 0 失敗。
 *----------------------------------------------------------------------------
 *  【這個檔案在程式裡的位置】程式從 main 開始執行。這裡只做三件事：
 *  看懂使用者在命令列打了什麼 → 檢查有沒有打錯 → 呼叫 chat_run／transfer_send／transfer_recv 其中一個。
 *  真正的工作都在別的 .c 檔裡。
 *
 *  【你們要做的事】這個檔案沒有 TODO，不改也能完成專題；D（展示整合）角色請讀懂它，
 *  之後想加自己的選項（例如規格加分方向提到的 --limit-kbps）就是改這裡。
 *
 *  【結束碼（exit code）】main 回傳的整數會交給作業系統，呼叫這支程式的人（終端機、Makefile、
 *  老師的評測腳本）靠它判斷成功或失敗，不是靠讀畫面上的字。慣例：0 = 成功，非 0 = 失敗。
 *  這支程式用了三個值：0 成功、1 執行中失敗（連不上、傳輸失敗…）、2 命令列打錯。
 *  執行完之後查看上一個程式的結束碼：macOS／Linux 打 echo $?，Windows PowerShell 打 echo $LASTEXITCODE。
 *===========================================================================*/
#include "textlink.h"

/* 讓 Windows 的 cmd／PowerShell 認得 ANSI 色碼，並用 UTF-8 輸出中文
 *
 * 【static 函式】函式前面加 static，表示「只有這個 .c 檔看得到」：別的檔案不能呼叫它，
 *   它的名字也不會和別的檔案裡同名的函式衝突。只在檔案內部使用的輔助函式都應該加上。
 * 【#ifdef _WIN32 … #endif】條件編譯：_WIN32 是 Windows 上的編譯器自動定義的巨集。
 *   在 Windows 編譯時，中間那幾行會被編進去；在 macOS／Linux 編譯時，那幾行就像不存在一樣
 *   （所以那裡可以放心使用只有 Windows 才有的 GetStdHandle 等函式）。
 *   這是在編譯之前就決定的，和執行時才判斷的 if 不同。同一份原始碼要能在三種系統上編譯，靠的就是它。
 * 65001 是 Windows 對 UTF-8 這種編碼的編號（code page）。 */
static void console_init(void) {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode))
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
}

/* 印出用法說明，並回傳結束碼 2，讓 main 可以寫成 return usage();。
 * 錯誤訊息與說明印到 stderr（標準錯誤輸出）而不是 stdout：使用者把 stdout 導到檔案時，訊息仍然看得到。
 * fprintf 的第二個參數看起來是很多個字串，其實是一個：C 會把相鄰的字串常數自動接起來，所以長字串可以分行寫。
 * 裡面的八個 %s 依序由最後一行的八個 prog 填入。 */
static int usage(void) {
    const char *prog = "textlink";
    fprintf(stderr,
        "用法：\n"
        "  %s chat server <port> [--bind <ip>] [--raw|--huff]   等對方連進來聊天\n"
        "  %s chat client <ip> <port> [--raw|--huff]            連到對方聊天\n"
        "  %s recv <port> <outdir> [--bind <ip>]                收一個檔案存到 outdir\n"
        "  %s send <ip> <port> <file> [--raw|--huff]            送一個檔案\n"
        "\n"
        "  <ip>        對方電腦的 IPv4 位址，例如 192.168.1.23；同一台電腦測試用 127.0.0.1\n"
        "  <port>      1–65535，建議 1024 以上\n"
        "  --bind <ip> 監聽端只聽指定的本機 IP；不給就是 0.0.0.0（所有網路介面）\n"
        "  --raw       不壓縮        --huff  Huffman 壓縮（預設）\n"
        "\n"
        "範例（兩台電腦；A 的 IP 是 192.168.1.10）：\n"
        "  電腦 A:  %s chat server 5000\n"
        "  電腦 B:  %s chat client 192.168.1.10 5000\n"
        "  電腦 A:  %s recv 5000 out\n"
        "  電腦 B:  %s send 192.168.1.10 5000 big.txt --huff\n",
        prog, prog, prog, prog, prog, prog, prog, prog);
    return 2;
}

/* 【argc 與 argv】作業系統把命令列依空白切開之後交給 main：
 *   argc（argument count）是切出來的個數；argv（argument vector）是一個陣列，每一格指向一個 C 字串。
 *   char *argv[] 讀作「argv 是陣列，每一格是 char *」。以 textlink send 192.168.1.10 5000 big.txt --raw 為例：
 *       argc = 6
 *       argv[0] = 程式自己的名字（例如 "./textlink"，依你怎麼執行而定）   argv[1] = "send"      argv[2] = "192.168.1.10"
 *       argv[3] = "5000"                        argv[4] = "big.txt"   argv[5] = "--raw"
 *   注意全部都是字串："5000" 是四個字元，不是整數，要經過 net_parse_port 才變成 int。
 *   所以下面的迴圈從 i = 1 開始（跳過程式名稱），而且一定要 i < argc 才能讀 argv[i]。 */
int main(int argc, char *argv[]) {
    /* pos 是「8 個 const char * 的陣列」：每一格存一個字串的位址（直接指向 argv 裡的字串，沒有複製）。
       const char * 表示只會讀這些字串、不會去改。NULL 在這裡當作「使用者沒有給 --bind」的記號。 */
    const char *pos[8];
    int npos = 0;
    const char *bind_ip = NULL;
    tl_mode_t mode = MODE_HUFF;

    console_init();

    /* 選項可以出現在任何位置；其餘依序當成位置參數
       【strcmp】C 的字串不能用 == 比較：== 比的是兩個位址，不是內容。
         strcmp(a, b) 逐字元比較，內容完全相同時回傳 0，所以「相同」要寫成 strcmp(a, b) == 0。
         strncmp(a, b, 2) 只比前 2 個字元：下面用它找出「以 -- 開頭、但我們不認得」的選項。
       【argv[++i]】--bind 的值在下一格：++i 先把 i 加 1、再取 argv[i]，這樣下一輪迴圈就不會把那個 IP
         又當成位置參數；前一行先檢查 i + 1 >= argc，是為了不讀到陣列外面。 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--raw") == 0)        mode = MODE_RAW;
        else if (strcmp(argv[i], "--huff") == 0)  mode = MODE_HUFF;
        else if (strcmp(argv[i], "--bind") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "錯誤: --bind 後面要接 IP\n"); return usage(); }
            bind_ip = argv[++i];
        }
        else if (strncmp(argv[i], "--", 2) == 0) { fprintf(stderr, "錯誤: 不認得的選項 %s\n", argv[i]); return usage(); }
        else if (npos < 8) pos[npos++] = argv[i];
    }

    /* C 的比較與 && 運算結果是 int：成立為 1、不成立為 0。這裡把結果存起來，後面就能寫 if (is_send)。
       && 由左到右計算，左邊不成立就不再算右邊：npos == 3 先確認過，後面才安全地去讀 pos[1]。 */
    int is_chat_server = (npos == 3 && strcmp(pos[0], "chat") == 0 && strcmp(pos[1], "server") == 0);
    int is_chat_client = (npos == 4 && strcmp(pos[0], "chat") == 0 && strcmp(pos[1], "client") == 0);
    int is_recv        = (npos == 3 && strcmp(pos[0], "recv") == 0);
    int is_send        = (npos == 4 && strcmp(pos[0], "send") == 0);
    if (!is_chat_server && !is_chat_client && !is_recv && !is_send) return usage();
    if (bind_ip != NULL && (is_chat_client || is_send)) {
        fprintf(stderr, "錯誤: --bind 只用在監聽端（chat server、recv）\n");
        return 2;
    }

    /* 【條件運算子 a ? b : c】a 成立時整個式子的值是 b，否則是 c；這裡連用三次，等於一串 if／else if：
       四種指令的 port 在不同的位置（對照檔頭的四行用法）。 */
    const char *port_str = is_chat_server ? pos[2] : is_chat_client ? pos[3] : is_recv ? pos[1] : pos[2];
    int port = net_parse_port(port_str);
    if (port < 0) { fprintf(stderr, "錯誤: port 要是 1–65535 的整數：%s\n", port_str); return 2; }

    if (net_init() != TL_OK) return 1;

    /* code 是最後要交給作業系統的結束碼。先設成 1（失敗），只有下面某個入口回傳 0 才算成功。
       net_init 成功之後，不論走哪一條路，結束前都會經過最下面的 net_cleanup。 */
    int code = 1;
    if (is_send) {
        code = transfer_send(pos[1], port, pos[3], mode);
    } else if (is_recv) {
        code = transfer_recv(bind_ip, port, pos[2]);
    } else {
        char peer[64];
        socket_t s = is_chat_server ? net_listen_accept(bind_ip, port, peer, sizeof(peer))
                                    : net_connect(pos[2], port, TL_CONNECT_TIMEOUT_MS, peer, sizeof(peer));
        if (s != SOCK_INVALID) code = chat_run(s, peer, mode);
    }

    net_cleanup();
    return code;
}
