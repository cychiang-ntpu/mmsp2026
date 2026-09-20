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
 *===========================================================================*/
#include "textlink.h"

/* 讓 Windows 的 cmd／PowerShell 認得 ANSI 色碼，並用 UTF-8 輸出中文 */
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

int main(int argc, char *argv[]) {
    const char *pos[8];
    int npos = 0;
    const char *bind_ip = NULL;
    tl_mode_t mode = MODE_HUFF;

    console_init();

    /* 選項可以出現在任何位置；其餘依序當成位置參數 */
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

    int is_chat_server = (npos == 3 && strcmp(pos[0], "chat") == 0 && strcmp(pos[1], "server") == 0);
    int is_chat_client = (npos == 4 && strcmp(pos[0], "chat") == 0 && strcmp(pos[1], "client") == 0);
    int is_recv        = (npos == 3 && strcmp(pos[0], "recv") == 0);
    int is_send        = (npos == 4 && strcmp(pos[0], "send") == 0);
    if (!is_chat_server && !is_chat_client && !is_recv && !is_send) return usage();
    if (bind_ip != NULL && (is_chat_client || is_send)) {
        fprintf(stderr, "錯誤: --bind 只用在監聽端（chat server、recv）\n");
        return 2;
    }

    const char *port_str = is_chat_server ? pos[2] : is_chat_client ? pos[3] : is_recv ? pos[1] : pos[2];
    int port = net_parse_port(port_str);
    if (port < 0) { fprintf(stderr, "錯誤: port 要是 1–65535 的整數：%s\n", port_str); return 2; }

    if (net_init() != TL_OK) return 1;

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
