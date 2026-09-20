/*============================================================================
 *  textlink.h  —  TextLink 各模組的共用介面
 *----------------------------------------------------------------------------
 *  模組地圖（★ = 你們要完成的 place holder，其餘是已經能動的殼）：
 *
 *    main.c      解析命令列 → 分派到 chat／send／recv
 *    net.c       TCP 監聽／連線（可指定 IP 與 port）、send_all／recv_all、計時
 *    frame.c   ★ frame 標頭的打包與解析；frame_send／frame_recv 已寫好
 *    utf8.c    ★ UTF-8 合法性檢查
 *    huffman.c ★ Huffman 編碼與解碼
 *    chat.c      聊天畫面（LINE 風格泡泡）、收發兩條執行緒、/files 與 /send 選檔傳送
 *    transfer.c  檔案傳輸流程（聊天與命令列共用）、進度條、STATS 輸出
 *
 *  ★ 的函式目前一律回傳 TL_ERR_TODO；殼看到這個回傳值會在畫面上告訴你
 *  「這一塊還沒實作」，不會當掉。
 *----------------------------------------------------------------------------
 *  【標頭檔（.h）是什麼】
 *  一個 .c 檔要呼叫「寫在別的 .c 檔裡」的函式，編譯器得先知道那個函式長什麼樣子：
 *  叫什麼名字、吃哪些參數、回傳什麼。這種「只有長相、沒有內容」的一行叫做宣告（declaration），
 *  以分號結尾、沒有大括號；函式真正的內容（definition）寫在某一個 .c 檔裡。
 *  把所有宣告集中在這個 .h，每個 .c 開頭寫一行 #include "textlink.h" 就全部看得到了。
 *  所以這個檔案也是整支程式的「目錄」：想知道某個函式怎麼用，先來這裡查。
 *
 *  【你們在這個檔案要做的事】
 *  不用改就能完成五個 TODO。五個 ★ 函式的參數與回傳值是殼與 tests/test_codec.c 呼叫它們的依據，
 *  請不要更動；你們自己另外寫的小函式（例如 bit writer）若只在一個 .c 裡用，
 *  直接寫在那個 .c 裡並加上 static 就好，不必放進來。
 *
 *  【這個專案共通的三個寫法】第一次看會不習慣，先在這裡講一次：
 *
 *  1. 函式的回傳值是「成功或失敗」，真正的結果從參數交出去。
 *     C 的函式只能 return 一個值。我們把 return 留給錯誤碼（TL_OK 或負的 TL_ERR_*），
 *     結果則由呼叫端先準備好變數、把變數的「位址」傳進來，函式把答案寫進那個位址：
 *
 *         size_t n;                                  呼叫端準備一個變數
 *         rc = frame_parse_header(hdr, &type, &n);   &n 是「n 的位址」
 *         在函式裡，參數的型別是 size_t *payload_len，寫 *payload_len = 值; 就是寫進呼叫端的 n
 *
 *     型別裡的 * 讀作「指向……的指標」：size_t * 是「指向 size_t 的指標」，存的是位址；
 *     運算式裡的 * 則是「到那個位址去讀或寫」。& 是反方向：取得變數的位址。
 *     呼叫之後一律先檢查回傳值，是 TL_OK 才可以使用那些結果。
 *
 *  2. 一塊 bytes 用「指標 + 長度」兩個參數表示：const uint8_t *in, size_t in_len。
 *     in 指向第一個 byte，in[0] 到 in[in_len-1] 是可以讀的範圍（註解裡寫成 in[0..in_len)）。
 *     這不是 C 字串：中間可以有 0x00，結尾也不保證有 '\0'，所以 strlen 不能用、
 *     也絕對不可以讀到 in[in_len] 之後。
 *     const 的意思是「本函式保證只讀、不改這塊資料」；不小心寫到，編譯器會報錯。
 *
 *  3. 誰 malloc、誰 free，宣告旁邊的註解一定會寫。
 *     輸出的長度事先不知道時（例如壓縮後有多大），由函式自己 malloc，
 *     再把「那塊記憶體的位址」交給呼叫端；這時參數會出現兩顆星：uint8_t **out，見 huff_encode。
 *===========================================================================*/

/* 【header guard】下面兩行加上檔尾的 #endif 是固定寫法：
 *   第一次 #include 這個檔時 TEXTLINK_H 還沒被定義 → 定義它，並讀進整個檔案的內容；
 *   同一個 .c 若又（經由別的 .h）間接 #include 到這裡，TEXTLINK_H 已經定義過 → 整段跳過。
 * 沒有它，typedef 與 enum 被讀進兩次，編譯器會報「重複定義」。 */
#ifndef TEXTLINK_H
#define TEXTLINK_H

/* #include "..." 用雙引號：先從專案自己的資料夾找（Makefile 的 -Iinclude 就是在指定這個資料夾）；
 * 用角括號的 #include <stdio.h> 則是找編譯器附的系統標頭。
 * platform.h 已經幫你 #include 了 <stdint.h> <stdio.h> <stdlib.h> <string.h>，
 * 所以 uint8_t、printf、malloc／free、memcpy、strcmp 在每個 .c 裡都可以直接用。
 *
 * 【<stdint.h> 的固定寬度整數】int 有多大由編譯器決定；但檔案與網路的格式規定的是
 * 「剛好幾個 byte」，所以這個專案一律用寬度寫在名字上的型別（u = unsigned，數字 = 幾個 bit）：
 *     uint8_t   0 到 255：就是「一個 byte」，整個專案拿它來表示原始資料
 *     uint16_t  0 到 65,535
 *     uint32_t  0 到 4,294,967,295：frame 的 length 欄位剛好 4 bytes，用它最自然
 *     uint64_t  0 到約 1.8 × 10^19：檔案大小、累計的 bytes 數、頻率的總和
 *     int16_t   −32,768 到 32,767：WAV 的一個 16-bit sample
 * 【size_t】「記憶體裡的大小或個數」專用的無號整數：sizeof、strlen 的結果，malloc、memcpy 的參數
 * 都是它。64-bit 電腦上通常是 8 bytes。它沒有負數：size_t 的 0 再減 1 會變成一個超大的正數，
 * 而不是 −1。所以要寫「a 會不會超過 b」的檢查時，先比較、再做減法。 */
#include "platform.h"

/*------------------------------- 規格常數 ---------------------------------*/
/* 【#define 常數】編譯之前，前置處理器會把程式裡的名字原封不動換成後面那串文字
 * （純文字取代，不是變數，沒有型別、也不佔記憶體）。好處是數字只寫在一個地方、而且有名字。
 * 數字後面的 u 表示 unsigned：16u * 1024u * 1024u 以無號整數計算。
 * 整串用括號包起來，是因為文字取代之後，外面若還有別的運算，優先順序才不會被打亂。 */
#define TL_HDR_LEN    5                        /* frame 標頭：length 4 bytes + type 1 byte */
#define TL_MAX_FRAME  (16u * 1024u * 1024u)    /* length 欄位的上限（16 MiB）             */
#define TL_CHUNK      (64u * 1024u)            /* 每個 FILE_DATA 的 payload 大小           */
#define TL_MAX_FILE   (64u * 1024u * 1024u)    /* 本程式願意處理的最大檔案                 */
#define TL_MAX_TEXT   4096                     /* 一則聊天訊息的最大 bytes 數              */
#define TL_CONNECT_TIMEOUT_MS 10000            /* 連線逾時上限                             */

/* 【enum】一次替一組整數常數取名字。和 #define 一樣是為了不要在程式裡到處寫 0x11 這種
 * 沒人看得懂的數字；差別是 enum 由編譯器處理，除錯器裡看得到名字。
 * 這裡的 enum 後面沒有型別名稱（匿名），單純拿來定義常數；0x 開頭是十六進位。 */

/* frame 的 type（規格固定，各組相同） */
enum {
    T_TEXT_RAW   = 0x01,
    T_TEXT_HUFF  = 0x02,
    T_FILE_BEGIN = 0x10,
    T_FILE_DATA  = 0x11,
    T_FILE_END   = 0x12
};

/* 回傳值：0 成功，負值失敗
 * 本專案回傳 int 的函式幾乎都用這一組（例外會另外註明）。固定的用法：
 *
 *     int rc = 某個函式(...);
 *     if (rc != TL_OK) return rc;        自己處理不了，就把錯誤原樣往上交給呼叫我的人
 *
 * 錯誤就這樣一層一層傳回 main 或聊天畫面，在那裡用 tl_strerror(rc) 印成看得懂的字。
 * 你們寫 TODO 時，請依註解指定的情況回傳指定的那個錯誤碼：測試會檢查是不是剛好那一個。 */
enum {
    TL_OK         =  0,
    TL_ERR_TODO   = -1,    /* place holder 尚未實作          */
    TL_ERR_NET    = -2,    /* socket 錯誤                    */
    TL_ERR_CLOSED = -3,    /* 對方關閉連線                   */
    TL_ERR_PROTO  = -4,    /* 封包不合規格                   */
    TL_ERR_NOMEM  = -5,    /* 記憶體不足                     */
    TL_ERR_IO     = -6,    /* 檔案讀寫失敗                   */
    TL_ERR_DATA   = -7     /* 資料內容不合法（壞 codebook、非法 UTF-8…） */
};

/* 【typedef enum】typedef 是「替型別取一個新名字」。這一行同時做兩件事：
 * 定義常數 MODE_RAW = 0、MODE_HUFF = 1，並把這種 enum 取名為 tl_mode_t。
 * 之後就能宣告 tl_mode_t mode;，讀程式的人一看就知道這個變數只該是這兩個值之一
 * （C 的 enum 骨子裡仍是整數，編譯器不會幫你擋掉其他值）。
 * 名字結尾的 _t 是「這是一個型別」的慣例，和 size_t、uint8_t 一樣。 */
typedef enum { MODE_RAW = 0, MODE_HUFF = 1 } tl_mode_t;

/* Huffman 的「符號」怎麼定：對什麼東西統計出現機率、對什麼東西編碼 */
typedef enum {
    SYM_BYTE = 0,      /* 符號 = byte：任何資料都能用；也是下面兩種不適用時的退路          */
    SYM_CHAR = 1,      /* 符號 = UTF-8 字元（code point）：文字用。統計每個「字」的機率   */
    SYM_S16  = 2       /* 符號 = 16-bit sample value：WAV 用。對 sample 值做 histogram  */
} tl_sym_t;

/* 回傳型別 const char * 是「指向不可修改字元的指標」，也就是一個唯讀的 C 字串。
 * 這兩個函式回傳的是寫死在程式裡的字串常數：可以直接拿去 printf("%s")，不可以改、也不可以 free。
 *     fprintf(stderr, "錯誤: %s\n", tl_strerror(rc)); */
const char *tl_sym_name(tl_sym_t sym);         /* "byte"／"char"／"s16" */

const char *tl_strerror(int rc);               /* 回傳值 → 看得懂的中文說明 */

/*------------------------------- net.c ------------------------------------*/
/* 這一節全部已經寫好，寫 TODO 時不會直接用到；列在這裡是因為 main.c、chat.c、transfer.c 要呼叫。
 * socket_t 定義在 platform.h：代表「一條 TCP 連線」的代號（Windows 與 macOS／Linux 的真實型別不同，
 * 用 typedef 取同一個名字，其他檔案就不必管是哪個平台）。 */

/* 參數列寫 (void) 表示「沒有參數」。在 C 裡空的 () 意思是「參數不明」，兩者不一樣。
 * net_init：程式開始用網路前呼叫一次，回傳 TL_OK 或 TL_ERR_NET。
 * net_cleanup：程式結束前呼叫一次。回傳型別 void 表示沒有回傳值。 */
int      net_init(void);
void     net_cleanup(void);

/* 把命令列的字串（例如 "5000"）轉成整數 port。
 * 注意：這個函式的回傳值不是 TL_* 錯誤碼，而是 port 本身；不是整數或超出範圍時回傳 -1。 */
int      net_parse_port(const char *s);        /* 合法回傳 1–65535，否則 -1 */

/* 監聽 bind_ip:port 並等一個人連進來。bind_ip 為 NULL 表示 0.0.0.0（所有網路介面）。
 * peer 會填入對方的 "IP:port"。失敗回傳 SOCK_INVALID（錯誤訊息已印出）。
 *
 * 【緩衝區 + 容量】peer 是呼叫端準備好的 char 陣列，函式把字串寫進去；
 * C 的陣列傳進函式後，函式量不到它有多大，所以要另外用 peer_cap 告訴函式「最多可以寫幾個 byte」：
 *     char peer[64];
 *     socket_t s = net_listen_accept(NULL, 5000, peer, sizeof(peer));
 *     if (s == SOCK_INVALID) { 失敗，結束程式 }
 * NULL 是「不指向任何東西」的指標值，這裡拿來表示「沒有指定」。 */
socket_t net_listen_accept(const char *bind_ip, int port, char *peer, size_t peer_cap);

/* 連到 ip:port，最多等 timeout_ms。失敗回傳 SOCK_INVALID（錯誤訊息已印出）。 */
socket_t net_connect(const char *ip, int port, int timeout_ms, char *peer, size_t peer_cap);

/* TCP 是位元組串流：send／recv 一次不一定處理完 n bytes，這兩個函式會迴圈到滿為止
 * 回傳 TL_OK 代表「剛好 n bytes 都送出／收到了」。
 * 失敗：send_all 回傳 TL_ERR_NET；recv_all 在對方關閉連線時回傳 TL_ERR_CLOSED，其他錯誤回傳 TL_ERR_NET。
 * 【void *】「不管指向什麼型別」的指標：這兩個函式只是搬 bytes，所以 uint8_t 陣列、char 陣列都可以傳進來。
 * recv_all 的 buf 由呼叫端準備，至少要有 n bytes 的空間。 */
int      send_all(socket_t s, const void *buf, size_t n);
int      recv_all(socket_t s, void *buf, size_t n);

/* 【extern】這兩個是全域變數，本體定義在 net.c；extern 的意思是
 * 「這個變數存在於別的 .c 檔，這裡只是讓大家知道它的名字與型別」，不會再多配置一份。
 * send_all／recv_all 每處理一些 bytes 就會把數量加上去。 */
extern uint64_t g_tx_bytes;                    /* 本程式送上 TCP 的總 bytes（除錯用的累計值） */
extern uint64_t g_rx_bytes;                    /* 本程式從 TCP 收到的總 bytes                */

/* 量一段程式花多久：double t0 = now_ms();  …要量的工作…  double ms = now_ms() - t0; */
double   now_ms(void);                         /* 單調時鐘，毫秒 */

/* 送出鎖：聊天時主執行緒（送訊息、送檔案）與接收執行緒（回覆收檔結果）都會送 frame，
 * frame_send 用它保證「一個 frame 的標頭與 payload 不會被另一個 frame 插隊」。 */
void     net_send_lock(void);
void     net_send_unlock(void);

/*------------------------------- frame.c ----------------------------------*/
/* ★ TODO：把 type 與 payload_len 打包成 5 bytes 的標頭（length 為 big-endian）。
 *   輸入  type：frame 的種類（上面的 T_*）；payload_len：payload 有幾個 byte（可以是 0）。
 *   輸出  hdr：呼叫端準備好的 5 bytes 陣列，由本函式填滿。
 *   回傳  TL_OK；length 超過 TL_MAX_FRAME 時回傳 TL_ERR_PROTO。
 *   記憶體：不配置任何東西。
 *   【陣列當參數】寫成 uint8_t hdr[TL_HDR_LEN] 只是提醒讀的人「這裡要傳 5 bytes 的陣列」；
 *   C 實際傳進來的是陣列第一格的位址（等同 uint8_t *hdr），所以在函式裡寫 hdr[i] 改到的就是
 *   呼叫端那個陣列——這正是我們要的。
 *       uint8_t hdr[TL_HDR_LEN];
 *       int rc = frame_pack_header(hdr, T_TEXT_RAW, 3);       成功後 hdr 是 00 00 00 04 01 */
int frame_pack_header(uint8_t hdr[TL_HDR_LEN], uint8_t type, size_t payload_len);

/* ★ TODO：解析 5 bytes 標頭，並檢查 length 是否在合法範圍。
 *   輸入  hdr：剛從網路收到的 5 bytes（const：只讀）。
 *   輸出  *type、*payload_len：兩個都是呼叫端變數的位址，由本函式寫入（見檔頭的共通寫法 1）。
 *   回傳  TL_OK；length 為 0 或超過 TL_MAX_FRAME 時回傳 TL_ERR_PROTO（這時呼叫端不會使用那兩個輸出）。
 *       uint8_t type;  size_t n;
 *       int rc = frame_parse_header(hdr, &type, &n);          hdr 是 00 00 01 00 02 → type = 0x02、n = 255 */
int frame_parse_header(const uint8_t hdr[TL_HDR_LEN], uint8_t *type, size_t *payload_len);

/* 已寫好：送出／收下一個完整的 frame。
 * frame_recv 會 malloc *payload（多配 1 byte 並補 '\0'），呼叫端負責 free。
 *
 * frame_send：payload 指向 len 個 bytes；len 為 0 時 payload 可以是 NULL（空的 frame）。
 *             回傳 TL_OK，或 frame_pack_header／send_all 回報的錯誤碼。
 * frame_recv：會一直等到收滿一個完整的 frame 才回來。三個輸出都是呼叫端變數的位址。
 *             回傳 TL_OK，或 TL_ERR_CLOSED／TL_ERR_NET／TL_ERR_PROTO／TL_ERR_NOMEM；
 *             失敗時 *payload 是 NULL，不需要 free。
 *
 * 【兩顆星 uint8_t **payload】呼叫端有一個「指標變數」 p，想讓函式把 malloc 得到的位址存進 p。
 * 要讓函式改到呼叫端的變數，就得傳那個變數的位址；p 本身的型別是 uint8_t *，
 * 它的位址的型別就多一顆星，變成 uint8_t **。函式裡寫 *payload = buf; 就是「把 buf 這個位址存進呼叫端的 p」。
 *
 *     uint8_t type;  uint8_t *p = NULL;  size_t len = 0;
 *     int rc = frame_recv(s, &type, &p, &len);
 *     if (rc == TL_OK) {
 *         …使用 p[0] 到 p[len-1]…
 *         free(p);                      這塊記憶體是 frame_recv 配置的，但交給你之後就歸你管：
 *     }                                 用完一定要 free，而且只能 free 一次
 *
 * malloc 出來的記憶體不會在函式結束時自動消失（這點和區域變數不同）；沒有人 free 就會一直佔著，
 * 聊天聊久了記憶體越用越多，這叫 memory leak。 */
int frame_send(socket_t s, uint8_t type, const uint8_t *payload, size_t len);
int frame_recv(socket_t s, uint8_t *type, uint8_t **payload, size_t *len);

/*------------------------------- utf8.c -----------------------------------*/
/* ★ TODO：s[0..n) 是合法 UTF-8（RFC 3629）回傳 TL_OK，否則 TL_ERR_DATA。
 *   輸入  s 指向 n 個 bytes（不是 C 字串：不保證有結尾的 '\0'，n 可以是 0）。
 *   輸出  只有回傳值。不修改資料、不配置記憶體、不印任何東西。
 *   字串常數的型別是 char *，傳進來時要轉型（tests/test_codec.c 的 utf8_case 就是這樣做）：
 *       if (utf8_validate((const uint8_t *)"abc", 3) == TL_OK) { 是合法的 UTF-8 } */
int utf8_validate(const uint8_t *s, size_t n);

/*------------------------------- huffman.c --------------------------------*/
/* ★ TODO：把 in[0..in_len) 依 sym 指定的方式切成符號、統計機率、編碼成「自己帶 codebook、
 *   可以獨立解碼」的一塊資料（區塊裡要記下用的是哪一種符號，解碼端才知道怎麼還原）。
 *   成功時 *out 由本函式 malloc，呼叫端 free。格式由各組自訂，寫進 docs/interface.md。
 *   資料不適用該種符號時回傳 TL_ERR_DATA（SYM_CHAR 遇到非法 UTF-8；SYM_S16 遇到不是 16-bit PCM 的 WAV），
 *   殼會自動改用 SYM_BYTE 再試一次。
 *
 *   輸入  in、in_len：原始資料（只讀；in_len 可以是 0）；sym：符號種類。
 *   輸出  *out：編碼結果的位址；*out_len：編碼結果有幾個 byte。只有回傳 TL_OK 時才有意義。
 *   回傳  TL_OK／TL_ERR_DATA（資料不適用這種符號）／TL_ERR_NOMEM（malloc 失敗）。
 *   記憶體：成功 → 呼叫端要 free(*out)。
 *           失敗 → 你們在函式裡 malloc 過的每一塊（包含暫時用的表格、樹、輸出緩衝區），回傳前都要自己 free 乾淨，
 *           而且不可以讓 *out 留著一個已經 free 掉的位址：殼與測試在失敗後仍可能對它呼叫 free，
 *           同一塊被 free 兩次程式會當掉。最簡單的做法：確定全部成功了，最後才寫 *out 與 *out_len。
 *   uint8_t **out 為什麼是兩顆星，見上面 frame_recv 的說明，用法完全相同：
 *       uint8_t *enc = NULL;  size_t enc_len = 0;
 *       int rc = huff_encode(data, data_len, SYM_BYTE, &enc, &enc_len);
 *       if (rc == TL_OK) { …使用 enc[0] 到 enc[enc_len-1]…  free(enc); } */
int huff_encode(const uint8_t *in, size_t in_len, tl_sym_t sym, uint8_t **out, size_t *out_len);

/* ★ TODO：huff_encode 的反運算。解出來超過 max_out bytes 要回報錯誤，不可以照著
 *   對方宣稱的大小去配置記憶體（對方可能是壞人，也可能只是傳壞了）。
 *
 *   輸入  in、in_len：huff_encode 產生的那一塊（經過網路之後，可能被截斷、被改過）；
 *         max_out：呼叫端願意接受的最大還原長度。殼在聊天時傳 TL_MAX_TEXT - 1，
 *                  收檔案時傳 FILE_BEGIN 裡宣稱的原始大小（已先確認不超過 TL_MAX_FILE）。
 *   輸出  *out：還原出來的資料（本函式 malloc）；*out_len：它的長度，要等於編碼前的 in_len。
 *   回傳  TL_OK／TL_ERR_DATA（任何看起來不對的輸入）／TL_ERR_NOMEM。
 *   記憶體：和 huff_encode 相同——成功由呼叫端 free(*out)；失敗時函式自己清乾淨。
 *   兩個函式合起來要滿足 round-trip：decode(encode(x)) 與 x 逐 byte 相同。 */
int huff_decode(const uint8_t *in, size_t in_len, size_t max_out, uint8_t **out, size_t *out_len);

/*------------------------------- transfer.c -------------------------------*/
/* 這一節全部已經寫好。
 * 【struct】把幾個相關的變數綁成一包，整包一起傳來傳去。typedef struct { … } tl_stats_t; 和上面的
 * typedef enum 一樣，是替這一包取型別名稱。取裡面的欄位：
 *     變數用「.」     tl_stats_t st;   st.file_bytes = 100;
 *     指標用「->」    tl_stats_t *p = &st;   p->file_bytes = 100;      p->x 是 (*p).x 的簡寫
 * 函式參數通常傳 struct 的位址（tl_stats_t *st）：不必複製整包，函式也才能把結果填回去。 */

/* 一次檔案傳輸的統計數字；file_send_frames 與 file_rx_finish 會填好，用來印 STATS 與聊天畫面上的數字 */
typedef struct {
    char      name[128];
    tl_mode_t mode;
    tl_sym_t  sym;                 /* huff 模式實際使用的符號種類                 */
    uint64_t  file_bytes;          /* 原檔大小                                  */
    uint64_t  wire_bytes;          /* 實際上線 bytes（含所有 frame 標頭與 codebook）*/
    double    encode_ms, decode_ms, send_ms, total_ms;
} tl_stats_t;

/* 【函式指標】這個 typedef 定義的型別是「指向某種函式的指標」：那種函式吃 (label, done, total)、沒有回傳值。
 * 用途是 callback：file_send_frames 每送完一段就呼叫一次你交給它的函式，
 * 命令列版拿來畫進度條、聊天版拿來更新畫面；傳 NULL 表示不需要通知。 */
typedef void (*tl_progress_fn)(const char *label, uint64_t done, uint64_t total);

double tl_ratio(uint64_t wire, uint64_t file);                 /* 壓縮率 = wire ÷ file */

/* 在已連線的 socket 上送出一個檔案（FILE_BEGIN → FILE_DATA × N → FILE_END）。不等對方回覆。
 * huff 模式會在裡面呼叫 huff_encode；回傳 TL_* 錯誤碼，*st 由本函式填寫。 */
int  file_send_frames(socket_t s, const char *path, tl_mode_t mode, tl_stats_t *st, tl_progress_fn progress);

/* 接收端的狀態：把收到的 FILE_* frame 依序餵進來 */
typedef struct {
    int       begun;
    tl_mode_t mode;
    uint64_t  orig_size, data_size, got, wire;
    char      name[128];
    uint8_t  *data;
    double    t0;
} file_rx_t;

/* 用法：收到 FILE_BEGIN 呼叫 file_rx_begin、每個 FILE_DATA 呼叫 file_rx_data、
 * FILE_END 呼叫 file_rx_finish（huff 模式會在裡面呼叫 huff_decode，成功才存檔），
 * 最後不論成功失敗都呼叫 file_rx_reset 釋放 rx->data。前三個回傳 TL_* 錯誤碼。 */
int  file_rx_begin(file_rx_t *rx, const uint8_t *payload, size_t len);
int  file_rx_data(file_rx_t *rx, const uint8_t *payload, size_t len);
int  file_rx_finish(file_rx_t *rx, const char *outdir, tl_stats_t *st, char *saved, size_t saved_cap);
void file_rx_reset(file_rx_t *rx);

/*------------------------------- 三個入口 ----------------------------------*/
/* main.c 解析完命令列後呼叫其中一個。注意：這三個的回傳值不是 TL_* 錯誤碼，
 * 而是「程式的結束碼」（0 成功、非 0 失敗），main 會直接把它 return 出去。 */
int chat_run(socket_t s, const char *peer, tl_mode_t mode);              /* 功能 1 */
int transfer_send(const char *ip, int port, const char *path, tl_mode_t mode);   /* 功能 2、3 */
int transfer_recv(const char *bind_ip, int port, const char *outdir);

#endif /* TEXTLINK_H */
