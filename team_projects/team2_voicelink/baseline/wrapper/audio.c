/*============================================================================
 *  audio.c  —  audio.h 的實作：把 miniaudio 包成三個函式
 *----------------------------------------------------------------------------
 *  miniaudio（https://miniaud.io/，public domain／MIT-0）是一個「單一檔頭」的函式庫：整個函式庫都在
 *  miniaudio.h 裡。它的規則是：所有 .c 都可以 #include 它拿到宣告，但「只能有一個 .c」在 include 之前
 *  定義 MINIAUDIO_IMPLEMENTATION，那個 .c 才會把函式本體編進去——就是這個檔案。別的檔案不要再定義一次。
 *  MA_NO_DECODING 等巨集把我們用不到的 MP3／FLAC 解碼、編碼、波形產生器、資源管理、node graph、engine 都關掉：
 *  編譯快很多，而且 node graph 那一段在 gcc -O2 下會冒出一個誤判的警告（-Wstringop-overflow），關掉就沒有了。
 *
 *  同學不需要改這個檔案；想換裝置、改取樣率時再來看 audio_start_duplex 裡的 config。
 *===========================================================================*/
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#include "miniaudio.h"

#include "audio.h"
#include <stdio.h>
#include <string.h>

/* 整支程式只會有一組裝置，用 static 全域變數記著就好 */
static ma_context g_ctx;
static int        g_ctx_ready = 0;
static ma_device  g_duplex;              /* 一個裝置同時錄與放 */
static ma_device  g_capture;             /* 退路：兩個裝置 */
static ma_device  g_playback;
static int        g_mode = 0;            /* 0 = 沒開、1 = duplex、2 = 兩個裝置 */
static audio_cb_t g_cb = NULL;
static void      *g_user = NULL;

/* miniaudio 的回呼：pInput 是錄到的、pOutput 要我們填。格式已經在 config 裡指定成 s16 單聲道，
 * 所以 frameCount 個 frame 就是 frameCount 個 int16_t。duplex 時兩個指標都有；單向裝置時其中一個是 NULL。 */
static void ma_cb(ma_device *dev, void *pOutput, const void *pInput, ma_uint32 frameCount) {
    (void)dev;
    if (pOutput) memset(pOutput, 0, (size_t)frameCount * sizeof(int16_t));   /* 先填靜音，callback 忘了填也不會爆音 */
    if (g_cb) g_cb((const int16_t *)pInput, (int16_t *)pOutput, (size_t)frameCount, g_user);
}

static int ensure_context(void) {
    if (g_ctx_ready) return 0;
    if (ma_context_init(NULL, 0, NULL, &g_ctx) != MA_SUCCESS) return -1;
    g_ctx_ready = 1;
    return 0;
}

/* 把共通的設定填好：16 kHz、s16、單聲道、每 20 ms 叫一次 callback */
static ma_device_config make_config(ma_device_type type) {
    ma_device_config c = ma_device_config_init(type);
    c.sampleRate         = AUDIO_RATE;
    c.periodSizeInFrames = AUDIO_FRAME_SAMPLES;
    c.capture.format     = ma_format_s16;
    c.capture.channels   = 1;
    c.playback.format    = ma_format_s16;
    c.playback.channels  = 1;
    c.dataCallback       = ma_cb;
    return c;
}

int audio_start_duplex(audio_cb_t cb, void *user) {
    if (g_mode != 0) { fprintf(stderr, "錯誤: 音訊裝置已經開著\n"); return -1; }
    if (ensure_context() != 0) { fprintf(stderr, "錯誤: 這台電腦沒有可用的音訊系統\n"); return -1; }
    g_cb = cb;
    g_user = user;

    /* 第一選擇：一個 duplex 裝置。同一個 callback 同時拿到 mic 與 spk，兩邊的時鐘一致，最簡單。 */
    ma_device_config c = make_config(ma_device_type_duplex);
    if (ma_device_init(&g_ctx, &c, &g_duplex) == MA_SUCCESS) {
        if (ma_device_start(&g_duplex) == MA_SUCCESS) {
            g_mode = 1;
            printf("音訊：duplex 裝置已啟動（錄 %s／放 %s）\n", g_duplex.capture.name, g_duplex.playback.name);
            fflush(stdout);
            return 0;
        }
        ma_device_uninit(&g_duplex);
    }

    /* 退路：分開開兩個裝置。callback 會被叫兩次：錄音那次 spk == NULL，播放那次 mic == NULL。 */
    ma_device_config cc = make_config(ma_device_type_capture);
    ma_device_config pc = make_config(ma_device_type_playback);
    if (ma_device_init(&g_ctx, &cc, &g_capture) != MA_SUCCESS) {
        fprintf(stderr, "錯誤: 打不開麥克風（用 `devices` 看看有沒有裝置；Windows 要允許 app 使用麥克風）\n");
        g_cb = NULL;
        return -1;
    }
    if (ma_device_init(&g_ctx, &pc, &g_playback) != MA_SUCCESS) {
        fprintf(stderr, "錯誤: 打不開喇叭\n");
        ma_device_uninit(&g_capture);
        g_cb = NULL;
        return -1;
    }
    if (ma_device_start(&g_playback) != MA_SUCCESS || ma_device_start(&g_capture) != MA_SUCCESS) {
        fprintf(stderr, "錯誤: 裝置啟動失敗\n");
        ma_device_uninit(&g_capture);
        ma_device_uninit(&g_playback);
        g_cb = NULL;
        return -1;
    }
    g_mode = 2;
    printf("音訊：兩個裝置已啟動（錄 %s／放 %s）\n", g_capture.capture.name, g_playback.playback.name);
    fflush(stdout);
    return 0;
}

void audio_stop(void) {
    if (g_mode == 1) {
        ma_device_uninit(&g_duplex);               /* uninit 會先 stop；之後 callback 保證不會再被呼叫 */
    } else if (g_mode == 2) {
        ma_device_uninit(&g_capture);
        ma_device_uninit(&g_playback);
    }
    g_mode = 0;
    g_cb = NULL;
    if (g_ctx_ready) { ma_context_uninit(&g_ctx); g_ctx_ready = 0; }
}

int audio_list_devices(void) {
    /* 沒有音訊系統的機器（例如 CI 的虛擬機）：印一行說明、回傳 0。這不是錯誤。 */
    if (ensure_context() != 0) { printf("（這台電腦沒有可用的音訊系統）\n"); return 0; }

    ma_device_info *play = NULL, *cap = NULL;
    ma_uint32 n_play = 0, n_cap = 0;
    if (ma_context_get_devices(&g_ctx, &play, &n_play, &cap, &n_cap) != MA_SUCCESS) {
        printf("（列不出裝置）\n");
        return 0;
    }
    printf("喇叭（playback）：%u 個\n", (unsigned)n_play);
    for (ma_uint32 i = 0; i < n_play; i++)
        printf("  [%u] %s%s\n", (unsigned)i, play[i].name, play[i].isDefault ? "  （預設）" : "");
    printf("麥克風（capture）：%u 個\n", (unsigned)n_cap);
    for (ma_uint32 i = 0; i < n_cap; i++)
        printf("  [%u] %s%s\n", (unsigned)i, cap[i].name, cap[i].isDefault ? "  （預設）" : "");
    printf("後端：%s\n", ma_get_backend_name(g_ctx.backend));
    fflush(stdout);
    return (int)(n_play + n_cap);
}
