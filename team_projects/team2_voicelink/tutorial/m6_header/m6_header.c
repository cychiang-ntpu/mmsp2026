/*============================================================================
 *  m6_header.c  —  模組 6：把 magic/type/seq/ts/ssrc 打包成 12 bytes 的 header（big-endian），印出 hex，再解析回來驗證
 *----------------------------------------------------------------------------
 *  用法：m6_header [seq] [ts] [ssrc]      （預設 seq=4660 ts=1280 ssrc=0xDEADBEEF）
 *  看到什麼：12 個 hex：56 01 12 34 00 00 05 00 de ad be ef，以及「解析回來：... OK」。
 *           0x1234 變成 12 34（高位 byte 在前 = big-endian）；1280 = 0x500 變成 00 00 05 00。
 *  試著改：seq 給 65535 再 +1 看它繞回 0；或把 put_u16 換成 memcpy(p + 2, &seq, 2)，看 hex 變成 34 12（little-endian，錯的）。
 *
 *  規格的封包格式（offset / 長度 / 欄位）：
 *     0  1  magic = 0x56 ('V')
 *     1  1  type  = 0x01 AUDIO、0x02 BYE、0x04 AUDIO_MULAW
 *     2  2  seq   每包 +1（16-bit 會繞回）
 *     4  4  ts    這包第一個 sample 是整段的第幾個 sample
 *     8  4  ssrc  本次串流的隨機識別碼
 *  不能把一個 struct 直接 memcpy 進封包：struct 有對齊空隙，而且 CPU 是 little-endian、網路慣例是 big-endian。
 *  要一個 byte 一個 byte 用位移放：put_u16(p, v) 就是 p[0] = v >> 8; p[1] = v & 0xff。
 *===========================================================================*/
#include "udp.h"                 /* put_u16／put_u32／get_u16／get_u32 在這裡 */

#include <stdio.h>
#include <stdlib.h>

#define MAGIC   0x56
#define T_AUDIO 0x01
#define HDR_LEN 12

static void pack_header(uint8_t *p, uint8_t type, uint16_t seq, uint32_t ts, uint32_t ssrc) {
    p[0] = MAGIC;
    p[1] = type;
    put_u16(p + 2, seq);         /* p[2] = 高 byte, p[3] = 低 byte */
    put_u32(p + 4, ts);          /* p[4] 最高 … p[7] 最低 */
    put_u32(p + 8, ssrc);
}

int main(int argc, char **argv) {
    uint16_t seq  = (argc > 1) ? (uint16_t)strtoul(argv[1], NULL, 0) : 0x1234;    /* strtoul 的 base 0：可以給 0x... */
    uint32_t ts   = (argc > 2) ? (uint32_t)strtoul(argv[2], NULL, 0) : 1280;
    uint32_t ssrc = (argc > 3) ? (uint32_t)strtoul(argv[3], NULL, 0) : 0xDEADBEEFu;

    uint8_t h[HDR_LEN];
    pack_header(h, T_AUDIO, seq, ts, ssrc);

    printf("打包：magic=0x%02X type=0x%02X seq=%u (0x%04X) ts=%u (0x%08X) ssrc=0x%08X\n",
           MAGIC, T_AUDIO, (unsigned)seq, (unsigned)seq, (unsigned)ts, (unsigned)ts, (unsigned)ssrc);
    printf("12 bytes：");
    for (int i = 0; i < HDR_LEN; i++) printf("%02x%s", h[i], i == 1 || i == 3 || i == 7 ? " | " : " ");
    printf("\n          magic type |  seq  |     ts      |    ssrc\n");

    /* 解析回來：先看長度與 magic，再用 get_u16／get_u32 一個欄位一個欄位讀 */
    if (HDR_LEN < 12 || h[0] != MAGIC) { printf("magic 不對\n"); return 1; }
    uint8_t  type2 = h[1];
    uint16_t seq2  = get_u16(h + 2);
    uint32_t ts2   = get_u32(h + 4);
    uint32_t ssrc2 = get_u32(h + 8);
    printf("解析回來：type=0x%02X seq=%u ts=%u ssrc=0x%08X\n", type2, (unsigned)seq2, (unsigned)ts2, (unsigned)ssrc2);

    int ok = (type2 == T_AUDIO && seq2 == seq && ts2 == ts && ssrc2 == ssrc);
    printf("%s\n", ok ? "OK：打包 → 解析 一模一樣" : "FAIL：解析出來不一樣");
    printf("seq 的下一個：%u → %u（uint16_t 到 65535 之後自動繞回 0）\n", (unsigned)seq, (unsigned)(uint16_t)(seq + 1));
    return ok ? 0 : 1;
}
