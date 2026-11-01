/*============================================================================
 *  wav.h  —  讀寫 WAV 檔（16-bit PCM）；header-only，#include 進來就能用
 *----------------------------------------------------------------------------
 *  提供三個函式：
 *    wav_read (path, &w)                     把整個 WAV 讀進記憶體：取樣率、聲道數、位元數、PCM 資料
 *    wav_write(path, pcm, n_samples, rate)   把 16-bit 單聲道的 PCM 寫成標準 44 bytes 標頭的 WAV
 *    wav_free (&w)                           釋放 wav_read 配置的記憶體
 *
 *  WAV 檔長什麼樣（第 3 週 2.6 節）：
 *    "RIFF" <檔案大小-8> "WAVE"              12 bytes 的檔頭
 *    然後是一個接一個的 chunk：<4 字元的名字> <4 bytes 長度（little-endian）> <內容>
 *      "fmt "  格式：編碼方式（1 = PCM）、聲道數、取樣率、每個 sample 幾個 bit ...
 *      "data"  PCM 本體
 *      其他（"LIST"、"fact"、"INFO" ...）我們不認識的，照長度跳過就好
 *  重點：data 不一定在第 44 byte！很多工具會在 fmt 和 data 之間塞 LIST（作者、軟體名稱…）。
 *  所以要「照 chunk 走」：讀名字和長度 → 認識就處理、不認識就跳過 → 直到找到 data。
 *  chunk 長度是奇數時，後面會補 1 個 byte 讓下一個 chunk 從偶數位置開始（pad byte），跳過時要算進去。
 *
 *  所有函式都是 static inline：每個 #include 它的 .c 檔各有一份，連結時不會「重複定義」；沒用到的也不會被 gcc 警告。
 *===========================================================================*/
#ifndef VL_WAV_H
#define VL_WAV_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t sample_rate;     /* 每秒幾個 sample，例如 16000 */
    uint16_t channels;        /* 1 = 單聲道、2 = 立體聲 */
    uint16_t bits;            /* 每個 sample 幾個 bit；這裡只支援 16 */
    int16_t *pcm;             /* 取樣值（立體聲時左右交錯：L R L R ...）；由 wav_read 用 malloc 配置 */
    size_t   n_samples;       /* pcm 陣列裡總共幾個 int16_t（立體聲時 = frame 數 × 2） */
} wav_t;

/* 從 4 個 bytes 組出 little-endian 的整數（WAV 標頭裡所有數字都是 little-endian）。
 * 不能直接把 char 陣列轉型成 uint32_t * 來讀：一來 CPU 可能是 big-endian，二來位址未對齊時有些 CPU 會當掉。 */
static inline uint32_t wav_le32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline uint16_t wav_le16(const unsigned char *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
/* 反過來：整數 → 4 個（或 2 個）little-endian 的 bytes，寫標頭時用 */
static inline void wav_put32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)(v & 0xff); p[1] = (unsigned char)((v >> 8) & 0xff);
    p[2] = (unsigned char)((v >> 16) & 0xff); p[3] = (unsigned char)((v >> 24) & 0xff);
}
static inline void wav_put16(unsigned char *p, uint16_t v) {
    p[0] = (unsigned char)(v & 0xff); p[1] = (unsigned char)((v >> 8) & 0xff);
}

/* wav_read：讀 path 這個 WAV 檔，結果填進 *w（成功時 w->pcm 要由呼叫端用 wav_free 釋放）。
 *   回傳  0 成功
 *        -1 檔案開不起來、或讀到一半檔案就結束了（I/O 問題）
 *        -2 格式不支援：不是 RIFF/WAVE、不是 PCM（format 1）、不是 16-bit、找不到 fmt 或 data
 * 只接受「未壓縮的 PCM、16-bit」；取樣率與聲道數不限制，要不要接受由呼叫端決定（voicelink_raw 只收 16 kHz 單聲道）。 */
static inline int wav_read(const char *path, wav_t *w) {
    memset(w, 0, sizeof(*w));
    FILE *f = fopen(path, "rb");                        /* "rb"：binary 模式。Windows 上少了 b 會把 0x0A 之類的 byte 動手腳 */
    if (!f) return -1;

    unsigned char hdr[12];
    if (fread(hdr, 1, 12, f) != 12) { fclose(f); return -1; }
    if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) { fclose(f); return -2; }

    int have_fmt = 0;
    uint16_t format = 0;
    /* 一個 chunk 一個 chunk 往下走 */
    for (;;) {
        unsigned char ch[8];
        if (fread(ch, 1, 8, f) != 8) { fclose(f); return -2; }   /* 走到檔案結尾還沒看到 data：格式有問題 */
        uint32_t size = wav_le32(ch + 4);

        if (memcmp(ch, "fmt ", 4) == 0) {
            if (size < 16) { fclose(f); return -2; }
            unsigned char fmt[16];
            if (fread(fmt, 1, 16, f) != 16) { fclose(f); return -1; }
            format         = wav_le16(fmt + 0);           /* 1 = PCM；3 = float、0xFFFE = extensible ... 都不收 */
            w->channels    = wav_le16(fmt + 2);
            w->sample_rate = wav_le32(fmt + 4);
            w->bits        = wav_le16(fmt + 14);
            have_fmt = 1;
            /* fmt chunk 有時比 16 bytes 長（多一個 cbSize 欄位等），剩下的跳過；奇數長度還要跳 pad byte */
            if (fseek(f, (long)(size - 16) + (long)(size & 1), SEEK_CUR) != 0) { fclose(f); return -1; }
        } else if (memcmp(ch, "data", 4) == 0) {
            if (!have_fmt) { fclose(f); return -2; }     /* 規格上 fmt 一定在 data 前面 */
            if (format != 1 || w->bits != 16 || w->channels == 0) { fclose(f); return -2; }
            w->n_samples = size / 2;                      /* 16-bit → 每個 sample 2 bytes */
            w->pcm = (int16_t *)malloc(w->n_samples ? w->n_samples * sizeof(int16_t) : 1);
            if (!w->pcm) { fclose(f); return -1; }
            /* 先把原始 bytes 讀進來，再逐一組成 int16_t（這樣在 big-endian 的 CPU 上也正確） */
            unsigned char *raw = (unsigned char *)w->pcm;
            size_t got = fread(raw, 1, w->n_samples * 2, f);
            if (got != w->n_samples * 2) { free(w->pcm); w->pcm = NULL; fclose(f); return -1; }
            for (size_t i = 0; i < w->n_samples; i++) {
                uint16_t u = wav_le16(raw + 2 * i);       /* 先當無號數組起來，再轉成有號（兩者的 bit 樣式相同） */
                w->pcm[i] = (int16_t)u;
            }
            fclose(f);
            return 0;
        } else {
            /* 不認識的 chunk（LIST、fact ...）：照長度跳過，奇數長度多跳 1 byte */
            if (fseek(f, (long)size + (long)(size & 1), SEEK_CUR) != 0) { fclose(f); return -1; }
        }
    }
}

/* wav_write：把 n_samples 個 16-bit 單聲道的 sample 以取樣率 sample_rate 寫成 WAV（標準 44 bytes 標頭）。
 *   回傳 0 成功、-1 檔案寫不進去。 */
static inline int wav_write(const char *path, const int16_t *pcm, size_t n_samples, uint32_t sample_rate) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    uint32_t data_bytes = (uint32_t)(n_samples * 2);
    unsigned char h[44];
    memcpy(h + 0, "RIFF", 4);
    wav_put32(h + 4, 36 + data_bytes);                  /* 檔案總長 - 8（"RIFF" 與這個數字本身不算） */
    memcpy(h + 8, "WAVE", 4);
    memcpy(h + 12, "fmt ", 4);
    wav_put32(h + 16, 16);                               /* fmt chunk 的長度 */
    wav_put16(h + 20, 1);                                /* 1 = PCM */
    wav_put16(h + 22, 1);                                /* 單聲道 */
    wav_put32(h + 24, sample_rate);
    wav_put32(h + 28, sample_rate * 2);                  /* byte rate = 每秒幾個 bytes = rate × 聲道 × 2 */
    wav_put16(h + 32, 2);                                /* block align = 一個 frame 幾個 bytes */
    wav_put16(h + 34, 16);                               /* bits per sample */
    memcpy(h + 36, "data", 4);
    wav_put32(h + 40, data_bytes);
    if (fwrite(h, 1, 44, f) != 44) { fclose(f); return -1; }

    /* 一次一小段（4096 個 sample）轉成 little-endian 再寫，不必為整個檔案再配一塊記憶體 */
    unsigned char buf[4096 * 2];
    size_t done = 0;
    while (done < n_samples) {
        size_t n = n_samples - done;
        if (n > 4096) n = 4096;
        for (size_t i = 0; i < n; i++) wav_put16(buf + 2 * i, (uint16_t)pcm[done + i]);
        if (fwrite(buf, 1, n * 2, f) != n * 2) { fclose(f); return -1; }
        done += n;
    }
    if (fclose(f) != 0) return -1;
    return 0;
}

/* 釋放 wav_read 配置的 PCM 記憶體；呼叫後 w 會被清成 0，重複呼叫也安全 */
static inline void wav_free(wav_t *w) {
    free(w->pcm);
    memset(w, 0, sizeof(*w));
}

#endif /* VL_WAV_H */
