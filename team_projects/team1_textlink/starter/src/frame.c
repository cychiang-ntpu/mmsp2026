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
 *----------------------------------------------------------------------------
 *  【為什麼需要這個檔案】
 *  TCP 是 byte stream，沒有「一則訊息」的邊界：對方 send 五次，你 recv 一次可能全部拿到（黏包），
 *  也可能只拿到半則（半包）——第 2 週 3.3 的實驗。解法是每則訊息前面先講「我有多長」（length prefix），
 *  見第 3 週講義 3.3。本程式送上網路的每一樣東西（聊天文字、檔案的每一段）都先包成 frame，
 *  全部經過本檔的 frame_send／frame_recv；而這兩個函式又靠你們的兩個 TODO 來產生與讀懂那 5 bytes 標頭。
 *
 *      送：chat.c／transfer.c → frame_send → frame_pack_header（TODO 1）→ send_all → 網路
 *      收：網路 → recv_all 收 5 bytes → frame_parse_header（TODO 2）→ recv_all 收 payload → chat.c／transfer.c
 *
 *  所以 TODO 1、2 沒做之前，整支程式什麼都送不出去；做完之後 /raw 聊天與 --raw 傳檔就會通。
 *  兩個函式都很短（合計約 10 行），但一個 byte 放錯，對方就完全讀不懂。
 *
 *  【big-endian 是什麼】一個 4 bytes 的整數要寫成 4 個 byte，得約定哪一個先寫。
 *  big-endian：最高位的 byte 先寫，和我們寫數字「由左到右、先寫大的位數」一樣。
 *      十進位 65537 = 十六進位 0x00010001 → 依序寫出 00 01 00 01
 *  你的電腦（x86、ARM）在記憶體裡用的是相反的 little-endian，這就是不能直接 memcpy 的原因。
 *
 *  【這裡會用到的 C 運算：位移與遮罩】把一個 32-bit 的整數想成 4 個 byte 排成一列：
 *      v >> 8       整個往右推 8 個 bit，也就是推掉最右邊 1 個 byte；>> 16 推掉 2 個、>> 24 推掉 3 個
 *      x & 0xFF     只留下最右邊 8 個 bit，其餘清成 0（0xFF 就是二進位的 1111 1111，這種用法叫遮罩 mask）
 *      兩個合起來：(v >> 24) & 0xFF 是「v 由左數來第 1 個 byte」。例如 v = 0x12345678 時得到 0x12；
 *      把 24 換成別的位移量，就能拿到其他位置的 byte。
 *      x << 8       反方向：往左推 1 個 byte，右邊空出來的位置補 0
 *      a | b        把兩個數的 bit 疊在一起；和 << 搭配，可以把幾個 byte 拼回一個整數（講義 3.3 的細節 1）
 *  兩個要小心的地方：
 *    - uint8_t 參與運算時會先被升級成 int。往左推 24 個 bit 之前，先轉型成 uint32_t 再推，
 *      免得最高位的 1 被推進 int 的正負號位。
 *    - 把比較寬的整數存進 uint8_t 時，加上明確的轉型 (uint8_t)(…)，表示「我知道只留最低 8 bits」。
 *===========================================================================*/
#include "textlink.h"

/*--------------------------------------------------------------------------
 * ★ TODO 1：打包標頭
 *   - length = payload_len + 1，用 big-endian 寫進 hdr[0..3]，type 寫進 hdr[4]。
 *   - length 超過 TL_MAX_FRAME 要回傳 TL_ERR_PROTO（不要送出對方一定會拒收的東西）。
 *   - 不可以用 memcpy(&hdr, &length, 4)：那樣寫出來的是「這台電腦的位元組順序」，
 *     Windows／Mac 互連、或之後遇到 big-endian 機器就會錯。請用位移運算一個 byte 一個 byte 放。
 *   - 完成後回傳 TL_OK。
 *
 *   契約整理：
 *     輸入  type（1 byte）、payload_len（payload 的 bytes 數，0 也合法：空的 frame，length 就是 1）
 *     輸出  hdr[0] 到 hdr[4] 共 5 bytes，全部由你填；回傳 TL_ERR_PROTO 時 hdr 的內容沒有人會用
 *     合法的 payload_len 最大是 TL_MAX_FRAME - 1（這時 length 剛好等於上限）
 *   邊界：payload_len 的型別是 size_t，在 64-bit 電腦上可以大到 4 bytes 裝不下；先確認沒有超過上限，
 *         再把 length 存進 uint32_t，順序反過來的話，超大的值會被截掉高位、看起來像合法的小數字。
 *   對應的測試（tests/test_codec.c 的 test_frame 前三項）：
 *         payload 3 bytes → 00 00 00 04 01；payload 65536 bytes → 00 01 00 01 11；
 *         payload_len = TL_MAX_FRAME → 要回傳 TL_ERR_PROTO
 *   想法：先在紙上把 length 寫成 8 位的十六進位，兩位一組切成 4 個 byte，
 *         再想每一個 byte 要用檔頭講的哪個位移量拿到、該放進 hdr 的第幾格。
 *   下面兩行 (void)hdr; … 只是讓編譯器不要警告「參數沒用到」；開始寫之後請把它們連同 return TL_ERR_TODO 一起換掉。
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
 *
 *   契約整理：
 *     輸入  hdr[0] 到 hdr[4]：剛從網路收到的 5 bytes（const：只能讀）
 *     輸出  *type 與 *payload_len：呼叫端變數的位址，用 *type = …; 的寫法把結果寫進去
 *           （指標當輸出參數的說明見 include/textlink.h 檔頭的「共通寫法 1」）
 *     回傳 TL_ERR_PROTO 時，呼叫端不會使用那兩個輸出
 *   type 是什麼值這裡不檢查：認不認得這個 type，是收到 frame 之後上一層（chat.c、transfer.c）的事。
 *   對應的測試（test_frame 後三項）：
 *         00 00 01 00 02 → type = 0x02、payload 255 bytes；length = 0 → TL_ERR_PROTO；FF FF FF FF → TL_ERR_PROTO
 *   想法：這是 TODO 1 的反運算。寫完後自己檢查一次 round-trip：
 *         同一組 type 與 payload_len 先 pack 再 parse，要拿回一模一樣的值。
 *-------------------------------------------------------------------------*/
int frame_parse_header(const uint8_t hdr[TL_HDR_LEN], uint8_t *type, size_t *payload_len) {
    (void)hdr; (void)type; (void)payload_len;
    return TL_ERR_TODO;
}

/*--------------------------------------------------------------------------
 * 以下是殼：已完成。
 * 不用改，但請讀懂（口試會問）：frame_recv 怎麼用「先收滿 5 bytes、再收滿 n bytes」同時解決半包與黏包，
 * 以及為什麼 malloc 之前一定先檢查 n。
 *
 * frame_send：payload 是 const uint8_t *（只讀）。len 為 0 時不送 payload，這時 payload 可以是 NULL。
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

/* frame_recv：收下一個完整的 frame。
 *   *type、*payload、*len 都是輸出；uint8_t **payload 為什麼有兩顆星、誰要 free，見 include/textlink.h。
 *   一進來先把 *payload 設成 NULL、*len 設成 0：這樣不論從哪一個 return 離開，
 *   呼叫端拿到的不是「有效的記憶體」就是 NULL，不會是沒初始化的垃圾位址（對 NULL 呼叫 free 是安全的）。 */
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

    /* malloc(大小) 向系統要一塊記憶體，回傳它的位址；要不到時回傳 NULL，所以一定要檢查。
       它回傳的型別是 void *，前面的 (uint8_t *) 是轉型成我們要的指標型別。
       多要 1 byte 是為了在結尾補 '\0'：萬一上一層把 payload 當成 C 字串來讀，也不會讀到這塊記憶體外面。 */
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
