/*============================================================================
 *  make_sequence.c  —  產生測試用的 JPEG 序列（mock 模式的輸入；評測的序列也是這樣做的）
 *----------------------------------------------------------------------------
 *  用法：make_sequence <outdir> [--frames 60] [--quality 75] [--seed 1]
 *
 *  每張 320×240：會動的漸層背景 ＋ 一顆會彈的球 ＋ 右下角用大方塊字寫 frame 編號（5×7 點陣字型）。
 *  同一個 seed 畫出來的每一張都一樣（deterministic），所以老師和你們產生的序列會逐 byte 相同。
 *  寫成 outdir/0000.jpg、0001.jpg…（四位數），結束印總 bytes 與每張 JPEG 的最小／最大／平均大小
 *  （拿來估「一張要切幾片」：大小 ÷ 1200 無條件進位）。
 *
 *  結束碼：0 正常；2 參數錯或寫檔失敗。
 *===========================================================================*/
#include "wrapper/jpeg.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
  #include <direct.h>
  #define MKDIR(p) _mkdir(p)
#else
  #include <sys/stat.h>
  #define MKDIR(p) mkdir(p, 0755)
#endif

#define W 320
#define H 240

/* 0–9 的 5×7 點陣字型：每個數字 7 列，每列 5 個位元（最高位在左） */
static const uint8_t FONT[10][7] = {
    { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E },   /* 0 */
    { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E },   /* 1 */
    { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F },   /* 2 */
    { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E },   /* 3 */
    { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 },   /* 4 */
    { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E },   /* 5 */
    { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E },   /* 6 */
    { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 },   /* 7 */
    { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E },   /* 8 */
    { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C },   /* 9 */
};

static uint8_t clamp8(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }

static void put_px(uint8_t *img, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || y < 0 || x >= W || y >= H) return;
    uint8_t *p = img + ((size_t)y * W + x) * 3;
    p[0] = r; p[1] = g; p[2] = b;
}

/* 在 (x0, y0) 畫一個數字，每個點放大成 scale×scale 的方塊 */
static void draw_digit(uint8_t *img, int d, int x0, int y0, int scale) {
    for (int row = 0; row < 7; row++)
        for (int col = 0; col < 5; col++)
            if (FONT[d][row] & (0x10 >> col))
                for (int dy = 0; dy < scale; dy++)
                    for (int dx = 0; dx < scale; dx++)
                        put_px(img, x0 + col * scale + dx, y0 + row * scale + dy, 255, 255, 255);
}

/* 簡單的線性同餘亂數（不用 rand()：各平台的 rand 不一樣，seed 相同也會畫出不同的圖） */
static uint32_t rng_state;
static uint32_t rng_next(void) { rng_state = rng_state * 1664525u + 1013904223u; return rng_state; }

/* 每個像素一個固定的「雜訊」值（由 x、y、frame 雜湊出來，不是真的亂數）：
 * 真的攝影機畫面有很多細節，JPEG 會到 10–25 KB；純漸層壓起來只有 7 KB，切片數就不像真的了。 */
static int texture(int x, int y, int frame) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)frame * 2246822519u;
    h ^= h >> 13; h *= 1274126177u; h ^= h >> 16;
    return (int)(h % 29) - 14;                                    /* -14 … +14 */
}

static void render(uint8_t *img, int frame, int bx, int by, int hue_shift) {
    /* 背景：斜向的漸層（顏色隨 frame 慢慢轉）＋ 細雜訊 */
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int t = (x + y + frame * 3 + hue_shift) % 256;
            int n = texture(x, y, frame);
            put_px(img, x, y, clamp8(t / 2 + 40 + n), clamp8(y * 160 / H + 30 + n), clamp8(255 - t / 2 + n));
        }
    /* 球：半徑 28 的圓，邊緣顏色不同，看得出有沒有被「半張」截掉 */
    for (int y = by - 28; y <= by + 28; y++)
        for (int x = bx - 28; x <= bx + 28; x++) {
            int dx = x - bx, dy = y - by, d2 = dx * dx + dy * dy;
            if (d2 <= 28 * 28) {
                if (d2 > 22 * 22) put_px(img, x, y, 255, 230, 40);
                else              put_px(img, x, y, 230, 50, 50);
            }
        }
    /* frame 編號：四位數、每個點 6×6，寫在右下角（黑底白字） */
    int scale = 6, dig_w = 5 * scale + scale, x0 = W - 4 * dig_w - 10, y0 = H - 7 * scale - 10;
    for (int y = y0 - 4; y < H - 6; y++)
        for (int x = x0 - 4; x < W - 6; x++) put_px(img, x, y, 20, 20, 20);
    int v = frame % 10000;
    for (int i = 3; i >= 0; i--) { draw_digit(img, v % 10, x0 + i * dig_w, y0, scale); v /= 10; }
}

static int usage(void) {
    fprintf(stderr, "用法: make_sequence <outdir> [--frames 60] [--quality 75] [--seed 1]\n");
    return 2;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    const char *outdir = argv[1];
    int frames = 60, quality = 75, seed = 1;
    for (int i = 2; i < argc; i++) {
        if (i + 1 >= argc) return usage();
        if      (strcmp(argv[i], "--frames")  == 0) frames  = atoi(argv[++i]);
        else if (strcmp(argv[i], "--quality") == 0) quality = atoi(argv[++i]);
        else if (strcmp(argv[i], "--seed")    == 0) seed    = atoi(argv[++i]);
        else return usage();
    }
    if (frames < 1 || frames > 9999 || quality < 1 || quality > 100) return usage();
    MKDIR(outdir);                                    /* 已存在就失敗，沒關係 */

    rng_state = (uint32_t)seed * 2654435761u + 12345u;
    /* 球的起點與速度由 seed 決定 */
    int bx = 40 + (int)(rng_next() % 240), by = 40 + (int)(rng_next() % 160);
    int vx = 3 + (int)(rng_next() % 6), vy = 2 + (int)(rng_next() % 5);
    int hue_shift = (int)(rng_next() % 256);

    uint8_t *img = (uint8_t *)malloc((size_t)W * H * 3);
    if (!img) return 2;
    size_t total = 0, min_sz = (size_t)-1, max_sz = 0;
    for (int f = 0; f < frames; f++) {
        render(img, f, bx, by, hue_shift);
        bx += vx; by += vy;
        if (bx < 28 || bx > W - 28) { vx = -vx; bx += 2 * vx; }   /* 碰到邊就反彈 */
        if (by < 28 || by > H - 28) { vy = -vy; by += 2 * vy; }

        uint8_t *jpg = NULL;
        size_t len = 0;
        if (jpeg_encode(img, W, H, quality, &jpg, &len) != 0) { fprintf(stderr, "錯誤: JPEG 編碼失敗\n"); free(img); return 2; }
        char path[1024];
        snprintf(path, sizeof(path), "%s/%04d.jpg", outdir, f);
        FILE *fp = fopen(path, "wb");
        if (!fp || fwrite(jpg, 1, len, fp) != len) {
            fprintf(stderr, "錯誤: 寫不出 %s\n", path);
            if (fp) fclose(fp);
            jpeg_free(jpg); free(img);
            return 2;
        }
        fclose(fp);
        jpeg_free(jpg);
        total += len;
        if (len < min_sz) min_sz = len;
        if (len > max_sz) max_sz = len;
    }
    free(img);
    printf("寫了 %d 張 320x240 JPEG（quality %d，seed %d）到 %s/：共 %u bytes，每張 min %u / max %u / avg %u bytes"
           "（最大的一張要切 %u 片）\n",
           frames, quality, seed, outdir, (unsigned)total, (unsigned)min_sz, (unsigned)max_sz,
           (unsigned)(total / (size_t)frames), (unsigned)((max_sz + 1199) / 1200));
    return 0;
}
