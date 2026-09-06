#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <locale.h>

#define MAX_CHARS 65536  // 可記錄的 Unicode 字元數 (UTF-16 BMP範圍)

int main() {
    // 設定地區，讓 fgetwc() 能正確處理 UTF-8 / Big5
    setlocale(LC_ALL, "");

    FILE *fin = stdin;
    FILE *fout = stdout;

    long count[MAX_CHARS] = {0}; // 計數陣列
    long total = 0; // 總共讀到的字元數

    wint_t wc;  // 用來暫存每次讀取到的 Unicode 字元
    while ((wc = fgetwc(fin)) != WEOF) {    //從 fin 讀取一個 wide char，直到檔案結尾 WEOF
        if (wc < MAX_CHARS) {
            count[wc]++;
            total++;
        }
    }

    // 輸出結果到 fout (CSV 格式)
    for (int i = 0; i < MAX_CHARS; i++) {
        if (count[i] > 0) { // 只輸出出現過的字元
            double prob = (double)count[i] / total; // 計算出現機率

            if (i == L'\n') // 處理特殊字元，方便讀取
                fwprintf(fout, L"\"\\n\",%ld,%.15f\n", count[i], prob);
            else if (i == L'\r')
                fwprintf(fout, L"\"\\r\",%ld,%.15f\n", count[i], prob);
            else if (i == L' ')
                fwprintf(fout, L"\"space\",%ld,%.15f\n", count[i], prob);
            else if (i == L'\t')
                fwprintf(fout, L"\"\\t\",%ld,%.15f\n", count[i], prob);
            else
                fwprintf(fout, L"\"%lc\",%ld,%.15f\n", (wchar_t)i, count[i], prob);
        }
    }

    return 0;
}