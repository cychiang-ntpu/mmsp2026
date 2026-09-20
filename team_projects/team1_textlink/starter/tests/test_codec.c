/*============================================================================
 *  test_codec.c  —  不用網路就能跑的單元測試：frame 標頭、UTF-8 檢查、Huffman（三種符號）
 *----------------------------------------------------------------------------
 *  執行：make test（Windows PowerShell：mingw32-make test）
 *  每一項的結果是 PASS／FAIL／TODO。TODO 代表那個 place holder 還沒寫。
 *  有任何 FAIL 或 TODO，結束碼就不是 0（make 會顯示 Error，這是正常的提醒）。
 *
 *  這只是起點：V（測試驗證）角色請繼續加測試，例如真的開 socket、把 frame
 *  切成 1 byte 1 byte 送、傳到一半斷線、把 codebook 改壞……
 *----------------------------------------------------------------------------
 *  【這個檔案在專案裡的位置】它是另一支獨立的程式（有自己的 main），不會被編進 textlink。
 *  Makefile 把它和 src/frame.c、src/utf8.c、src/huffman.c（以及提供 tl_strerror 等函式的 src/net.c）
 *  編在一起，直接呼叫你們的五個函式、檢查結果。不開 socket，所以一台電腦、不用網路就能跑。
 *
 *  【怎麼用它】它就是五個 TODO 的「答案卷」：每寫完一小段就跑一次 make test。
 *      TODO  函式還回傳 TL_ERR_TODO（還沒寫）
 *      FAIL  寫了，但結果和預期不同。每一項的名稱直接寫出輸入與預期的輸出，先照著名稱用紙筆算一次
 *      PASS  這一項對了
 *  一開始是 46 個 TODO；全部完成是 82 個 PASS（Huffman 的 round-trip 通過之後會再多測幾項，所以總數會變多）。
 *  如果程式跑到一半直接當掉、後面的結果沒有印出來（在終端機直接執行時）：最後印出來的那一項的「下一項」就是出事的測試，
 *  多半是讀寫超出陣列範圍，或 free 了不該 free 的位址。
 *
 *  【怎麼讀這個檔案】先看最下面的 main：依序呼叫 test_frame、test_utf8、test_huffman。
 *  每個測試都是同一個模式：準備輸入 → 呼叫你們的函式 → 把「結果對不對」交給 report 記錄。
 *  要加自己的測試，照樣複製一段、改輸入與預期結果就可以。
 *===========================================================================*/
#include "textlink.h"

/* 三個計數器寫在所有函式的外面，是全域變數：整個檔案的函式都能讀寫，程式執行期間一直存在。
 * 前面的 static 表示只有這個 .c 檔看得到它們（和 static 函式同一個意思）。 */
static int n_pass = 0, n_fail = 0, n_todo = 0;

/* 記錄並印出一項測試的結果。
 *   name     這一項的名稱
 *   rc_todo  不是 0 表示函式回傳了 TL_ERR_TODO → 算 TODO（這時 ok 是多少都不看）
 *   ok       不是 0 表示結果符合預期 → PASS；是 0 → FAIL
 * 呼叫的地方會直接把「比較運算式」當參數傳進來：rc == TL_ERR_TODO 這種式子成立時值是 1、不成立是 0。
 * 第一行的 a ? b : c 是條件運算子：a 成立取 b，否則取 c；連用兩次，就是三選一。 */
static void report(const char *name, int rc_todo, int ok) {
    const char *tag = rc_todo ? "TODO" : ok ? "PASS" : "FAIL";
    if (rc_todo) n_todo++; else if (ok) n_pass++; else n_fail++;
    printf("  [%s] %s\n", tag, name);
}

/*------------------------------- frame ------------------------------------*/
/* 六項，前三項測 frame_pack_header（TODO 1），後三項測 frame_parse_header（TODO 2）。
 *   uint8_t hdr[TL_HDR_LEN] = {0}：宣告 5 格的陣列並全部設成 0。
 *   const uint8_t good[…] = {0x00, …}：直接用一串 byte 當作「從網路收到的標頭」；const 表示之後不會改它。
 *   &type、&n：把變數的位址傳進去，讓函式把結果寫回來（見 include/textlink.h 檔頭的共通寫法 1）。
 *   第三項的 (size_t)TL_MAX_FRAME 是明確轉型：payload 剛好 TL_MAX_FRAME bytes 時 length 會超過上限 1，要被拒絕。
 *   report 的第三個參數用 && 把好幾個條件串起來，全部成立才算 PASS。 */
static void test_frame(void) {
    uint8_t hdr[TL_HDR_LEN] = {0}, type = 0;
    size_t n = 0;
    int rc;

    printf("frame 標頭\n");
    rc = frame_pack_header(hdr, T_TEXT_RAW, 3);
    report("pack：type=0x01、payload 3 bytes → 00 00 00 04 01", rc == TL_ERR_TODO,
           rc == TL_OK && hdr[0] == 0 && hdr[1] == 0 && hdr[2] == 0 && hdr[3] == 4 && hdr[4] == 0x01);

    rc = frame_pack_header(hdr, T_FILE_DATA, 65536);
    report("pack：payload 65536 bytes → 00 01 00 01 11（big-endian）", rc == TL_ERR_TODO,
           rc == TL_OK && hdr[0] == 0 && hdr[1] == 1 && hdr[2] == 0 && hdr[3] == 1 && hdr[4] == 0x11);

    rc = frame_pack_header(hdr, T_FILE_DATA, (size_t)TL_MAX_FRAME);
    report("pack：length 超過上限要拒絕", rc == TL_ERR_TODO, rc == TL_ERR_PROTO);

    const uint8_t good[TL_HDR_LEN] = {0x00, 0x00, 0x01, 0x00, 0x02};
    rc = frame_parse_header(good, &type, &n);
    report("parse：00 00 01 00 02 → type=0x02、payload 255 bytes", rc == TL_ERR_TODO,
           rc == TL_OK && type == 0x02 && n == 255);

    const uint8_t zero[TL_HDR_LEN] = {0, 0, 0, 0, 0x01};
    rc = frame_parse_header(zero, &type, &n);
    report("parse：length = 0 要拒絕", rc == TL_ERR_TODO, rc == TL_ERR_PROTO);

    const uint8_t huge[TL_HDR_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0x01};
    rc = frame_parse_header(huge, &type, &n);
    report("parse：length = FF FF FF FF 要拒絕", rc == TL_ERR_TODO, rc == TL_ERR_PROTO);
}

/*------------------------------- UTF-8 ------------------------------------*/
/* 一個 UTF-8 測試案例：bytes 指向 n 個 byte 的輸入，want 是預期的回傳值。
 * 字串常數的型別是 char *，而 utf8_validate 要的是 const uint8_t *，所以呼叫時用 (const uint8_t *) 轉型：
 * 只是換一種型別去看同一塊記憶體，bytes 本身沒有變。
 *
 * 【字串裡的 \x】"\xC3\xA9" 不是 8 個字元：\x 後面接十六進位數字，表示「值為這個數的一個 byte」，
 * 所以它是 C3 A9 兩個 bytes。測試用這種寫法，才能放進鍵盤打不出來的非法序列。
 * 長度 n 另外用參數給，不靠 strlen，因為 utf8_validate 的契約就是「指標 + 長度」。
 * 有些地方把字串拆成相鄰的兩段，例如 "abc\xC0\x80" "def"：C 會把相鄰的字串常數接成一個；
 * 拆開是因為 \x 會一直往後吃十六進位數字，不拆的話 \x80 後面的 d、e、f 會被當成同一個數的一部分。 */
static void utf8_case(const char *name, const char *bytes, size_t n, int want) {
    int rc = utf8_validate((const uint8_t *)bytes, n);
    report(name, rc == TL_ERR_TODO, rc == want);
}

static void test_utf8(void) {
    printf("UTF-8 合法性\n");
    utf8_case("合法：空字串",                 "", 0, TL_OK);
    utf8_case("合法：ASCII",                  "Hello, MMSP", 11, TL_OK);
    utf8_case("合法：2 bytes（é Ω я）",        "\xC3\xA9\xCE\xA9\xD1\x8F", 6, TL_OK);
    utf8_case("合法：3 bytes（多媒體）",        "\xE5\xA4\x9A\xE5\xAA\x92\xE9\xAB\x94", 9, TL_OK);
    utf8_case("合法：4 bytes（😀 𠮷）",         "\xF0\x9F\x98\x80\xF0\xA0\xAE\xB7", 8, TL_OK);
    utf8_case("合法：ZWJ 家庭 emoji（18 bytes）",
              "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7", 18, TL_OK);
    utf8_case("合法：最大 code point U+10FFFF", "\xF4\x8F\xBF\xBF", 4, TL_OK);
    utf8_case("非法：孤立的續位元組 80",        "\x80", 1, TL_ERR_DATA);
    utf8_case("非法：結尾被截斷 E5 A4",         "\xE5\xA4", 2, TL_ERR_DATA);
    utf8_case("非法：續位元組不對 E5 41 9A",    "\xE5\x41\x9A", 3, TL_ERR_DATA);
    utf8_case("非法：overlong C0 80",          "\xC0\x80", 2, TL_ERR_DATA);
    utf8_case("非法：overlong E0 80 80",       "\xE0\x80\x80", 3, TL_ERR_DATA);
    utf8_case("非法：overlong F0 80 80 80",    "\xF0\x80\x80\x80", 4, TL_ERR_DATA);
    utf8_case("非法：代理區 ED A0 80",          "\xED\xA0\x80", 3, TL_ERR_DATA);
    utf8_case("非法：超過 U+10FFFF（F4 90 80 80）", "\xF4\x90\x80\x80", 4, TL_ERR_DATA);
    utf8_case("非法：前導 byte F5",            "\xF5\x80\x80\x80", 4, TL_ERR_DATA);
    utf8_case("非法：FF",                      "\xFF", 1, TL_ERR_DATA);
}

/*------------------------------- Huffman ----------------------------------*/
/* 三個字串的陣列，用 tl_sym_t 的值（0、1、2）當索引，拿到要印在畫面上的名字 */
static const char *SYM_LABEL[3] = {"byte", "char", "s16"};

/* round-trip 一次；回傳編碼後的大小（失敗或 TODO 回傳 0），給後面比較不同符號定義用
 *
 * 流程：huff_encode(in) → huff_decode(編碼結果) → 檢查長度相同，而且 memcmp 逐 byte 比對內容相同
 *       （memcmp(a, b, n) 比較兩塊記憶體的前 n bytes，完全相同時回傳 0）。
 * 注意記憶體的用法，這就是 huff_encode／huff_decode 的呼叫端該有的樣子：
 *   enc、dec 一開始設成 NULL，把它們的位址 &enc、&dec 交給函式去填；用完之後由這裡 free。
 *   free(NULL) 是安全的、什麼都不做，所以 decode 失敗（dec 仍是 NULL）時最後的 free(dec) 也沒問題——
 *   前提是你們的函式失敗時沒有把一個已經 free 過的位址留在 *out 裡。
 * show_ratio 不是 0 時，名稱後面會加印「原始 → 編碼後 bytes 與百分比」（含 codebook）。
 * snprintf(label, sizeof(label), …) 和 printf 一樣，只是「印」到 label 這個字元陣列裡；
 *   第二個參數是陣列的容量，保證不會寫超過。(unsigned) 轉型是為了配合格式字串裡的 %u。
 * round-trip 成功而且輸入不是空的，才會接著測兩種壞輸入（所以 PASS 的總數比一開始的 TODO 多）。 */
static size_t huff_roundtrip(const char *name, const uint8_t *in, size_t n, tl_sym_t sym, int show_ratio) {
    uint8_t *enc = NULL, *dec = NULL;
    size_t enc_len = 0, dec_len = 0;
    char label[200];

    int rc = huff_encode(in, n, sym, &enc, &enc_len);
    if (rc == TL_ERR_TODO) { report(name, 1, 0); return 0; }
    if (rc != TL_OK) { report(name, 0, 0); return 0; }

    rc = huff_decode(enc, enc_len, n, &dec, &dec_len);
    int ok = (rc == TL_OK && dec_len == n && (n == 0 || memcmp(in, dec, n) == 0));
    if (show_ratio && n > 0)
        snprintf(label, sizeof(label), "[%s] %s（%u → %u bytes，%.1f%%，含 codebook）", SYM_LABEL[sym],
                 name, (unsigned)n, (unsigned)enc_len, 100.0 * (double)enc_len / (double)n);
    else
        snprintf(label, sizeof(label), "[%s] %s", SYM_LABEL[sym], name);
    report(label, rc == TL_ERR_TODO, ok);

    if (ok && n > 0) {
        /* 壞輸入：不可以當機、不可以越界，而且要回報失敗 */
        uint8_t *d2 = NULL;
        size_t d2_len = 0;
        rc = huff_decode(enc, enc_len / 2, n, &d2, &d2_len);           /* 截掉後半 */
        if (rc == TL_OK) free(d2);
        snprintf(label, sizeof(label), "[%s] %s：資料被截掉一半時要回報失敗", SYM_LABEL[sym], name);
        report(label, 0, rc != TL_OK);

        d2 = NULL;
        rc = huff_decode(enc, enc_len, n - 1, &d2, &d2_len);           /* max_out 比實際小 */
        if (rc == TL_OK) free(d2);
        snprintf(label, sizeof(label), "[%s] %s：解出來會超過 max_out 時要拒絕", SYM_LABEL[sym], name);
        report(label, 0, rc != TL_OK);
    }
    free(enc);
    free(dec);
    return ok ? enc_len : 0;
}

/* 資料不適用某種符號時，huff_encode 要回傳 TL_ERR_DATA（殼才會退回 SYM_BYTE） */
static void huff_must_reject(const char *name, const uint8_t *in, size_t n, tl_sym_t sym) {
    uint8_t *enc = NULL;
    size_t enc_len = 0;
    char label[200];
    int rc = huff_encode(in, n, sym, &enc, &enc_len);
    if (rc == TL_OK) free(enc);
    snprintf(label, sizeof(label), "[%s] %s → 要回傳 TL_ERR_DATA", SYM_LABEL[sym], name);
    report(label, rc == TL_ERR_TODO, rc == TL_ERR_DATA);
}

/* 自己寫的小亂數產生器（linear congruential generator）。不用 rand()，是因為 rand() 在每個平台產生的
 * 數列不同；這樣寫，每一台電腦、每一次執行的測試資料都完全一樣，FAIL 才能重現、才能除錯。 */
static uint32_t g_seed = 12345u;
static uint32_t lcg(void) { g_seed = g_seed * 1664525u + 1013904223u; return g_seed >> 8; }

/* 把 v 的最低 n 個 bytes 以 little-endian（低位的 byte 先寫）寫進 p[0..n)：WAV 檔頭的欄位與 sample 都是
 * little-endian，和 frame 標頭的 big-endian 相反。v & 0xFF 取最低 8 bits、v >>= 8 把 v 往右推掉 1 個 byte。 */
static void put_le(uint8_t *p, uint32_t v, int n) { for (int i = 0; i < n; i++) { p[i] = (uint8_t)(v & 0xFF); v >>= 8; } }

/* 在 buf 組出一個 PCM WAV；before／after 是插在 data chunk 前、後的額外 chunk。回傳總長度。 */
static size_t make_wav(uint8_t *buf, int channels, int bits, const uint8_t *pcm, size_t pcm_len,
                       const uint8_t *before, size_t before_len, const uint8_t *after, size_t after_len) {
    uint32_t rate = 16000;
    size_t p = 12;
    memcpy(buf, "RIFF----WAVE", 12);
    memcpy(buf + p, "fmt ", 4); put_le(buf + p + 4, 16, 4);
    put_le(buf + p + 8, 1, 2);  put_le(buf + p + 10, (uint32_t)channels, 2);
    put_le(buf + p + 12, rate, 4); put_le(buf + p + 16, rate * (uint32_t)(channels * bits / 8), 4);
    put_le(buf + p + 20, (uint32_t)(channels * bits / 8), 2); put_le(buf + p + 22, (uint32_t)bits, 2);
    p += 24;
    if (before_len) { memcpy(buf + p, before, before_len); p += before_len; }
    memcpy(buf + p, "data", 4); put_le(buf + p + 4, (uint32_t)pcm_len, 4); p += 8;
    if (pcm_len) memcpy(buf + p, pcm, pcm_len);
    p += pcm_len;
    if (pcm_len & 1u) buf[p++] = 0;                                     /* chunk 長度為奇數時補 1 byte */
    if (after_len) { memcpy(buf + p, after, after_len); p += after_len; }
    put_le(buf + 4, (uint32_t)(p - 8), 4);
    return p;
}

/* 【函式裡的 static 陣列】下面三個大陣列加了 static，就不放在 stack 上（stack 通常只有 1–8 MB，
 *   放太大的區域陣列，程式一執行就當掉），而是和全域變數放在一起，只是名字只有這個函式看得到。
 *   sizeof(buf) 是整個陣列佔的 bytes 數；元素是 uint8_t，所以剛好等於格數。
 *   注意 sizeof 只對「陣列本身」有這個效果，對函式參數收到的指標不行。
 * 三組測試的輸入都是用程式現場產生的，每一組在測什麼，看 huff_roundtrip 的第一個參數（測試名稱）就知道。 */
static void test_huffman(void) {
    static uint8_t buf[200000], pcm[120001], wav[121000];
    size_t n;

    printf("Huffman：符號 = byte\n");
    huff_roundtrip("空輸入（0 byte）", buf, 0, SYM_BYTE, 0);
    buf[0] = 'a';
    huff_roundtrip("只有 1 個 byte", buf, 1, SYM_BYTE, 0);
    memset(buf, 'a', 1000);
    huff_roundtrip("1000 個相同的 byte（樹只有一個葉）", buf, 1000, SYM_BYTE, 1);
    for (n = 0; n < 256; n++) buf[n] = (uint8_t)n;
    huff_roundtrip("256 種 byte 各出現一次", buf, 256, SYM_BYTE, 1);
    for (n = 0; n < sizeof(buf); n++) buf[n] = (uint8_t)(lcg() >> 16);
    huff_roundtrip("近似均勻的亂數（幾乎壓不下去）", buf, sizeof(buf), SYM_BYTE, 1);
    for (n = 0; n < sizeof(buf); n++) {                                  /* 極度偏斜：code 長度會很長 */
        unsigned r = lcg(), sym = 0;
        while ((r & 1u) && sym < 40) { r >>= 1; sym++; }
        buf[n] = (uint8_t)sym;
    }
    huff_roundtrip("極度偏斜的分布（頻率每次減半）", buf, sizeof(buf), SYM_BYTE, 1);

    printf("Huffman：符號 = UTF-8 字元（文字）\n");
    huff_roundtrip("空字串", buf, 0, SYM_CHAR, 0);
    const char *tiny = "\xE5\x97\xA8";                                   /* 「嗨」：看短訊息膨脹多少 */
    huff_roundtrip("很短的聊天訊息「嗨」", (const uint8_t *)tiny, 3, SYM_CHAR, 1);
    n = 0;
    while (n + 3 <= 3000) { memcpy(buf + n, "\xE5\xA4\x9A", 3); n += 3; }  /* 1000 個「多」：只有一種符號 */
    huff_roundtrip("1000 個相同的字", buf, n, SYM_CHAR, 1);
    /* BOM + 1／2／3／4 bytes 的字元 + ZWJ emoji + tab、引號、CRLF */
    const char *zh = "\xEF\xBB\xBF" "多媒體訊號處理：把系統做出來。Hello, MMSP 2026! "
                     "\xC3\xA9 \xCE\xA9 \xD1\x8F \xF0\x9F\x98\x80 "
                     "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7 \xF0\xA0\xAE\xB7\t\"q\"\r\n";
    n = 0;
    while (n + strlen(zh) < 60000) { memcpy(buf + n, zh, strlen(zh)); n += strlen(zh); }
    size_t as_char = huff_roundtrip("中英文、emoji、BOM、CRLF 混合的文字", buf, n, SYM_CHAR, 1);
    size_t as_byte = huff_roundtrip("同一份文字改以 byte 為符號", buf, n, SYM_BYTE, 1);
    if (as_char && as_byte)
        report("以字元為符號要比以 byte 為符號壓得小", 0, as_char < as_byte);
    huff_must_reject("含非法 UTF-8（C0 80）的資料", (const uint8_t *)"abc\xC0\x80" "def", 8, SYM_CHAR);
    huff_must_reject("結尾被截斷的字元（E5 A4）", (const uint8_t *)"ok\xE5\xA4", 4, SYM_CHAR);

    printf("Huffman：符號 = 16-bit sample（WAV）\n");
    /* 測試訊號：只有 64 種 sample 值（k×517），k 接近三角分布。以 sample 為符號約 5–6 bits／sample；
       拆成 byte 後高、低 byte 各自都像有幾十種值，要 10 bits 以上。 */
    for (n = 0; n + 1 < 120000; n += 2) {
        int k = (int)(lcg() % 32u) + (int)(lcg() % 32u) - 31;
        put_le(pcm + n, (uint32_t)(uint16_t)(int16_t)(k * 517), 2);
    }
    const uint8_t list[] = {'L','I','S','T', 12,0,0,0, 'I','N','F','O','I','S','F','T', 0,0,0,0};
    size_t wn = make_wav(wav, 1, 16, pcm, 120000, NULL, 0, NULL, 0);
    size_t as_s16 = huff_roundtrip("單聲道 16-bit PCM", wav, wn, SYM_S16, 1);
    as_byte = huff_roundtrip("同一個 WAV 改以 byte 為符號", wav, wn, SYM_BYTE, 1);
    if (as_s16 && as_byte)
        report("以 sample 為符號要比以 byte 為符號壓得小", 0, as_s16 < as_byte);
    wn = make_wav(wav, 2, 16, pcm, 120000, NULL, 0, NULL, 0);
    huff_roundtrip("雙聲道", wav, wn, SYM_S16, 0);
    wn = make_wav(wav, 1, 16, pcm, 120000, list, sizeof(list), NULL, 0);
    huff_roundtrip("data 之前有 LIST chunk（data 不在第 44 byte）", wav, wn, SYM_S16, 0);
    wn = make_wav(wav, 1, 16, pcm, 120000, NULL, 0, list, sizeof(list));
    huff_roundtrip("data 之後還有其他 chunk", wav, wn, SYM_S16, 0);
    wn = make_wav(wav, 1, 16, pcm, 120001, NULL, 0, NULL, 0);
    huff_roundtrip("data 長度是奇數（最後 1 byte 湊不成 sample）", wav, wn, SYM_S16, 0);
    wn = make_wav(wav, 1, 16, pcm, 0, NULL, 0, NULL, 0);
    huff_roundtrip("只有檔頭、沒有 sample", wav, wn, SYM_S16, 0);
    wn = make_wav(wav, 1, 16, pcm, 2, NULL, 0, NULL, 0);
    huff_roundtrip("只有 1 個 sample", wav, wn, SYM_S16, 0);
    wn = make_wav(wav, 1, 8, pcm, 1000, NULL, 0, NULL, 0);
    huff_must_reject("8-bit PCM 的 WAV", wav, wn, SYM_S16);
    huff_must_reject("根本不是 WAV 的資料", (const uint8_t *)"just some text, not RIFF", 24, SYM_S16);
}

/* 結束碼：全部 PASS 才是 0。make 看到非 0 會顯示 Error，這是提醒還有 TODO 或 FAIL，不是 make 壞掉。
 * Windows 上先把主控台的輸出設成 UTF-8（code page 65001），中文的測試名稱才不會變成亂碼。 */
int main(void) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif
    test_frame();
    test_utf8();
    test_huffman();
    printf("\n結果：PASS %d、FAIL %d、TODO %d\n", n_pass, n_fail, n_todo);
    if (n_todo > 0) printf("TODO 的項目請到 src/frame.c、src/utf8.c、src/huffman.c 完成對應的 place holder。\n");
    return (n_fail > 0 || n_todo > 0) ? 1 : 0;
}
