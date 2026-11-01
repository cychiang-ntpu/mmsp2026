/*============================================================================
 *  camera_util.h  —  三個 camera 後端共用的小工具（header-only，static）
 *----------------------------------------------------------------------------
 *    cam_yuv_to_rgb          一個 YUV 像素 → R、G、B（BT.601，limited range：攝影機的 YUY2／NV12 都是這種）
 *    cam_yuyv_to_rgb         整張 YUYV（YUY2）→ RGB24
 *    cam_nv12_to_rgb         整張 NV12 → RGB24
 *    cam_fit_rgb             任意尺寸的 RGB → 目標尺寸：先把長寬比裁成目標的比例（取中間），再最近鄰縮放。
 *                            攝影機給的尺寸和要的一樣時就只是 memcpy。
 *===========================================================================*/
#ifndef ML_CAMERA_UTIL_H
#define ML_CAMERA_UTIL_H

#include <stdint.h>
#include <string.h>

static inline uint8_t cam_clamp8(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }

/* BT.601 limited range：Y 16–235、Cb/Cr 16–240。整數版（×256 避免浮點）。 */
static inline void cam_yuv_to_rgb(int y, int u, int v, uint8_t *rgb) {
    int c = 298 * (y - 16);
    int d = u - 128;
    int e = v - 128;
    rgb[0] = cam_clamp8((c + 409 * e + 128) >> 8);
    rgb[1] = cam_clamp8((c - 100 * d - 208 * e + 128) >> 8);
    rgb[2] = cam_clamp8((c + 516 * d + 128) >> 8);
}

/* YUYV：每 4 bytes 是兩個像素 Y0 U Y1 V（U、V 兩個像素共用）。stride = 一列的 bytes 數（通常 w*2） */
static inline void cam_yuyv_to_rgb(const uint8_t *src, int stride, int w, int h, uint8_t *rgb) {
    for (int y = 0; y < h; y++) {
        const uint8_t *row = src + (size_t)y * (size_t)stride;
        uint8_t *out = rgb + (size_t)y * (size_t)w * 3;
        for (int x = 0; x + 1 < w; x += 2) {
            const uint8_t *p = row + x * 2;
            cam_yuv_to_rgb(p[0], p[1], p[3], out + x * 3);
            cam_yuv_to_rgb(p[2], p[1], p[3], out + x * 3 + 3);
        }
    }
}

/* NV12：前面 h 列 Y（每列 stride bytes），後面 h/2 列 UV 交錯（每列 stride bytes，每 2×2 個像素共用一組） */
static inline void cam_nv12_to_rgb(const uint8_t *src, int stride, int w, int h, uint8_t *rgb) {
    const uint8_t *uv = src + (size_t)stride * (size_t)h;
    for (int y = 0; y < h; y++) {
        const uint8_t *yrow = src + (size_t)y * (size_t)stride;
        const uint8_t *uvrow = uv + (size_t)(y / 2) * (size_t)stride;
        uint8_t *out = rgb + (size_t)y * (size_t)w * 3;
        for (int x = 0; x < w; x++)
            cam_yuv_to_rgb(yrow[x], uvrow[(x / 2) * 2], uvrow[(x / 2) * 2 + 1], out + x * 3);
    }
}

/* src：sw×sh 的 RGB（每列 sstride bytes）→ dst：dw×dh 的 RGB（緊密排列） */
static inline void cam_fit_rgb(const uint8_t *src, int sw, int sh, int sstride, uint8_t *dst, int dw, int dh) {
    if (sw == dw && sh == dh) {
        for (int y = 0; y < dh; y++) memcpy(dst + (size_t)y * dw * 3, src + (size_t)y * sstride, (size_t)dw * 3);
        return;
    }
    /* 裁成目標的長寬比（取中間那塊），避免把人臉拉扁 */
    int cw = sw, ch = sh;
    if ((long)sw * dh > (long)dw * sh) cw = (int)((long)sh * dw / dh);   /* 來源太寬：左右裁 */
    else                               ch = (int)((long)sw * dh / dw);   /* 來源太高：上下裁 */
    int ox = (sw - cw) / 2, oy = (sh - ch) / 2;
    for (int y = 0; y < dh; y++) {
        int sy = oy + (int)((long)y * ch / dh);
        const uint8_t *row = src + (size_t)sy * sstride;
        uint8_t *out = dst + (size_t)y * dw * 3;
        for (int x = 0; x < dw; x++) {
            int sx = ox + (int)((long)x * cw / dw);
            out[x * 3] = row[sx * 3]; out[x * 3 + 1] = row[sx * 3 + 1]; out[x * 3 + 2] = row[sx * 3 + 2];
        }
    }
}

#endif /* ML_CAMERA_UTIL_H */
