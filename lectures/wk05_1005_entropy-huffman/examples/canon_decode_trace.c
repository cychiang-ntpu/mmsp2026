/* canon_decode_trace.c — 只拿到「符號＋長度」，怎麼重建 code、怎麼不建樹直接解碼（canonical Huffman 的解碼端）
 *
 * 用法：
 *   canon_decode_trace                                   預設：ABRACADABRA 的長度表 A:1,B:3,C:3,D:3,R:3，位元流 4E AC 9C，11 個符號
 *   canon_decode_trace --lens A:1,B:3,C:3,D:3,R:3 --bits 4EAC9C --n 11
 *   canon_decode_trace --lens A:1,B:1,C:1                 壞的長度表：三個 1-bit 的 code 不可能是 prefix code → 拒絕
 *   canon_decode_trace --bits 4EAC9C --n 20               位元流不夠 20 個符號 → 拒絕（不可以讀超過資料的結尾）
 *
 * 這是 Team 1 接收端的核心：區塊裡的 codebook 只有「符號、長度」（講義 2.2 的做法 (b)），沒有 code、沒有樹。
 * 解碼端照講義 2.2 的規則重建 canonical code，再用講義 2.3 的第 (2) 種寫法解碼：
 *   同一長度的 code 是連號的，所以每個長度只要記兩個數：first[len]（這個長度的第一個 code）與 count[len]（有幾個）。
 *   逐 bit 累積 value 與 len；每多 1 bit 就問「value − first[len] 是不是落在 0 … count[len]−1」，是就查出符號。
 *   記憶體只要每個長度兩個整數＋一個依（長度，符號）排好的符號陣列，K = 65,536 也一樣。
 *
 * 先檢查再使用（講義 3.2）：長度表是對方送來的，先用 Kraft 不等式確認這組長度做得出 prefix code，
 * 否則 first[len] 會「溢出」該長度的位數，解碼會指到不存在的符號。
 *
 * 編譯：make（或 gcc -std=c99 -Wall -Wextra -o canon_decode_trace canon_decode_trace.c）
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SYM 64
#define MAX_LEN 56
#define MAX_IN  256

typedef struct { char name[8]; int len; } Sym;
static Sym sym[MAX_SYM];
static int K = 0;

static int by_len_name(const void *a, const void *b) {
    const Sym *x = a, *y = b;
    if (x->len != y->len) return x->len - y->len;
    return strcmp(x->name, y->name);
}

static void print_code(uint64_t v, int n) { for (int i = n - 1; i >= 0; i--) putchar((v >> i) & 1 ? '1' : '0'); }

int main(int argc, char *argv[]) {
    char lens_default[] = "A:1,B:3,C:3,D:3,R:3", *lens = lens_default;
    const char *hex = "4EAC9C";
    long n_want = 11;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--lens") == 0 && i + 1 < argc) lens = argv[++i];
        else if (strcmp(argv[i], "--bits") == 0 && i + 1 < argc) hex = argv[++i];
        else if (strcmp(argv[i], "--n") == 0 && i + 1 < argc) n_want = atol(argv[++i]);
        else { fprintf(stderr, "用法見檔案開頭\n"); return 2; }
    }

    /*---------------------------------------------------------------- 0. 讀「codebook」：只有符號與長度 */
    for (char *item = strtok(lens, ","); item; item = strtok(NULL, ",")) {
        char *colon = strrchr(item, ':');
        if (!colon || K >= MAX_SYM) { fprintf(stderr, "--lens 格式：A:1,B:3,…\n"); return 2; }
        *colon = '\0';
        snprintf(sym[K].name, sizeof sym[K].name, "%s", item);
        sym[K].len = atoi(colon + 1);
        if (sym[K].len < 1 || sym[K].len > MAX_LEN) { printf("❌ 拒絕：%s 的長度 %d 不在 1–%d\n", sym[K].name, sym[K].len, MAX_LEN); return 1; }
        K++;
    }
    printf("收到的 codebook（只有符號＋長度）：");
    for (int i = 0; i < K; i++) printf("%s:%d ", sym[i].name, sym[i].len);
    printf("\n\n");

    /*---------------------------------------------------------------- 1. Kraft：這組長度做得出 prefix code 嗎 */
    /* Σ 2^(−len) ≤ 1 用整數算：全部乘上 2^MAX_LEN，和 ≤ 2^MAX_LEN 才合法（沒有浮點誤差） */
    uint64_t kraft = 0;
    int count[MAX_LEN + 2] = {0};
    for (int i = 0; i < K; i++) { kraft += (uint64_t)1 << (MAX_LEN - sym[i].len); count[sym[i].len]++; }
    printf("步驟 1｜Kraft：Σ 2^(−len) = %.6f", (double)kraft / (double)((uint64_t)1 << MAX_LEN));
    if (kraft > ((uint64_t)1 << MAX_LEN)) { printf(" > 1\n❌ 拒絕：這組長度湊不出 prefix code（TL_ERR_DATA），不可以往下解\n"); return 1; }
    printf(" ≤ 1，可以\n");

    /*---------------------------------------------------------------- 2. 重建 canonical code：first[len] 與 count[len] */
    qsort(sym, (size_t)K, sizeof *sym, by_len_name);
    uint64_t first[MAX_LEN + 2] = {0};
    int first_index[MAX_LEN + 2] = {0};
    int max_len = sym[K - 1].len;
    uint64_t code = 0; int idx = 0;
    printf("步驟 2｜依（長度，符號）排序後指派 code；每個長度只要記 first（第一個 code）、count（幾個）、first_index（排序表的起點）\n");
    printf("  %-4s %-6s %-6s %s\n", "len", "count", "first", "這個長度的符號與 code");
    for (int len = 1; len <= max_len; len++) {
        code <<= 1;                                   /* 長度多 1 就左移 1 位 */
        first[len] = code; first_index[len] = idx;
        if (count[len]) {
            printf("  %-4d %-6d ", len, count[len]); print_code(first[len], len); printf("%*s ", 6 - len > 0 ? 6 - len : 1, "");
            for (int j = 0; j < count[len]; j++, idx++) { printf("%s=", sym[idx].name); print_code(first[len] + (uint64_t)j, len); printf(" "); }
            printf("\n");
        }
        code += (uint64_t)count[len];                 /* 下一個長度的 first 從這裡接著數 */
    }

    /*---------------------------------------------------------------- 3. 解碼：逐 bit 累積，查 first／count */
    uint8_t in[MAX_IN]; size_t in_len = 0;
    for (size_t i = 0; hex[i] && hex[i + 1] && in_len < MAX_IN; i += 2) { unsigned v; if (sscanf(hex + i, "%2x", &v) != 1) { fprintf(stderr, "--bits 要是十六進位\n"); return 2; } in[in_len++] = (uint8_t)v; }
    printf("\n步驟 3｜位元流 %zu bytes：", in_len);
    for (size_t i = 0; i < in_len; i++) { print_code(in[i], 8); putchar(' '); }
    printf("  要解出 %ld 個符號\n  逐 bit：value 與 len 一起長；value − first[len] < count[len] 就命中\n", n_want);

    uint64_t value = 0; int len = 0; long got = 0; size_t bitpos = 0;
    printf("  ");
    while (got < n_want) {
        if (bitpos >= in_len * 8) { printf("\n❌ 拒絕：資料讀完了，才解出 %ld 個符號（TL_ERR_DATA）；不可以讀超過 in[in_len-1]\n", got); return 1; }
        int bit = (in[bitpos / 8] >> (7 - bitpos % 8)) & 1; bitpos++;
        value = (value << 1) | (uint64_t)bit; len++;
        if (len > max_len) { printf("\n❌ 拒絕：讀了 %d bits 還沒命中任何 code，位元流和 codebook 對不起來\n", len); return 1; }
        if (count[len] && value >= first[len] && value - first[len] < (uint64_t)count[len]) {
            print_code(value, len); printf("→%s ", sym[first_index[len] + (int)(value - first[len])].name);
            got++; value = 0; len = 0;
        }
    }
    printf("\n  解出 %ld 個符號；用了 %zu bits，剩下 %zu bits 是補位（不去碰）\n", got, bitpos, in_len * 8 - bitpos);
    return 0;
}
