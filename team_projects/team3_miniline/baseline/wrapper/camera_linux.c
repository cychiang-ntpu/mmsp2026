/*============================================================================
 *  camera_linux.c  —  camera.h 的 Linux 實作：V4L2（Video for Linux 2），/dev/videoN ＋ mmap
 *----------------------------------------------------------------------------
 *  V4L2 的攝影機就是一個裝置檔 /dev/video0，用 ioctl 設定、用 mmap 把核心的緩衝區映射進來、用 select 等畫面：
 *    VIDIOC_QUERYCAP   這是不是攝影機（有 VIDEO_CAPTURE 能力）、名字叫什麼
 *    VIDIOC_S_FMT      要求 w×h 的 YUYV（驅動可能改成它支援的尺寸，之後用 camera_util.h 裁切縮放）
 *    VIDIOC_REQBUFS    向核心要 4 個緩衝區（mmap 模式）
 *    VIDIOC_QUERYBUF   問每個緩衝區的位置與大小 → mmap
 *    VIDIOC_QBUF       把緩衝區交給核心（「你可以往這裡放畫面」）
 *    VIDIOC_STREAMON   開始
 *    select + VIDIOC_DQBUF   等一張 → 拿回裝了畫面的緩衝區 → 轉 RGB → VIDIOC_QBUF 還回去
 *  只接受 YUYV（最常見的 USB／筆電攝影機格式）；驅動只給 MJPEG 的攝影機這個 baseline 不支援（印錯誤）。
 *  不需要額外的函式庫（標頭 linux/videodev2.h 在 kernel headers 裡，一般發行版都有）。
 *  權限：使用者要在 video 群組（sudo usermod -aG video $USER，重新登入）。
 *===========================================================================*/
#ifndef _POSIX_C_SOURCE
  #define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
  #define _DEFAULT_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <linux/videodev2.h>

#include "camera.h"
#include "camera_util.h"

#define MAX_DEV   16
#define N_BUFS    4

static struct {
    int      fd;
    int      src_w, src_h, stride;
    int      dst_w, dst_h;
    uint8_t *tmp_rgb;
    struct { void *start; size_t len; } bufs[N_BUFS];
    int      n_bufs;
    int      streaming;
} g = { -1, 0, 0, 0, 0, 0, NULL, { { NULL, 0 } }, 0, 0 };

/* ioctl 被訊號打斷要重試 */
static int xioctl(int fd, unsigned long req, void *arg) {
    int r;
    do { r = ioctl(fd, req, arg); } while (r < 0 && errno == EINTR);
    return r;
}

/* 打開 /dev/videoN 並確認它是攝影機；回傳 fd 或 -1。name 不是 NULL 就填名字 */
static int open_capture_dev(int n, char *name, size_t cap) {
    char path[32];
    snprintf(path, sizeof(path), "/dev/video%d", n);
    int fd = open(path, O_RDWR | O_NONBLOCK);
    if (fd < 0) return -1;
    struct v4l2_capability vc;
    memset(&vc, 0, sizeof(vc));
    if (xioctl(fd, VIDIOC_QUERYCAP, &vc) < 0) { close(fd); return -1; }
    unsigned caps = (vc.capabilities & V4L2_CAP_DEVICE_CAPS) ? vc.device_caps : vc.capabilities;
    if (!(caps & V4L2_CAP_VIDEO_CAPTURE) || !(caps & V4L2_CAP_STREAMING)) { close(fd); return -1; }
    /* 有些 /dev/videoN 是「metadata」節點、不是畫面：試著列舉格式，一個都沒有就跳過 */
    struct v4l2_fmtdesc fd_desc;
    memset(&fd_desc, 0, sizeof(fd_desc));
    fd_desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd, VIDIOC_ENUM_FMT, &fd_desc) < 0) { close(fd); return -1; }
    if (name) snprintf(name, cap, "%s (%s)", (const char *)vc.card, path);
    return fd;
}

/* 第 i 台攝影機對應的 /dev/videoN 編號；找不到回傳 -1 */
static int nth_device(int i, char *name, size_t cap) {
    int k = 0;
    for (int n = 0; n < MAX_DEV; n++) {
        int fd = open_capture_dev(n, name, cap);
        if (fd < 0) continue;
        close(fd);
        if (k == i) return n;
        k++;
    }
    return -1;
}

int camera_count(void) {
    int k = 0;
    for (int n = 0; n < MAX_DEV; n++) {
        int fd = open_capture_dev(n, NULL, 0);
        if (fd >= 0) { close(fd); k++; }
    }
    return k;
}

int camera_name(int i, char *buf, size_t cap) {
    if (!buf || cap == 0) return -1;
    buf[0] = '\0';
    return nth_device(i, buf, cap) < 0 ? -1 : 0;
}

int camera_open(int index, int w, int h) {
    if (g.fd >= 0) camera_close();
    char name[128];
    int n = nth_device(index, name, sizeof(name));
    if (n < 0) {
        if (camera_count() == 0) fprintf(stderr, "錯誤: 這台電腦沒有攝影機（沒有可用的 /dev/video*；或不在 video 群組）\n");
        else fprintf(stderr, "錯誤: 沒有第 %d 台攝影機\n", index);
        return -1;
    }
    g.fd = open_capture_dev(n, NULL, 0);
    if (g.fd < 0) { fprintf(stderr, "錯誤: 打不開 /dev/video%d：%s\n", n, strerror(errno)); return -1; }

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = (unsigned)w;
    fmt.fmt.pix.height = (unsigned)h;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (xioctl(g.fd, VIDIOC_S_FMT, &fmt) < 0) {
        fprintf(stderr, "錯誤: VIDIOC_S_FMT 失敗：%s\n", strerror(errno));
        camera_close();
        return -1;
    }
    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV) {
        fprintf(stderr, "錯誤: 攝影機不支援 YUYV（給的是 %c%c%c%c）；這個 baseline 只處理 YUYV\n",
                (char)(fmt.fmt.pix.pixelformat & 0xff), (char)((fmt.fmt.pix.pixelformat >> 8) & 0xff),
                (char)((fmt.fmt.pix.pixelformat >> 16) & 0xff), (char)((fmt.fmt.pix.pixelformat >> 24) & 0xff));
        camera_close();
        return -1;
    }
    g.src_w = (int)fmt.fmt.pix.width;
    g.src_h = (int)fmt.fmt.pix.height;
    g.stride = (int)fmt.fmt.pix.bytesperline;
    if (g.stride <= 0) g.stride = g.src_w * 2;
    g.dst_w = w; g.dst_h = h;
    g.tmp_rgb = (uint8_t *)malloc((size_t)g.src_w * g.src_h * 3);
    if (!g.tmp_rgb) { camera_close(); return -1; }

    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = N_BUFS;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(g.fd, VIDIOC_REQBUFS, &req) < 0 || req.count < 2) {
        fprintf(stderr, "錯誤: VIDIOC_REQBUFS 失敗\n");
        camera_close();
        return -1;
    }
    g.n_bufs = (int)req.count;
    for (int i = 0; i < g.n_bufs; i++) {
        struct v4l2_buffer b;
        memset(&b, 0, sizeof(b));
        b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        b.memory = V4L2_MEMORY_MMAP;
        b.index = (unsigned)i;
        if (xioctl(g.fd, VIDIOC_QUERYBUF, &b) < 0) {
            fprintf(stderr, "錯誤: VIDIOC_QUERYBUF 失敗\n");
            camera_close();
            return -1;
        }
        g.bufs[i].len = b.length;
        g.bufs[i].start = mmap(NULL, b.length, PROT_READ | PROT_WRITE, MAP_SHARED, g.fd, b.m.offset);
        if (g.bufs[i].start == MAP_FAILED) {
            g.bufs[i].start = NULL;
            fprintf(stderr, "錯誤: mmap 失敗\n");
            camera_close();
            return -1;
        }
        if (xioctl(g.fd, VIDIOC_QBUF, &b) < 0) { fprintf(stderr, "錯誤: VIDIOC_QBUF 失敗\n"); camera_close(); return -1; }
    }
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(g.fd, VIDIOC_STREAMON, &type) < 0) {
        fprintf(stderr, "錯誤: VIDIOC_STREAMON 失敗：%s\n", strerror(errno));
        camera_close();
        return -1;
    }
    g.streaming = 1;
    fprintf(stderr, "攝影機：%s %dx%d YUYV → %dx%d RGB\n", name, g.src_w, g.src_h, w, h);
    return 0;
}

int camera_read(uint8_t *rgb, int timeout_ms) {
    if (g.fd < 0 || !rgb) return -1;
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(g.fd, &rfds);
    struct timeval tv, *ptv = NULL;
    if (timeout_ms >= 0) { tv.tv_sec = timeout_ms / 1000; tv.tv_usec = (timeout_ms % 1000) * 1000; ptv = &tv; }
    int r;
    do { r = select(g.fd + 1, &rfds, NULL, NULL, ptv); } while (r < 0 && errno == EINTR);
    if (r == 0) return 0;
    if (r < 0) return -1;

    /* 把排隊的全部拿出來，只留最新的一張（攝影機 30 fps、我們 5 fps） */
    struct v4l2_buffer b, last;
    int have = 0;
    for (;;) {
        memset(&b, 0, sizeof(b));
        b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        b.memory = V4L2_MEMORY_MMAP;
        if (xioctl(g.fd, VIDIOC_DQBUF, &b) < 0) {
            if (errno == EAGAIN) break;
            return -1;
        }
        if (have) xioctl(g.fd, VIDIOC_QBUF, &last);          /* 舊的直接還回去 */
        last = b; have = 1;
    }
    if (!have) return 0;
    cam_yuyv_to_rgb((const uint8_t *)g.bufs[last.index].start, g.stride, g.src_w, g.src_h, g.tmp_rgb);
    cam_fit_rgb(g.tmp_rgb, g.src_w, g.src_h, g.src_w * 3, rgb, g.dst_w, g.dst_h);
    xioctl(g.fd, VIDIOC_QBUF, &last);
    return 1;
}

void camera_close(void) {
    if (g.fd >= 0) {
        if (g.streaming) {
            enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            xioctl(g.fd, VIDIOC_STREAMOFF, &type);
            g.streaming = 0;
        }
        for (int i = 0; i < g.n_bufs; i++)
            if (g.bufs[i].start) { munmap(g.bufs[i].start, g.bufs[i].len); g.bufs[i].start = NULL; }
        g.n_bufs = 0;
        close(g.fd);
        g.fd = -1;
    }
    free(g.tmp_rgb); g.tmp_rgb = NULL;
}
