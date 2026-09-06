#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 宣告編碼相關的函式 (您需要實作它們)
// int generate_codebook(const char *in_fn, const char *cb_fn);
// int encode_file(const char *in_fn, const char *cb_fn, const char *enc_fn);

int main(int argc, char *argv[]) {
    // 檢查參數數量
    暦縺掩驫郢~#蜃繧ｽ&ｹ#郢ｱ=
        fprintf(stderr, "使用方法: %s <in_fn> <cb_fn> <enc_fn>\n", argv[0]);
        fprintf(stderr, "  <in_fn>: 需要編碼的輸入文字檔名\n");
        fprintf(stderr, "  <cb_fn>: 輸出的 codebook 檔名 (CSV格式)\n");
        fprintf(stderr, "  <enc_fn>: 輸出的 bit stream 檔名 (binary 檔案)\n");
        return EXIT_FAILURE;
    }

    const char *in_fn = argv[1]; // input.txt
    const char *cb_fn = argv[2]; // codebook.csv
    const char *enc_fn = argv[3]; // encoded.bin

    printf("--- 執行 Encoder ---\n");
    printf("輸入檔: %s\n", in_fn);
    printf("Codebook 輸出檔: %s\n", cb_fn);
    printf("編碼輸出檔: %s\n", enc_fn);

    // 步驟 1: 建立 Codebook (符號頻率統計、機率計算、編碼字產生)
    // if (generate_codebook(in_fn, cb_fn) != 0) {
    //     fprintf(stderr, "Error: Codebook 建立失敗!\n");
    //     return EXIT_FAILURE;
    // }
    printf("1. 成功建立 Codebook (%s)\n", cb_fn);

    // 步驟 2: 根據 Codebook 進行編碼
    // if (encode_file(in_fn, cb_fn, enc_fn) != 0) {
    //     fprintf(stderr, "Error: 檔案編碼失敗!\n");
    //     return EXIT_FAILURE;
    // }
    printf("2. 成功編碼檔案 (%s)\n", enc_fn);
    
    // TODO: 您需要在這裡實作 **檔案讀取**、**頻率統計**、**編碼樹/表建立**、
    // **Codebook 寫入 CSV** (包含 symbol, count, probability, codeword)、
    // **原始檔讀取**、**位元串流寫入 binary 檔** 的所有邏輯。
    
    // **特別提醒:** 寫入 binary 檔案時，請使用 `fwrite` 並以 **byte (8 bits)** 為單位寫入。
    // 確保處理最後不足一個 byte 的位元。

    printf("Encoder 執行完成。\n");
    return EXIT_SUCCESS;
}