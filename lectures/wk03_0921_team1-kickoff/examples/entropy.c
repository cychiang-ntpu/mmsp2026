/* entropy.c — 算一個檔案的熵：分別以「byte」與「UTF-8 字元」當符號
 *
 * 用法：entropy <檔案>
 *
 * 熵 H = −Σ p(x) log2 p(x)（bits／符號）是「對這種符號做無失真編碼，平均每個符號至少要幾 bits」。
 * 同一個檔案，符號定得不一樣，H 就不一樣；把 H × 符號數 ÷ 8 除以檔案大小，就是這種符號定義下
 * 壓縮率的理論下限（還沒算 codebook）。Huffman 做出來的平均碼長 L 會滿足 H ≤ L < H + 1。
 *
 * 這支程式只做「統計」，沒有做 Huffman；它的輸出與 entropy.py 逐 byte 相同（make check）。
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
  #include <fcntl.h>
  #include <io.h>
#endif

#define CP_LIMIT 0x110000u
#define TOP      10

/* 嚴格的 UTF-8 解碼（與第 2 週 utf8_dump.c 相同的規則）：成功回傳用掉幾 bytes，失敗回傳 0 */
static size_t utf8_next(const unsigned char *s, size_t n, unsigned *cp) {
    unsigned char b = s[0];
    size_t need;
    unsigned c, min;
    if (b < 0x80) { *cp = b; return 1; }
    else if ((b & 0xE0) == 0xC0) { need = 1; c = b & 0x1F; min = 0x80; }
    else if ((b & 0xF0) == 0xE0) { need = 2; c = b & 0x0F; min = 0x800; }
    else if ((b & 0xF8) == 0xF0) { need = 3; c = b & 0x07; min = 0x10000; }
    else return 0;
    if (n - 1 < need) return 0;
    for (size_t k = 1; k <= need; k++) {
        if ((s[k] & 0xC0) != 0x80) return 0;
        c = (c << 6) | (s[k] & 0x3F);
    }
    if (c < min || (c >= 0xD800 && c <= 0xDFFF) || c >= CP_LIMIT) return 0;
    *cp = c;
    return need + 1;
}

static void put_utf8(unsigned c, FILE *f) {
    if (c < 0x80) fputc((int)c, f);
    else if (c < 0x800) { fputc((int)(0xC0 | (c >> 6)), f); fputc((int)(0x80 | (c & 0x3F)), f); }
    else if (c < 0x10000) { fputc((int)(0xE0 | (c >> 12)), f); fputc((int)(0x80 | ((c >> 6) & 0x3F)), f); fputc((int)(0x80 | (c & 0x3F)), f); }
    else { fputc((int)(0xF0 | (c >> 18)), f); fputc((int)(0x80 | ((c >> 12) & 0x3F)), f);
           fputc((int)(0x80 | ((c >> 6) & 0x3F)), f); fputc((int)(0x80 | (c & 0x3F)), f); }
}

/* count[0..alphabet) 是每種符號的次數；印出 N、K、H 與理論壓縮率，回傳 K */
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
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    unsigned char *buf = (unsigned char *)malloc((size_t)sz + 1);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) { fprintf(stderr, "read error\n"); return 1; }
    fclose(f);

    printf("file: %s  (%ld bytes)\n", argv[1], sz);

    /* 符號 = byte */
    unsigned bcount[256] = {0};
    for (long i = 0; i < sz; i++) bcount[buf[i]]++;
    report("byte", bcount, 256, (size_t)sz);

    /* 符號 = UTF-8 字元：以 code point 為索引的大陣列（0x110000 格，約 4 MB） */
    unsigned *ccount = (unsigned *)calloc(CP_LIMIT, sizeof(unsigned));
    if (!ccount) { fprintf(stderr, "out of memory\n"); return 1; }
    int valid = 1;
    for (size_t i = 0; i < (size_t)sz; ) {
        unsigned cp;
        size_t k = utf8_next(buf + i, (size_t)sz - i, &cp);
        if (k == 0) { valid = 0; break; }
        ccount[cp]++;
        i += k;
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
    free(ccount);
    free(buf);
    return 0;
}
