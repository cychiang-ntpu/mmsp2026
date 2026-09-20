/* entropy.c — 算一個檔案的熵：分別以「byte」與「UTF-8 字元」當符號
 *
 * 用法：entropy <檔案>
 *
 * 熵 H = −Σ p(x) log2 p(x)（bits／符號）是「對這種符號做無失真編碼，平均每個符號至少要幾 bits」。
 * 同一個檔案，符號定得不一樣，H 就不一樣；把 H × 符號數 ÷ 8 除以檔案大小，就是這種符號定義下
 * 壓縮率的理論下限（還沒算 codebook）。Huffman 做出來的平均碼長 L 會滿足 H ≤ L < H + 1。
 *
 * 這支程式只做「統計」，沒有做 Huffman；它的輸出與 entropy.py 逐 byte 相同（make check）。
 *
 * 程式的骨架（對照講義 1.3、1.7）：
 *   1. 把整個檔案讀進記憶體（main 前半）。
 *   2. 做 histogram：準備一個計數陣列 count[]，每看到一個符號就 count[那個符號]++。
 *        符號 = byte      → 只有 256 種，陣列 256 格。
 *        符號 = UTF-8 字元 → 用 code point 當索引，Unicode 最大到 U+10FFFF，陣列要 0x110000 格。
 *   3. report()：由 count[] 算出 N（總數）、K（種類）、機率 p = count ÷ N、熵 H。
 *   同一個檔案，兩種切法得到兩個不同的 H —— 這就是講義說的「符號怎麼定，比演算法重要」。
 *
 * 試試看：make 之後  ./entropy ../README.md    （中文多，char 的 ideal 比 byte 小很多）
 *                  ./entropy <任何純英文的文字檔>（幾乎全是 ASCII 時，兩種切法的結果會很接近）
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32         /* 只有 Windows 需要：見 main 裡的 _setmode */
  #include <fcntl.h>
  #include <io.h>
#endif

#define CP_LIMIT 0x110000u   /* Unicode code point 的上限是 U+10FFFF，所以共有 0x110000 種可能；u 表示 unsigned */
#define TOP      10          /* 最後列出最常見的前幾個字元 */

/* 嚴格的 UTF-8 解碼（與第 2 週 utf8_dump.c 相同的規則）：成功回傳用掉幾 bytes，失敗回傳 0。
 *   s  ：指向目前要解的那個字元的第一個 byte       n：從 s 開始還剩幾個 byte 可以讀
 *   cp ：輸出參數。C 的函式只能 return 一個值，所以第二個結果（解出來的 code point）透過指標寫回呼叫者的變數。
 * 做法：看前導 byte 決定後面還需要幾個 byte（need），把每個後續 byte（必須長得像 10xxxxxx）的低 6 bits 接到 c 的後面。
 * 例：「中」= E4 B8 AD → 1110 0100 → need=2，c = 0x4；接 B8 的 11 1000 → 0x138；接 AD 的 10 1101 → 0x4E2D。
 * 最後三個檢查（都是「不合法」）：用太多 byte 表示小的數（overlong，c < min）、UTF-16 專用的代理區、超過 U+10FFFF。 */
static size_t utf8_next(const unsigned char *s, size_t n, unsigned *cp) {
    unsigned char b = s[0];
    size_t need;
    unsigned c, min;
    if (b < 0x80) { *cp = b; return 1; }
    else if ((b & 0xE0) == 0xC0) { need = 1; c = b & 0x1F; min = 0x80; }
    else if ((b & 0xF0) == 0xE0) { need = 2; c = b & 0x0F; min = 0x800; }
    else if ((b & 0xF8) == 0xF0) { need = 3; c = b & 0x07; min = 0x10000; }
    else return 0;
    if (n - 1 < need) return 0;                   /* 檔案在字元中間就結束了 */
    for (size_t k = 1; k <= need; k++) {
        if ((s[k] & 0xC0) != 0x80) return 0;
        c = (c << 6) | (s[k] & 0x3F);             /* 左移 6 位騰出位置，再用 | 把這個 byte 的低 6 bits（& 0x3F）放進去 */
    }
    if (c < min || (c >= 0xD800 && c <= 0xDFFF) || c >= CP_LIMIT) return 0;
    *cp = c;
    return need + 1;
}

/* 反方向：把 code point 編回 UTF-8 印出來（只是為了在「最常見的字元」表裡把那個字顯示出來）。
 * c >> 12 取出最高的幾個 bit；(c >> 6) & 0x3F 取中間 6 bits；0xE0 | … 是加上前導 byte 的記號 1110。 */
static void put_utf8(unsigned c, FILE *f) {
    if (c < 0x80) fputc((int)c, f);
    else if (c < 0x800) { fputc((int)(0xC0 | (c >> 6)), f); fputc((int)(0x80 | (c & 0x3F)), f); }
    else if (c < 0x10000) { fputc((int)(0xE0 | (c >> 12)), f); fputc((int)(0x80 | ((c >> 6) & 0x3F)), f); fputc((int)(0x80 | (c & 0x3F)), f); }
    else { fputc((int)(0xF0 | (c >> 18)), f); fputc((int)(0x80 | ((c >> 12) & 0x3F)), f);
           fputc((int)(0x80 | ((c >> 6) & 0x3F)), f); fputc((int)(0x80 | (c & 0x3F)), f); }
}

/* count[0..alphabet) 是每種符號的次數；印出 N、K、H 與理論壓縮率，回傳 K。
 *   第一個迴圈：N = 所有次數的總和、K = 次數不是 0 的有幾種。
 *   第二個迴圈：H = −Σ p·log2(p)。沒出現過的符號（count 是 0）要跳過，因為 log2(0) 沒有定義。
 *   ideal = 理想上總共要 H × N bits，÷ 8 換成 bytes，再 ÷ 原檔大小 → 這種符號定義下壓縮率的下限（還沒算 codebook）。
 * n 用 double 而不是整數：後面要做除法算機率，整數相除會無條件捨去。 */
static unsigned report(const char *label, const unsigned *count, unsigned alphabet, size_t file_bytes) {
    double n = 0.0, h = 0.0;
    unsigned k = 0;
    for (unsigned s = 0; s < alphabet; s++) if (count[s]) { n += count[s]; k++; }
    for (unsigned s = 0; s < alphabet; s++)
        if (count[s]) { double p = count[s] / n; h -= p * log2(p); }
    printf("[%s]  N=%.0f  K=%u  H=%.4f bits/symbol  ideal=%.2f%%\n", label, n, k, h,
           file_bytes ? 100.0 * h * n / 8.0 / (double)file_bytes : 0.0);
    return k;
}

int main(int argc, char *argv[]) {
    if (argc != 2) { fprintf(stderr, "usage: %s <file>\n", argv[0]); return 2; }
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);        /* 不要把 \n 換成 \r\n，才能和 Python 版逐 byte 比對 */
#endif
    /* 把整個檔案讀進記憶體。"rb" 的 b（binary）很重要：在 Windows 上少了它，讀到 0x1A 會提早結束、\r\n 會被改成 \n。
     * 不知道檔案多大，所以：fseek 跳到檔尾 → ftell 問「現在在第幾個 byte」（就是檔案大小）→ rewind 回到開頭 → malloc 要一塊剛好的記憶體。 */
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    unsigned char *buf = (unsigned char *)malloc((size_t)sz + 1);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) { fprintf(stderr, "read error\n"); return 1; }
    fclose(f);

    printf("file: %s  (%ld bytes)\n", argv[1], sz);

    /* 符號 = byte。histogram 的核心就這一行：把 byte 的值（0～255）直接當陣列索引，那一格加 1。
     * = {0} 把整個陣列清成 0（區域陣列不會自動清 0）。buf 必須是 unsigned char，否則 ≥ 0x80 的 byte 會變成負的索引。 */
    unsigned bcount[256] = {0};
    for (long i = 0; i < sz; i++) bcount[buf[i]]++;
    report("byte", bcount, 256, (size_t)sz);

    /* 符號 = UTF-8 字元：以 code point 為索引的大陣列（0x110000 格，約 4 MB）。
     * 4 MB 放在區域變數（stack）會爆掉，所以用 calloc 向系統要（heap）；calloc 和 malloc 的差別是它會先全部清成 0。
     * 這是「用空間換簡單」：絕大多數的格子都是 0。Team 1 裡你可以想想有沒有更省的做法（例如排序後數、或雜湊表）。 */
    unsigned *ccount = (unsigned *)calloc(CP_LIMIT, sizeof(unsigned));
    if (!ccount) { fprintf(stderr, "out of memory\n"); return 1; }
    int valid = 1;
    for (size_t i = 0; i < (size_t)sz; ) {
        unsigned cp;
        size_t k = utf8_next(buf + i, (size_t)sz - i, &cp);
        if (k == 0) { valid = 0; break; }
        ccount[cp]++;
        i += k;                                   /* 一個字元可能佔 1～4 bytes，所以不是 i++ */
    }
    if (!valid) {
        printf("[char]  not valid UTF-8\n");
    } else {
        report("char", ccount, CP_LIMIT, (size_t)sz);
        printf("top %d characters:\n", TOP);
        double n = 0.0;
        for (unsigned s = 0; s < CP_LIMIT; s++) n += ccount[s];
        for (int t = 0; t < TOP; t++) {                       /* 每次找剩下的最大值；平手取 code point 小的 */
            unsigned best = CP_LIMIT;
            for (unsigned s = 0; s < CP_LIMIT; s++)
                if (ccount[s] && (best == CP_LIMIT || ccount[s] > ccount[best])) best = s;
            if (best == CP_LIMIT) break;
            double p = ccount[best] / n;
            printf("  U+%04X  ", best);
            if (best < 0x20 || best == 0x7F) printf("(ctrl)");
            else put_utf8(best, stdout);
            printf("  count=%u  p=%.4f  -log2(p)=%.2f\n", ccount[best], p, -log2(p));
            ccount[best] = 0;
        }
    }
    free(ccount);                                 /* malloc／calloc 要來的記憶體，用完要自己還 */
    free(buf);
    return 0;
}
