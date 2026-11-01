/*============================================================================
 *  jpeg.c  —  jpeg.h 的實作：把 stb_image（解碼）與 stb_image_write（編碼）的函式本體編進來
 *----------------------------------------------------------------------------
 *  stb 是「單一標頭檔」函式庫：標頭裡同時有宣告與實作，只有定義了 STB_IMAGE_IMPLEMENTATION 的那個 .c
 *  才會把實作編進去——整支程式只能有一個，就是這個檔案（和 audio.c 對 miniaudio 的做法一樣）。
 *  其他檔案只要 #include "wrapper/jpeg.h"，不要再 include stb_*.h。
 *
 *  STBI_ONLY_JPEG：stb_image 本來還能讀 PNG／BMP／GIF…，我們只要 JPEG，關掉其他格式讓程式小一點、警告少一點。
 *  stb_image_write 只有 JPEG 會被我們呼叫，其他格式的程式碼會被編譯器當成沒人用的 static 函式拿掉。
 *===========================================================================*/
#include "jpeg.h"

#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_FAILURE_STRINGS                 /* 失敗只回傳 NULL，不要錯誤字串（少一些未使用的程式碼） */
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

/* 可長大的 byte 緩衝區：stb 編碼時是一小段、一小段地「吐」資料出來，我們在 callback 裡接起來 */
typedef struct {
    uint8_t *buf;
    size_t   len, cap;
    int      fail;
} growbuf_t;

static void grow_write(void *ctx, void *data, int size) {
    growbuf_t *g = (growbuf_t *)ctx;
    if (g->fail || size <= 0) return;
    if (g->len + (size_t)size > g->cap) {
        size_t nc = g->cap ? g->cap * 2 : 16384;
        while (nc < g->len + (size_t)size) nc *= 2;
        uint8_t *p = (uint8_t *)realloc(g->buf, nc);
        if (!p) { g->fail = 1; return; }
        g->buf = p;
        g->cap = nc;
    }
    memcpy(g->buf + g->len, data, (size_t)size);
    g->len += (size_t)size;
}

int jpeg_encode(const uint8_t *rgb, int w, int h, int quality, uint8_t **out, size_t *out_len) {
    if (!rgb || !out || !out_len || w <= 0 || h <= 0) return -1;
    if (quality < 1) quality = 1;
    if (quality > 100) quality = 100;
    growbuf_t g = { NULL, 0, 0, 0 };
    /* 參數：寫入 callback、callback 的 ctx、寬、高、channel 數（3 = RGB）、資料、品質 */
    int ok = stbi_write_jpg_to_func(grow_write, &g, w, h, 3, rgb, quality);
    if (!ok || g.fail || g.len == 0) { free(g.buf); return -1; }
    *out = g.buf;
    *out_len = g.len;
    return 0;
}

int jpeg_decode(const uint8_t *data, size_t len, uint8_t **rgb, int *w, int *h) {
    if (!data || len == 0 || !rgb || !w || !h) return -1;
    int comp = 0;
    /* 最後一個參數 3 = 不管原圖幾個 channel，都轉成 RGB 三個 */
    uint8_t *p = stbi_load_from_memory(data, (int)len, w, h, &comp, 3);
    if (!p) return -1;
    *rgb = p;
    return 0;
}

void jpeg_free(void *p) {
    free(p);                /* stb 用的也是 malloc，所以 free 就好 */
}
