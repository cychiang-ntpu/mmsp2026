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
 *
 * 怎麼讀這支程式（建議順序）：
 *   1. 先看 Node 這個 struct 與 node[]、queue[] 兩個陣列（整支程式的資料都在這裡）。
 *   2. 跳到 main()，它照「步驟 0 → 4」寫，和講義 1.6 的步驟、螢幕上印出來的標題一一對應。
 *   3. 真正屬於 Huffman 演算法的只有 queue_insert()（排序）與 main 裡步驟 2 的 while 迴圈（合併）、
 *      步驟 3 的 for 迴圈（由葉走到根讀出 code）。其餘的 print_xxx() 都只是把記憶體的內容印漂亮，第一次讀可以跳過。
 *
 * 編譯：make（或 gcc -std=c11 -Wall -Wextra -o huffman_trace huffman_trace.c -lm；-lm 是因為用到 log2）
 */
#include <math.h>     /* log2()：算熵用 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32         /* 這一段只有在 Windows 上編譯時才會被編進去：處理中文命令列參數與彩色輸出（見 main 開頭） */
  #include <windows.h>
  #include <shellapi.h>
#endif

/* 陣列大小在編譯時就固定（這支程式完全不用 malloc）。
 * K 種符號的 Huffman 樹：每合併一次少 2 個節點、多 1 個，合併 K-1 次後剩 1 個，所以節點總數是 K + (K-1) = 2K-1。 */
#define MAX_SYM  64
#define MAX_NODE (2 * MAX_SYM)
#define NAME_LEN 24

/* 一個節點。注意 parent／left／right 不是指標，而是「node[] 陣列的索引（第幾格）」：
 * 例如 node[5].left == 2 的意思是「5 號節點的左小孩是 2 號節點」。用 -1 代表「沒有」。
 * 好處：整棵樹就是一個普通陣列，可以整個印出來看（print_memory），也不會有忘記 free 的問題。 */
typedef struct {
    char name[NAME_LEN];      /* 葉：符號；內部節點：空字串 */
    long weight;              /* 次數 */
    int  parent, left, right; /* 陣列索引；-1 = 沒有 */
} Node;

/* 全域變數前面的 static：只有這個 .c 檔看得到。全域陣列一開始全部是 0。 */
static Node node[MAX_NODE];      /* 所有節點：[0, n_leaf) 是葉，[n_leaf, n_node) 是合併出來的內部節點 */
static int  n_node = 0, n_leaf = 0;
static int  queue[MAX_NODE], q_len = 0;   /* 佇列裡放的是「節點的索引」，不是節點本身；q_len 是目前有幾個 */
static int  step_mode = 0;                /* 有沒有加 --step */

/* ANSI escape sequence：終端機看到 ESC（\x1b）開頭的這串字不會印出來，而是改變之後文字的顏色。
 * C 會把相鄰的字串常數自動接起來，所以 printf(C_PICK "abc" C_RESET) 就是「黃底的 abc，然後恢復原色」。 */
#define C_RESET "\x1b[0m"
#define C_PICK  "\x1b[30;43m"      /* 黃底：這一輪被取出的兩個 */
#define C_NEW   "\x1b[30;42m"      /* 綠底：新合併出來的節點   */
#define C_DIM   "\x1b[90m"
#define C_HEAD  "\x1b[1;36m"

/* --step 模式：停下來等使用者按 Enter。getchar() 一次讀一個字元，讀到換行（或輸入結束 EOF）為止。 */
static void pause_step(void) {
    if (!step_mode) return;
    printf(C_DIM "  ── 按 Enter 繼續 ──" C_RESET);
    fflush(stdout);
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { }
}

/* 節點顯示名稱：葉印符號（空白、換行這些看不見的字元改印成看得見的字），內部節點印 #索引。
 * 回傳的是字串的位址，所以字串不能放在函式結束就消失的區域變數裡 → 用 static 陣列。
 * 準備 8 份輪流用，是因為同一個 printf 裡可能呼叫 label() 好幾次，只有一份的話後面的會蓋掉前面的。
 * 「node[i].left < 0」= 沒有小孩 = 這是葉。 */
static const char *label(int i) {
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

/* 印出佇列。最前面 pick_n 個塗黃色（這一輪要取出的）；索引等於 new_idx 的塗綠色（剛插回來的新節點）。 */
static void print_queue(const char *title, int pick_n, int new_idx) {
    printf("  %s\n    front → ", title);
    for (int i = 0; i < q_len; i++) {
        const char *c = (i < pick_n) ? C_PICK : (queue[i] == new_idx) ? C_NEW : "";
        printf("%s[%s:%ld]%s ", c, label(queue[i]), node[queue[i]].weight, *c ? C_RESET : "");
    }
    printf("← back   （共 %d 個）\n", q_len);
}

/* 把 node[] 一格一格印出來：這就是「樹在記憶體裡真正的樣子」。parent 還是 -1 的節點＝還沒被合併＝還在佇列裡。 */
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

/* 把索引 idx 插入 queue，維持 weight 由小到大；weight 相同時排在既有的後面（穩定）。回傳插入的位置。
 *
 * 做法和整理撲克牌一樣：從最後面往前看，比新來的大的就往後挪一格，直到遇到不比它大的（或到最前面），把新來的放進空位。
 * 例：queue = [A:1][B:2][D:5]，插入 C:2
 *       D:5 > 2 → 往後挪    [A:1][B:2][   ][D:5]
 *       B:2 > 2？不是 → 停   [A:1][B:2][C:2][D:5]   （C 排在一樣大的 B 後面：這就是「穩定」）
 * 條件用的是 > 而不是 >=，平手規則就是由這一個符號決定的。
 * 最壞要挪 q_len 格，所以一次插入是 O(n)；符號很多時會改用 heap（O(log n)），觀念相同。 */
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

/* 登記一個符號出現了 weight 次：已經有這種符號就把次數加上去，沒有就在 node[] 後面新增一個葉。回傳它的索引。
 * 用「從頭找到尾」的線性搜尋，符號種類少的時候夠用；strcmp 回傳 0 代表兩個字串相同。 */
static int add_leaf(const char *name, long weight) {
    for (int i = 0; i < n_leaf; i++)
        if (strcmp(node[i].name, name) == 0) { node[i].weight += weight; return i; }
    if (n_leaf >= MAX_SYM) { fprintf(stderr, "符號種類超過 %d 種，這支示範程式放不下\n", MAX_SYM); exit(1); }
    Node *p = &node[n_leaf];                      /* p 指向陣列裡下一個空格；p->weight 就是 node[n_leaf].weight */
    snprintf(p->name, NAME_LEN, "%s", name);
    p->weight = weight;
    p->parent = p->left = p->right = -1;
    return n_leaf++;                              /* 先回傳現在的值，再加 1 */
}

/* 由前導 byte 看這個 UTF-8 字元佔幾 bytes（第 2 週的內容）：
 *   0xxxxxxx → 1（ASCII）   110xxxxx → 2   1110xxxx → 3（大部分中文字）   11110xxx → 4（emoji）
 * (b & 0xE0) == 0xC0 的意思：0xE0 = 1110 0000，先用 & 只留下最高 3 個 bit，再看它是不是 110。
 * 「條件 ? 甲 : 乙」是 if-else 的簡寫，這裡連用了四次。 */
static int utf8_len(unsigned char b) {
    return b < 0x80 ? 1 : (b & 0xE0) == 0xC0 ? 2 : (b & 0xF0) == 0xE0 ? 3 : (b & 0xF8) == 0xF0 ? 4 : 1;
}

/* 給 qsort 用的比較函式。qsort 不知道陣列裡放什麼，所以只給兩個 void*（「不知道型別的位址」），
 * 我們自己轉回 Node* 再比；回傳負數／0／正數分別代表 a 應該排在 b 的前面／一樣／後面。 */
static int by_symbol_name(const void *a, const void *b) { return strcmp(((const Node *)a)->name, ((const Node *)b)->name); }

/* 把以 i 為根的子樹橫著印出來（右子樹在上、左子樹在下），分支標上 1／0。
 * 這是遞迴（函式呼叫自己）：印一棵樹 = 先印右子樹、再印自己、再印左子樹；葉沒有小孩，遞迴就在那裡停下來。
 * prefix 是每一行前面要先印的縮排；bit 是「我是爸爸的 1 分支還是 0 分支」（根傳 -1）。只跟畫面有關，和演算法無關。 */
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
    /* Windows 的 argv 是本地字碼頁（繁中是 Big5），不是 UTF-8：改從寬字元命令列轉成 UTF-8。
     * 後面三行：把主控台的輸出設成 UTF-8（65001），並打開 ANSI 顏色碼的支援。macOS／Linux 不需要這一段。 */
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
    /* 讀命令列參數。argv[0] 是程式自己的名字，所以從 1 開始。 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--step") == 0) step_mode = 1;
        else if (strcmp(argv[i], "--freq") == 0 && i + 1 < argc) table = argv[++i];
        else text = argv[i];
    }
    if (!text && !table) text = "ABRACADABRA";

    /*---------------------------------------------------------------- 步驟 0：統計次數
     * 兩種輸入：--freq 直接給「名稱:次數」的表；否則把字串切成一個一個 UTF-8 字元來數。 */
    long total = 0;
    static int seq[4096];                     /* 原文依序是哪些符號（存葉的索引），步驟 4 印編碼結果用 */
    int seq_len = 0;
    if (table) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "%s", table);
        /* strtok 會把 buf 裡的逗號改成 '\0' 來切字串（所以要先複製一份到 buf，不能直接切 argv）；
         * 第一次傳 buf，之後傳 NULL 表示「繼續切同一個字串」；切完回傳 NULL。 */
        for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
            char *colon = strrchr(tok, ':');          /* 找最後一個冒號：左邊是名稱，右邊是次數 */
            if (!colon) { fprintf(stderr, "頻率表格式：名稱:次數,名稱:次數,…\n"); return 2; }
            *colon = '\0';
            add_leaf(tok, atol(colon + 1));
        }
    } else {
        /* p 指向目前這個字元的第一個 byte；*p 是 0（字串結尾）就停。
         * 轉成 unsigned char 是因為中文的 byte 都 ≥ 0x80，用有號的 char 會變成負數，比大小會出錯。 */
        for (const unsigned char *p = (const unsigned char *)text; *p; ) {
            char one[8] = {0};                        /* 放「一個字元」的小字串；全部先填 0，結尾自然有 '\0' */
            int k = utf8_len(*p);
            for (int j = 0; j < k && p[j]; j++) one[j] = (char)p[j];
            p += strlen(one);                         /* 往後跳過這個字元（1～4 bytes） */
            add_leaf(one, 1);
        }
    }
    qsort(node, (size_t)n_leaf, sizeof(Node), by_symbol_name);           /* 葉依符號排序：讓每次執行、每種語言的版本結果一致 */
    n_node = n_leaf;                                                /* 內部節點從葉的後面開始放 */
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
    /* 熵（講義 1.3）：p 是這種符號出現的機率；log2(1/p) 是「這種符號理想上該用幾 bits」，越少見的越長。
     * (double) 是強制轉型：兩個整數相除在 C 裡會無條件捨去（3/4 = 0），所以要先轉成浮點數再除。 */
    double H = 0.0;
    printf("    symbol       次數        p    log2(1/p)   p·log2(1/p)\n");
    for (int i = 0; i < n_leaf; i++) {
        double p = (double)node[i].weight / (double)total;
        H += p * log2(1.0 / p);
        printf("    %-10s %6ld   %6.4f   %8.3f   %10.3f\n", label(i), node[i].weight, p, log2(1.0 / p), p * log2(1.0 / p));
    }
    printf("  共 %ld 個符號、%d 種；熵 H = %.4f bits／符號\n\n", total, n_leaf, H);
    pause_step();

    /*---------------------------------------------------------------- 步驟 1：排序，建立佇列
     * 把 K 個葉一個一個 queue_insert 進去，全部插完佇列就排好了：這就是 insertion sort。 */
    printf(C_HEAD "步驟 1｜把葉節點依 weight 由小到大排好，放進佇列（insertion sort：一個一個插到該在的位置）" C_RESET "\n");
    for (int i = 0; i < n_leaf; i++) {
        int pos = queue_insert(i);
        printf("  插入 [%s:%ld] → 位置 %d    ", label(i), node[i].weight, pos);
        for (int j = 0; j < q_len; j++) printf("%s[%s:%ld]%s ", j == pos ? C_NEW : "", label(queue[j]), node[queue[j]].weight, j == pos ? C_RESET : "");
        printf("\n");
    }
    printf("\n");
    pause_step();

    /*---------------------------------------------------------------- 步驟 2：反覆合併（Huffman 演算法的核心）
     * 佇列永遠是排好的，所以「最小的兩個」就是 queue[0] 與 queue[1]。每一輪佇列少 2 個、多 1 個，K-1 輪後只剩樹根。 */
    printf(C_HEAD "步驟 2｜反覆：取出最前面兩個（最小的兩個）→ 合併成新節點 → 插回佇列，直到只剩一個" C_RESET "\n\n");
    int round = 1;
    while (q_len > 1) {
        printf(C_HEAD "  第 %d 輪" C_RESET "\n", round++);
        print_queue("合併前的佇列（黃色是要取出的兩個）", 2, -1);
        int a = queue[0], b = queue[1];
        /* dequeue 兩次：後面的全部往前挪 2 格。memmove(目的, 來源, 幾個 byte)；queue + 2 是「第 2 格的位址」；
         * 單位是 byte 所以要乘 sizeof(int)。來源與目的有重疊時要用 memmove，不能用 memcpy。 */
        memmove(queue, queue + 2, sizeof(int) * (size_t)(q_len - 2));
        q_len -= 2;

        int p = n_node++;                         /* 在 node[] 的尾巴拿一個新的空格當爸爸 */
        node[p].name[0] = '\0';
        node[p].weight = node[a].weight + node[b].weight;
        node[p].left = a;  node[p].right = b;  node[p].parent = -1;   /* 先取出來的（較小、或一樣大但先排隊的）放左邊 */
        node[a].parent = node[b].parent = p;      /* 兩個小孩都記下爸爸是誰：步驟 3 要靠它往上走 */
        int pos = queue_insert(p);

        printf("  取出 [%s:%ld] 與 [%s:%ld] → 新節點 #%d，weight = %ld + %ld = %ld，left=%d right=%d；插回佇列的位置 %d\n",
               label(a), node[a].weight, label(b), node[b].weight, p, node[a].weight, node[b].weight, node[p].weight, a, b, pos);
        print_queue("合併後的佇列（綠色是新節點）", 0, p);
        print_memory(a, b, p);
        printf("\n");
        pause_step();
    }
    int root = queue[0];                          /* 佇列裡剩下的最後一個就是樹根 */
    if (n_leaf == 1) printf("  只有一種符號：不需要合併。code 長度定為 1（不能是 0，否則解碼端不知道有幾個符號）。\n\n");

    /*---------------------------------------------------------------- 步驟 3：讀出 code
     * 每個節點只記了 parent，所以從葉出發往上走最方便；但 code 是「從根往下」讀的，所以走完要倒過來。 */
    printf(C_HEAD "步驟 3｜樹建好了（根是 #%d）。左分支 0、右分支 1" C_RESET "\n", root);
    print_tree(root, "", -1);
    printf("\n  每個葉沿著 parent 一路走到根，記下自己是左（0）還是右（1）小孩；走到根之後把順序倒過來就是 code：\n");
    static char code[MAX_SYM][MAX_NODE];          /* code[i] 是第 i 種符號的 code，存成 "0110" 這種字串（方便印，不是真的 bit） */
    long total_bits = 0;
    printf("    symbol       次數   走訪：葉 → 根，#節點(自己是左0/右1)               長度  code\n");
    for (int i = 0; i < n_leaf; i++) {
        char rev[MAX_NODE], path[256] = "";
        int len = 0;
        for (int c = i; node[c].parent >= 0; c = node[c].parent) {     /* c 從葉開始，每次換成自己的爸爸，直到沒有爸爸（根） */
            int bit = (node[node[c].parent].right == c);               /* 我是爸爸的右小孩嗎？是 → 1，不是 → 0（C 的比較結果就是 1 或 0） */
            rev[len++] = (char)('0' + bit);
            char hop[32];
            snprintf(hop, sizeof(hop), "%s#%d(%d)", len > 1 ? " " : "", node[c].parent, bit);
            strncat(path, hop, sizeof(path) - strlen(path) - 1);
        }
        if (len == 0) rev[len++] = '0';                                /* 只有一種符號 */
        for (int j = 0; j < len; j++) code[i][j] = rev[len - 1 - j];   /* 倒過來：rev 的最後一個變成 code 的第一個 */
        code[i][len] = '\0';
        total_bits += node[i].weight * len;                            /* 這種符號總共佔幾 bits = 次數 × code 長度 */
        printf("    %-10s %6ld   %-50s %4d  %s\n", label(i), node[i].weight, path, len, code[i]);
    }
    printf("  所有符號都在葉上，所以沒有任何 code 是另一個 code 的開頭（prefix code）：位元流不用分隔符號也能唯一解碼。\n\n");
    pause_step();

    /*---------------------------------------------------------------- 步驟 4：編碼與比較
     * 編碼 = 把原文每個符號換成它的 code 接起來。這裡只「印」出 0 與 1 的字串；
     * 真的要存檔或傳送，得把每 8 個 bit 塞進 1 個 byte（bit packing）——那是 MP4 與 Team 1 要你自己寫的部分。 */
    printf(C_HEAD "步驟 4｜編碼，並和熵、定長編碼比較" C_RESET "\n");
    if (seq_len > 0) {
        printf("  ");
        for (int i = 0; i < seq_len && i < 48; i++) printf("%s ", code[seq[i]]);
        printf("%s\n", seq_len > 48 ? "..." : "");
    }
    double L = (double)total_bits / (double)total;
    int flc = 1;                                  /* 定長編碼每個符號要幾 bits：最小的 n 使得 2^n ≥ 種類數 */
    while ((1 << flc) < n_leaf) flc++;            /* 1 << n 是把 1 往左移 n 位，也就是 2 的 n 次方 */
    printf("  平均碼長 L = Σ p·len = %ld ÷ %ld = %.4f bits／符號；熵 H = %.4f；H ≤ L < H+1：%s；效率 H÷L = %.1f%%\n",
           total_bits, total, L, H,
           n_leaf == 1 ? "例外（只有一種符號時 H = 0，但 code 長度不能是 0，所以 L = 1）"
                       : (H <= L + 1e-12 && L < H + 1.0) ? "成立" : "不成立", 100.0 * H / L);
    printf("  定長編碼（FLC）每個符號 %d bits → %ld bits；Huffman %ld bits；壓縮率 %.1f%%（約 %.2f:1）\n",
           flc, flc * total, total_bits, 100.0 * (double)total_bits / (double)(flc * total), (double)(flc * total) / (double)total_bits);
    printf("  注意：解碼端還需要 codebook，這裡還沒算它的大小。\n");
    return 0;
}
