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
 *===========================================================================*/
#include "textlink.h"
#include <inttypes.h>
#ifdef _WIN32
  #include <direct.h>
#endif

#define BEGIN_FIXED 17          /* FILE_BEGIN 固定欄位：1 + 8 + 8 */

static void put_u64be(uint8_t *p, uint64_t v) {
    for (int i = 7; i >= 0; i--) { p[i] = (uint8_t)(v & 0xFF); v >>= 8; }
}

static uint64_t get_u64be(const uint8_t *p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v = (v << 8) | p[i];
    return v;
}

/* 壓縮率 = 實際上線 bytes ÷ 原檔 bytes（含所有 frame 標頭與 codebook）；越小越好，大於 1 代表變大了 */
double tl_ratio(uint64_t wire, uint64_t file) {
    return file ? (double)wire / (double)file : 0.0;
}

/* 路徑最後一段；不讓對方指定的檔名帶有資料夾（../../ 之類）*/
static const char *base_name(const char *path) {
    const char *b = path;
    for (const char *p = path; *p; p++)
        if (*p == '/' || *p == '\\') b = p + 1;
    return b;
}

/* 檔名只保留安全的 ASCII 字元，其餘換成 '_'。
 * （中文檔名在 Windows 上要用 _wfopen 等寬字元 API 才能正確開檔，這個殼先不處理。）*/
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
static tl_sym_t sym_for_file(const char *path) {
    if (ends_with_nocase(path, ".wav")) return SYM_S16;
    if (ends_with_nocase(path, ".txt")) return SYM_CHAR;
    return SYM_BYTE;
}

static int read_whole_file(const char *path, uint8_t **buf, size_t *len) {
    FILE *f = fopen(path, "rb");                 /* "rb"：Windows 才不會動 \r\n */
    if (f == NULL) return TL_ERR_IO;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return TL_ERR_IO; }
    long sz = ftell(f);
    rewind(f);
    if (sz < 0 || (unsigned long)sz > TL_MAX_FILE) { fclose(f); return TL_ERR_IO; }   /* 上限 64 MiB */
    uint8_t *b = (uint8_t *)malloc((size_t)sz + 1);
    if (b == NULL) { fclose(f); return TL_ERR_NOMEM; }
    if (sz > 0 && fread(b, 1, (size_t)sz, f) != (size_t)sz) { free(b); fclose(f); return TL_ERR_IO; }
    fclose(f);
    *buf = b;
    *len = (size_t)sz;
    return TL_OK;
}

/*========================= 傳送端（聊天與命令列共用） ======================*/
int file_send_frames(socket_t s, const char *path, tl_mode_t mode, tl_stats_t *st, tl_progress_fn progress) {
    uint8_t *file = NULL, *enc = NULL;
    size_t file_len = 0, enc_len = 0;
    double t0 = now_ms();
    int rc;

    memset(st, 0, sizeof(*st));
    st->mode = mode;
    sanitize_name(path, st->name, sizeof(st->name));

    rc = read_whole_file(path, &file, &file_len);
    if (rc != TL_OK) return rc;
    st->file_bytes = (uint64_t)file_len;

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
        if (rc != TL_OK) goto done;
        data = enc;
        data_len = enc_len;
    }

    uint8_t begin[BEGIN_FIXED + sizeof(st->name)];
    size_t name_len = strlen(st->name);
    begin[0] = (uint8_t)mode;
    put_u64be(begin + 1, (uint64_t)file_len);
    put_u64be(begin + 9, (uint64_t)data_len);
    memcpy(begin + BEGIN_FIXED, st->name, name_len);
    rc = frame_send(s, T_FILE_BEGIN, begin, BEGIN_FIXED + name_len);
    if (rc != TL_OK) goto done;
    st->wire_bytes += TL_HDR_LEN + BEGIN_FIXED + name_len;

    double ts = now_ms();
    for (size_t off = 0; off < data_len; off += TL_CHUNK) {
        size_t n = data_len - off < TL_CHUNK ? data_len - off : TL_CHUNK;
        rc = frame_send(s, T_FILE_DATA, data + off, n);
        if (rc != TL_OK) goto done;
        st->wire_bytes += TL_HDR_LEN + n;
        if (progress) progress("傳送", off + n, data_len);
    }
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
void file_rx_reset(file_rx_t *rx) {
    free(rx->data);
    memset(rx, 0, sizeof(*rx));
}

int file_rx_begin(file_rx_t *rx, const uint8_t *p, size_t len) {
    if (rx->begun || len < BEGIN_FIXED + 1 || p[0] > 1) return TL_ERR_PROTO;
    rx->t0 = now_ms();
    rx->mode = (tl_mode_t)p[0];
    rx->orig_size = get_u64be(p + 1);
    rx->data_size = get_u64be(p + 9);
    /* 大小是對方說的：先檢查再配置 */
    if (rx->orig_size > TL_MAX_FILE || rx->data_size > TL_MAX_FILE ||
        (rx->mode == MODE_RAW && rx->orig_size != rx->data_size)) return TL_ERR_PROTO;

    char raw_name[128];
    size_t nl = len - BEGIN_FIXED < sizeof(raw_name) - 1 ? len - BEGIN_FIXED : sizeof(raw_name) - 1;
    memcpy(raw_name, p + BEGIN_FIXED, nl);
    raw_name[nl] = '\0';
    sanitize_name(raw_name, rx->name, sizeof(rx->name));

    rx->data = (uint8_t *)malloc((size_t)rx->data_size + 1);
    if (rx->data == NULL) return TL_ERR_NOMEM;
    rx->wire = TL_HDR_LEN + len;
    rx->begun = 1;
    return TL_OK;
}

int file_rx_data(file_rx_t *rx, const uint8_t *p, size_t len) {
    if (!rx->begun || rx->got + len > rx->data_size) return TL_ERR_PROTO;     /* 比宣稱的多：拒收 */
    memcpy(rx->data + rx->got, p, len);
    rx->got += len;
    rx->wire += TL_HDR_LEN + len;
    return TL_OK;
}

static int write_file_atomically(const char *path, const uint8_t *buf, size_t len) {
    char tmp[600];
    snprintf(tmp, sizeof(tmp), "%s.part", path);

    /* 先寫到 .part，全部成功才改名：失敗時不會留下「看起來完整、其實壞掉」的檔案 */
    FILE *f = fopen(tmp, "wb");
    if (f == NULL) return TL_ERR_IO;
    size_t w = len ? fwrite(buf, 1, len, f) : 0;
    if (fclose(f) != 0 || w != len) { remove(tmp); return TL_ERR_IO; }
    remove(path);                                  /* Windows 的 rename 不會覆蓋既有檔案 */
    if (rename(tmp, path) != 0) { remove(tmp); return TL_ERR_IO; }
    return TL_OK;
}

/* 收到傳送端的 FILE_END 之後呼叫：檢查、解碼、存檔。saved 會填入存檔路徑。 */
int file_rx_finish(file_rx_t *rx, const char *outdir, tl_stats_t *st, char *saved, size_t saved_cap) {
    uint8_t *dec = NULL;
    int rc = TL_OK;

    memset(st, 0, sizeof(*st));
    if (!rx->begun || rx->got != rx->data_size) return TL_ERR_PROTO;
    rx->wire += TL_HDR_LEN;                                         /* FILE_END 本身 */

    const uint8_t *out = rx->data;
    size_t out_len = (size_t)rx->got;
    if (rx->mode == MODE_HUFF) {
        double td = now_ms();
        rc = huff_decode(rx->data, (size_t)rx->got, (size_t)rx->orig_size, &dec, &out_len);
        st->decode_ms = now_ms() - td;
        out = dec;
        if (rc == TL_OK && out_len != rx->orig_size) rc = TL_ERR_DATA;
    }
    if (rc == TL_OK) {
#ifdef _WIN32
        _mkdir(outdir);
#else
        mkdir(outdir, 0755);
#endif
        snprintf(saved, saved_cap, "%s/%s", outdir, rx->name);
        rc = write_file_atomically(saved, out, out_len);
    }
    snprintf(st->name, sizeof(st->name), "%s", rx->name);
    st->mode = rx->mode;
    st->file_bytes = rx->orig_size;
    st->wire_bytes = rx->wire;
    st->total_ms = now_ms() - rx->t0;
    free(dec);
    return rc;
}

/*========================= 命令列：textlink send ===========================*/
static void draw_progress(const char *label, uint64_t done, uint64_t total) {
    const int width = 30;
    int fill = total ? (int)((double)done * width / (double)total) : width;
    printf("\r%s [", label);
    for (int i = 0; i < width; i++) putchar(i < fill ? '#' : '-');
    printf("] %3.0f%%  %.2f / %.2f MB", total ? 100.0 * (double)done / (double)total : 100.0,
           (double)done / 1048576.0, (double)total / 1048576.0);
    fflush(stdout);
}

static void explain(const char *what, int rc) {
    fprintf(stderr, "\n錯誤: %s：%s\n", what, tl_strerror(rc));
    if (rc == TL_ERR_TODO)
        fprintf(stderr, "      （frame 標頭在 src/frame.c、Huffman 在 src/huffman.c；raw 傳輸只需要先完成 frame）\n");
    if (rc == TL_ERR_IO)
        fprintf(stderr, "      （檔案開不了、超過 %u MiB，或寫不進去）\n", TL_MAX_FILE / (1024u * 1024u));
}

int transfer_send(const char *ip, int port, const char *path, tl_mode_t mode) {
    char peer[64];
    tl_stats_t st;
    int ok = 0;

    socket_t s = net_connect(ip, port, TL_CONNECT_TIMEOUT_MS, peer, sizeof(peer));
    if (s == SOCK_INVALID) return 1;

    double t0 = now_ms();
    int rc = file_send_frames(s, path, mode, &st, draw_progress);
    printf("\n");
    if (rc != TL_OK) { explain("傳送失敗", rc); CLOSESOCK(s); return 1; }

    /* 等接收端回覆：對方真的解碼、寫檔成功，這次傳輸才算成功 */
    uint8_t type = 0, *reply = NULL;
    size_t reply_len = 0;
    rc = frame_recv(s, &type, &reply, &reply_len);
    if (rc != TL_OK) { explain("等不到接收端的回覆", rc); CLOSESOCK(s); return 1; }
    ok = (type == T_FILE_END && reply_len == 1 && reply[0] == 0);
    free(reply);
    if (!ok) fprintf(stderr, "錯誤: 接收端回報失敗（解碼或寫檔沒成功）\n");

    fprintf(stderr, "STATS role=send mode=%s sym=%s file_bytes=%" PRIu64 " wire_bytes=%" PRIu64
                    " ratio=%.4f encode_ms=%.1f send_ms=%.1f total_ms=%.1f\n",
            mode == MODE_HUFF ? "huff" : "raw", mode == MODE_HUFF ? tl_sym_name(st.sym) : "none",
            st.file_bytes, st.wire_bytes,
            tl_ratio(st.wire_bytes, st.file_bytes), st.encode_ms, st.send_ms, now_ms() - t0);
    printf("壓縮率 %.2f%%（上線 %" PRIu64 " bytes ÷ 原檔 %" PRIu64 " bytes，含 frame 標頭與 codebook）\n",
           100.0 * tl_ratio(st.wire_bytes, st.file_bytes), st.wire_bytes, st.file_bytes);

    CLOSESOCK(s);
    return ok ? 0 : 1;
}

/*========================= 命令列：textlink recv ===========================*/
int transfer_recv(const char *bind_ip, int port, const char *outdir) {
    char peer[64], saved[512] = "";
    file_rx_t rx;
    tl_stats_t st;
    int ended = 0, ok = 0, rc = TL_OK;

    memset(&rx, 0, sizeof(rx));
    memset(&st, 0, sizeof(st));
    socket_t s = net_listen_accept(bind_ip, port, peer, sizeof(peer));
    if (s == SOCK_INVALID) return 1;

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
        free(p);
    }
    if (rx.begun) printf("\n");

    if (ended) {
        rc = file_rx_finish(&rx, outdir, &st, saved, sizeof(saved));
        ok = (rc == TL_OK);
        uint8_t status = ok ? 0 : 1;                /* 告訴傳送端結果（盡力而為） */
        frame_send(s, T_FILE_END, &status, 1);
    }
    if (ok) {
        printf("已存檔：%s（%" PRIu64 " bytes）\n", saved, st.file_bytes);
        fprintf(stderr, "STATS role=recv mode=%s file_bytes=%" PRIu64 " wire_bytes=%" PRIu64
                        " ratio=%.4f decode_ms=%.1f total_ms=%.1f\n",
                st.mode == MODE_HUFF ? "huff" : "raw", st.file_bytes, st.wire_bytes,
                tl_ratio(st.wire_bytes, st.file_bytes), st.decode_ms, st.total_ms);
    } else {
        explain("接收失敗，沒有產生輸出檔", rc);
    }

    file_rx_reset(&rx);
    CLOSESOCK(s);
    return ok ? 0 : 1;
}
