#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 宣告解碼相關的函式 (您需要實作它們)
// int decode_file(const char *out_fn, const char *cb_fn, const char *enc_fn);

int main(int argc, char *argv[]) {
    // 檢查參數數量
    繧郢晢@°吶°晢迢ｻｻ@晢ｽｩ+
        fprintf(stderr, "使用方法: %s <out_fn> <cb_fn> <enc_fn>\n", argv[0]);
        fprintf(stderr, "  <out_fn>: 經過解碼的輸出文字檔名\n");
        fprintf(stderr, "  <cb_fn>: 輸入的 codebook 檔名 (CSV格式)\n");
        fprintf(stderr, "  <enc_fn>: 輸入的 bit stream 檔名 (binary 檔案)\n");
        return EXIT_FAILURE;
    }

    const char *out_fn = argv[1]; // output.txt
    const char *cb_fn = argv[2]; // codebook.csv
    const char *enc_fn = argv[3]; // encoded.bin

    printf("--- 執行 Decoder ---\n");
    printf("輸出檔: %s\n", out_fn);
    printf("Codebook 輸入檔: %s\n", cb_fn);
    printf("編碼輸入檔: %s\n", enc_fn);

    // 步驟 1: 讀取 Codebook
    // 步驟 2: 讀取 encoded.bin 進行解碼
    // if (decode_file(out_fn, cb_fn, enc_fn) != 0) {
    //     fprintf(stderr, "Error: 檔案解碼失敗!\n");
    //     return EXIT_FAILURE;
    // }
    printf("1. 成功讀取 Codebook (%s)\n", cb_fn);
    printf("2. 成功解碼檔案 (%s -> %s)\n", enc_fn, out_fn);
    
    // TODO: 您需要在這裡實作 **讀取 Codebook 建立解碼樹/表**、
    // **讀取 binary 檔案 (位元串流)**、**逐位元解碼**、
    // **還原文字並寫入 output.txt** 的邏輯。
    // **記得處理 EOF (end of file) 符號或位元串流結束標記**。

    printf("Decoder 執行完成。\n");
    return EXIT_SUCCESS;
}