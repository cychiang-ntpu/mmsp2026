/*============================================================================
 *  transfer.c  —  功能 2、3：檔案傳輸流程、進度條、STATS（殼：已完成）
 *----------------------------------------------------------------------------
 *  傳輸流程對大文字檔與 WAV 是同一套；差別只在 Huffman 的「符號」：
 *    .txt → SYM_CHAR（UTF-8 字元）   .wav → SYM_S16（16-bit sample）   其他 → SYM_BYTE
 *  huff_encode 說資料不適用（TL_ERR_DATA：文字檔其實不是合法 UTF-8、WAV 不是 16-bit PCM）時，退回 SYM_BYTE。
 *  同一套流程有兩個入口：
 *    - 命令列 `textlink send`／`textlink recv`（本檔下半部）：給自動測試與量測用
 *    - 聊天畫面的 `/send`（src/chat.c）：在同一條連線上傳檔，對方的聊天畫面自動接收
 *
 *   傳送端 file_send_frames                     接收端 file_rx_*
 *   ─────────────────────────                  ──────────────────────────
 *   讀整個檔案進記憶體
 *   (huff) huff_encode  ← encode_ms
 *   FILE_BEGIN ─────────────────────────────►   file_rx_begin：記下模式、大小、檔名，配置緩衝區
 *   FILE_DATA × N（每個 64 KiB）─────────────►   file_rx_data ：逐段接到緩衝區
 *   FILE_END（空）──────────────────────────►   file_rx_finish：檢查大小 → (huff) huff_decode ← decode_ms
 *                                               → 寫到 <檔名>.part → 改名成 <檔名>
 *   等回覆 ◄──────────────── FILE_END(status)    回覆 1 byte：0=成功、1=失敗
 *
 *  這個殼用的 payload 格式（你們可以改，改了要更新 docs/interface.md）：
 *    FILE_BEGIN：mode 1 byte（0=raw、1=huff）｜原始大小 8 bytes BE｜傳輸資料大小 8 bytes BE｜檔名（UTF-8）
 *    FILE_DATA ：傳輸資料的一段（raw 時就是檔案內容；huff 時是 huff_encode 結果的一段）
 *    FILE_END  ：傳送端→接收端為空；接收端→傳送端為 1 byte 狀態（0 成功、其他失敗）
 *  可以考慮的強化：在 FILE_END 放原檔的 checksum（CRC-32 等），接收端解碼後比對。
 *
 *  這個檔案裡沒有 TODO；huff 模式要等 src/huffman.c 完成後才會通。
 *
 *  ── 這個檔案的地圖 ─────────────────────────────────────────────────────────
 *  建議的閱讀順序（先看上面那張流程圖，再照這個順序對照程式）：
 *    1. file_send_frames          傳送端的完整流程：讀檔 → (編碼) → BEGIN → DATA × N → END
 *    2. file_rx_begin／file_rx_data／file_rx_finish
 *                                 接收端：每收到一個 FILE_* frame 就呼叫對應的一個；三個函式共用一個 file_rx_t 記錄進度
 *    3. transfer_send／transfer_recv
 *                                 命令列的入口：連線 → 呼叫上面的函式 → 最後的回覆（ack）→ 印 STATS
 *    4. 其餘的小工具              需要時再看
 *
 *  資料流：
 *    傳送端  檔案 → read_whole_file → (huff_encode) → 切成 64 KiB 一段 → frame_send → socket
 *    接收端  socket → frame_recv → file_rx_data 接回一整塊 → (huff_decode) → write_file_atomically → 檔案
 *
 *  這個檔案不直接呼叫 send／recv，一律透過 frame_send／frame_recv（src/frame.c）；也不知道 Huffman 怎麼做，
 *  只知道 huff_encode／huff_decode 是「一塊 bytes 進去、另一塊 bytes 出來」。
 *
 *  可以跳過：ends_with_nocase、draw_progress、explain 的細節。
 *  口試會問：為什麼收到的檔名要消毒（sanitize_name）、為什麼對方宣稱的大小要先檢查再配置（file_rx_begin）、
 *            為什麼先寫 .part 再改名（write_file_atomically）。
 *===========================================================================*/
#include "textlink.h"
#include <inttypes.h>           /* PRIu64：用 printf 印 uint64_t 時，各平台通用的格式寫法（見 transfer_send） */
#ifdef _WIN32
  #include <direct.h>           /* _mkdir */
#endif

#define BEGIN_FIXED 17          /* FILE_BEGIN 固定欄位：1 + 8 + 8 */

/* 把 64-bit 整數 v 寫成 8 個 bytes 放進 p[0..7]，big-endian（最高位的 byte 在最前面）。
 * 為什麼不直接 memcpy(p, &v, 8)：那樣寫出來的是「這台電腦的 byte 順序」，兩台電腦的 CPU 不同就會讀錯；
 * 用位移一個 byte 一個 byte 處理，結果和 CPU 無關。做法：每次取出 v 的最低 8 bits 放到目前最後面的位置，
 * 再把 v 右移 8 bits，從 p[7] 往 p[0] 填。只給這個檔案內部用（FILE_BEGIN 的兩個大小欄位）。 */
static void put_u64be(uint8_t *p, uint64_t v) {
    for (int i = 7; i >= 0; i--) { p[i] = (uint8_t)(v & 0xFF); v >>= 8; }
}

/* put_u64be 的反向：把 p[0..7] 組回一個 64-bit 整數。每讀一個 byte，就把目前的值左移 8 bits 再把新的 byte 併進來。 */
static uint64_t get_u64be(const uint8_t *p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v = (v << 8) | p[i];
    return v;
}

/* 壓縮率 = 實際上線 bytes ÷ 原檔 bytes（含所有 frame 標頭與 codebook）；越小越好，大於 1 代表變大了 */
/* 先轉成 double 再除：兩個整數相除在 C 裡會把小數丟掉（7 / 10 是 0）。file 為 0（空檔）時不能除，直接回傳 0。
 * 呼叫者：本檔的 STATS 輸出、src/chat.c 的系統訊息。 */
double tl_ratio(uint64_t wire, uint64_t file) {
    return file ? (double)wire / (double)file : 0.0;
}

/* 路徑最後一段；不讓對方指定的檔名帶有資料夾（../../ 之類）*/
/* 例："C:\music\a.wav" → "a.wav"、"../../etc/passwd" → "passwd"。回傳的指標指向 path 內部，沒有複製。
 * 做法：從頭掃到尾，每遇到一個 '/' 或 '\\' 就把 b 移到它的下一個字元；掃完時 b 停在最後一個分隔符號之後。 */
static const char *base_name(const char *path) {
    const char *b = path;
    for (const char *p = path; *p; p++)
        if (*p == '/' || *p == '\\') b = p + 1;
    return b;
}

/* 檔名只保留安全的 ASCII 字元，其餘換成 '_'。
 * （中文檔名在 Windows 上要用 _wfopen 等寬字元 API 才能正確開檔，這個殼先不處理。）*/
/* in 是原始檔名（可能含路徑），out 是輸出（容量 cap，結果一定以 '\0' 結尾）。
 * 為什麼要做（安全）：接收端的檔名是「對方說的」。如果照單全收，對方傳一個叫 "../../某個重要檔案" 的名字，
 * 我們就會把自己電腦上 outdir 以外的檔案蓋掉。所以只留最後一段，而且只允許英數字與 . - _ 。
 * 迴圈條件 k + 1 < cap：永遠替結尾的 '\0' 留一格。
 * 最後一個 if：結果是空字串、"." 或 ".."（這兩個是「目前資料夾」與「上一層」，不是檔案）時，改用固定的名字。
 * 傳送端也用它（file_send_frames），所以兩端畫面上顯示的檔名會一致。 */
static void sanitize_name(const char *in, char *out, size_t cap) {
    size_t k = 0;
    for (const char *p = base_name(in); *p && k + 1 < cap; p++) {
        unsigned char c = (unsigned char)*p;
        int ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                 c == '.' || c == '-' || c == '_';
        out[k++] = ok ? (char)c : '_';
    }
    out[k] = '\0';
    if (k == 0 || strcmp(out, ".") == 0 || strcmp(out, "..") == 0)
        snprintf(out, cap, "received.bin");
}

/* s 是否以 ext 結尾，不分大小寫（"A.WAV" 也算 ".wav"）。ext 要傳小寫。是回傳 1，不是回傳 0。
 * 做法：把 s 的最後 e 個字元逐一轉成小寫再和 ext 比。 */
static int ends_with_nocase(const char *s, const char *ext) {
    size_t n = strlen(s), e = strlen(ext);
    if (n < e) return 0;
    for (size_t i = 0; i < e; i++) {
        char c = s[n - e + i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c != ext[i]) return 0;
    }
    return 1;
}

/* 依副檔名決定 Huffman 的符號種類 */
/* 這只是「先猜」：副檔名是 .txt 不代表內容真的是合法 UTF-8。猜錯的時候 huff_encode 會回傳 TL_ERR_DATA，
 * file_send_frames 再改用 SYM_BYTE 重試。 */
static tl_sym_t sym_for_file(const char *path) {
    if (ends_with_nocase(path, ".wav")) return SYM_S16;
    if (ends_with_nocase(path, ".txt")) return SYM_CHAR;
    return SYM_BYTE;
}

/* 把整個檔案讀進一塊新配置的記憶體。
 *   path   檔案路徑
 *   buf    輸出：指向檔案內容的指標（這裡 malloc，呼叫端負責 free）
 *   len    輸出：檔案大小（bytes）
 *   回傳   TL_OK／TL_ERR_IO（開不了、讀不完、超過 64 MiB）／TL_ERR_NOMEM
 * buf 的型別是 uint8_t **（指標的指標）：函式要把「一個指標」交還給呼叫端，就得拿到那個指標變數的位址。
 * 注意：這個殼是「一次整個讀進來」，不是邊讀邊送；切成 64 KiB 是之後送出時才切的（見 file_send_frames）。
 * 好處是 huff_encode 可以一次看到全部資料來統計；代價是檔案多大、記憶體就要多大，所以設了 TL_MAX_FILE 上限。 */
static int read_whole_file(const char *path, uint8_t **buf, size_t *len) {
    /* fopen 的 "rb"：r = 讀、b = binary（原樣）。少了 b，Windows 會用「文字模式」開檔：讀的時候把 \r\n 換成 \n、
     * 遇到 0x1A 當成檔案結束。我們要的是逐 byte 相同，所以讀、寫都一定要加 b。（macOS／Linux 加不加都一樣。） */
    FILE *f = fopen(path, "rb");                 /* "rb"：Windows 才不會動 \r\n */
    if (f == NULL) return TL_ERR_IO;
    /* 量檔案大小的老方法：把讀寫位置移到檔尾（fseek ... SEEK_END）→ 問現在的位置（ftell）→ 移回開頭（rewind） */
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return TL_ERR_IO; }
    long sz = ftell(f);
    rewind(f);
    if (sz < 0 || (unsigned long)sz > TL_MAX_FILE) { fclose(f); return TL_ERR_IO; }   /* 上限 64 MiB */
    /* 多配 1 byte：空檔（sz = 0）時 malloc(0) 在某些系統會回傳 NULL，會被誤判成記憶體不足 */
    uint8_t *b = (uint8_t *)malloc((size_t)sz + 1);
    if (b == NULL) { fclose(f); return TL_ERR_NOMEM; }
    /* fread(b, 1, sz, f)：每個元素 1 byte、讀 sz 個，回傳實際讀到幾個；不等於 sz 就是沒讀完 */
    if (sz > 0 && fread(b, 1, (size_t)sz, f) != (size_t)sz) { free(b); fclose(f); return TL_ERR_IO; }
    fclose(f);
    *buf = b;
    *len = (size_t)sz;
    return TL_OK;
}

/*========================= 傳送端（聊天與命令列共用） ======================*/
/* file_send_frames：在已接通的 socket 上送出一個檔案。只負責送，不等對方回覆（回覆由呼叫端處理）。
 *   s         已接通的 socket
 *   path      要送的檔案
 *   mode      MODE_RAW 原樣送／MODE_HUFF 先 huff_encode 再送
 *   st        輸出：這次傳輸的統計（檔名、file_bytes、wire_bytes、各段時間），STATS 與聊天畫面都用它
 *   progress  函式指標：每送完一段就呼叫它一次來更新進度顯示；傳 NULL 表示不要顯示。
 *             命令列傳的是本檔的 draw_progress，聊天傳的是 src/chat.c 的 chat_progress；這個函式不需要知道畫面長怎樣。
 *   回傳      TL_OK 或 TL_ERR_*
 *   呼叫者    本檔的 transfer_send、src/chat.c 的 send_file_cmd
 *
 * 時間怎麼量：每一段都是「開始前 now_ms()、結束後 now_ms()，相減」。
 *   encode_ms  只包 huff_encode（含退回 SYM_BYTE 重試的那一次）
 *   send_ms    從第一個 FILE_DATA 到 FILE_END 送完。注意 frame_send 返回只代表資料已經交給作業系統，
 *              不代表對方已經收到；對方「真的收完並還原」要看最後的回覆（transfer_send、src/chat.c）。
 *   total_ms   從進入這個函式（讀檔之前）到 FILE_END 送完 */
int file_send_frames(socket_t s, const char *path, tl_mode_t mode, tl_stats_t *st, tl_progress_fn progress) {
    uint8_t *file = NULL, *enc = NULL;          /* 先設成 NULL：不管從哪裡跳到 done，free(NULL) 都是安全的 */
    size_t file_len = 0, enc_len = 0;
    double t0 = now_ms();
    int rc;

    memset(st, 0, sizeof(*st));                 /* 統計先全部歸零，再一項一項填 */
    st->mode = mode;
    sanitize_name(path, st->name, sizeof(st->name));   /* 送給對方的只有檔名，不含我們這邊的資料夾路徑 */

    rc = read_whole_file(path, &file, &file_len);
    if (rc != TL_OK) return rc;
    st->file_bytes = (uint64_t)file_len;

    /* data／data_len 指向「真正要上線的那一塊」：raw 模式就是檔案本身；huff 模式編碼成功後改指向編碼結果 */
    const uint8_t *data = file;
    size_t data_len = file_len;
    if (mode == MODE_HUFF) {
        double te = now_ms();
        st->sym = sym_for_file(path);
        rc = huff_encode(file, file_len, st->sym, &enc, &enc_len);
        if (rc == TL_ERR_DATA && st->sym != SYM_BYTE) {        /* 內容與副檔名不符：退回以 byte 為符號 */
            st->sym = SYM_BYTE;
            rc = huff_encode(file, file_len, SYM_BYTE, &enc, &enc_len);
        }
        st->encode_ms = now_ms() - te;
        /* goto done：C 沒有 try／finally，出錯時跳到函式最後統一 free，是 C 裡常見的收尾寫法（不是亂跳）*/
        if (rc != TL_OK) goto done;
        data = enc;
        data_len = enc_len;
    }

    /* FILE_BEGIN：先告訴對方「接下來是什麼」，格式見檔頭：
     *   begin[0]      mode
     *   begin[1..8]   原始大小  ← 接收端用它檢查還原結果的大小
     *   begin[9..16]  傳輸大小  ← 接收端用它配置緩衝區、判斷收齊了沒
     *   begin[17..]   檔名（沒有 '\0' 結尾；長度由 frame 的 length 推得）
     * 陣列大小取 17 + sizeof(st->name)，檔名再長也放得下。 */
    uint8_t begin[BEGIN_FIXED + sizeof(st->name)];
    size_t name_len = strlen(st->name);
    begin[0] = (uint8_t)mode;
    put_u64be(begin + 1, (uint64_t)file_len);
    put_u64be(begin + 9, (uint64_t)data_len);
    memcpy(begin + BEGIN_FIXED, st->name, name_len);
    rc = frame_send(s, T_FILE_BEGIN, begin, BEGIN_FIXED + name_len);
    if (rc != TL_OK) goto done;
    /* wire_bytes（實際上線的 bytes）是自己一筆一筆加出來的：每個 frame = 5 bytes 標頭（TL_HDR_LEN）+ payload。
     * 所以壓縮率的分子包含所有標頭、FILE_BEGIN、以及 huff_encode 輸出裡的 codebook：這樣才是誠實的數字。 */
    st->wire_bytes += TL_HDR_LEN + BEGIN_FIXED + name_len;

    /* FILE_DATA：把 data 切成每段最多 TL_CHUNK（64 KiB）送出；最後一段通常比較短。
     * 為什麼要切：(1) 一個 frame 最大只能 16 MiB，檔案可以到 64 MiB；(2) 每送一段就能更新一次進度；
     * (3) 聊天時一個 frame 只鎖住「送出鎖」一下子，對方的訊息與回覆還是有機會穿插進來。
     * off 是目前送到第幾個 byte；n 是這一段的長度 = min(剩下的, TL_CHUNK)。空檔時 data_len 是 0，迴圈一次都不跑。 */
    double ts = now_ms();
    for (size_t off = 0; off < data_len; off += TL_CHUNK) {
        size_t n = data_len - off < TL_CHUNK ? data_len - off : TL_CHUNK;
        rc = frame_send(s, T_FILE_DATA, data + off, n);
        if (rc != TL_OK) goto done;
        st->wire_bytes += TL_HDR_LEN + n;
        if (progress) progress("傳送", off + n, data_len);
    }
    /* FILE_END（空的 payload）：告訴對方「送完了，可以開始檢查、解碼、存檔」 */
    rc = frame_send(s, T_FILE_END, NULL, 0);
    if (rc != TL_OK) goto done;
    st->wire_bytes += TL_HDR_LEN;
    st->send_ms = now_ms() - ts;
    st->total_ms = now_ms() - t0;

done:
    free(file);
    free(enc);
    return rc;
}

/*========================= 接收端（聊天與命令列共用） ======================*/
/* 接收端寫成「狀態機」：file_rx_t 這個 struct 記著目前收到哪裡（begun、got、data…），每收到一個 frame 就把它餵給
 * 對應的函式。這樣寫，命令列的 transfer_recv 和聊天的接收執行緒（src/chat.c 的 handle_file_frame）可以共用同一套邏輯，
 * 即使兩者「怎麼拿到 frame」的迴圈長得不一樣。
 * 參數 rx 是 struct 的指標；rx->got 的意思是「rx 指到的那個 struct 裡的 got 欄位」。 */

/* 放掉緩衝區並把狀態清回「還沒開始」。第一次使用之前，rx 必須已經全部是 0（free(NULL) 才安全）：
 * transfer_recv 用 memset 清，src/chat.c 的 g_rx 是全域變數，本來就從 0 開始。 */
void file_rx_reset(file_rx_t *rx) {
    free(rx->data);
    memset(rx, 0, sizeof(*rx));
}

/* 收到 FILE_BEGIN 時呼叫。p／len 是那個 frame 的 payload。回傳 TL_OK、TL_ERR_PROTO（內容不合理）或 TL_ERR_NOMEM。
 * 原則：payload 裡每一個數字都是「對方說的」，對方可能有 bug，也可能不懷好意；全部檢查過才拿來用。 */
int file_rx_begin(file_rx_t *rx, const uint8_t *p, size_t len) {
    /* 三個拒收的理由：上一個檔案還沒收完又來一個 BEGIN；payload 短到連固定欄位＋1 個字元的檔名都放不下；mode 不是 0 或 1。
     * 先檢查 len 才讀 p[...]，就不會讀到 payload 以外的記憶體。 */
    if (rx->begun || len < BEGIN_FIXED + 1 || p[0] > 1) return TL_ERR_PROTO;
    rx->t0 = now_ms();                            /* 接收端的 total_ms 從收到 FILE_BEGIN 開始算 */
    rx->mode = (tl_mode_t)p[0];
    rx->orig_size = get_u64be(p + 1);
    rx->data_size = get_u64be(p + 9);
    /* 大小是對方說的：先檢查再配置 */
    /* 不檢查的話，對方只要在這 8 bytes 填一個天文數字，我們就會去 malloc 那麼多，輕則失敗、重則把電腦拖垮。
     * raw 模式沒有壓縮，兩個大小應該相等；不相等就是對方寫錯了。 */
    if (rx->orig_size > TL_MAX_FILE || rx->data_size > TL_MAX_FILE ||
        (rx->mode == MODE_RAW && rx->orig_size != rx->data_size)) return TL_ERR_PROTO;

    /* 取出檔名：payload 第 17 byte 之後全部都是檔名，沒有 '\0'，所以要自己算長度、自己補 '\0'。
     * nl = min(檔名長度, 127)：太長就截斷，不能寫超出 raw_name 陣列。然後一樣要消毒（理由見 sanitize_name）。 */
    char raw_name[128];
    size_t nl = len - BEGIN_FIXED < sizeof(raw_name) - 1 ? len - BEGIN_FIXED : sizeof(raw_name) - 1;
    memcpy(raw_name, p + BEGIN_FIXED, nl);
    raw_name[nl] = '\0';
    sanitize_name(raw_name, rx->name, sizeof(rx->name));

    /* 一次配好整塊緩衝區，之後每個 FILE_DATA 依序接在後面。+1 的理由同 read_whole_file（data_size 可能是 0）。 */
    rx->data = (uint8_t *)malloc((size_t)rx->data_size + 1);
    if (rx->data == NULL) return TL_ERR_NOMEM;
    rx->wire = TL_HDR_LEN + len;                  /* 接收端也自己累加上線 bytes：FILE_BEGIN 這個 frame = 標頭 + payload */
    rx->begun = 1;
    return TL_OK;
}

/* 收到 FILE_DATA 時呼叫：把這一段接到緩衝區裡已經收到的資料後面。got 是目前累計收到的 bytes 數。
 * 接收端不假設每段剛好 64 KiB，任何切法都收得下；只要求總量不超過 FILE_BEGIN 宣稱的 data_size。 */
int file_rx_data(file_rx_t *rx, const uint8_t *p, size_t len) {
    if (!rx->begun || rx->got + len > rx->data_size) return TL_ERR_PROTO;     /* 比宣稱的多：拒收 */
    memcpy(rx->data + rx->got, p, len);
    rx->got += len;
    rx->wire += TL_HDR_LEN + len;
    return TL_OK;
}

/* 把 buf[0..len) 寫成檔案 path。回傳 TL_OK 或 TL_ERR_IO。只給 file_rx_finish 用。 */
static int write_file_atomically(const char *path, const uint8_t *buf, size_t len) {
    char tmp[600];
    snprintf(tmp, sizeof(tmp), "%s.part", path);

    /* 先寫到 .part，全部成功才改名：失敗時不會留下「看起來完整、其實壞掉」的檔案 */
    /* （寫到一半磁碟滿了、程式被關掉時，留下的是 xxx.part，使用者和測試腳本都不會把它當成成品。）
     * "wb"：w = 寫入（檔案已存在就清空重寫）、b = binary。少了 b，Windows 會把每個 \n 寫成 \r\n，
     * 檔案就變大、不再逐 byte 相同；對 WAV 這種二進位檔是直接毀掉。 */
    FILE *f = fopen(tmp, "wb");
    if (f == NULL) return TL_ERR_IO;
    size_t w = len ? fwrite(buf, 1, len, f) : 0;
    /* fclose 也要檢查：fwrite 常常只是寫進記憶體裡的緩衝區，fclose 時才真的寫進磁碟，磁碟滿了是這時候才知道 */
    if (fclose(f) != 0 || w != len) { remove(tmp); return TL_ERR_IO; }
    remove(path);                                  /* Windows 的 rename 不會覆蓋既有檔案 */
    if (rename(tmp, path) != 0) { remove(tmp); return TL_ERR_IO; }
    return TL_OK;
}

/* 收到傳送端的 FILE_END 之後呼叫：檢查、解碼、存檔。saved 會填入存檔路徑。 */
/*   rx        接收狀態（這個函式不會釋放它；呼叫端之後要 file_rx_reset）
 *   outdir    存檔的資料夾（不存在會建立一層）
 *   st        輸出：統計（接收端的 STATS 與聊天畫面用）
 *   saved     輸出：存檔路徑 "<outdir>/<檔名>"；saved_cap 是它的容量
 *   回傳      TL_OK = 檔案已經完整寫好。呼叫端要把成敗用 FILE_END(status) 回覆給傳送端。
 *   呼叫者    本檔的 transfer_recv、src/chat.c 的 handle_file_frame */
int file_rx_finish(file_rx_t *rx, const char *outdir, tl_stats_t *st, char *saved, size_t saved_cap) {
    uint8_t *dec = NULL;
    int rc = TL_OK;

    memset(st, 0, sizeof(*st));
    /* 收到的量必須剛好等於 FILE_BEGIN 宣稱的量；少了就是傳到一半出事，不能拿不完整的資料去解碼、存檔 */
    if (!rx->begun || rx->got != rx->data_size) return TL_ERR_PROTO;
    rx->wire += TL_HDR_LEN;                                         /* FILE_END 本身 */

    /* out／out_len 指向「要寫進檔案的那一塊」：raw 模式就是收到的資料；huff 模式是解碼的結果 */
    const uint8_t *out = rx->data;
    size_t out_len = (size_t)rx->got;
    if (rx->mode == MODE_HUFF) {
        double td = now_ms();
        /* 第三個參數是「最多允許解出幾 bytes」：給的是 FILE_BEGIN 宣稱的原始大小（已經檢查過不超過 64 MiB），
         * 壞掉或惡意的資料就不可能讓 huff_decode 無止境地配置記憶體。 */
        rc = huff_decode(rx->data, (size_t)rx->got, (size_t)rx->orig_size, &dec, &out_len);
        st->decode_ms = now_ms() - td;            /* decode_ms 只包 huff_decode，不含寫檔 */
        out = dec;
        if (rc == TL_OK && out_len != rx->orig_size) rc = TL_ERR_DATA;   /* 解得開、但大小和宣稱的不同：也算失敗 */
    }
    if (rc == TL_OK) {
        /* 建立輸出資料夾。已經存在時 mkdir 會失敗，這裡刻意不檢查：真的有問題的話，下面開檔會失敗並回報。
         * 0755 是 POSIX 的權限（自己可讀寫、其他人可讀）；Windows 的 _mkdir 沒有這個參數。 */
#ifdef _WIN32
        _mkdir(outdir);
#else
        mkdir(outdir, 0755);
#endif
        snprintf(saved, saved_cap, "%s/%s", outdir, rx->name);     /* 路徑用 '/'：Windows 的開檔函式也接受 */
        rc = write_file_atomically(saved, out, out_len);
    }
    /* 不論成敗都把統計填好。wire_bytes 是接收端自己數的，應該和傳送端的 wire_bytes 相同，可以拿來互相驗證。 */
    snprintf(st->name, sizeof(st->name), "%s", rx->name);
    st->mode = rx->mode;
    st->file_bytes = rx->orig_size;
    st->wire_bytes = rx->wire;
    st->total_ms = now_ms() - rx->t0;             /* 從收到 FILE_BEGIN 到存檔完成 */
    free(dec);
    return rc;
}

/*========================= 命令列：textlink send ===========================*/
/* 命令列用的進度條，型別符合 tl_progress_fn。例：傳送 [###########-------------------]  37%  0.41 / 1.10 MB
 * 關鍵是開頭的 '\r'（carriage return）：游標回到「這一行的最前面」但不換行，下一次印的內容就蓋在同一行上，
 * 看起來像在原地更新。fflush 是因為沒有 '\n' 的輸出可能留在緩衝區不顯示。
 * fill = 30 格之中要填幾格 '#'。total 為 0（空檔）時不能除，直接當成 100%。1048576 = 1024 × 1024（1 MB）。 */
static void draw_progress(const char *label, uint64_t done, uint64_t total) {
    const int width = 30;
    int fill = total ? (int)((double)done * width / (double)total) : width;
    printf("\r%s [", label);
    for (int i = 0; i < width; i++) putchar(i < fill ? '#' : '-');
    printf("] %3.0f%%  %.2f / %.2f MB", total ? 100.0 * (double)done / (double)total : 100.0,
           (double)done / 1048576.0, (double)total / 1048576.0);
    fflush(stdout);
}

/* 印錯誤訊息到 stderr，並對兩種常見的錯誤多給一行提示。what 是「哪件事失敗」，rc 是錯誤碼。
 * 開頭的 "\n" 是為了離開進度條那一行。 */
static void explain(const char *what, int rc) {
    fprintf(stderr, "\n錯誤: %s：%s\n", what, tl_strerror(rc));
    if (rc == TL_ERR_TODO)
        fprintf(stderr, "      （frame 標頭在 src/frame.c、Huffman 在 src/huffman.c；raw 傳輸只需要先完成 frame）\n");
    if (rc == TL_ERR_IO)
        fprintf(stderr, "      （檔案開不了、超過 %u MiB，或寫不進去）\n", TL_MAX_FILE / (1024u * 1024u));
}

/* `textlink send <ip> <port> <file> [--raw|--huff]` 的本體：連線 → 送檔 → 等回覆 → 印 STATS → 關線。
 *   回傳值就是整支程式的結束碼：0 = 對方確認已成功還原並存檔；1 = 任何一步失敗。自動評測看的就是這個數字。
 *   呼叫者：src/main.c */
int transfer_send(const char *ip, int port, const char *path, tl_mode_t mode) {
    char peer[64];
    tl_stats_t st;
    int ok = 0;

    socket_t s = net_connect(ip, port, TL_CONNECT_TIMEOUT_MS, peer, sizeof(peer));
    if (s == SOCK_INVALID) return 1;

    double t0 = now_ms();                 /* 這裡的 t0 在連線成功之後才開始：STATS 的 total_ms 不含連線時間 */
    int rc = file_send_frames(s, path, mode, &st, draw_progress);
    printf("\n");                         /* 進度條一直停在同一行，這裡才換行 */
    if (rc != TL_OK) { explain("傳送失敗", rc); CLOSESOCK(s); return 1; }

    /* 等接收端回覆：對方真的解碼、寫檔成功，這次傳輸才算成功 */
    /* 這就是流程圖最下面那一步（ack = acknowledgement，確認）。為什麼不是送完就算數：
     * frame_send 成功只代表資料交給了我們這邊的作業系統；對方可能解碼失敗、磁碟滿了、程式當了。
     * 所以傳送端送完 FILE_END 之後，反過來當一次接收者，等一個 type = FILE_END、payload 剛好 1 byte、值是 0 的 frame。
     * 三個條件都成立才算成功。frame_recv 配置的 reply 用完要 free。
     * 另一個效果：total_ms 因此包含了對方收完、解碼、寫檔的時間，是「整件事做完」的時間。 */
    uint8_t type = 0, *reply = NULL;
    size_t reply_len = 0;
    rc = frame_recv(s, &type, &reply, &reply_len);
    if (rc != TL_OK) { explain("等不到接收端的回覆", rc); CLOSESOCK(s); return 1; }
    ok = (type == T_FILE_END && reply_len == 1 && reply[0] == 0);
    free(reply);
    if (!ok) fprintf(stderr, "錯誤: 接收端回報失敗（解碼或寫檔沒成功）\n");

    /* STATS：規格規定的一行 key=value，印在 stderr，老師的腳本會直接讀，欄位名稱與順序不要改。
     *   印在 stderr 而不是 stdout：進度條等給人看的輸出走 stdout，使用者可以用 2>stats.txt 只把這一行存下來。
     *   ratio    = wire_bytes ÷ file_bytes（tl_ratio），小數 4 位
     *   total_ms = 現在 − 上面的 t0（含等對方回覆）；不是 st.total_ms（那個只到 FILE_END 送完）
     *   sym      raw 模式沒有做 Huffman，印 none
     * PRIu64 是 <inttypes.h> 提供的巨集，會展開成這個平台上印 uint64_t 該用的格式字元（例如 "llu"）；
     * C 會把相鄰的字串常數自動接起來，所以 "...=%" PRIu64 " ..." 是一個完整的格式字串。 */
    fprintf(stderr, "STATS role=send mode=%s sym=%s file_bytes=%" PRIu64 " wire_bytes=%" PRIu64
                    " ratio=%.4f encode_ms=%.1f send_ms=%.1f total_ms=%.1f\n",
            mode == MODE_HUFF ? "huff" : "raw", mode == MODE_HUFF ? tl_sym_name(st.sym) : "none",
            st.file_bytes, st.wire_bytes,
            tl_ratio(st.wire_bytes, st.file_bytes), st.encode_ms, st.send_ms, now_ms() - t0);
    /* 給人看的版本：同一個比值 × 100 變成百分比。例：上線 743210 ÷ 原檔 1048576 = 0.7088 → 70.88% */
    printf("壓縮率 %.2f%%（上線 %" PRIu64 " bytes ÷ 原檔 %" PRIu64 " bytes，含 frame 標頭與 codebook）\n",
           100.0 * tl_ratio(st.wire_bytes, st.file_bytes), st.wire_bytes, st.file_bytes);

    CLOSESOCK(s);
    return ok ? 0 : 1;
}

/*========================= 命令列：textlink recv ===========================*/
/* `textlink recv <port> <outdir> [--bind <ip>]` 的本體：等一個人連進來 → 收一個檔案 → 回覆結果 → 印 STATS → 結束。
 *   回傳值是程式的結束碼：0 = 檔案已完整存好；1 = 失敗（這時不會留下輸出檔）。呼叫者：src/main.c */
int transfer_recv(const char *bind_ip, int port, const char *outdir) {
    char peer[64], saved[512] = "";
    file_rx_t rx;
    tl_stats_t st;
    int ended = 0, ok = 0, rc = TL_OK;

    memset(&rx, 0, sizeof(rx));           /* 區域變數的初值是垃圾；rx 一定要先清成 0（理由見 file_rx_reset） */
    memset(&st, 0, sizeof(st));
    socket_t s = net_listen_accept(bind_ip, port, peer, sizeof(peer));
    if (s == SOCK_INVALID) return 1;

    /* 主迴圈：一次收一個完整的 frame，看 type 決定交給誰。半包、黏包在 frame_recv 裡面已經處理掉了，這裡看不到。
     * 離開迴圈的三種情況：收到 FILE_END（ended = 1）、frame_recv 失敗（斷線、標頭不合規格）、內容不合規格（rc != TL_OK）。 */
    while (!ended && rc == TL_OK) {
        uint8_t type = 0, *p = NULL;
        size_t len = 0;
        rc = frame_recv(s, &type, &p, &len);
        if (rc != TL_OK) break;

        if (type == T_FILE_BEGIN) {
            rc = file_rx_begin(&rx, p, len);
            if (rc == TL_OK)
                printf("開始接收 %s：原始 %" PRIu64 " bytes，%s 傳輸 %" PRIu64 " bytes\n", rx.name, rx.orig_size,
                       rx.mode == MODE_HUFF ? "huff" : "raw", rx.data_size);
        } else if (type == T_FILE_DATA) {
            rc = file_rx_data(&rx, p, len);
            if (rc == TL_OK) draw_progress("接收", rx.got, rx.data_size);
        } else if (type == T_FILE_END && rx.begun) {
            ended = 1;
        } else {
            rc = TL_ERR_PROTO;                      /* 順序不對或不認得的 type */
        }
        free(p);                          /* payload 是 frame_recv 配置的；內容已經被複製進 rx，每一圈都要還 */
    }
    if (rx.begun) printf("\n");           /* 離開進度條那一行 */

    if (ended) {
        rc = file_rx_finish(&rx, outdir, &st, saved, sizeof(saved));
        ok = (rc == TL_OK);
        uint8_t status = ok ? 0 : 1;                /* 告訴傳送端結果（盡力而為） */
        /* 「盡力而為」：不檢查這個 frame_send 的回傳值。對方如果已經斷線，我們也沒有別的事能做；
         * 傳送端等不到回覆，自己會判定這次傳輸失敗。 */
        frame_send(s, T_FILE_END, &status, 1);
    }
    if (ok) {
        printf("已存檔：%s（%" PRIu64 " bytes）\n", saved, st.file_bytes);
        /* 接收端的 STATS：沒有 sym、encode_ms、send_ms，多了 decode_ms。wire_bytes 與 ratio 應該和傳送端印的一樣。 */
        fprintf(stderr, "STATS role=recv mode=%s file_bytes=%" PRIu64 " wire_bytes=%" PRIu64
                        " ratio=%.4f decode_ms=%.1f total_ms=%.1f\n",
                st.mode == MODE_HUFF ? "huff" : "raw", st.file_bytes, st.wire_bytes,
                tl_ratio(st.wire_bytes, st.file_bytes), st.decode_ms, st.total_ms);
    } else {
        explain("接收失敗，沒有產生輸出檔", rc);
    }

    file_rx_reset(&rx);                   /* 釋放接收緩衝區 */
    CLOSESOCK(s);
    return ok ? 0 : 1;
}
