/*============================================================================
 *  frame.c  —  封包外框（length prefix）
 *----------------------------------------------------------------------------
 *  規格（各組相同，見 ../README.md「封包外框」）：
 *
 *      +----------------------+-----------+----------------------+
 *      | length：4 bytes      | type：1   | payload：length-1    |
 *      | big-endian，無號整數  | byte      | bytes                |
 *      +----------------------+-----------+----------------------+
 *      length = type 與 payload 的總 bytes 數；合法範圍 1 到 TL_MAX_FRAME
 *
 *  例：type = 0x01、payload 是 3 bytes 的 "abc"
 *      → 標頭 00 00 00 04 01，後面接 61 62 63
 *
 *  ★ 你們要完成的是下面兩個 TODO 函式；frame_send／frame_recv 已經寫好。
 *===========================================================================*/
#include "textlink.h"

/*--------------------------------------------------------------------------
 * ★ TODO 1：打包標頭
 *   - length = payload_len + 1，用 big-endian 寫進 hdr[0..3]，type 寫進 hdr[4]。
 *   - length 超過 TL_MAX_FRAME 要回傳 TL_ERR_PROTO（不要送出對方一定會拒收的東西）。
 *   - 不可以用 memcpy(&hdr, &length, 4)：那樣寫出來的是「這台電腦的位元組順序」，
 *     Windows／Mac 互連、或之後遇到 big-endian 機器就會錯。請用位移運算一個 byte 一個 byte 放。
 *   - 完成後回傳 TL_OK。
 *-------------------------------------------------------------------------*/
int frame_pack_header(uint8_t hdr[TL_HDR_LEN], uint8_t type, size_t payload_len) {
    (void)hdr; (void)type; (void)payload_len;
    return TL_ERR_TODO;
}

/*--------------------------------------------------------------------------
 * ★ TODO 2：解析標頭
 *   - 從 hdr[0..3] 組回 length（big-endian），hdr[4] 是 type。
 *   - length 為 0 或超過 TL_MAX_FRAME：回傳 TL_ERR_PROTO。
 *     這一步是安全關鍵：length 是「對方說的」，不檢查就照著去 malloc／recv，
 *     對方送 FF FF FF FF 就能讓你的程式配置 4 GB 記憶體。
 *   - *payload_len = length - 1，完成後回傳 TL_OK。
 *-------------------------------------------------------------------------*/
int frame_parse_header(const uint8_t hdr[TL_HDR_LEN], uint8_t *type, size_t *payload_len) {
    (void)hdr; (void)type; (void)payload_len;
    return TL_ERR_TODO;
}

/*--------------------------------------------------------------------------
 * 以下是殼：已完成。
 *-------------------------------------------------------------------------*/
int frame_send(socket_t s, uint8_t type, const uint8_t *payload, size_t len) {
    uint8_t hdr[TL_HDR_LEN];
    int rc = frame_pack_header(hdr, type, len);
    if (rc != TL_OK) return rc;

    net_send_lock();                                 /* 標頭與 payload 要連在一起送，不能被別的 frame 插隊 */
    rc = send_all(s, hdr, TL_HDR_LEN);
    if (rc == TL_OK && len > 0) rc = send_all(s, payload, len);
    net_send_unlock();
    return rc;
}

int frame_recv(socket_t s, uint8_t *type, uint8_t **payload, size_t *len) {
    uint8_t hdr[TL_HDR_LEN];
    *payload = NULL;
    *len = 0;

    /* 先拿滿 5 bytes 標頭：就算對方 1 byte 1 byte 送（半包），recv_all 也會等到滿 */
    int rc = recv_all(s, hdr, TL_HDR_LEN);
    if (rc != TL_OK) return rc;

    size_t n = 0;
    rc = frame_parse_header(hdr, type, &n);
    if (rc != TL_OK) return rc;
    if (n >= TL_MAX_FRAME) return TL_ERR_PROTO;      /* 殼的第二道保險；正確的檢查仍要寫在 TODO 2 */

    uint8_t *buf = (uint8_t *)malloc(n + 1);
    if (buf == NULL) return TL_ERR_NOMEM;
    if (n > 0) {
        /* 再拿剛好 n bytes：多的留在 TCP 緩衝區給下一個 frame（黏包就是這樣被切開的） */
        rc = recv_all(s, buf, n);
        if (rc != TL_OK) { free(buf); return rc; }
    }
    buf[n] = '\0';
    *payload = buf;
    *len = n;
    return TL_OK;
}
