/* huffman_trace.c — 在終端機上「看得見」的 Huffman 建樹：把記憶體裡的節點陣列、排序、佇列一步一步印出來
 *
 * 用法：
 *   huffman_trace                          預設字串 ABRACADABRA
 *   huffman_trace "多媒體多媒多"             符號 = UTF-8 字元
 *   huffman_trace --freq black:100,white:100,yellow:20,blue:20,orange:5,red:5,purple:3,green:3
 *   加上 --step：每一步停下來等你按 Enter（上課示範用）
 *
 * 用到的資料結構只有兩個陣列：
 *   node[]   所有節點。前 K 個是葉（一種符號一個），後面是合併出來的內部節點；最多 2K-1 個。
 *            每個節點記 weight、parent、left、right ——「樹」就是陣列裡互相指來指去的索引，不需要 malloc。
 *   queue[]  還沒被合併的節點的索引，永遠維持「weight 由小到大」。
 *            取最小的兩個 = 拿走最前面兩個（dequeue）；放回新節點 = 插入到排序後該在的位置（insertion）。
 *            這就是最簡單的 priority queue。
 *
 * 這支程式只示範「建樹」與「指派 code」。位元打包、codebook 怎麼存、解碼，是 MP4 與 Team 1 要你自己寫的部分。
 * 平手規則與 huffman_demo.py、slides/huffman_steps.html 相同：weight 相同時，先進佇列的排前面。
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
  #include <windows.h>
  #include <shellapi.h>
#endif

#define MAX_SYM  64
#define MAX_NODE (2 * MAX_SYM)
#define NAME_LEN 24

typedef struct {
    char name[NAME_LEN];      /* 葉：符號；內部節點：空字串 */
    long weight;              /* 次數 */
    int  parent, left, right; /* 陣列索引；-1 = 沒有 */
} Node;

static Node node[MAX_NODE];
static int  n_node = 0, n_leaf = 0;
static int  queue[MAX_NODE], q_len = 0;
static int  step_mode = 0;

#define C_RESET "\x1b[0m"
#define C_PICK  "\x1b[30;43m"      /* 黃底：這一輪被取出的兩個 */
#define C_NEW   "\x1b[30;42m"      /* 綠底：新合併出來的節點   */
#define C_DIM   "\x1b[90m"
#define C_HEAD  "\x1b[1;36m"

static void pause_step(void) {
    if (!step_mode) return;
    printf(C_DIM "  ── 按 Enter 繼續 ──" C_RESET);
    fflush(stdout);
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { }
}

static const char *label(int i) {                 /* 節點顯示名稱：葉印符號，內部節點印 #索引 */
    static char buf[8][NAME_LEN + 8];
    static int k = 0;
    char *b = buf[k = (k + 1) % 8];
    if (node[i].left < 0) {
        const char *s = node[i].name;
        snprintf(b, NAME_LEN + 8, "%s", strcmp(s, " ") == 0 ? "(空白)" : strcmp(s, "\n") == 0 ? "\\n" :
                                         strcmp(s, "\t") == 0 ? "\\t" : strcmp(s, "\r") == 0 ? "\\r" : s);
    } else {
        snprintf(b, NAME_LEN + 8, "#%d", i);
    }
    return b;
}

static void print_queue(const char *title, int pick_n, int new_idx) {
    printf("  %s\n    front → ", title);
    for (int i = 0; i < q_len; i++) {
        const char *c = (i < pick_n) ? C_PICK : (queue[i] == new_idx) ? C_NEW : "";
        printf("%s[%s:%ld]%s ", c, label(queue[i]), node[queue[i]].weight, *c ? C_RESET : "");
    }
    printf("← back   （共 %d 個）\n", q_len);
}

static void print_memory(int hi_a, int hi_b, int hi_new) {
    printf(C_DIM "  node[] 陣列在記憶體裡的樣子：" C_RESET "\n");
    printf("    idx  symbol       weight  parent  left  right\n");
    for (int i = 0; i < n_node; i++) {
        const char *c = (i == hi_a || i == hi_b) ? C_PICK : (i == hi_new) ? C_NEW : "";
        printf("    %s%3d  %-10s %8ld  %6d  %4d  %5d%s%s\n", c, i, node[i].left < 0 ? label(i) : "(internal)",
               node[i].weight, node[i].parent, node[i].left, node[i].right, *c ? C_RESET : "",
               node[i].parent < 0 ? "   ← 還在佇列裡" : "");
    }
}

/* 把索引 idx 插入 queue，維持 weight 由小到大；weight 相同時排在既有的後面（穩定）。回傳插入的位置。 */
static int queue_insert(int idx) {
    int pos = q_len;
    while (pos > 0 && node[queue[pos - 1]].weight > node[idx].weight) {
        queue[pos] = queue[pos - 1];               /* 比它大的往後挪一格：insertion sort 的核心動作 */
        pos--;
    }
    queue[pos] = idx;
    q_len++;
    return pos;
}

static int add_leaf(const char *name, long weight) {
    for (int i = 0; i < n_leaf; i++)
        if (strcmp(node[i].name, name) == 0) { node[i].weight += weight; return i; }
    if (n_leaf >= MAX_SYM) { fprintf(stderr, "符號種類超過 %d 種，這支示範程式放不下\n", MAX_SYM); exit(1); }
    Node *p = &node[n_leaf];
    snprintf(p->name, NAME_LEN, "%s", name);
    p->weight = weight;
    p->parent = p->left = p->right = -1;
    return n_leaf++;
}

static int utf8_len(unsigned char b) {            /* 由前導 byte 看這個字元幾 bytes（第 2 週的內容） */
    return b < 0x80 ? 1 : (b & 0xE0) == 0xC0 ? 2 : (b & 0xF0) == 0xE0 ? 3 : (b & 0xF8) == 0xF0 ? 4 : 1;
}

static int by_symbol_name(const void *a, const void *b) { return strcmp(((const Node *)a)->name, ((const Node *)b)->name); }

/* 把以 i 為根的子樹橫著印出來（右子樹在上、左子樹在下），分支標上 1／0 */
static void print_tree(int i, const char *prefix, int bit) {
    char next[512];
    if (node[i].right >= 0) {
        snprintf(next, sizeof(next), "%s%s", prefix, bit == 1 ? "        " : bit == 0 ? "   |    " : "");
        print_tree(node[i].right, next, 1);
    }
    printf("    %s%s", prefix, bit == 1 ? "   ┌─1─ " : bit == 0 ? "   └─0─ " : "");
    if (node[i].left < 0) printf("[%s:%ld]\n", label(i), node[i].weight);
    else                  printf("(%ld)\n", node[i].weight);
    if (node[i].left >= 0) {
        snprintf(next, sizeof(next), "%s%s", prefix, bit == 0 ? "        " : bit == 1 ? "   |    " : "");
        print_tree(node[i].left, next, 0);
    }
}

int main(int argc, char *argv[]) {
    const char *text = NULL, *table = NULL;
#ifdef _WIN32
    /* Windows 的 argv 是本地字碼頁（繁中是 Big5），不是 UTF-8：改從寬字元命令列轉成 UTF-8 */
    int wargc = 0;
    LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    static char u8[16][1024];
    if (wargv && wargc <= 16) {
        for (int i = 0; i < wargc; i++) { WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, u8[i], sizeof(u8[i]), NULL, NULL); argv[i] = u8[i]; }
        argc = wargc;
    }
    SetConsoleOutputCP(65001);
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) SetConsoleMode(h, mode | 0x0004 /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */);
#endif
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--step") == 0) step_mode = 1;
        else if (strcmp(argv[i], "--freq") == 0 && i + 1 < argc) table = argv[++i];
        else text = argv[i];
    }
    if (!text && !table) text = "ABRACADABRA";

    /*---------------------------------------------------------------- 步驟 0：統計次數 */
    long total = 0;
    static int seq[4096];
    int seq_len = 0;
    if (table) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "%s", table);
        for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
            char *colon = strrchr(tok, ':');
            if (!colon) { fprintf(stderr, "頻率表格式：名稱:次數,名稱:次數,…\n"); return 2; }
            *colon = '\0';
            add_leaf(tok, atol(colon + 1));
        }
    } else {
        for (const unsigned char *p = (const unsigned char *)text; *p; ) {
            char one[8] = {0};
            int k = utf8_len(*p);
            for (int j = 0; j < k && p[j]; j++) one[j] = (char)p[j];
            p += strlen(one);
            add_leaf(one, 1);
        }
    }
    qsort(node, (size_t)n_leaf, sizeof(Node), by_symbol_name);           /* 葉依符號排序：讓每次執行、每種語言的版本結果一致 */
    n_node = n_leaf;
    for (int i = 0; i < n_leaf; i++) total += node[i].weight;
    if (text)                                                       /* 記下原文的符號序列，最後編碼用 */
        for (const unsigned char *p = (const unsigned char *)text; *p && seq_len < 4096; ) {
            char one[8] = {0};
            int k = utf8_len(*p);
            for (int j = 0; j < k && p[j]; j++) one[j] = (char)p[j];
            p += strlen(one);
            for (int i = 0; i < n_leaf; i++) if (strcmp(node[i].name, one) == 0) { seq[seq_len++] = i; break; }
        }
    if (n_leaf == 0) { fprintf(stderr, "沒有任何符號\n"); return 2; }

    printf(C_HEAD "步驟 0｜統計每種符號的次數，算熵  H = Σ p·log2(1/p)" C_RESET "\n");
    if (text) printf("  輸入：\"%s\"\n", text);
    double H = 0.0;
    printf("    symbol       次數        p    log2(1/p)   p·log2(1/p)\n");
    for (int i = 0; i < n_leaf; i++) {
        double p = (double)node[i].weight / (double)total;
        H += p * log2(1.0 / p);
        printf("    %-10s %6ld   %6.4f   %8.3f   %10.3f\n", label(i), node[i].weight, p, log2(1.0 / p), p * log2(1.0 / p));
    }
    printf("  共 %ld 個符號、%d 種；熵 H = %.4f bits／符號\n\n", total, n_leaf, H);
    pause_step();

    /*---------------------------------------------------------------- 步驟 1：排序，建立佇列 */
    printf(C_HEAD "步驟 1｜把葉節點依 weight 由小到大排好，放進佇列（insertion sort：一個一個插到該在的位置）" C_RESET "\n");
    for (int i = 0; i < n_leaf; i++) {
        int pos = queue_insert(i);
        printf("  插入 [%s:%ld] → 位置 %d    ", label(i), node[i].weight, pos);
        for (int j = 0; j < q_len; j++) printf("%s[%s:%ld]%s ", j == pos ? C_NEW : "", label(queue[j]), node[queue[j]].weight, j == pos ? C_RESET : "");
        printf("\n");
    }
    printf("\n");
    pause_step();

    /*---------------------------------------------------------------- 步驟 2：反覆合併 */
    printf(C_HEAD "步驟 2｜反覆：取出最前面兩個（最小的兩個）→ 合併成新節點 → 插回佇列，直到只剩一個" C_RESET "\n\n");
    int round = 1;
    while (q_len > 1) {
        printf(C_HEAD "  第 %d 輪" C_RESET "\n", round++);
        print_queue("合併前的佇列（黃色是要取出的兩個）", 2, -1);
        int a = queue[0], b = queue[1];
        memmove(queue, queue + 2, sizeof(int) * (size_t)(q_len - 2));  /* dequeue 兩次：後面的全部往前挪 2 格 */
        q_len -= 2;

        int p = n_node++;
        node[p].name[0] = '\0';
        node[p].weight = node[a].weight + node[b].weight;
        node[p].left = a;  node[p].right = b;  node[p].parent = -1;
        node[a].parent = node[b].parent = p;
        int pos = queue_insert(p);

        printf("  取出 [%s:%ld] 與 [%s:%ld] → 新節點 #%d，weight = %ld + %ld = %ld，left=%d right=%d；插回佇列的位置 %d\n",
               label(a), node[a].weight, label(b), node[b].weight, p, node[a].weight, node[b].weight, node[p].weight, a, b, pos);
        print_queue("合併後的佇列（綠色是新節點）", 0, p);
        print_memory(a, b, p);
        printf("\n");
        pause_step();
    }
    int root = queue[0];
    if (n_leaf == 1) printf("  只有一種符號：不需要合併。code 長度定為 1（不能是 0，否則解碼端不知道有幾個符號）。\n\n");

    /*---------------------------------------------------------------- 步驟 3：讀出 code */
    printf(C_HEAD "步驟 3｜樹建好了（根是 #%d）。左分支 0、右分支 1" C_RESET "\n", root);
    print_tree(root, "", -1);
    printf("\n  每個葉沿著 parent 一路走到根，記下自己是左（0）還是右（1）小孩；走到根之後把順序倒過來就是 code：\n");
    static char code[MAX_SYM][MAX_NODE];
    long total_bits = 0;
    printf("    symbol       次數   走訪：葉 → 根，#節點(自己是左0/右1)               長度  code\n");
    for (int i = 0; i < n_leaf; i++) {
        char rev[MAX_NODE], path[256] = "";
        int len = 0;
        for (int c = i; node[c].parent >= 0; c = node[c].parent) {
            int bit = (node[node[c].parent].right == c);
            rev[len++] = (char)('0' + bit);
            char hop[32];
            snprintf(hop, sizeof(hop), "%s#%d(%d)", len > 1 ? " " : "", node[c].parent, bit);
            strncat(path, hop, sizeof(path) - strlen(path) - 1);
        }
        if (len == 0) rev[len++] = '0';                                /* 只有一種符號 */
        for (int j = 0; j < len; j++) code[i][j] = rev[len - 1 - j];
        code[i][len] = '\0';
        total_bits += node[i].weight * len;
        printf("    %-10s %6ld   %-50s %4d  %s\n", label(i), node[i].weight, path, len, code[i]);
    }
    printf("  所有符號都在葉上，所以沒有任何 code 是另一個 code 的開頭（prefix code）：位元流不用分隔符號也能唯一解碼。\n\n");
    pause_step();

    /*---------------------------------------------------------------- 步驟 4：編碼與比較 */
    printf(C_HEAD "步驟 4｜編碼，並和熵、定長編碼比較" C_RESET "\n");
    if (seq_len > 0) {
        printf("  ");
        for (int i = 0; i < seq_len && i < 48; i++) printf("%s ", code[seq[i]]);
        printf("%s\n", seq_len > 48 ? "..." : "");
    }
    double L = (double)total_bits / (double)total;
    int flc = 1;
    while ((1 << flc) < n_leaf) flc++;
    printf("  平均碼長 L = Σ p·len = %ld ÷ %ld = %.4f bits／符號；熵 H = %.4f；H ≤ L < H+1：%s；效率 H÷L = %.1f%%\n",
           total_bits, total, L, H,
           n_leaf == 1 ? "例外（只有一種符號時 H = 0，但 code 長度不能是 0，所以 L = 1）"
                       : (H <= L + 1e-12 && L < H + 1.0) ? "成立" : "不成立", 100.0 * H / L);
    printf("  定長編碼（FLC）每個符號 %d bits → %ld bits；Huffman %ld bits；壓縮率 %.1f%%（約 %.2f:1）\n",
           flc, flc * total, total_bits, 100.0 * (double)total_bits / (double)(flc * total), (double)(flc * total) / (double)total_bits);
    printf("  注意：解碼端還需要 codebook，這裡還沒算它的大小。\n");
    return 0;
}
