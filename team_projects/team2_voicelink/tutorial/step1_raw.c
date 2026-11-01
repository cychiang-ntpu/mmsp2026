/*============================================================================
 *  step1_raw.c  —  第 1 步：最天真的 UDP 語音串流（沒有 header、沒有 buffer）
 *----------------------------------------------------------------------------
 *  用法：
 *    step1_raw send <ip> <port> <in.wav>      讀 WAV，每 20 ms 送一包 640 bytes
 *    step1_raw recv <port> <out.wav>          收到什麼就照到達順序寫什麼；2 秒沒封包就結束
 *
 *  這一步只做兩件事：把 WAV 切成 20 ms 的 frame 用 UDP 送出去；收端把收到的 bytes 接起來寫成 WAV。
 *  網路正常（127.0.0.1）時輸出會和輸入逐 byte 相同；一經過 udp_impair.py 就會壞——那是第 2 步以後要解決的事。
 *
 *  只用 baseline/wrapper/ 的四個標頭（這一步用三個）：
 *    udp.h    udp_open／udp_set_peer／udp_send／udp_recv（有逾時）
 *    clock.h  clock_ms（單調時鐘，毫秒）、sleep_until_ms（睡到某個絕對時刻）
 *    wav.h    wav_read／wav_write／wav_free
 *  編譯時加 -I../baseline/wrapper，所以 #include "udp.h" 找得到。
 *===========================================================================*/
#include "udp.h"                 /* 一定要第一個 include：裡面有 _POSIX_C_SOURCE 與 winsock 的順序要求 */
#include "clock.h"
#include "wav.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FRAME_SAMPLES 320                        /* 20 ms @ 16 kHz = 320 個 sample */
#define FRAME_BYTES   (FRAME_SAMPLES * 2)        /* 每個 sample 2 bytes（16-bit）→ 640 bytes */
#define FRAME_MS      20                         /* 一個 frame 的時間 */
#define MAX_DGRAM     2048                       /* 收封包的緩衝區：比 640 大很多，收到奇怪的東西也裝得下 */

/* 印用法；回傳 2 讓 main 直接 return 它（結束碼 2 = 參數錯誤） */
static int usage(void) {
    fprintf(stderr,
        "用法:\n"
        "  step1_raw send <ip> <port> <in.wav>    讀 16 kHz/16-bit/單聲道 WAV，每 20 ms 送一包 640 bytes\n"
        "  step1_raw recv <port> <out.wav>        照到達順序寫成 WAV；2 秒沒封包就結束\n");
    return 2;
}

/* "5004" → 5004；不合法回傳 -1。
 * 不用 atoi：atoi("abc") 會回 0、atoi("12abc") 會回 12，都看不出錯。strtol 會把「停在哪個字」填進 end，
 * 所以可以檢查整串都是數字（*end == '\0'）。end 是「輸出參數」：我們把一個指標的位址交給 strtol，它把結果寫進去。 */
static int parse_port(const char *s) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v < 1 || v > 65535) return -1;
    return (int)v;
}

/* 把 n 個 int16_t 轉成 2n 個 bytes（little-endian：低位的 byte 在前，和 WAV 檔裡一樣）。
 * 不能直接 memcpy：CPU 可能是 big-endian（現在少見，但寫法要對）。
 * (uint16_t)v 先把有號數當成無號數看（bit 樣式不變），再用 >> 8 取高 byte、& 0xff 取低 byte。 */
static void pcm_to_bytes(const int16_t *pcm, size_t n, uint8_t *out) {
    for (size_t i = 0; i < n; i++) {
        uint16_t v = (uint16_t)pcm[i];
        out[2 * i]     = (uint8_t)(v & 0xff);
        out[2 * i + 1] = (uint8_t)(v >> 8);
    }
}

/* 反過來：2n 個 bytes → n 個 int16_t。| 把兩個 byte 拼回 16 bits，再轉成有號數。 */
static void bytes_to_pcm(const uint8_t *in, size_t n, int16_t *pcm) {
    for (size_t i = 0; i < n; i++) pcm[i] = (int16_t)(in[2 * i] | (in[2 * i + 1] << 8));
}

/* 讀 WAV 並檢查格式。成功回傳 0（w 填好）；失敗印錯誤、回傳 2（參數／檔案格式錯誤）。
 * wav_t *w 是「輸出參數」：呼叫端準備一個 wav_t 變數，把它的位址傳進來，這裡把結果填進去。 */
static int load_wav_16k_mono(const char *path, wav_t *w) {
    int rc = wav_read(path, w);
    if (rc == -1) { fprintf(stderr, "錯誤: 讀不到 %s\n", path); return 2; }
    if (rc == -2) { fprintf(stderr, "錯誤: %s 不是 16-bit PCM 的 WAV\n", path); return 2; }
    if (w->sample_rate != 16000 || w->channels != 1 || w->bits != 16) {
        fprintf(stderr, "錯誤: %s 是 %u Hz／%u 聲道／%u-bit；只接受 16000 Hz／1 聲道／16-bit\n",
                path, (unsigned)w->sample_rate, (unsigned)w->channels, (unsigned)w->bits);
        wav_free(w);
        return 2;
    }
    return 0;
}

/*--------------------------------------------------------------------------
 * send：WAV → 每 20 ms 一個 640 bytes 的 datagram
 *-------------------------------------------------------------------------*/
static int cmd_send(const char *ip, int port, const char *path) {
    wav_t w;
    if (load_wav_16k_mono(path, &w) != 0) return 2;

    udp_t u;
    /* bind port 0 = 讓作業系統隨便挑一個 port（純送出端不需要固定的 port）；set_peer 記下要寄給誰 */
    if (udp_open(&u, NULL, 0) != 0 || udp_set_peer(&u, ip, port) != 0) { wav_free(&w); return 1; }

    /* 無條件進位：最後不足 320 個 sample 也算一包（不足的部分補 0） */
    size_t n_frames = (w.n_samples + FRAME_SAMPLES - 1) / FRAME_SAMPLES;
    printf("send：%s（%u samples → %u 包）→ %s:%d，每 %d ms 一包\n",
           path, (unsigned)w.n_samples, (unsigned)n_frames, ip, port, FRAME_MS);
    fflush(stdout);

    uint8_t pkt[FRAME_BYTES];
    double t0 = clock_ms();                       /* 記下開始的時刻；之後每一包的「截止時刻」都從它算 */
    for (size_t k = 0; k < n_frames; k++) {
        /* 第 k 個 frame 從第 k×320 個 sample 開始；最後一包可能不足 320，先把整包清 0 再填 */
        memset(pkt, 0, sizeof(pkt));
        size_t start = k * FRAME_SAMPLES;
        size_t n = w.n_samples - start;
        if (n > FRAME_SAMPLES) n = FRAME_SAMPLES;
        pcm_to_bytes(w.pcm + start, n, pkt);      /* w.pcm + start：指到第 start 個 sample 的指標 */

        if (udp_send(&u, pkt, sizeof(pkt)) < 0) { udp_close(&u); wav_free(&w); return 1; }

        /* 第 k 包送完，睡到「t0 + 20×(k+1)」這個絕對時刻。
         * 不要寫 sleep_ms(20)：sendto 本身也花時間、sleep 會多睡一點，誤差一包一包累積，30 秒下來會慢好幾百 ms。
         * 用絕對時刻，就算某一包晚了 3 ms，下一包還是在正確的時間送出，誤差不會累積。 */
        sleep_until_ms(t0 + (double)FRAME_MS * (double)(k + 1));
    }
    printf("送完 %u 包，花了 %.0f ms\n", (unsigned)n_frames, clock_ms() - t0);

    udp_close(&u);
    wav_free(&w);
    return 0;
}

/*--------------------------------------------------------------------------
 * recv：收到什麼、照到達順序寫什麼
 *-------------------------------------------------------------------------*/
static int cmd_recv(int port, const char *path) {
    udp_t u;
    if (udp_open(&u, NULL, port) != 0) return 1;  /* NULL = 0.0.0.0：這台電腦所有網卡的這個 port 都收 */
    printf("recv：聽 0.0.0.0:%d → %s；第一包之後 2000 ms 沒有封包就結束（第一包最多等 30 秒）\n", port, path);
    fflush(stdout);

    /* 可長大的 sample 陣列：收到的 sample 全部接在後面。
     * out_len = 目前有幾個 sample、out_cap = 配置了幾個的空間；不夠就 realloc 成兩倍。 */
    int16_t *out = NULL;
    size_t out_len = 0, out_cap = 0;
    size_t n_pkts = 0;
    uint8_t buf[MAX_DGRAM];
    int rc_exit = 0;

    for (;;) {
        int wait_ms = (n_pkts == 0) ? 30000 : 2000;             /* 第一包可以等久一點（對方還沒開） */
        int k = udp_recv(&u, buf, sizeof(buf), wait_ms, NULL);  /* 回傳 bytes 數；0 = 逾時；-1 = 錯誤 */
        if (k < 0) { rc_exit = 1; break; }
        if (k == 0) {                                           /* 逾時：送端送完了（或根本沒開） */
            if (n_pkts == 0) fprintf(stderr, "30 秒內一個封包都沒收到；送端開了嗎？port 對嗎？\n");
            break;
        }
        /* 收到 k 個 bytes → k/2 個 sample，直接接在 out 後面 */
        size_t n = (size_t)k / 2;
        if (out_len + n > out_cap) {
            size_t nc = out_cap ? out_cap * 2 : 65536;
            while (nc < out_len + n) nc *= 2;
            int16_t *p = (int16_t *)realloc(out, nc * sizeof(int16_t));
            if (!p) { fprintf(stderr, "錯誤: 記憶體不足\n"); rc_exit = 1; break; }
            out = p;
            out_cap = nc;
        }
        bytes_to_pcm(buf, n, out + out_len);
        out_len += n;
        n_pkts++;
    }
    udp_close(&u);

    if (wav_write(path, out, out_len, 16000) != 0) {
        fprintf(stderr, "錯誤: 寫不出 %s\n", path);
        rc_exit = 2;
    }
    printf("收到 %u 包，%u samples，寫到 %s\n", (unsigned)n_pkts, (unsigned)out_len, path);
    free(out);
    return rc_exit;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (udp_global_init() != 0) return 1;         /* Windows 要先「開機」Winsock；其他平台沒事做 */
    int rc;

    if (strcmp(argv[1], "send") == 0 && argc == 5) {
        int port = parse_port(argv[3]);
        if (port < 0) { fprintf(stderr, "錯誤: port 不合法：%s\n", argv[3]); rc = 2; }
        else rc = cmd_send(argv[2], port, argv[4]);
    } else if (strcmp(argv[1], "recv") == 0 && argc == 4) {
        int port = parse_port(argv[2]);
        if (port < 0) { fprintf(stderr, "錯誤: port 不合法：%s\n", argv[2]); rc = 2; }
        else rc = cmd_recv(port, argv[3]);
    } else {
        rc = usage();
    }

    udp_global_cleanup();
    return rc;
}
