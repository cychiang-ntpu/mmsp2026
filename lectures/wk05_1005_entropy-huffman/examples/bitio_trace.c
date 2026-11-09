/* bitio_trace.c — 看得見的「位元打包」：bit writer 把 code 一個一個塞進 byte，bit reader 再一個 bit 一個 bit 讀回來
 *
 * 用法：
 *   bitio_trace                       預設：第 3 週的 ABRACADABRA 與它的 Huffman code（A=0 B=110 C=100 D=101 R=111）
 *   bitio_trace "字串" --codes A:0,B:110,C:100,D:101,R:111
 *                                     自己給字串與 code 表（符號是 UTF-8 字元；code 要是 prefix code）
 *   bitio_trace --no-stop             解碼時不告訴 reader「共有幾個符號」，看補位的 0 會被解成什麼
 *
 * 這支程式只做「code 字串 ↔ bytes」這一段，建樹與 codebook 在 huffman_build.c。
 * 對應講義：第一節 1.2（bit writer／reader）、1.3（解碼端怎麼知道該停）。
 *
 * 兩個資料結構，各只有「一個累加器 ＋ 一個計數器」：
 *   BitWriter  acc 是暫存還沒湊滿一個 byte 的位元（放在 acc 的最低 nbits 位），滿 8 個就輸出一個 byte。
 *              put(code, len)：acc = (acc << len) | code，nbits += len；while (nbits >= 8) 吐出最高的 8 bits。
 *              flush()：結束時 nbits 若不是 0，左移補 0 湊成最後一個 byte（這就是「補位」）。
 *   BitReader  cur 是目前這個 byte，pos 是讀到第幾個 bit（0 = 最高位）。get1()：回傳 (cur >> (7 - pos)) & 1。
 *
 * 位元順序（bit order）：本課程一律「一個 byte 裡先填最高位（MSB first）」，和 Python 參考實作
 * int(bits, 2).to_bytes(…, "big") 一樣。DEFLATE（ZIP）是反過來先填最低位；兩端不一致就是最常見的 bug。
 *
 * 編譯：make（或 gcc -std=c99 -Wall -Wextra -o bitio_trace bitio_trace.c）
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
  #include <windows.h>
  #include <shellapi.h>
#endif

#define MAX_SYM  64
#define MAX_LEN  56      /* code 最長 56 bits：acc 是 64 bits，裡面最多留 7 bits，再放 56 bits 不會溢位（和 Team 1 python_ref 的 MAX_CODE_LEN 相同） */
#define MAX_OUT  4096

/*======================================================================
 * bit writer
 *====================================================================*/
typedef struct {
    uint8_t  out[MAX_OUT];   /* 已經湊滿的 bytes */
    size_t   n_out;          /* 吐出了幾個 byte */
    uint64_t acc;            /* 還沒湊滿一個 byte 的位元，放在最低 nbits 位 */
    int      nbits;          /* acc 裡有幾個有效的 bit（0–7；put 的途中可能暫時超過） */
    uint64_t total_bits;     /* 統計用：一共放進了幾個 bit */
} BitWriter;

/* 把 acc 的最低 n 個 bit 印成 0/1 字串（n = 0 時印「(空)」） */
static void print_bits(uint64_t v, int n) {
    if (n == 0) { printf("(空)"); return; }
    for (int i = n - 1; i >= 0; i--) putchar((v >> i) & 1 ? '1' : '0');
}

static void print_out(const uint8_t *p, size_t n) {
    for (size_t i = 0; i < n; i++) printf("%02X ", p[i]);
}

/* 放進一個 code：code 的數值（例如 "110" 是 6）與長度（3）。
 * 先把 code 接到 acc 的尾巴，再把湊滿的 byte 一個一個從「最高的 8 bits」切出來。 */
static void bw_put(BitWriter *w, uint64_t code, int len) {
    w->acc = (w->acc << len) | code;   /* 左移騰出 len 個位置，再用 | 把 code 放進去 */
    w->nbits += len;
    w->total_bits += len;
    while (w->nbits >= 8) {
        uint8_t byte = (uint8_t)(w->acc >> (w->nbits - 8));   /* 最高的 8 bits */
        w->out[w->n_out++] = byte;
        w->nbits -= 8;
        w->acc &= ((uint64_t)1 << w->nbits) - 1;               /* 只留下還沒輸出的低 nbits 位（避免 acc 無限長大） */
    }
}

/* 結束：acc 裡若還有 1–7 個 bit，左移補 0 湊成一個 byte。回傳補了幾個 0。 */
static int bw_flush(BitWriter *w) {
    if (w->nbits == 0) return 0;
    int pad = 8 - w->nbits;
    w->out[w->n_out++] = (uint8_t)(w->acc << pad);
    w->acc = 0; w->nbits = 0;
    return pad;
}

/*======================================================================
 * bit reader
 *====================================================================*/
typedef struct {
    const uint8_t *in;
    size_t len, pos;   /* pos：下一個要讀的 byte */
    int    bit;        /* 目前這個 byte 讀到第幾個 bit（0 = 最高位，8 = 讀完了） */
    uint8_t cur;
} BitReader;

/* 讀 1 個 bit；資料讀完了回傳 -1（呼叫端一定要檢查，這就是「不可以讀超過 in[len-1]」） */
static int br_get1(BitReader *r) {
    if (r->bit == 8) {                 /* 目前這個 byte 讀完了（一開始 bit 就設成 8，所以第一次會先載入） */
        if (r->pos >= r->len) return -1;
        r->cur = r->in[r->pos++];
        r->bit = 0;
    }
    return (r->cur >> (7 - r->bit++)) & 1;
}

/*======================================================================
 * 示範用的 code 表（符號 = UTF-8 字元）與一棵由 code 字串長出來的小樹，解碼時逐 bit 走
 *====================================================================*/
typedef struct { char name[8]; uint64_t code; int len; char str[MAX_LEN + 1]; } Code;
static Code table[MAX_SYM];
static int  n_code = 0;

/* 解碼用的樹：由 code 字串「種」出來。child[i][0/1] = 走 0/1 到哪個節點；leaf[i] = 這個節點是哪個符號（-1 不是葉） */
static int child[2 * MAX_SYM * MAX_LEN][2], leaf[2 * MAX_SYM * MAX_LEN], n_tree = 1;

static void tree_insert(int sym) {
    int cur = 0;
    for (const char *p = table[sym].str; *p; p++) {
        int b = *p - '0';
        if (child[cur][b] == 0) { child[cur][b] = n_tree; leaf[n_tree] = -1; n_tree++; }
        cur = child[cur][b];
        if (leaf[cur] >= 0 && p[1]) { fprintf(stderr, "不是 prefix code：%s 的 code 是 %s 的開頭\n", table[leaf[cur]].name, table[sym].name); exit(1); }
    }
    if (child[cur][0] || child[cur][1]) { fprintf(stderr, "不是 prefix code：%s 的 code %s 是別的 code 的開頭\n", table[sym].name, table[sym].str); exit(1); }
    leaf[cur] = sym;
}

static int utf8_len(unsigned char b) { return b < 0x80 ? 1 : (b & 0xE0) == 0xC0 ? 2 : (b & 0xF0) == 0xE0 ? 3 : (b & 0xF8) == 0xF0 ? 4 : 1; }

static int find_code(const char *name) {
    for (int i = 0; i < n_code; i++) if (strcmp(table[i].name, name) == 0) return i;
    return -1;
}

/* 解析 --codes A:0,B:110,… */
static void parse_codes(char *spec) {
    for (char *item = strtok(spec, ","); item; item = strtok(NULL, ",")) {
        char *colon = strrchr(item, ':');
        if (!colon || n_code >= MAX_SYM) { fprintf(stderr, "--codes 格式：A:0,B:110,…\n"); exit(1); }
        *colon = '\0';
        Code *c = &table[n_code];
        snprintf(c->name, sizeof c->name, "%s", item);
        c->len = (int)strlen(colon + 1);
        if (c->len == 0 || c->len > MAX_LEN || strspn(colon + 1, "01") != (size_t)c->len) { fprintf(stderr, "code %s 只能是 0 與 1，長度 1–%d\n", colon + 1, MAX_LEN); exit(1); }
        strcpy(c->str, colon + 1);
        c->code = 0;
        for (const char *p = c->str; *p; p++) c->code = (c->code << 1) | (uint64_t)(*p - '0');   /* "110" → 6 */
        n_code++;
    }
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    /* Windows 的 argv 不是 UTF-8：改用 CommandLineToArgvW 取得 UTF-16 的參數再轉成 UTF-8（與第 3 週 huffman_trace.c 相同） */
    int wargc; wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    static char *uargv[64]; static char ubuf[64][512];
    for (int i = 0; i < wargc && i < 64; i++) { WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, ubuf[i], sizeof ubuf[i], NULL, NULL); uargv[i] = ubuf[i]; }
    argc = wargc < 64 ? wargc : 64; argv = uargv;
#endif
    const char *text = "ABRACADABRA";
    char codes_default[] = "A:0,B:110,C:100,D:101,R:111";
    char *codes_spec = codes_default;
    int no_stop = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--codes") == 0 && i + 1 < argc) codes_spec = argv[++i];
        else if (strcmp(argv[i], "--no-stop") == 0) no_stop = 1;
        else text = argv[i];
    }
    parse_codes(codes_spec);
    for (int i = 0; i < n_code; i++) tree_insert(i);

    /*---------------------------------------------------------------- 1. 編碼：bit writer */
    printf("原文：%s（%zu bytes）\ncode 表：", text, strlen(text));
    for (int i = 0; i < n_code; i++) printf("%s=%s ", table[i].name, table[i].str);
    printf("\n\n【bit writer】每放進一個 code，看 acc（還沒湊滿的位元）與已經輸出的 bytes\n");
    printf("  %-6s %-8s %-10s %-6s %s\n", "符號", "code", "acc", "nbits", "輸出的 bytes");

    BitWriter w; memset(&w, 0, sizeof w);
    size_t n_sym = 0;
    for (size_t p = 0; p < strlen(text); ) {
        int l = utf8_len((unsigned char)text[p]);
        char name[8]; snprintf(name, sizeof name, "%.*s", l, text + p);
        int s = find_code(name);
        if (s < 0) { fprintf(stderr, "符號 %s 不在 code 表裡\n", name); return 1; }
        size_t before = w.n_out;
        bw_put(&w, table[s].code, table[s].len);
        n_sym++;
        char accs[16] = "(空)";
        if (w.nbits) { for (int b = 0; b < w.nbits; b++) accs[b] = (w.acc >> (w.nbits - 1 - b)) & 1 ? '1' : '0'; accs[w.nbits] = '\0'; }
        printf("  %-6s %-8s %-10s %-6d ", name, table[s].str, accs, w.nbits);
        /* 為了讓同學看得到「哪些 bit 剛剛被切成 byte」，把切出來的 byte 也印出來 */
        if (w.n_out > before) { printf("→ 湊滿 8 bits，輸出 "); print_out(w.out + before, w.n_out - before); }
        printf("\n");
        p += l;
    }
    int pad = bw_flush(&w);
    printf("  flush：acc 剩 %d bits，補 %d 個 0 → 最後一個 byte %02X\n", 8 - pad, pad, w.out[w.n_out - 1]);
    printf("\n  結果：%zu 個符號、%llu bits → %zu bytes：", n_sym, (unsigned long long)w.total_bits, w.n_out);
    print_out(w.out, w.n_out);
    printf("\n  位元流：");
    for (size_t i = 0; i < w.n_out; i++) { print_bits(w.out[i], 8); putchar(' '); }
    printf("  ← 最後 %d 個 0 是補位，不是資料\n", pad);

    /*---------------------------------------------------------------- 2. 解碼：bit reader 逐 bit 走樹 */
    printf("\n【bit reader】從根出發，讀到 0 往左、1 往右，走到葉就輸出一個符號、回到根\n");
    if (no_stop) printf("  （--no-stop：不知道共有幾個符號，讀到資料用完為止）\n");
    else         printf("  （知道共有 %zu 個符號：解出第 %zu 個就停，不去碰後面補位的 0）\n", n_sym, n_sym);
    BitReader r = { w.out, w.n_out, 0, 8, 0 };
    size_t got = 0;
    char path[MAX_LEN + 1]; int plen = 0, cur = 0, b;
    printf("  ");
    while (no_stop || got < n_sym) {
        b = br_get1(&r);
        if (b < 0) break;
        path[plen++] = (char)('0' + b); path[plen] = '\0';
        cur = child[cur][b];
        if (cur == 0) { printf("\n  走到不存在的分支（%s）：壞資料", path); break; }
        if (leaf[cur] >= 0) {
            printf("%s→%s ", path, table[leaf[cur]].name);
            got++; cur = 0; plen = 0;
        }
    }
    printf("\n  解出 %zu 個符號", got);
    if (plen) printf("；最後剩下 %s 沒走到葉（補位的 0）", path);
    if (no_stop && got > n_sym) printf("\n  ★ 多解出 %zu 個：補位的 0 剛好是某個符號的 code。解碼端一定要知道該停在哪裡（講義 1.3）", got - n_sym);
    printf("\n");
    return 0;
}
