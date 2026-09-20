/*============================================================================
 *  huffman.c  —  Huffman 編碼與解碼（本專題的主菜）
 *----------------------------------------------------------------------------
 *  Huffman coding 的第一個問題不是「怎麼建樹」，而是「符號是什麼」——
 *  你對什麼東西統計出現機率，就決定了能壓到多小。本專題規定：
 *
 *    SYM_CHAR  文字：符號 = UTF-8 字元（code point）
 *              先把 bytes 切成一個一個「字」（MP1 做過的事），統計每個字出現的機率，再編碼。
 *              「多」是一個符號，不是 E5、A4、9A 三個符號。
 *    SYM_S16   WAV ：符號 = 16-bit sample value
 *              解析 RIFF 檔頭找到 data 區，把每 2 bytes（little-endian）當成一個 sample，
 *              對 sample 值做 histogram，再編碼。雙聲道就是左右交錯的 sample，一樣處理。
 *              data 區以外的 bytes（檔頭、其他 chunk）不是 sample，要原樣保留在區塊裡。
 *    SYM_BYTE  其他：符號 = byte。任何資料都適用；也是上面兩種不適用時的退路。
 *
 *  為什麼要這樣分？把「多」拆成三個 byte 分開統計，等於丟掉「E5 後面常接 A4」這種資訊；
 *  把一個 sample 拆成高、低兩個 byte 也是。符號定得對，同一套 Huffman 演算法壓縮率差很多
 *  ——報告要你們用數字比較（規格「壓縮率」一節）。代價是符號種類變多（sample 最多 65,536 種、
 *  字元上千種），codebook 變大、建樹也不能再用「每輪線性找最小」的寫法。
 *
 *  huff_encode 產生的那一塊資料必須「自己帶 codebook、可以獨立解碼」，建議的長相：
 *
 *      +------+----------+--------+----------------+-------------------+------------------+
 *      | 符號 | 原始長度  | 符號數  | codebook       | （SYM_S16）檔頭等   | bitstream        |
 *      | 種類 | （bytes） |        | （怎麼存自訂）  | 非 sample 的 bytes | （末尾補 0）      |
 *      +------+----------+--------+----------------+-------------------+------------------+
 *
 *  codebook 怎麼存是最重要的設計決策：
 *      (a) 存整張頻率表：簡單，但 65,536 種 sample 就要幾百 KB，不可行
 *      (b) 只存出現過的符號與它的 code 長度，兩端用同一個規則重建 code（canonical Huffman）
 *      (c) 把樹的形狀用前序走訪存成位元（內部節點 0、葉節點 1 + 符號）
 *  每個欄位幾 bytes、位元組順序、位元順序，都要寫進 docs/interface.md。
 *===========================================================================*/
#include "textlink.h"

/*--------------------------------------------------------------------------
 * 建議的內部步驟（函式怎麼切、資料結構怎麼定，由你們決定）：
 *
 *   0. 切符號          SYM_CHAR：逐字元解 UTF-8（遇到非法序列 → TL_ERR_DATA）
 *                      SYM_S16 ：走訪 RIFF 的 chunk 找 "fmt "（確認 PCM、16 bits）與 "data"；
 *                                data 不一定緊接在第 44 byte（中間可能有 LIST chunk）；
 *                                不是 16-bit PCM 的 WAV → TL_ERR_DATA
 *                      建議寫成「取下一個符號」的函式，統計與編碼兩趟都用它，
 *                      不要把所有符號另存成陣列（64 MiB 的檔案會變成 256 MiB）
 *   1. 統計頻率        以符號值為索引的陣列最簡單：byte 256 格、sample 65,536 格、
 *                      字元 0x110000 格（約 4 MB，可接受）；再收集出現過的 K 種符號
 *   2. 建 Huffman tree 每次取出頻率最小的兩個節點合併。K 最多數萬，請用 heap，
 *                      或「先排序、再用兩個佇列」的 O(K log K) 做法
 *   3. 產生 codebook   code 長度可能超過 32 bits，想清楚用什麼型別存
 *   4. 位元打包        需要一個「bit writer」
 *   5. 解碼            「bit reader」一次讀 1 bit；依區塊記載的符號種類把符號還原成 bytes
 *                      （字元 → UTF-8 的 1–4 bytes；sample → 2 bytes little-endian）
 *
 * 一定要處理的邊界（tests/test_codec.c 會測，評測也會測）：
 *   - in_len == 0；只有一種符號（樹只有一個葉，code 長度不能是 0）
 *   - SYM_CHAR：1–4 bytes 的字元都有、開頭有 BOM、含 \r\n；還原後逐 byte 相同
 *   - SYM_S16 ：data 前後有其他 chunk、data 長度是奇數、只有檔頭沒有 sample、雙聲道
 *   - 最後一個 byte 沒填滿：解碼端要靠符號數知道該停了，不能多解出幾個符號
 *-------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------
 * ★ TODO 4：編碼
 *   sym 指定符號種類。資料不適用（見步驟 0）回傳 TL_ERR_DATA，殼會改用 SYM_BYTE 再呼叫一次。
 *   成功：*out = malloc 出來的結果、*out_len = 它的長度，回傳 TL_OK。
 *   記憶體不足回傳 TL_ERR_NOMEM。
 *-------------------------------------------------------------------------*/
int huff_encode(const uint8_t *in, size_t in_len, tl_sym_t sym, uint8_t **out, size_t *out_len) {
    (void)in; (void)in_len; (void)sym; (void)out; (void)out_len;
    return TL_ERR_TODO;
}

/*--------------------------------------------------------------------------
 * ★ TODO 5：解碼
 *   符號種類由區塊自己記載，所以這裡不需要 sym 參數。
 *   in 是「對方送來的」，要當成可能是壞的：
 *   - in_len 比你們格式的最小長度還短        → TL_ERR_DATA
 *   - 宣稱的原始長度 > max_out                → TL_ERR_DATA（不可以先 malloc 再說）
 *   - 宣稱的符號種類數多到 codebook 根本放不進 in_len → TL_ERR_DATA（不可以照著它去配置）
 *   - codebook 不合理（重複的符號、長度為 0、不是合法的 code point、建不出樹…）→ TL_ERR_DATA
 *   - bitstream 讀完了，符號數還沒到；或解出來的 bytes 數與宣稱的原始長度不符 → TL_ERR_DATA
 *   任何情況都不可以讀超過 in[in_len-1]、寫超過你配置的輸出緩衝區。
 *   成功：*out = malloc 出來的結果（空輸入時 malloc(1) 也可以）、*out_len = 原始長度，回傳 TL_OK。
 *-------------------------------------------------------------------------*/
int huff_decode(const uint8_t *in, size_t in_len, size_t max_out, uint8_t **out, size_t *out_len) {
    (void)in; (void)in_len; (void)max_out; (void)out; (void)out_len;
    return TL_ERR_TODO;
}
