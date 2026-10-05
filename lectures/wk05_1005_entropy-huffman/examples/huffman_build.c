/* huffman_build.c — K 很大時怎麼建樹（先排序一次＋兩個佇列）、由「長度」指派 canonical code
 *
 * 用法：
 *   huffman_build                                 預設字串 ABRACADABRA（符號 = UTF-8 字元）
 *   huffman_build "多媒體多媒多"
 *   huffman_build --freq black:100,white:100,yellow:20,blue:20,orange:5,red:5,purple:3,green:3
 *   huffman_build --file 檔案.txt                  整個檔案，符號 = UTF-8 字元（開頭的 BOM 跳過；\r 保留）
 *   huffman_build --file 檔案.wav --sym s16        符號 = 16-bit sample（逐個 chunk 找 data，和 Team 1 規定相同）
 *   huffman_build --file 任何檔案 --sym byte       符號 = byte
 *   給 --file 時最後會印出 Team 1 報告「壓縮率分析表」要的那一列：N、K、H、L、理論／純編碼壓縮率、codebook 與區塊的估計大小
 *   （codebook 以「符號＋長度」估：byte 2、char 4、s16 3 bytes 一組；區塊照 python_ref 的格式估，數字可以和 python_ref inspect 對）
 *   加 --eof     ：多加一個出現 1 次的 EOF 符號（MP3、MP4 的做法，講義 1.3）
 *   加 --summary ：只印一行 N=… K=… H=… L=… bits=… bytes=…（make check 用它和 codebook_check.py 比對）
 *
 * 第 3 週的 huffman_trace.c 用「一個排序好的佇列」：每合併一次就把新節點插回該在的位置，K 種符號要搬 K² 次。
 * K = 8 無所謂；Team 1 的 WAV 以 sample 為符號，K 可以到上萬（真實語音檔 K = 12,343），字元也有上千種。
 * 這支程式改用 O(K log K) 的做法（講義 2.1）：
 *   1. 葉節點先依 weight 排好一次（qsort）→ 佇列 Q1。
 *   2. 合併出來的新節點依序放進另一個佇列 Q2 的尾巴。關鍵：新節點的 weight 一定「不會比上一個新節點小」
 *      （兩個最小的相加 ≥ 上一輪兩個最小的相加），所以 Q2 不排序也自然是由小到大。
 *   3. 每一輪「最小的兩個」只可能在 Q1 的頭或 Q2 的頭：比較兩個頭、取小的，做兩次。
 *   排序一次 O(K log K)，之後每輪 O(1)，不再有「插回佇列」的搬移。
 *
 * 樹建好之後只留下每個符號的「code 長度」（＝葉的深度），再用講義 2.2 的 canonical 規則指派 code：
 *   依（長度，符號值）排序；第一個 code 是 0；下一個 = 上一個 + 1；長度變長時左移補 0。
 *   兩端只要約定這條規則，codebook 就只需要傳「符號＋長度」，不必傳 code 本身。
 *
 * 對應作業：MP4 的 codebook.csv 要印出 code 字串（用樹走出來的或 canonical 的都可以，CI 只比 bin 大小＋交叉解碼）；
 *          Team 1 的 codebook 用二進位傳，用 canonical 最省。
 *
 * 編譯：make（或 gcc -std=c99 -Wall -Wextra -o huffman_build huffman_build.c -lm）
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
  #include <windows.h>
  #include <shellapi.h>
#endif

#define CP_LIMIT  0x110000u      /* Unicode code point 的上限 U+10FFFF（與第 3 週 entropy.c 相同） */
#define SYM_EOF   CP_LIMIT       /* EOF 符號：用一個不可能是 code point 的值，排序時自然排在所有字元後面 */
#define NAME_LEN  24
#define MAX_LEN   56             /* code 最長 56 bits（和 bitio_trace.c、python_ref 一致） */
#define TRACE_MAX 16             /* K 超過這個數就不逐輪印（太長），只印結果 */

typedef struct {
    uint64_t weight;             /* 次數；根的 weight 是 N，1 MB 的檔案就超過 int 的安全範圍，用 64 bits */
    uint32_t sym;                /* 葉：符號值（code point 或 --freq 的序號）；內部節點不用 */
    int      parent, left, right;
    int      len;                /* 葉：code 長度（深度） */
} Node;

static Node     *node;           /* 2K−1 個節點：[0,K) 是葉，之後是合併出來的 */
static int       K = 0, n_node = 0;
static char    (*fname)[NAME_LEN];   /* --freq 模式下每個符號的名字 */
static int       with_eof = 0, summary = 0;
static int       kind = 1;               /* 0 byte、1 char、2 s16（和 Team 1 的 tl_sym_t 相同） */
static const char *kind_name[] = { "byte", "char", "s16" };
static const int   kind_width[] = { 1, 3, 2 };   /* codebook 裡一個符號佔幾 bytes（python_ref 的 SYM_WIDTH） */

/* 把符號印成看得見的字（空白 → ␠、換行 → \n…；--freq 用名字；EOF → EOF） */
static void put_sym(uint32_t c, FILE *f) {
    if (fname) { fputs(fname[c], f); return; }
    if (c == SYM_EOF) { fputs("EOF", f); return; }
    if (kind == 0) { fprintf(f, "0x%02X", c); return; }
    if (kind == 2) { fprintf(f, "%+d", c >= 32768 ? (int)c - 65536 : (int)c); return; }
    if (c == ' ') { fputs("\xe2\x90\xa0", f); return; }
    if (c == '\n') { fputs("\\n", f); return; }
    if (c == '\r') { fputs("\\r", f); return; }
    if (c == '\t') { fputs("\\t", f); return; }
    if (c < 0x80) fputc((int)c, f);
    else if (c < 0x800) { fputc((int)(0xC0 | (c >> 6)), f); fputc((int)(0x80 | (c & 0x3F)), f); }
    else if (c < 0x10000) { fputc((int)(0xE0 | (c >> 12)), f); fputc((int)(0x80 | ((c >> 6) & 0x3F)), f); fputc((int)(0x80 | (c & 0x3F)), f); }
    else { fputc((int)(0xF0 | (c >> 18)), f); fputc((int)(0x80 | ((c >> 12) & 0x3F)), f); fputc((int)(0x80 | ((c >> 6) & 0x3F)), f); fputc((int)(0x80 | (c & 0x3F)), f); }
}

/* 嚴格的 UTF-8 解碼（與第 2、3 週相同）：成功回傳用掉幾 bytes，失敗回傳 0 */
static size_t utf8_next(const unsigned char *s, size_t n, uint32_t *cp) {
    unsigned char b = s[0]; size_t need; uint32_t c, min;
    if (b < 0x80) { *cp = b; return 1; }
    else if ((b & 0xE0) == 0xC0) { need = 1; c = b & 0x1F; min = 0x80; }
    else if ((b & 0xF0) == 0xE0) { need = 2; c = b & 0x0F; min = 0x800; }
    else if ((b & 0xF8) == 0xF0) { need = 3; c = b & 0x07; min = 0x10000; }
    else return 0;
    if (n - 1 < need) return 0;
    for (size_t k = 1; k <= need; k++) { if ((s[k] & 0xC0) != 0x80) return 0; c = (c << 6) | (s[k] & 0x3F); }
    if (c < min || (c >= 0xD800 && c <= 0xDFFF) || c >= CP_LIMIT) return 0;
    *cp = c; return need + 1;
}

/* 複製一個字串（strdup 不在 C99 標準裡，自己寫三行） */
static char *dup_str(const char *s) { size_t n = strlen(s) + 1; char *d = malloc(n); if (d) memcpy(d, s, n); return d; }

/* qsort 用的比較函式：weight 小的在前；weight 相同時符號值小的在前（平手規則：和 huffman_demo.py 一樣「葉依符號排序」） */
static int by_weight(const void *a, const void *b) {
    const Node *x = a, *y = b;
    if (x->weight != y->weight) return x->weight < y->weight ? -1 : 1;
    return x->sym < y->sym ? -1 : x->sym > y->sym;
}

/* canonical 指派用：長度短的在前；長度相同時符號值小的在前 */
static int by_len_sym(const void *a, const void *b) {
    const Node *x = &node[*(const int *)a], *y = &node[*(const int *)b];
    if (x->len != y->len) return x->len - y->len;
    return x->sym < y->sym ? -1 : x->sym > y->sym;
}

static void print_queue(const char *title, const int *q, int head, int tail) {
    printf("%s", title);
    for (int i = head; i < tail; i++) { printf("["); if (q[i] < K) put_sym(node[q[i]].sym, stdout); else printf("#%d", q[i]); printf(":%llu] ", (unsigned long long)node[q[i]].weight); }
    if (head >= tail) printf("(空)");
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    int wargc; wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    static char *uargv[64]; static char ubuf[64][512];
    for (int i = 0; i < wargc && i < 64; i++) { WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, ubuf[i], sizeof ubuf[i], NULL, NULL); uargv[i] = ubuf[i]; }
    argc = wargc < 64 ? wargc : 64; argv = uargv;
#endif
    const char *text = "ABRACADABRA", *freq_spec = NULL, *path = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--freq") == 0 && i + 1 < argc) freq_spec = argv[++i];
        else if (strcmp(argv[i], "--file") == 0 && i + 1 < argc) path = argv[++i];
        else if (strcmp(argv[i], "--eof") == 0) with_eof = 1;
        else if (strcmp(argv[i], "--sym") == 0 && i + 1 < argc) { i++; kind = strcmp(argv[i], "byte") == 0 ? 0 : strcmp(argv[i], "s16") == 0 ? 2 : 1; }
        else if (strcmp(argv[i], "--summary") == 0) summary = 1;
        else text = argv[i];
    }

    /*---------------------------------------------------------------- 步驟 0：統計次數 → K 個葉 */
    uint64_t *count = NULL;      /* 以符號值為索引的計數表（講義 1.7、entropy.c 的做法） */
    uint64_t  N = 0, orig_bytes = 0, head_tail = 0;
    if (freq_spec) {
        /* --freq：名字:次數,…；符號值就是出現的順序 */
        char *spec = dup_str(freq_spec); int n = 0;
        for (char *p = spec; *p; p++) if (*p == ',') n++;
        n++;
        fname = calloc((size_t)n + 1, sizeof *fname);
        count = calloc((size_t)n + 1, sizeof *count);
        for (char *item = strtok(spec, ","); item; item = strtok(NULL, ",")) {
            char *colon = strchr(item, ':');
            if (!colon) { fprintf(stderr, "--freq 格式：名字:次數,…\n"); return 1; }
            *colon = '\0';
            snprintf(fname[K], NAME_LEN, "%s", item);
            count[K] = strtoull(colon + 1, NULL, 10);
            K++;
        }
        if (with_eof) { snprintf(fname[K], NAME_LEN, "EOF"); count[K] = 1; K++; }
        for (int i = 0; i < K; i++) N += count[i];
        node = calloc((size_t)(2 * K), sizeof *node);
        for (int i = 0; i < K; i++) { node[i].sym = (uint32_t)i; node[i].weight = count[i]; node[i].parent = node[i].left = node[i].right = -1; }
    } else {
        unsigned char *buf; size_t len;
        if (path) {
            FILE *f = fopen(path, "rb");
            if (!f) { perror(path); return 1; }
            fseek(f, 0, SEEK_END); len = (size_t)ftell(f); fseek(f, 0, SEEK_SET);
            buf = malloc(len + 1); if (fread(buf, 1, len, f) != len) { perror(path); return 1; } fclose(f);
        } else { len = strlen(text); buf = (unsigned char *)dup_str(text); }
        orig_bytes = len;
        count = calloc(CP_LIMIT + 1, sizeof *count);                                /* 0x110001 格 × 8 bytes ≈ 8.9 MB，用 calloc 不放 stack */
        if (!count) { fprintf(stderr, "記憶體不足\n"); return 1; }
        if (kind == 1) {                                                            /* 符號 = UTF-8 字元 */
            size_t p = 0;
            if (len >= 3 && buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF) p = 3;   /* BOM 跳過、不算內容（第 2 週 1.5） */
            while (p < len) {
                uint32_t cp; size_t used = utf8_next(buf + p, len - p, &cp);
                if (!used) { fprintf(stderr, "第 %zu byte 起不是合法的 UTF-8：Team 1 這時要回傳 TL_ERR_DATA，殼會改用 --sym byte\n", p); return 1; }
                count[cp]++; N++; p += used;
            }
        } else if (kind == 2) {                                                     /* 符號 = 16-bit sample：逐個 chunk 找 data（第 3 週 2.6） */
            if (len < 12 || memcmp(buf, "RIFF", 4) || memcmp(buf + 8, "WAVE", 4)) { fprintf(stderr, "不是 RIFF/WAVE：Team 1 這時回傳 TL_ERR_DATA\n"); return 1; }
            size_t pos = 12, start = 0, end = 0; int fmt_ok = 0;
            while (pos + 8 <= len) {
                uint32_t size = buf[pos + 4] | (uint32_t)buf[pos + 5] << 8 | (uint32_t)buf[pos + 6] << 16 | (uint32_t)buf[pos + 7] << 24;   /* little-endian */
                if (!memcmp(buf + pos, "fmt ", 4)) {
                    if (size < 16 || pos + 24 > len) { fprintf(stderr, "fmt chunk 太短\n"); return 1; }
                    unsigned fmt_tag = buf[pos + 8] | buf[pos + 9] << 8, bits = buf[pos + 22] | buf[pos + 23] << 8;
                    if (fmt_tag != 1 || bits != 16) { fprintf(stderr, "不是 16-bit PCM（format %u、%u bits）：Team 1 這時回傳 TL_ERR_DATA\n", fmt_tag, bits); return 1; }
                    fmt_ok = 1;
                } else if (!memcmp(buf + pos, "data", 4)) {
                    if (!fmt_ok) { fprintf(stderr, "data 之前沒有 fmt\n"); return 1; }
                    start = pos + 8;
                    size_t avail = len - start;
                    end = start + (size < avail ? size : avail) / 2 * 2;            /* 奇數個 byte 時最後 1 byte 不算 sample */
                    break;
                }
                pos += 8 + size + (size & 1);
            }
            if (!end && !start) { fprintf(stderr, "找不到 data chunk\n"); return 1; }
            head_tail = start + (len - end);                                        /* data 區以外的 bytes，區塊要原樣帶著 */
            for (size_t q = start; q + 1 < end; q += 2) { count[buf[q] | (uint32_t)buf[q + 1] << 8]++; N++; }
        } else {                                                                    /* 符號 = byte */
            for (size_t q = 0; q < len; q++) { count[buf[q]]++; N++; }
        }
        if (with_eof) { count[SYM_EOF] = 1; N++; }
        for (uint32_t c = 0; c <= CP_LIMIT; c++) if (count[c]) K++;
        node = calloc((size_t)(2 * K + 1), sizeof *node);
        for (uint32_t c = 0; c <= CP_LIMIT; c++) if (count[c]) { node[n_node].sym = c; node[n_node].weight = count[c]; node[n_node].parent = node[n_node].left = node[n_node].right = -1; n_node++; }
        free(buf);
    }
    n_node = K;
    if (K == 0) { printf("沒有任何符號（空輸入）\n"); return 0; }

    /*---------------------------------------------------------------- 步驟 1：葉排序一次 → Q1 */
    qsort(node, (size_t)K, sizeof *node, by_weight);
    int *q1 = malloc((size_t)K * sizeof *q1), *q2 = malloc((size_t)K * sizeof *q2);
    int h1 = 0, t1 = K, h2 = 0, t2 = 0;      /* 兩個佇列各自的頭（head）與尾（tail）；佇列裡放的是節點索引 */
    for (int i = 0; i < K; i++) q1[i] = i;
    int trace = !summary && K <= TRACE_MAX;
    if (!summary) {
        printf("步驟 0｜符號 = %s；K = %d 種符號，N = %llu 個%s\n", freq_spec ? "名字" : kind_name[kind], K, (unsigned long long)N, with_eof ? "（含 EOF 一個）" : "");
        printf("步驟 1｜葉依 weight 排序一次（qsort）→ Q1；Q2 一開始是空的\n");
        if (trace) { print_queue("  Q1：", q1, h1, t1); print_queue("\n  Q2：", q2, h2, t2); printf("\n"); }
        printf("步驟 2｜每一輪：比較 Q1 的頭與 Q2 的頭，取小的那個，做兩次 → 合併 → 放到 Q2 的尾巴\n");
    }

    /*---------------------------------------------------------------- 步驟 2：反覆合併（K−1 輪） */
    if (K == 1) node[0].len = 1;              /* 只有一種符號：code 長度定為 1（講義 1.6 的例外） */
    for (int round = 1; K > 1 && round < K; round++) {
        int pick[2];
        for (int j = 0; j < 2; j++) {
            /* Q1 空了拿 Q2；Q2 空了拿 Q1；都有就比頭：weight 相同時先拿葉（Q1），和 huffman_demo.py 的平手規則一致 */
            if (h1 < t1 && (h2 >= t2 || node[q1[h1]].weight <= node[q2[h2]].weight)) pick[j] = q1[h1++];
            else pick[j] = q2[h2++];
        }
        int p = n_node++;
        node[p].weight = node[pick[0]].weight + node[pick[1]].weight;
        node[p].left = pick[0]; node[p].right = pick[1]; node[p].parent = -1;
        node[pick[0]].parent = node[pick[1]].parent = p;
        q2[t2++] = p;
        if (trace) {
            printf("  第 %d 輪 ", round);
            for (int j = 0; j < 2; j++) { if (pick[j] < K) put_sym(node[pick[j]].sym, stdout); else printf("#%d", pick[j]); printf(":%llu%s", (unsigned long long)node[pick[j]].weight, j ? "" : " + "); }
            printf(" → #%d:%llu\n          ", p, (unsigned long long)node[p].weight);
            print_queue("Q1：", q1, h1, t1); print_queue("   Q2：", q2, h2, t2); printf("\n");
        }
    }
    int root = K == 1 ? 0 : q2[h2];

    /*---------------------------------------------------------------- 步驟 3：每個葉的深度 = code 長度 */
    int max_len = 0;
    for (int i = 0; i < K && K > 1; i++) {
        int d = 0; for (int c = i; c != root; c = node[c].parent) d++;
        node[i].len = d; if (d > max_len) max_len = d;
    }
    if (K == 1) max_len = 1;
    if (max_len > MAX_LEN) { fprintf(stderr, "code 長度 %d 超過 %d：這種極端的頻率分布要另外處理（講義 2.2）\n", max_len, MAX_LEN); return 1; }
    if (!summary) {
        printf("步驟 3｜樹建好了（根是 #%d）。只記每個葉的深度 = code 長度；各長度有幾個符號：", root);
        int *bl = calloc((size_t)max_len + 1, sizeof *bl);
        for (int i = 0; i < K; i++) bl[node[i].len]++;
        for (int l = 1; l <= max_len; l++) if (bl[l]) printf("%d bits×%d ", l, bl[l]);
        printf("\n");
        free(bl);
    }

    /*---------------------------------------------------------------- 步驟 4：canonical code */
    int *order = malloc((size_t)K * sizeof *order);
    for (int i = 0; i < K; i++) order[i] = i;
    qsort(order, (size_t)K, sizeof *order, by_len_sym);
    uint64_t code = 0; int prev_len = 0;
    double H = 0, Lsum = 0, kraft = 0; uint64_t total_bits = 0;
    if (!summary) {
        printf("步驟 4｜canonical code：依（長度，符號）排序，第一個是 0，下一個 = 上一個 + 1，長度變長就左移補 0\n");
        printf("  %-10s %10s %4s  %-*s  %s\n", "symbol", "次數", "長度", max_len, "code", "理想 log2(1/p)");
    }
    for (int i = 0; i < K; i++) {
        Node *n = &node[order[i]];
        code <<= (n->len - prev_len); prev_len = n->len;       /* 長度沒變：左移 0 位；變長 d：左移 d 位 */
        double p = (double)n->weight / (double)N;
        kraft += pow(2.0, -n->len);
        if (n->weight) { H -= p * log2(p); Lsum += p * n->len; total_bits += n->weight * (uint64_t)n->len; }
        if (!summary) {
            printf("  "); put_sym(n->sym, stdout);
            int w = fname ? (int)strlen(fname[n->sym]) : (n->sym == SYM_EOF ? 3 : 1);
            printf("%*s %10llu %4d  ", 10 - w, "", (unsigned long long)n->weight, n->len);
            for (int b = n->len - 1; b >= 0; b--) putchar((code >> b) & 1 ? '1' : '0');
            printf("%*s  %.3f\n", max_len - n->len, "", log2(1.0 / p));
        }
        code++;
    }
    uint64_t bytes = (total_bits + 7) / 8;
    if (summary) { printf("N=%llu K=%d H=%.4f L=%.4f bits=%llu bytes=%llu\n", (unsigned long long)N, K, H, Lsum, (unsigned long long)total_bits, (unsigned long long)bytes); return 0; }
    printf("\n  平均碼長 L = Σ p·len = %llu ÷ %llu = %.4f bits／符號；熵 H = %.4f；H ≤ L < H+1：%s\n",
           (unsigned long long)total_bits, (unsigned long long)N, Lsum, H,
           K == 1 ? "只有一種符號，是定理的例外（H = 0 但 code 長度不能是 0，所以 L = 1）" : (H <= Lsum + 1e-9 && Lsum < H + 1) ? "成立" : "★不成立，程式有錯");
    printf("  Kraft：Σ 2^(−len) = %.6f（Huffman 的樹是滿的，K ≥ 2 時應該剛好是 1）；位元流 %llu bits → %llu bytes（還沒算 codebook）\n",
           kraft, (unsigned long long)total_bits, (unsigned long long)bytes);
    if (path) {
        /* Team 1 報告的「壓縮率分析表」這一列（規格「壓縮率」一節）。codebook 與區塊是依 python_ref 的格式估的：
         * 區塊 = 檔頭 21 + codebook K × (符號 bytes + 1) + （s16）4 + head + 4 + tail + bitstream。 */
        uint64_t book = (uint64_t)K * (uint64_t)(kind_width[kind] + 1);
        uint64_t block = 21 + book + (kind == 2 ? 8 + head_tail : 0) + bytes;
        printf("\n報告表｜%s：%llu bytes，符號 = %s\n", path, (unsigned long long)orig_bytes, kind_name[kind]);
        printf("  %-10s %10s %8s %9s %9s %12s %12s %10s %10s %8s\n", "符號", "N", "K", "H", "L", "H×N÷8÷原始", "L×N÷8÷原始", "codebook", "區塊(估)", "壓縮率");
        printf("  %-10s %10llu %8d %9.4f %9.4f %11.2f%% %11.2f%% %10llu %10llu %7.2f%%\n", kind_name[kind], (unsigned long long)N, K, H, Lsum,
               orig_bytes ? H * (double)N / 8.0 / (double)orig_bytes * 100 : 0.0, orig_bytes ? Lsum * (double)N / 8.0 / (double)orig_bytes * 100 : 0.0,
               (unsigned long long)book, (unsigned long long)block, orig_bytes ? (double)block / (double)orig_bytes * 100 : 0.0);
        printf("  （同一個檔案再跑一次 --sym byte 就是「對照：改以 byte 為符號」那一欄；python_ref 的 inspect 會印同一張表，拿來對答案）\n");
    }
    free(order); free(q1); free(q2); free(node); free(count); free(fname);
    return 0;
}
