/*============================================================================
 *  jpeg.h  —  JPEG 編碼／解碼（底下是 stb_image_write / stb_image；實作在 jpeg.c）
 *----------------------------------------------------------------------------
 *  jpeg_encode(rgb, w, h, quality, &out, &len)
 *      把 w×h 的 RGB（每個像素 3 bytes，R G B，由左到右、由上到下，沒有對齊補位）壓成 baseline JPEG。
 *      quality 1–100（規格預設 75：越高越大、越清楚）。成功回傳 0，並把新 malloc 的 JPEG bytes 放進 *out、
 *      長度放進 *len；用完要 jpeg_free(out)。失敗回傳 -1。
 *
 *  jpeg_decode(data, len, &rgb, &w, &h)
 *      把 JPEG bytes 解成 RGB（固定 3 個 channel，就算原圖是灰階也會變 3 bytes）。成功回傳 0，
 *      *rgb 是新 malloc 的 w×h×3 bytes，用完要 jpeg_free(rgb)。資料不是合法的 JPEG 回傳 -1
 *      （接收端收到「半張」JPEG 時解碼會失敗，或是解出一張下半部是灰色的圖——這正是「不能寫出半張」的原因）。
 *
 *  jpeg_free(p)   釋放上面兩個函式給你的記憶體（就是 free；包一層是為了對稱、而且 miniline 其他地方不必知道 stb）。
 *
 *  這兩個函式都是「同步」的、純 CPU：320×240 編碼約 1–2 ms、解碼約 1 ms，但請不要在收語音的執行緒裡做（規格）。
 *  stb 的授權是 public domain／MIT 二選一（見 stb_image.h、stb_image_write.h 檔尾）。
 *===========================================================================*/
#ifndef ML_JPEG_H
#define ML_JPEG_H

#include <stdint.h>
#include <stddef.h>

int  jpeg_encode(const uint8_t *rgb, int w, int h, int quality, uint8_t **out, size_t *out_len);
int  jpeg_decode(const uint8_t *data, size_t len, uint8_t **rgb, int *w, int *h);
void jpeg_free(void *p);

#endif /* ML_JPEG_H */
