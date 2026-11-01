/*============================================================================
 *  display_win.c  —  display.h 的 Windows 實作：一個普通的 GDI 視窗 ＋ StretchDIBits
 *----------------------------------------------------------------------------
 *  Windows 的視窗靠「訊息」驅動：作業系統把「請重畫」「使用者按了關閉」這類事件排進佇列，程式要用
 *  PeekMessage／DispatchMessage 把它們一個個拿出來交給 WndProc 處理——這就是 display_poll 做的事。
 *  畫面放在一個 BGR、bottom-up 的 DIB（裝置無關點陣圖）裡；WM_PAINT 時用 StretchDIBits 貼到視窗上。
 *  連結：-lgdi32 -luser32
 *===========================================================================*/
#ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0601
#endif
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "display.h"

static struct {
    HWND     hwnd;
    int      w, h;
    uint8_t *dib;          /* BGR，每列補到 4 的倍數，bottom-up（Windows 的點陣圖是下到上） */
    int      row_bytes;
    int      closed;
} g;

static void paint(HDC dc) {
    if (!g.dib) return;
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = g.w;
    bi.bmiHeader.biHeight = g.h;                  /* 正的 = bottom-up */
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 24;
    bi.bmiHeader.biCompression = BI_RGB;
    RECT rc;
    GetClientRect(g.hwnd, &rc);
    StretchDIBits(dc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, 0, 0, g.w, g.h,
                  g.dib, &bi, DIB_RGB_COLORS, SRCCOPY);
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        paint(dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;                                 /* 我們每次都整個畫滿，不必先清底色（避免閃爍） */
    case WM_CLOSE:
        g.closed = 1;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        g.hwnd = NULL;
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

int display_open(int w, int h, const char *title) {
    if (g.hwnd) display_close();
    g.w = w; g.h = h; g.closed = 0;
    g.row_bytes = (w * 3 + 3) & ~3;
    g.dib = (uint8_t *)calloc((size_t)g.row_bytes * h, 1);
    if (!g.dib) return -1;

    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"miniline_display";
    RegisterClassW(&wc);                          /* 第二次開會說「已註冊」，沒關係 */

    /* 視窗外框會占掉空間：先算出「內容區剛好 w×h」要多大的視窗 */
    DWORD style = WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;
    RECT rc = { 0, 0, w, h };
    AdjustWindowRect(&rc, style, FALSE);

    WCHAR wtitle[256];
    MultiByteToWideChar(CP_UTF8, 0, title ? title : "miniline", -1, wtitle, 256);
    g.hwnd = CreateWindowExW(0, L"miniline_display", wtitle, style, CW_USEDEFAULT, CW_USEDEFAULT,
                             rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, wc.hInstance, NULL);
    if (!g.hwnd) {
        fprintf(stderr, "錯誤: 開不了視窗（沒有桌面？）\n");
        free(g.dib); g.dib = NULL;
        return -1;
    }
    ShowWindow(g.hwnd, SW_SHOWNORMAL);
    UpdateWindow(g.hwnd);
    return 0;
}

void display_show(const uint8_t *rgb) {
    if (!g.hwnd || !g.dib || !rgb) return;
    for (int y = 0; y < g.h; y++) {
        const uint8_t *src = rgb + (size_t)y * g.w * 3;
        uint8_t *dst = g.dib + (size_t)(g.h - 1 - y) * g.row_bytes;   /* 上下顛倒放 */
        for (int x = 0; x < g.w; x++) {                                /* R G B → B G R */
            dst[x * 3] = src[x * 3 + 2]; dst[x * 3 + 1] = src[x * 3 + 1]; dst[x * 3 + 2] = src[x * 3];
        }
    }
    HDC dc = GetDC(g.hwnd);
    if (dc) { paint(dc); ReleaseDC(g.hwnd, dc); }     /* 立刻畫，不等 WM_PAINT */
}

int display_poll(void) {
    MSG m;
    while (g.hwnd && PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return (g.hwnd && !g.closed) ? 1 : 0;
}

void display_close(void) {
    if (g.hwnd) { DestroyWindow(g.hwnd); g.hwnd = NULL; }
    display_poll();
    free(g.dib); g.dib = NULL;
    g.closed = 1;
}
