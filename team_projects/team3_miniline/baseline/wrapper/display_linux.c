/*============================================================================
 *  display_linux.c  —  display.h 的 Linux 實作：Xlib（XCreateSimpleWindow ＋ XPutImage）
 *----------------------------------------------------------------------------
 *  X11 是 Linux 桌面最底層的視窗系統（Wayland 桌面也有 XWayland 相容層，一樣能跑）。
 *  和 Windows／macOS 一樣是事件驅動：XPending／XNextEvent 把排隊的事件拿出來處理，就是 display_poll。
 *  畫面：把 RGB 轉成 32-bit 的 BGRX（TrueColor、深度 24 的畫面幾乎都是這種排法）放進一個 XImage，
 *  用 XPutImage 貼到視窗。關閉鈕：向 window manager 登記 WM_DELETE_WINDOW，收到 ClientMessage 就當作關閉。
 *  需要：sudo apt install libx11-dev；連結 -lX11。沒有 DISPLAY（SSH、headless）時 XOpenDisplay 失敗 → 回傳 -1。
 *===========================================================================*/
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "display.h"

static struct {
    Display *dpy;
    Window   win;
    GC       gc;
    XImage  *img;
    uint8_t *pix;          /* w×h×4，BGRX */
    int      w, h;
    Atom     wm_delete;
    int      closed;
} g;

int display_open(int w, int h, const char *title) {
    if (g.dpy) display_close();
    g.dpy = XOpenDisplay(NULL);
    if (!g.dpy) { fprintf(stderr, "錯誤: 開不了 X11 視窗（沒有 DISPLAY？SSH 進來的？）\n"); return -1; }
    int screen = DefaultScreen(g.dpy);
    int depth = DefaultDepth(g.dpy, screen);
    Visual *vis = DefaultVisual(g.dpy, screen);
    if (depth != 24 && depth != 32) {
        fprintf(stderr, "錯誤: 畫面深度 %d 不支援（只處理 24／32-bit TrueColor）\n", depth);
        XCloseDisplay(g.dpy); g.dpy = NULL;
        return -1;
    }
    g.w = w; g.h = h; g.closed = 0;
    g.pix = (uint8_t *)calloc((size_t)w * h * 4, 1);
    if (!g.pix) { XCloseDisplay(g.dpy); g.dpy = NULL; return -1; }

    g.win = XCreateSimpleWindow(g.dpy, RootWindow(g.dpy, screen), 0, 0, (unsigned)w, (unsigned)h, 0,
                                BlackPixel(g.dpy, screen), BlackPixel(g.dpy, screen));
    XStoreName(g.dpy, g.win, title ? title : "miniline");
    XSizeHints hints;                                   /* 固定大小 */
    memset(&hints, 0, sizeof(hints));
    hints.flags = PMinSize | PMaxSize;
    hints.min_width = hints.max_width = w;
    hints.min_height = hints.max_height = h;
    XSetWMNormalHints(g.dpy, g.win, &hints);
    XSelectInput(g.dpy, g.win, ExposureMask | StructureNotifyMask);
    g.wm_delete = XInternAtom(g.dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g.dpy, g.win, &g.wm_delete, 1);
    g.gc = XCreateGC(g.dpy, g.win, 0, NULL);
    g.img = XCreateImage(g.dpy, vis, (unsigned)depth, ZPixmap, 0, (char *)g.pix, (unsigned)w, (unsigned)h, 32, w * 4);
    if (!g.img) { fprintf(stderr, "錯誤: XCreateImage 失敗\n"); display_close(); return -1; }
    XMapWindow(g.dpy, g.win);
    XFlush(g.dpy);
    return 0;
}

static void put(void) {
    if (g.dpy && g.img) {
        XPutImage(g.dpy, g.win, g.gc, g.img, 0, 0, 0, 0, (unsigned)g.w, (unsigned)g.h);
        XFlush(g.dpy);
    }
}

void display_show(const uint8_t *rgb) {
    if (!g.dpy || !g.pix || !rgb) return;
    for (int i = 0; i < g.w * g.h; i++) {                /* R G B → B G R X */
        g.pix[i * 4] = rgb[i * 3 + 2];
        g.pix[i * 4 + 1] = rgb[i * 3 + 1];
        g.pix[i * 4 + 2] = rgb[i * 3];
        g.pix[i * 4 + 3] = 0;
    }
    put();
}

int display_poll(void) {
    if (!g.dpy) return 0;
    while (XPending(g.dpy) > 0) {
        XEvent ev;
        XNextEvent(g.dpy, &ev);
        if (ev.type == Expose && ev.xexpose.count == 0) put();
        else if (ev.type == ClientMessage && (Atom)ev.xclient.data.l[0] == g.wm_delete) g.closed = 1;
        else if (ev.type == DestroyNotify) g.closed = 1;
    }
    return g.closed ? 0 : 1;
}

void display_close(void) {
    if (!g.dpy) return;
    if (g.img) { g.img->data = NULL; XDestroyImage(g.img); g.img = NULL; }   /* pix 自己 free，不讓 X 來 free */
    if (g.gc) { XFreeGC(g.dpy, g.gc); g.gc = NULL; }
    if (g.win) { XDestroyWindow(g.dpy, g.win); g.win = 0; }
    XCloseDisplay(g.dpy);
    g.dpy = NULL;
    free(g.pix); g.pix = NULL;
    g.closed = 1;
}
