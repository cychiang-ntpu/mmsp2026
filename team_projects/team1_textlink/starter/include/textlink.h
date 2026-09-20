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
 *===========================================================================*/
#ifndef TEXTLINK_H
#define TEXTLINK_H

#include "platform.h"

/*------------------------------- 規格常數 ---------------------------------*/
#define TL_HDR_LEN    5                        /* frame 標頭：length 4 bytes + type 1 byte */
#define TL_MAX_FRAME  (16u * 1024u * 1024u)    /* length 欄位的上限（16 MiB）             */
#define TL_CHUNK      (64u * 1024u)            /* 每個 FILE_DATA 的 payload 大小           */
#define TL_MAX_FILE   (64u * 1024u * 1024u)    /* 本程式願意處理的最大檔案                 */
#define TL_MAX_TEXT   4096                     /* 一則聊天訊息的最大 bytes 數              */
#define TL_CONNECT_TIMEOUT_MS 10000            /* 連線逾時上限                             */

/* frame 的 type（規格固定，各組相同） */
enum {
    T_TEXT_RAW   = 0x01,
    T_TEXT_HUFF  = 0x02,
    T_FILE_BEGIN = 0x10,
    T_FILE_DATA  = 0x11,
    T_FILE_END   = 0x12
};

/* 回傳值：0 成功，負值失敗 */
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

typedef enum { MODE_RAW = 0, MODE_HUFF = 1 } tl_mode_t;

/* Huffman 的「符號」怎麼定：對什麼東西統計出現機率、對什麼東西編碼 */
typedef enum {
    SYM_BYTE = 0,      /* 符號 = byte：任何資料都能用；也是下面兩種不適用時的退路          */
    SYM_CHAR = 1,      /* 符號 = UTF-8 字元（code point）：文字用。統計每個「字」的機率   */
    SYM_S16  = 2       /* 符號 = 16-bit sample value：WAV 用。對 sample 值做 histogram  */
} tl_sym_t;

const char *tl_sym_name(tl_sym_t sym);         /* "byte"／"char"／"s16" */

const char *tl_strerror(int rc);               /* 回傳值 → 看得懂的中文說明 */

/*------------------------------- net.c ------------------------------------*/
int      net_init(void);
void     net_cleanup(void);
int      net_parse_port(const char *s);        /* 合法回傳 1–65535，否則 -1 */

/* 監聽 bind_ip:port 並等一個人連進來。bind_ip 為 NULL 表示 0.0.0.0（所有網路介面）。
 * peer 會填入對方的 "IP:port"。失敗回傳 SOCK_INVALID（錯誤訊息已印出）。 */
socket_t net_listen_accept(const char *bind_ip, int port, char *peer, size_t peer_cap);

/* 連到 ip:port，最多等 timeout_ms。失敗回傳 SOCK_INVALID（錯誤訊息已印出）。 */
socket_t net_connect(const char *ip, int port, int timeout_ms, char *peer, size_t peer_cap);

/* TCP 是位元組串流：send／recv 一次不一定處理完 n bytes，這兩個函式會迴圈到滿為止 */
int      send_all(socket_t s, const void *buf, size_t n);
int      recv_all(socket_t s, void *buf, size_t n);

extern uint64_t g_tx_bytes;                    /* 本程式送上 TCP 的總 bytes（wire_bytes 用） */
extern uint64_t g_rx_bytes;                    /* 本程式從 TCP 收到的總 bytes                */

double   now_ms(void);                         /* 單調時鐘，毫秒 */

/* 送出鎖：聊天時主執行緒（送訊息、送檔案）與接收執行緒（回覆收檔結果）都會送 frame，
 * frame_send 用它保證「一個 frame 的標頭與 payload 不會被另一個 frame 插隊」。 */
void     net_send_lock(void);
void     net_send_unlock(void);

/*------------------------------- frame.c ----------------------------------*/
/* ★ TODO：把 type 與 payload_len 打包成 5 bytes 的標頭（length 為 big-endian）。 */
int frame_pack_header(uint8_t hdr[TL_HDR_LEN], uint8_t type, size_t payload_len);

/* ★ TODO：解析 5 bytes 標頭，並檢查 length 是否在合法範圍。 */
int frame_parse_header(const uint8_t hdr[TL_HDR_LEN], uint8_t *type, size_t *payload_len);

/* 已寫好：送出／收下一個完整的 frame。
 * frame_recv 會 malloc *payload（多配 1 byte 並補 '\0'），呼叫端負責 free。 */
int frame_send(socket_t s, uint8_t type, const uint8_t *payload, size_t len);
int frame_recv(socket_t s, uint8_t *type, uint8_t **payload, size_t *len);

/*------------------------------- utf8.c -----------------------------------*/
/* ★ TODO：s[0..n) 是合法 UTF-8（RFC 3629）回傳 TL_OK，否則 TL_ERR_DATA。 */
int utf8_validate(const uint8_t *s, size_t n);

/*------------------------------- huffman.c --------------------------------*/
/* ★ TODO：把 in[0..in_len) 依 sym 指定的方式切成符號、統計機率、編碼成「自己帶 codebook、
 *   可以獨立解碼」的一塊資料（區塊裡要記下用的是哪一種符號，解碼端才知道怎麼還原）。
 *   成功時 *out 由本函式 malloc，呼叫端 free。格式由各組自訂，寫進 docs/interface.md。
 *   資料不適用該種符號時回傳 TL_ERR_DATA（SYM_CHAR 遇到非法 UTF-8；SYM_S16 遇到不是 16-bit PCM 的 WAV），
 *   殼會自動改用 SYM_BYTE 再試一次。 */
int huff_encode(const uint8_t *in, size_t in_len, tl_sym_t sym, uint8_t **out, size_t *out_len);

/* ★ TODO：huff_encode 的反運算。解出來超過 max_out bytes 要回報錯誤，不可以照著
 *   對方宣稱的大小去配置記憶體（對方可能是壞人，也可能只是傳壞了）。 */
int huff_decode(const uint8_t *in, size_t in_len, size_t max_out, uint8_t **out, size_t *out_len);

/*------------------------------- transfer.c -------------------------------*/
typedef struct {
    char      name[128];
    tl_mode_t mode;
    tl_sym_t  sym;                 /* huff 模式實際使用的符號種類                 */
    uint64_t  file_bytes;          /* 原檔大小                                  */
    uint64_t  wire_bytes;          /* 實際上線 bytes（含所有 frame 標頭與 codebook）*/
    double    encode_ms, decode_ms, send_ms, total_ms;
} tl_stats_t;

typedef void (*tl_progress_fn)(const char *label, uint64_t done, uint64_t total);

double tl_ratio(uint64_t wire, uint64_t file);                 /* 壓縮率 = wire ÷ file */

/* 在已連線的 socket 上送出一個檔案（FILE_BEGIN → FILE_DATA × N → FILE_END）。不等對方回覆。 */
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

int  file_rx_begin(file_rx_t *rx, const uint8_t *payload, size_t len);
int  file_rx_data(file_rx_t *rx, const uint8_t *payload, size_t len);
int  file_rx_finish(file_rx_t *rx, const char *outdir, tl_stats_t *st, char *saved, size_t saved_cap);
void file_rx_reset(file_rx_t *rx);

/*------------------------------- 三個入口 ----------------------------------*/
int chat_run(socket_t s, const char *peer, tl_mode_t mode);              /* 功能 1 */
int transfer_send(const char *ip, int port, const char *path, tl_mode_t mode);   /* 功能 2、3 */
int transfer_recv(const char *bind_ip, int port, const char *outdir);

#endif /* TEXTLINK_H */
