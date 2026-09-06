// 需求重點 stdin/stdout (stdin用rb，CR回車用"\r"表示，LF換行用"\n"表示，輸出CSV)
// 輸入可能是ANSI(ASCII+BIG-5)+UTF-8混合，以UTF-8優先解碼，之後嘗試Big5，最後才是單位元組

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <locale.h>
#include <stdint.h>

#ifdef _WIN32
  #include <io.h>
  #include <fcntl.h>
  #include <windows.h>  // 用於Big5 CP950(Code Page 950)轉寬字元
#endif

#define TOTAL_UNICODE_NUM 65536  // 支援BMP區間的Unicode字元2^16

typedef struct {
    wchar_t character;    // 字元
    int count;            // 出現次數
    double probability;   // 出現機率
} CharInfo;

// 全域變數
static int numUnicode[TOTAL_UNICODE_NUM] = {0};
static int totalCharacters = 0;

// 函式宣告
static void countUnicode(FILE *f);
static void calculateProbability(CharInfo *charInfo, int totalCharacters);
static int  compare_count_char(const void *a, const void *b);
static void printResult(CharInfo *charInfo);

// UTF-8解碼一個碼(不合就回傳used=0)
static size_t utf8_try_decode_one(const unsigned char* s, size_t n, uint32_t* out_cp) {
    if (n == 0) return 0;
    unsigned char b0 = s[0];

    // 1 byte:0xxxxxxx
    if ((b0 & 0x80u) == 0) { *out_cp = b0; return 1; }

    // 2 byte:110xxxxx 10xxxxxx
    if ((b0 & 0xE0u) == 0xC0u && n >= 2) {
        unsigned char b1 = s[1];
        if ((b1 & 0xC0u) == 0x80u) {
            uint32_t cp = ((b0 & 0x1Fu) << 6) | (b1 & 0x3Fu);
            if (cp >= 0x80) { *out_cp = cp; return 2; } // 避免 overlong
        }
        return 0;
    }

    // 3 byte:1110xxxx 10xxxxxx 10xxxxxx
    if ((b0 & 0xF0u) == 0xE0u && n >= 3) {
        unsigned char b1 = s[1], b2 = s[2];
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u) {
            uint32_t cp = ((b0 & 0x0Fu) << 12) | ((b1 & 0x3Fu) << 6) | (b2 & 0x3Fu);
            if (cp >= 0x800 && !(cp >= 0xD800 && cp <= 0xDFFF)) { *out_cp = cp; return 3; }
        }
        return 0;
    }

    // 4 byte:11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    if ((b0 & 0xF8u) == 0xF0u && n >= 4) {
        unsigned char b1 = s[1], b2 = s[2], b3 = s[3];
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u && (b3 & 0xC0u) == 0x80u) {
            uint32_t cp = ((b0 & 0x07u) << 18) | ((b1 & 0x3Fu) << 12) |
                          ((b2 & 0x3Fu) << 6)  |  (b3 & 0x3Fu);
            if (cp >= 0x10000 && cp <= 0x10FFFF) { *out_cp = cp; return 4; }
        }
        return 0;
    }
    return 0; // 其他情況視為不合法UTF-8起頭
}

// 嘗試Big5兩位元組轉寬字元(限Windows)失敗回0
static size_t big5_try_decode_two(const unsigned char* s, size_t n, wchar_t* out_wc) {
#if defined(_WIN32)
    if (n < 2) return 0;
    unsigned char b1 = s[0], b2 = s[1];

    // Big5的先後位元組大致範圍(常見對照表)
    // lead: 0x81-0xFE trail: 0x40-0x7E 或 0xA1-0xFE
    int is_lead = (b1 >= 0x81 && b1 <= 0xFE);
    int is_trail = (b2 >= 0x40 && b2 <= 0x7E) || (b2 >= 0xA1 && b2 <= 0xFE);
    if (!is_lead || !is_trail) return 0;

    wchar_t wc = 0;
    int chars = MultiByteToWideChar(950 /*CP950 Big5*/, 0, (LPCCH)s, 2, &wc, 1);
    if (chars == 1) { *out_wc = wc; return 2; }
    return 0;
#else
    (void)s; (void)n; (void)out_wc;
    return 0;
#endif
}

int main(void) {
    setlocale(LC_ALL, "");  // 讓寬字元能正確輸出(printResult仍用 %lc)

#ifdef _WIN32
    //stdin/stdout設為binary 避免CR LF轉換
    _setmode(_fileno(stdin),  _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    countUnicode(stdin);    // 從標準輸入讀取字元 rb

    // 建立字元陣列
    CharInfo charInfo[TOTAL_UNICODE_NUM];
    for (int i = 0; i < TOTAL_UNICODE_NUM; i++) {
        charInfo[i].character = (wchar_t)i;
        charInfo[i].count = numUnicode[i];
        charInfo[i].probability = 0.0;
    }

    calculateProbability(charInfo, totalCharacters);

    // 排序規則：count次數由多到少，再字元編號由小到大
    qsort(charInfo, TOTAL_UNICODE_NUM, sizeof(CharInfo), compare_count_char);

    // 輸出結果
    printResult(charInfo);

    return 0;
}

// 計算字元次數(以rb模式讀入 優先UTF-8 之後Big5 最後單位元組)
// \r與\n獨立計算
static void countUnicode(FILE *f) {
    unsigned char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        size_t i = 0;
        while (i < n) {
            uint32_t cp = 0;
            size_t used = utf8_try_decode_one(buf + i, n - i, &cp);
            if (used > 0) {
                i += used;
                if (cp < TOTAL_UNICODE_NUM) {
                    numUnicode[(unsigned)cp]++;
                    totalCharacters++;
                }
                continue;
            }

            // UTF-8不成立 試Big5兩位元組
            wchar_t wc = 0;
            size_t used_b5 = big5_try_decode_two(buf + i, n - i, &wc);
            if (used_b5 > 0) {
                i += used_b5;
                if ((unsigned)wc < TOTAL_UNICODE_NUM) {
                    numUnicode[(unsigned)wc]++;
                    totalCharacters++;
                }
                continue;
            }

            // 仍不成立 回到單位元組
            unsigned char b = buf[i++];
            if (b < TOTAL_UNICODE_NUM) {
                numUnicode[(unsigned)b]++;
                totalCharacters++;
            }
        }
    }
}

// 計算機率
static void calculateProbability(CharInfo charInfo[], int total) {
    if (total == 0) return;
    for (int i = 0; i < TOTAL_UNICODE_NUM; i++) {
        if (charInfo[i].count > 0) {
            charInfo[i].probability = (double)charInfo[i].count / (double)total;
        }
    }
}

// 排序規則：count次數由多到少，再字元編號由小到大
static int compare_count_char(const void *a, const void *b) {
    const CharInfo *x = (const CharInfo *)a;
    const CharInfo *y = (const CharInfo *)b;

    if (x->count < y->count) return  1;  // 出現次數多的在前
    if (x->count > y->count) return -1;

    if (x->character < y->character) return -1; // 字元編號小的在前
    if (x->character > y->character) return  1;

    return 0;
}

// 輸出結果 CSV格式
static void printResult(CharInfo charInfo[]) {
    for (int i = 0; i < TOTAL_UNICODE_NUM; i++) {
        if (charInfo[i].count > 0) {
            switch (charInfo[i].character) {
                case L'\n':  // LF換行
                    printf("\"\\n\",%d,%.15f\n",
                           charInfo[i].count, charInfo[i].probability);
                    break;
                case L'\r':  // CR回車
                    printf("\"\\r\",%d,%.15f\n",
                           charInfo[i].count, charInfo[i].probability);
                    break;
                case L'\t':  // Tab
                    printf("\"\\t\",%d,%.15f\n",
                           charInfo[i].count, charInfo[i].probability);
                    break;
                case L',':   // 逗號
                    printf("\",\",%d,%.15f\n",
                           charInfo[i].count, charInfo[i].probability);
                    break;
                case L'\"':  // 雙引號 """
                    printf("\"\"\",%d,%.15f\n",
                           charInfo[i].count, charInfo[i].probability);
                    break;
                case L'\\':  // 反斜線 "\"
                    printf("\"\\\",%d,%.15f\n",
                           charInfo[i].count, charInfo[i].probability);
                    break;
                default:     // 一般字元
                    printf("\"%lc\",%d,%.15f\n",
                           charInfo[i].character, charInfo[i].count, charInfo[i].probability);
                    break;
            }
        }
    }
}