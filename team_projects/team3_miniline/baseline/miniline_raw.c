/*============================================================================
 *  miniline_raw.c  —  最天真的 UDP 視訊串流（Team 3 的起點；故意什麼都沒做）
 *----------------------------------------------------------------------------
 *  用法：
 *    miniline_raw video-send <ip> <port> <jpegdir|camera> [--fps 5] [--quality 75] [--camera 0]
 *        mock：讀 jpegdir/0000.jpg、0001.jpg…；camera：從攝影機抓 320×240 → JPEG。每張「一個」UDP datagram 送出
 *    miniline_raw video-recv <port> <outdir> [--idle 3000] [--display none|window]
 *        收到一個 datagram 就寫成 outdir/NNNN.jpg（到達順序）；--display window 同時解碼顯示；idle 這麼久沒封包就結束
 *    miniline_raw camera-test [--display window] [--seconds 5] [--out file.jpg] [--camera 0]
 *        抓攝影機 seconds 秒（能開視窗就顯示），最後一張寫成 file.jpg
 *    miniline_raw devices
 *        列出攝影機、麥克風、喇叭
 *
 *  【這支程式故意很笨】一張 JPEG（10–25 KB）直接當「一個」UDP datagram 送。UDP 允許一包最大約 65 507 bytes，
 *  但乙太網路一次只能載 1 500 bytes（MTU），所以 IP 層會把它切成十幾個「IP 分片」送出去、對方的 IP 層再拼回來。
 *  問題是：**只要有任何一個分片不見，整個 datagram 就不見**——應用程式連「收到一半」都看不到。
 *    127.0.0.1    不經過網卡，大多到得了，看起來能用
 *    真的網路     Wi-Fi 掉 2% 的分片 → 一張 15 片的 JPEG 有 1−0.98^15 ≈ 26% 的機率整張消失
 *    有些路由器   乾脆不轉送 IP 分片（防火牆政策）→ 一張都到不了
 *  而且沒有 header：接收端不知道這是第幾張、不知道掉了幾張、不知道對方掛斷了沒。
 *  這些就是你們要解決的問題。README（team3_miniline/README.md）規定了封包格式與 STATS；要加的東西各放在哪裡：
 *    16 bytes header（magic／type／seq／frame_id／piece_idx／piece_cnt／ssrc）
 *                        → send_one_frame 裡「一張一包」那幾行改成「切成 ⌈len/1200⌉ 片，每片前面放 header」
 *                          （用 udp.h 的 put_u16／put_u32）；recv 端在 cmd_video_recv「收到一包」之後先解析、檢查 magic 與長度
 *    分片／重組          → cmd_video_recv：收到的片依 frame_id 放進「重組表」（frame_id → 收到哪些片、第一片的時間），
 *                          收齊才算一張完整的 frame；現在是「收到什麼就寫什麼」
 *    逾時丟棄            → 重組表裡超過 --timeout 還沒收齊的、或 frame_id 落後太多的 → 丟掉並計數（frames_dropped_timeout）
 *    bounded queue       → 完整的 frame 先放進上限 n 的 queue；滿了丟最舊（frames_dropped_queue）
 *    顯示時鐘            → cmd_video_recv 要改成「每 200 ms 從 queue 拿最舊的一張寫檔／顯示」，不等網路（display_idle）
 *    HELLO／BYE／ssrc    → cmd_video_send 開頭送 3 次 HELLO、結尾送 3 次 BYE；recv 收到 HELLO 或新 ssrc 就清空重組表與 queue，
 *                          收到 BYE 就把 queue 裡的顯示完再結束
 *    壞封包              → 解析 header 的地方：magic 錯、長度 < 16、piece_idx ≥ piece_cnt、片段 > 1200 → 丟掉並計數（pieces_bad）
 *    STATS               → 每個 cmd_* 的結尾印到 stderr（格式見 README）
 *    live（call）        → 文字（chat.c 的 TCP）、語音（Team 2 的 UDP）、視訊各自的 socket 與執行緒；視訊的解碼不能卡住語音
 *  wrapper/ 裡的標頭（jpeg.h、camera.h、display.h、udp.h、clock.h、audio.h、wav.h）都是完成的，可以直接用、也可以改。
 *
 *  結束碼：0 正常；1 網路或裝置錯誤；2 參數或檔案錯誤。
 *===========================================================================*/
#include "wrapper/udp.h"                 /* 一定要第一個：裡面有 _POSIX_C_SOURCE 與 winsock 的順序要求 */
#include "wrapper/clock.h"
#include "wrapper/jpeg.h"
#include "wrapper/camera.h"
#include "wrapper/display.h"
#include "wrapper/audio.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
  #include <direct.h>
  #define MKDIR(p) _mkdir(p)
#else
  #include <sys/stat.h>
  #define MKDIR(p) mkdir(p, 0755)
#endif

#define VID_W      320
#define VID_H      240
#define MAX_DGRAM  65536                 /* 收封包的緩衝區：UDP 一包最大 65 507 bytes */
#define MAX_FRAMES 10000

static int usage(void) {
    fprintf(stderr,
        "用法:\n"
        "  miniline_raw video-send <ip> <port> <jpegdir|camera> [--fps 5] [--quality 75] [--camera 0]\n"
        "        每張 JPEG 一個 UDP datagram（沒有 header、沒有切片）；camera = 用第 --camera 台攝影機（devices 的編號）\n"
        "  miniline_raw video-recv <port> <outdir> [--idle 3000] [--display none|window]\n"
        "        收到什麼就照到達順序寫成 outdir/NNNN.jpg；idle ms 沒封包就結束\n"
        "  miniline_raw camera-test [--display window] [--seconds 5] [--out file.jpg] [--camera 0]\n"
        "        抓第 --camera 台攝影機幾秒鐘（能開視窗就顯示），最後一張寫成 file.jpg（預設 camera.jpg）\n"
        "  miniline_raw devices\n"
        "        列出攝影機、麥克風、喇叭\n");
    return 2;
}

/* "5004" → 5004；不合法回傳 -1（和 Team 1 的 net_parse_port 一樣，不用 atoi） */
static int parse_port(const char *s) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v < 1 || v > 65535) return -1;
    return (int)v;
}

/* 讀整個檔案到新 malloc 的緩衝區；回傳 0 成功、-1 開不了 */
static int read_file(const char *path, uint8_t **out, size_t *len) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (n < 0) { fclose(fp); return -1; }
    uint8_t *buf = (uint8_t *)malloc((size_t)n ? (size_t)n : 1);
    if (!buf) { fclose(fp); return -1; }
    if (fread(buf, 1, (size_t)n, fp) != (size_t)n) { fclose(fp); free(buf); return -1; }
    fclose(fp);
    *out = buf; *len = (size_t)n;
    return 0;
}

static int write_file(const char *path, const uint8_t *data, size_t len) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    size_t k = fwrite(data, 1, len, fp);
    fclose(fp);
    return k == len ? 0 : -1;
}

/*--------------------------------------------------------------------------
 * video-send：每張 JPEG → 一個 datagram（這就是問題所在）
 *-------------------------------------------------------------------------*/
static int send_one_frame(udp_t *u, const uint8_t *jpg, size_t len) {
    /* ── 一張一包 ──  要切片就在這裡：for (idx = 0; idx * 1200 < len; idx++) { 組 16 bytes header + 片段; udp_send } */
    if (udp_send(u, jpg, len) < 0) {
        /* sendto 失敗（例如超過 65 507 bytes 的 EMSGSIZE／WSAEMSGSIZE）：印了錯誤就繼續，不要整支程式結束 */
        fprintf(stderr, "  這一張 %u bytes 送不出去，跳過\n", (unsigned)len);
        return -1;
    }
    return 0;
}

static int cmd_video_send(const char *ip, int port, const char *src, int fps, int quality, int cam_index) {
    int use_camera = (strcmp(src, "camera") == 0);
    udp_t u;
    if (udp_open(&u, NULL, 0) != 0 || udp_set_peer(&u, ip, port) != 0) return 1;

    uint8_t *rgb = NULL;
    if (use_camera) {
        if (camera_open(cam_index, VID_W, VID_H) != 0) { udp_close(&u); return 1; }
        rgb = (uint8_t *)malloc((size_t)VID_W * VID_H * 3);
        if (!rgb) { camera_close(); udp_close(&u); return 1; }
        printf("攝影機 → JPEG（quality %d）→ %s:%d，每張一個 datagram，%d fps；按 Ctrl-C 結束\n", quality, ip, port, fps);
    } else {
        char path[1024];
        snprintf(path, sizeof(path), "%s/0000.jpg", src);
        FILE *fp = fopen(path, "rb");
        if (!fp) { fprintf(stderr, "錯誤: 找不到 %s（jpegdir 要有 0000.jpg）\n", path); udp_close(&u); return 2; }
        fclose(fp);
        printf("送 %s/NNNN.jpg → %s:%d，每張一個 datagram，%d fps\n", src, ip, port, fps);
    }
    fflush(stdout);

    double period = 1000.0 / fps;
    double t0 = clock_ms();
    unsigned sent = 0, failed = 0;
    size_t bytes = 0;
    int rc_exit = 0;
    for (int k = 0; k < MAX_FRAMES; k++) {
        uint8_t *jpg = NULL;
        size_t len = 0;
        if (use_camera) {
            int r = camera_read(rgb, 1000);
            if (r < 0) { fprintf(stderr, "錯誤: 攝影機讀取失敗\n"); rc_exit = 1; break; }
            if (r == 0) { fprintf(stderr, "攝影機 1 秒沒有畫面\n"); continue; }
            if (jpeg_encode(rgb, VID_W, VID_H, quality, &jpg, &len) != 0) {
                fprintf(stderr, "錯誤: JPEG 編碼失敗\n");
                rc_exit = 1;
                break;
            }
        } else {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%04d.jpg", src, k);
            if (read_file(path, &jpg, &len) != 0) break;            /* 沒有下一張了：送完 */
        }
        if (send_one_frame(&u, jpg, len) == 0) { sent++; bytes += len; } else failed++;
        /* ── 這裡該送 BYE 嗎？沒有 header 就沒有 BYE；接收端只能靠 --idle 逾時猜你送完了 ── */
        jpeg_free(jpg);
        sleep_until_ms(t0 + period * (double)(k + 1));             /* 第 k 張睡到 t0 + period·(k+1)：誤差不累積 */
    }
    double elapsed = clock_ms() - t0;
    printf("sent %u frames (%u bytes) in %.0f ms, %u failed\n", sent, (unsigned)bytes, elapsed, failed);
    /* ── STATS 應該印在這裡：STATS role=video-send frames=… pieces=… bytes=… fps_target=… fps_actual=… duration_ms=… ── */

    if (use_camera) { camera_close(); free(rgb); }
    udp_close(&u);
    return rc_exit;
}

/*--------------------------------------------------------------------------
 * video-recv：收到一包就當一張寫出去（沒有 header、沒有重組、沒有 queue）
 *-------------------------------------------------------------------------*/
static int cmd_video_recv(int port, const char *outdir, int idle_ms, int want_window) {
    udp_t u;
    if (udp_open(&u, NULL, port) != 0) return 1;
    MKDIR(outdir);
    int have_window = 0;
    if (want_window) {
        have_window = (display_open(VID_W, VID_H, "miniline_raw video-recv") == 0);
        if (!have_window) fprintf(stderr, "開不了視窗，改成只寫檔\n");
    }
    printf("聽 0.0.0.0:%d，寫到 %s/NNNN.jpg；第一包之後 %d ms 沒有封包就結束（第一包最多等 30 秒）\n", port, outdir, idle_ms);
    fflush(stdout);

    uint8_t *buf = (uint8_t *)malloc(MAX_DGRAM);
    if (!buf) { udp_close(&u); return 1; }
    unsigned n_pkts = 0, n_bad = 0;
    size_t bytes = 0;
    int rc_exit = 0;
    double last_pkt = clock_ms();                                   /* 最後一包（或開始）的時間：算 idle 逾時用 */
    for (;;) {
        /* 第一包最多等 30 秒，之後 idle_ms 沒封包就結束。有視窗時一次只等 100 ms 就回來處理視窗事件
         * （不然視窗會「沒回應」）——這就是「顯示端要有自己的時鐘、不等網路」的雛形。 */
        int limit = (n_pkts == 0) ? 30000 : idle_ms;
        int wait = have_window ? 100 : limit;
        int k = udp_recv(&u, buf, MAX_DGRAM, wait, NULL);
        if (k < 0) { rc_exit = 1; break; }
        if (have_window && !display_poll()) { printf("視窗被關了，結束\n"); break; }
        if (k == 0) {
            if (clock_ms() - last_pkt < (double)limit) continue;    /* 還沒到 idle：繼續等 */
            if (n_pkts == 0) fprintf(stderr, "30 秒內一個封包都沒收到；送端開了嗎？port 對嗎？\n");
            break;
        }
        last_pkt = clock_ms();
        /* ── 收到一包 ──  這裡應該先解析 16 bytes header、檢查 magic／長度、放進重組表。
         * 現在是：整包就是一張 JPEG。IP 分片有一片不見的話，這一包根本不會出現在這裡（UDP 的規則）。 */
        n_pkts++;
        bytes += (size_t)k;
        char path[1024];
        snprintf(path, sizeof(path), "%s/%04u.jpg", outdir, n_pkts - 1);
        if (write_file(path, buf, (size_t)k) != 0) { fprintf(stderr, "錯誤: 寫不出 %s\n", path); rc_exit = 2; break; }
        if (have_window) {
            uint8_t *rgb = NULL;
            int w = 0, h = 0;
            if (jpeg_decode(buf, (size_t)k, &rgb, &w, &h) == 0) {
                if (w == VID_W && h == VID_H) display_show(rgb);
                jpeg_free(rgb);
            } else {
                n_bad++;                                            /* 不是合法 JPEG（也可能是壞封包） */
            }
        }
    }
    free(buf);
    if (have_window) display_close();
    udp_close(&u);
    printf("received %u datagrams, %u bytes, written to %s/0000.jpg..%04u.jpg%s\n",
           n_pkts, (unsigned)bytes, outdir, n_pkts ? n_pkts - 1 : 0, n_pkts ? "" : " (nothing)");
    if (n_bad) printf("%u datagrams were not decodable JPEG\n", n_bad);
    /* ── STATS 應該印在這裡（stderr）：STATS role=video-recv queue=… frames_shown=… pieces_lost=… ── */
    return rc_exit;
}

/*--------------------------------------------------------------------------
 * camera-test：攝影機 → 視窗（開得了的話），最後一張存成 JPEG
 *-------------------------------------------------------------------------*/
static int cmd_camera_test(int want_window, int seconds, const char *out_path, int cam_index) {
    if (camera_open(cam_index, VID_W, VID_H) != 0) return 1;
    int have_window = 0;
    if (want_window) {
        have_window = (display_open(VID_W, VID_H, "miniline_raw camera-test") == 0);
        if (!have_window) fprintf(stderr, "開不了視窗，只存檔\n");
    }
    uint8_t *rgb = (uint8_t *)malloc((size_t)VID_W * VID_H * 3);
    if (!rgb) { camera_close(); return 1; }
    printf("抓 %d 秒%s，最後一張寫到 %s\n", seconds, have_window ? "（有視窗）" : "", out_path);
    fflush(stdout);

    double t_end = clock_ms() + seconds * 1000.0;
    unsigned frames = 0, timeouts = 0;
    int rc_exit = 0;
    while (clock_ms() < t_end) {
        int r = camera_read(rgb, 200);
        if (r < 0) { fprintf(stderr, "錯誤: 攝影機讀取失敗\n"); rc_exit = 1; break; }
        if (r == 0) { timeouts++; }
        else { frames++; if (have_window) display_show(rgb); }
        if (have_window && !display_poll()) { printf("視窗被關了\n"); break; }
    }
    printf("%u frames in %d s（%.1f fps），%u 次 200 ms 內沒有新畫面\n", frames, seconds, frames / (double)seconds, timeouts);
    if (frames > 0) {
        uint8_t *jpg = NULL;
        size_t len = 0;
        if (jpeg_encode(rgb, VID_W, VID_H, 75, &jpg, &len) == 0) {
            if (write_file(out_path, jpg, len) == 0) printf("寫了 %s（%u bytes）\n", out_path, (unsigned)len);
            else { fprintf(stderr, "錯誤: 寫不出 %s\n", out_path); rc_exit = 2; }
            jpeg_free(jpg);
        }
    } else if (rc_exit == 0) {
        fprintf(stderr, "錯誤: %d 秒內一張都沒抓到\n", seconds);
        rc_exit = 1;
    }
    if (have_window) display_close();
    camera_close();
    free(rgb);
    return rc_exit;
}

/*--------------------------------------------------------------------------
 * devices：攝影機 ＋ 麥克風／喇叭
 *-------------------------------------------------------------------------*/
static int cmd_devices(void) {
    int n = camera_count();
    printf("攝影機：%d 台\n", n);
    for (int i = 0; i < n; i++) {
        char name[256];
        if (camera_name(i, name, sizeof(name)) == 0) printf("  [%d] %s\n", i, name);
    }
    int m = audio_list_devices();
    printf("共 %d 個攝影機、%d 個音訊裝置\n", n, m);
    return 0;
}

/* 讀「--key value」形式的正整數選項；回傳 0 成功、-1 格式錯 */
static int opt_int(int argc, char **argv, int *i, int *out) {
    if (*i + 1 >= argc) return -1;
    char *end = NULL;
    long v = strtol(argv[*i + 1], &end, 10);
    if (end == argv[*i + 1] || *end != '\0' || v <= 0) return -1;
    *out = (int)v;
    *i += 1;
    return 0;
}

/* 和 opt_int 一樣，但允許 0（攝影機編號從 0 開始） */
static int opt_index(int argc, char **argv, int *i, int *out) {
    if (*i + 1 >= argc) return -1;
    char *end = NULL;
    long v = strtol(argv[*i + 1], &end, 10);
    if (end == argv[*i + 1] || *end != '\0' || v < 0 || v > 99) return -1;
    *out = (int)v;
    *i += 1;
    return 0;
}

static int opt_display(int argc, char **argv, int *i, int *want_window) {
    if (*i + 1 >= argc) return -1;
    const char *v = argv[*i + 1];
    if (strcmp(v, "window") == 0) *want_window = 1;
    else if (strcmp(v, "none") == 0) *want_window = 0;
    else return -1;
    *i += 1;
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (udp_global_init() != 0) return 1;
    int rc;

    if (strcmp(argv[1], "devices") == 0) {
        rc = cmd_devices();
    } else if (strcmp(argv[1], "video-send") == 0 && argc >= 5) {
        int port = parse_port(argv[3]);
        int fps = 5, quality = 75, cam = 0;
        rc = 0;
        for (int i = 5; i < argc && rc == 0; i++) {
            if      (strcmp(argv[i], "--fps") == 0)     { if (opt_int(argc, argv, &i, &fps) != 0) rc = usage(); }
            else if (strcmp(argv[i], "--quality") == 0) { if (opt_int(argc, argv, &i, &quality) != 0) rc = usage(); }
            else if (strcmp(argv[i], "--camera") == 0)  { if (opt_index(argc, argv, &i, &cam) != 0) rc = usage(); }
            else rc = usage();
        }
        if (rc == 0 && (fps > 60 || quality > 100)) rc = usage();
        if (rc == 0 && port < 0) { fprintf(stderr, "錯誤: port 不合法：%s\n", argv[3]); rc = 2; }
        if (rc == 0) rc = cmd_video_send(argv[2], port, argv[4], fps, quality, cam);
    } else if (strcmp(argv[1], "video-recv") == 0 && argc >= 4) {
        int port = parse_port(argv[2]);
        int idle = 3000, want_window = 0;
        rc = 0;
        for (int i = 4; i < argc && rc == 0; i++) {
            if      (strcmp(argv[i], "--idle") == 0)    { if (opt_int(argc, argv, &i, &idle) != 0) rc = usage(); }
            else if (strcmp(argv[i], "--display") == 0) { if (opt_display(argc, argv, &i, &want_window) != 0) rc = usage(); }
            else rc = usage();
        }
        if (rc == 0 && port < 0) { fprintf(stderr, "錯誤: port 不合法：%s\n", argv[2]); rc = 2; }
        if (rc == 0) rc = cmd_video_recv(port, argv[3], idle, want_window);
    } else if (strcmp(argv[1], "camera-test") == 0) {
        int want_window = 0, seconds = 5, cam = 0;
        const char *out = "camera.jpg";
        rc = 0;
        for (int i = 2; i < argc && rc == 0; i++) {
            if      (strcmp(argv[i], "--display") == 0) { if (opt_display(argc, argv, &i, &want_window) != 0) rc = usage(); }
            else if (strcmp(argv[i], "--seconds") == 0) { if (opt_int(argc, argv, &i, &seconds) != 0) rc = usage(); }
            else if (strcmp(argv[i], "--camera") == 0)  { if (opt_index(argc, argv, &i, &cam) != 0) rc = usage(); }
            else if (strcmp(argv[i], "--out") == 0)     { if (i + 1 >= argc) rc = usage(); else out = argv[++i]; }
            else rc = usage();
        }
        if (rc == 0) rc = cmd_camera_test(want_window, seconds, out, cam);
    } else {
        rc = usage();
    }

    udp_global_cleanup();
    return rc;
}
