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
 *----------------------------------------------------------------------------
 *  【這個檔案在程式裡的位置】殼只在三個地方呼叫這兩個函式，而且完全不看區塊裡面長什麼樣子：
 *      聊天送出   chat.c      huff_encode(文字, SYM_CHAR) → 整塊當成一個 TEXT_HUFF frame 的 payload
 *      聊天收到   chat.c      huff_decode(payload, max_out = TL_MAX_TEXT - 1) → 檢查 UTF-8 → 顯示
 *      傳檔案     transfer.c  huff_encode(整個檔案, 依副檔名決定 sym；回傳 TL_ERR_DATA 就改用 SYM_BYTE 再呼叫一次)
 *                             → 切成多個 FILE_DATA 送出 → 對方收齊後 huff_decode(整塊, max_out = 宣稱的原始大小)
 *  也就是說：兩個函式都是「記憶體進、記憶體出」，和 socket、檔案完全無關。
 *  所以可以（也應該）只用 make test 離線把它們做對，全部 PASS 了再接上網路。
 *
 *  【你們要做的事】TODO 4 與 TODO 5，外加你們自己決定要切出來的輔助函式與資料結構。
 *  這是五個 TODO 裡份量最重的一塊，不要想一次寫完。建議的節奏：
 *      先只做 SYM_BYTE（符號就是 byte，不用切）→ make test 的「符號 = byte」那一組全過
 *      → 加 SYM_CHAR → 加 SYM_S16。三種符號只差「怎麼切出符號、怎麼把符號寫回 bytes」，
 *      中間的統計、建樹、codebook、位元打包是同一套，寫成共用的函式。
 *  動手之前，兩個人一起把區塊格式畫在紙上（每個欄位幾 bytes、什麼順序），先寫進 docs/interface.md 再寫程式：
 *  encode 和 decode 常常是不同人寫的，格式沒有先講好，兩邊一定對不起來。
 *
 *  【先複習、先看做完的樣子】
 *      第 3 週講義 1.3（熵）、1.6（Huffman 由下往上合併）、1.7（符號的定義、codebook 的成本）、
 *      2.6（WAV 的 chunk 結構）、2.7（sample 的 histogram）
 *      lectures/wk03_0921_team1-kickoff/slides/huffman_steps.html   建樹的逐步動畫
 *      同一週 examples/huffman_trace.c   用兩個陣列表示樹與佇列、建樹、由樹讀出 code（沒有位元打包、codebook、解碼）
 *      同一週 examples/entropy.c         byte 與 UTF-8 字元兩種符號的 histogram
 *      同一週 examples/wav_info.py       逐個 chunk 走訪 WAV
 *      ../python_ref/textlink.py --probe inspect 檔案   每一步的中間結果，拿來對你們 C 程式算出來的 N、K 與 code 長度
 *
 *  【這個檔案會用到、而你可能還不熟的 C】
 *    static 函式   在函式定義前面加 static，表示「只有這個 .c 檔看得到」。你們的輔助函式請都加上：
 *                  不會和別的檔案的函式撞名，也不必在 textlink.h 宣告。
 *                  （函式要寫在第一次被呼叫的位置之前；不然就在檔案前面先放一行它的宣告。）
 *    大陣列        區域變數放在 stack，空間通常只有 1–8 MB；幾十萬格以上的表格放不下，程式一執行就當掉。
 *                  大表格請用 malloc 或 calloc 配置。calloc(格數, 每格幾 bytes) 和 malloc 的差別是
 *                  它會把內容全部清成 0，很適合拿來當計數用的表格。兩者都可能回傳 NULL，都要 free。
 *    struct        樹的節點有好幾個欄位（出現次數、左右子節點……），用 struct 綁成一包、再開一個 struct 的陣列；
 *                  huffman_trace.c 的 Node 就是例子。用陣列索引表示「誰是誰的子節點」，可以完全不用指標。
 *    整數的寬度    64 MiB 的檔案最多有幾千萬個符號，樹根的權重就是所有次數的總和：想清楚每個數量
 *                  用 uint32_t 還是 uint64_t 才不會溢位。sample 是有號的 −32768 到 32767，
 *                  拿來當陣列索引之前，要先對應到 0 到 65535。
 *    回傳前的清理  中途任何一步失敗都要把已經配置的記憶體全部 free 掉再回傳。
 *                  transfer.c 的 file_send_frames 示範了一種整理方式：所有指標一開始設成 NULL，
 *                  失敗時跳到函式結尾同一段清理程式（對 NULL 呼叫 free 是安全的）。
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
 *
 *   契約整理：
 *     輸入  in[0..in_len)：原始 bytes，只讀；in_len 可以是 0（這時 in 指向哪裡都不可以去讀）
 *           sym：SYM_BYTE／SYM_CHAR／SYM_S16
 *     輸出  *out：你們 malloc 的一塊記憶體，內容是完整的區塊（符號種類、長度資訊、codebook、bitstream，
 *                 SYM_S16 還包含那些不是 sample 的 bytes）；*out_len：這一塊有幾個 byte
 *     區塊要能「獨立解碼」：huff_decode 只拿得到這一塊和它的長度，拿不到 sym、拿不到原始長度、
 *           也拿不到上一次呼叫留下的任何東西。所以不要用全域變數在 encode 與 decode 之間傳資料——
 *           實際執行時，encode 和 decode 是在兩台不同的電腦上。
 *     in_len 為 0 也要回傳 TL_OK 並產生一個 huff_decode 解得回「0 個 byte」的區塊。
 *     失敗時的記憶體規則（不要讓 *out 留著已經 free 的位址）見 include/textlink.h 的 huff_encode 說明。
 *   同一份輸入每次都要產生可以解回來的結果；區塊內容不必和別組或 python_ref 相同
 *   （除非你們想和 python_ref 用 --huff 互通，那就要採用它的格式）。
 *   對應的測試：tests/test_codec.c 的 test_huffman。
 *     huff_roundtrip(…)    encode → decode → 和原始資料逐 byte 比對；test_huffman 裡每一個呼叫就是一種邊界情況
 *     huff_must_reject(…)  encode 必須回傳 TL_ERR_DATA：含 C0 80 的文字、結尾被截斷的字元、8-bit 的 WAV、不是 WAV 的資料
 *     另有兩項比大小：同一份資料以字元（或 sample）為符號，輸出要比以 byte 為符號小——
 *     符號切錯了、或 codebook 存得太浪費，這兩項就不會過。
 *   想法：
 *     - 拿講義的 ABRACADABRA，先用紙筆做完整個流程（次數 → 樹 → 每個符號的 code → 位元串 → 切成 bytes），
 *       再用你們的程式跑同一個字串，把每一步的中間結果印出來和紙上的對。
 *     - 需要走訪資料兩趟：第一趟統計，第二趟才編碼（要先有 codebook 才能編碼）。
 *     - 在 SYM_S16，先回答這個問題：檔案裡哪些 bytes 是 sample、哪些不是？不是的那些要怎麼原樣還原？
 *     - codebook 與 bitstream 的位元順序（一個 byte 裡先填高位還是低位）兩邊要一致；這是最常見的 bug 來源。
 *   下面一整行 (void)… 只是讓編譯器不要警告「參數沒用到」，開始寫之後請拿掉。
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
 *
 *   為什麼空輸入也要 malloc(1)：呼叫端成功後會 free(*out)，給它一個真的配置過的位址最單純
 *   （malloc(0) 回傳什麼，各平台不一樣）。
 *   對應的測試：每一個通過的 huff_roundtrip（輸入不是空的）會再多測兩項壞輸入——
 *     把區塊截掉後半（in_len 變成一半）再解：必須回傳錯誤；
 *     max_out 給「原始長度 - 1」再解：必須回傳錯誤。
 *     兩項都只要求「不是 TL_OK」，但請照上面的規定回傳 TL_ERR_DATA。
 *   想法：
 *     - 把 in 當成一份「陌生人填的表單」：每讀一個欄位之前先問「剩下的 bytes 夠不夠讀這個欄位？」，
 *       每相信一個數字之前先問「這個數字合理嗎？最大可能是多少？」。
 *       寫一個「讀下一個欄位、不夠就回報失敗」的小函式，比在每個地方各自檢查不容易漏。
 *     - 檢查的順序很重要：所有「對方宣稱的大小」都要先和 max_out、in_len 比過，才可以拿去 malloc 或當迴圈次數。
 *     - 什麼時候停：靠區塊裡記載的數量，不是靠「bitstream 讀完了」（最後一個 byte 後面補的 0 不是資料）。
 *     - 自己再多做幾種破壞來測（V 角色的工作）：改掉區塊的第一個 byte、把中間某個 byte 換掉、
 *       只留前 1 個 byte、in_len 給 0。程式可以回報錯誤，但不可以當掉、不可以卡住不動。
 *-------------------------------------------------------------------------*/
int huff_decode(const uint8_t *in, size_t in_len, size_t max_out, uint8_t **out, size_t *out_len) {
    (void)in; (void)in_len; (void)max_out; (void)out; (void)out_len;
    return TL_ERR_TODO;
}
