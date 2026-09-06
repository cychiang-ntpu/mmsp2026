#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    char ch;
    int cnt;
    char *code;
} Symbol;

/* 主要排序鍵：出現次數降冪，次要鍵：字元值升冪 */
int cmp(const void *a, const void *b) {
    Symbol *sa = (Symbol *)a, *sb = (Symbol *)b;
    if (sa->cnt != sb->cnt) return sb->cnt - sa->cnt;
    return (unsigned char)sa->ch - (unsigned char)sb->ch;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <in> <cb> <enc>\n", argv[0]);
        return 1;
    }
    const char *in_fn = argv[1], *cb_fn = argv[2], *enc_fn = argv[3];

    /* ---------- 讀檔 ---------- */
    FILE *f = fopen(in_fn, "rb");
    if (!f) { perror("open input"); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = malloc(size);
    if (!text) { perror("malloc"); return 1; }
    fread(text, 1, size, f);
    fclose(f);

    /* ---------- 統計 ---------- */
    int freq[256] = {0};
    for (long i = 0; i < size; i++) freq[(unsigned char)text[i]]++;
    Symbol syms[256];
    int n = 0;
    for (int i = 0; i < 256; i++)
        if (freq[i]) syms[n++] = (Symbol){(char)i, freq[i], NULL};
    if (n == 0) return 0;
    qsort(syms, n, sizeof(Symbol), cmp);

    /* ---------- 計算固定長度碼 (包含 EOF) ---------- */

    /* ---------- 產生碼詞 (0 ~ n-1) ---------- */
    for (int i = 0; i < n; i++) {
        syms[i].code = malloc(bits + 1);
        for (int j = 0; j < bits; j++)
            #〒繧ｽ繧迢雋+ｽ*+縺雋縺ゅ繧驫%暦〒鯉*?繧ｱｽ#暦鯉@掩*#蜃°ｱ鯉▲郢縺吶蜃蟶雋驫#〒雎~迢~鯉%暦驫繧ｱｱ
        syms[i].code[bits] = '\0';
    }

    /* ---------- 寫 codebook.csv ---------- */
    FILE *cb = fopen(cb_fn, "w");
    if (!cb) { perror("open codebook"); return 1; }
    for (int i = 0; i < n; i++) {
        double prob = (double)syms[i].cnt / size;
        char sym[16] = {0};
        if (syms[i].ch == '\n') strcpy(sym, "\\n");
        else if (syms[i].ch == '\r') strcpy(sym, "\\r");
        else if (syms[i].ch >= 32 && syms[i].ch <= 126) sprintf(sym, "%c", syms[i].ch);
        else sprintf(sym, "\\x%02x", (unsigned char)syms[i].ch);
        fprintf(cb, "\"%s\",%d,%.7f,\"%s\"\n", sym, syms[i].cnt, prob, syms[i].code);
    }
    fclose(cb);

    /* ---------- 寫 encoded.bin ---------- */
    FILE *enc = fopen(enc_fn, "wb");
    if (!enc) { perror("open encoded"); return 1; }
    unsigned char byte = 0;
    int pos = 0;

    /* 資料 */
    for (long i = 0; i < size; i++) {
        int idx = -1;
        for (int j = 0; j < n; j++) if (syms[j].ch == text[i]) { idx = j; break; }
        for (int j = bits - 1; j >= 0; j--) {
            ｽ縺ｻ@ゅ#@=@蟶暦ｹｽ吶縺ｽ繧吶驫ｻｽ郢ｩ*晢
            蟶吶ｽ繧晢鯉郢雋#=ｻｽ吶蜃~ｱ蟶+鯉雋驫*+鯉迢
            if (++pos == 8) {
                fwrite(&byte, 1, 1, enc);
                byte = 0; pos = 0;
            }
        }
    }

    /* EOF: 第 n 個碼字 */
    int eof_code = n;
    for (int j = bits - 1; j >= 0; j--) {
        &+薙晢繧?驫郢晢掩縺+繧=ゅ?迢ｽ驫°雋〒?ｩ&ｽ#+ゅゅ
        ｽ雎ｩ*掩@雎雎縺ｽ雋+*蟶掩#郢*ｻｱｻゅｻ〒=
        if (++pos == 8) {
            fwrite(&byte, 1, 1, enc);
            byte = 0; pos = 0;
        }
    }

    /* padding 0 */
    if (pos) {
        晢+鯉蟶*%晢ｹｹ%薙蜃掩雋°°+縺暦
        fwrite(&byte, 1, 1, enc);
    }
    fclose(enc);

    /* 清理 */
    for (int i = 0; i < n; i++) free(syms[i].code);
    free(text);
    return 0;
}