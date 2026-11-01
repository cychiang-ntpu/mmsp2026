/*============================================================================
 *  m7_ringbuffer.c  —  模組 7：沒有網路的 jitter buffer——用假的到達順序餵 16 格環狀陣列，跑 8 拍播放時鐘
 *----------------------------------------------------------------------------
 *  用法：m7_ringbuffer
 *  看到什麼：先印 8 個「到達」事件（放進第幾格／重複丟掉），再印 8 拍：next_seq、buffer 裡有誰、PLAY 或 REPEAT。
 *           到達順序 0,1,3,2,5,5,4,7：3 比 2 先到（亂序）、5 到兩次（重複）、6 從來沒到（遺失）。
 *           結果：拍 6 印 REPEAT（補一個），其他 7 拍都 PLAY，而且播出的順序是 0..7——亂序的自動排好了。
 *  試著改：把到達順序改成 0,1,2,3,4,5,6,7（沒事）、或 0,2,4,6（每隔一個掉）看 REPEAT 變幾個；
 *         把 RING_SLOTS 改成 4 看 seq 7 會不會把 seq 3 蓋掉（格子 7 % 4 = 3）。
 *
 *  這就是 step3_buffer.c 的心臟：
 *    環狀陣列：第 seq 包放在第 seq % 16 格，「下一個該播的在不在」只看一格（slot[next % 16].seq == next）
 *    播放時鐘：每一拍 next_seq +1，不管有沒有東西（有 → PLAY、沒有 → REPEAT 上一個）
 *===========================================================================*/
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define RING_SLOTS 16

typedef struct {                 /* 一格：有沒有東西、是哪個 seq（真的程式還有 320 個 sample，這裡省略） */
    int      valid;
    uint16_t seq;
} slot_t;

static slot_t ring[RING_SLOTS];

static void show_buffer(void) {  /* 印出 buffer 裡有哪些 seq（照格子順序掃一遍） */
    printf("buffer={");
    for (int i = 0; i < RING_SLOTS; i++) if (ring[i].valid) printf(" %u", (unsigned)ring[i].seq);
    printf(" }");
}

int main(void) {
    const uint16_t arrivals[] = { 0, 1, 3, 2, 5, 5, 4, 7 };          /* 假的到達順序：6 沒到、5 到兩次、3 和 2 對調 */
    const int n_arrivals = (int)(sizeof(arrivals) / sizeof(arrivals[0]));

    printf("== 到達（放進 jitter buffer）==\n");
    for (int i = 0; i < n_arrivals; i++) {
        uint16_t seq = arrivals[i];
        slot_t *s = &ring[seq % RING_SLOTS];                          /* 取餘數就是它的格子 */
        if (s->valid && s->seq == seq) { printf("收到 seq=%u：已經在第 %u 格 → 重複，丟掉\n", seq, seq % RING_SLOTS); continue; }
        s->valid = 1;
        s->seq = seq;
        printf("收到 seq=%u：放進第 %2u 格   ", seq, seq % RING_SLOTS);
        show_buffer();
        printf("\n");
    }

    printf("\n== 播放時鐘：8 拍，每拍要「next_seq」==\n");
    uint16_t next = 0;                                                /* 下一個該播的 seq（真的程式從開播前看過最小的開始） */
    uint16_t last_played = 0;
    int plays = 0, repeats = 0;
    for (int tick = 0; tick < 8; tick++) {
        slot_t *s = &ring[next % RING_SLOTS];
        printf("tick %d  next_seq=%u  ", tick, next);
        show_buffer();
        if (s->valid && s->seq == next) {                             /* 有：播它、清掉格子 */
            s->valid = 0;
            last_played = next;
            plays++;
            printf("  PLAY seq=%u\n", next);
        } else {                                                      /* 沒有：PLC repeat 上一個播出的 */
            repeats++;
            printf("  REPEAT（補 seq=%u 的內容；seq %u 沒到）\n", last_played, next);
        }
        next++;                                                       /* 不管有沒有，拍子都前進 */
    }
    printf("\n結果：PLAY %d 次、REPEAT %d 次；concealed = %d = 遺失的包數（seq 6）\n", plays, repeats, repeats);
    return 0;
}
