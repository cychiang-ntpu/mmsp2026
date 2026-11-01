/*============================================================================
 *  camera_win.c  —  camera.h 的 Windows 實作：Media Foundation 的 IMFSourceReader
 *----------------------------------------------------------------------------
 *  Media Foundation（MF）是 Windows 的多媒體框架，用 COM 介面：每個物件是一個指標，呼叫方法要寫
 *  obj->lpVtbl->Method(obj, ...)，這裡用 COBJMACROS 提供的 IMFSourceReader_ReadSample(obj, ...) 這種巨集。
 *  COM 物件用完要 Release（引用計數 -1），不然攝影機會一直亮著。
 *
 *  流程：
 *    1. MFEnumDeviceSources   列出所有「視訊擷取」裝置（IMFActivate 陣列），名字在 MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME
 *    2. ActivateObject        把第 index 台變成 IMFMediaSource，再包成 IMFSourceReader（同步地「要一張」的介面）
 *    3. 挑原生格式            攝影機自己支援的格式（GetNativeMediaType 列舉）裡只看未壓縮的 YUY2／NV12／RGB，
 *                             依「尺寸最接近 w×h、幀率合理」排序，由好到壞一個個試（真的讀一張才算成功）
 *    4. 自己轉 RGB            不用 MF 的轉換器（MF_READWRITE_DISABLE_CONVERTERS）：有些筆電的攝影機驅動
 *                             接上轉換器就開不了串流。YUY2／NV12 → RGB 在 camera_util.h，幾行而已
 *    5. 擷取執行緒            ReadSample 是「同步」的（卡到下一張為止），所以開一條執行緒一直讀、
 *                             只保留最新的一張；camera_read 用 event 等「有新的一張」最多 timeout_ms
 *
 *  連結：-lmfplat -lmf -lmfreadwrite -lmfuuid -lstrmiids -lole32
 *  曝光：USB 攝影機會「記住」上一個程式設的手動曝光（Windows 不會幫你還原）；別的程式把曝光調到 1/180 秒之後，
 *  我們開起來就是一片黑。所以 camera_open 一律把曝光設回自動（IAMCameraControl），和「相機」app 一樣。
 *  注意：MF 的 RGB24 是 B G R 的順序、而且通常是「下到上」（bottom-up：pitch 為負）；用 IMF2DBuffer::Lock2D
 *  拿到 scanline0 與 pitch 之後，第 y 列 = scanline0 + y*pitch，正負都對。
 *===========================================================================*/
#ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0601
#endif
#define COBJMACROS
#include <winsock2.h>                  /* 和 udp.h 一樣：先於 windows.h，避免舊版 winsock 衝突 */
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <strmif.h>                    /* IAMCameraControl：曝光（連結 -lstrmiids） */
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camera.h"
#include "camera_util.h"

/* MF_MT_FRAME_SIZE 是一個 64-bit：高 32 位寬、低 32 位高（C++ 才有 MFGetAttributeSize，C 自己拆） */
static void get_frame_size(IMFMediaType *mt, UINT32 *w, UINT32 *h) {
    UINT64 v = 0;
    *w = *h = 0;
    if (SUCCEEDED(IMFMediaType_GetUINT64(mt, &MF_MT_FRAME_SIZE, &v))) {
        *w = (UINT32)(v >> 32);
        *h = (UINT32)(v & 0xffffffffu);
    }
}

/* 設環境變數 ML_CAMERA_DEBUG=1 會印出攝影機的每一種原生格式與選擇過程（找問題用） */
static int debug_on(void) {
    static int v = -1;
    if (v < 0) { const char *e = getenv("ML_CAMERA_DEBUG"); v = (e && *e && *e != '0') ? 1 : 0; }
    return v;
}

static void fourcc_str(const GUID *sub, char out[5]) {
    uint32_t d = sub->Data1;
    out[0] = (char)(d & 0xff); out[1] = (char)((d >> 8) & 0xff); out[2] = (char)((d >> 16) & 0xff); out[3] = (char)(d >> 24);
    out[4] = '\0';
    for (int i = 0; i < 4; i++) if (out[i] < 32 || out[i] > 126) out[i] = '?';
}

typedef enum { FMT_NONE, FMT_RGB24, FMT_RGB32, FMT_NV12, FMT_YUY2 } fmt_t;

static struct {
    int                 inited;        /* CoInitialize + MFStartup 做過了 */
    IMFActivate       **devs;          /* 列舉出來的裝置 */
    UINT32              n_devs;
    IMFMediaSource     *source;
    IMFSourceReader    *reader;
    fmt_t               fmt;
    int                 src_w, src_h;  /* 攝影機實際給的尺寸 */
    int                 dst_w, dst_h;  /* 呼叫端要的尺寸 */
    LONG                stride;        /* MF_MT_DEFAULT_STRIDE（沒有就 0） */
    uint8_t            *tmp_rgb;       /* src_w×src_h×3：轉成 RGB 後、縮放前 */
    /* 擷取執行緒與「最新一張」 */
    HANDLE              thread;
    HANDLE              evt;           /* 有新的一張（auto-reset） */
    CRITICAL_SECTION    lock;
    uint8_t            *latest;        /* dst_w×dst_h×3 */
    int                 has_latest;
    volatile int        stop;
    volatile int        failed;
} g;

static int ensure_init(void) {
    if (g.inited) return 0;
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        fprintf(stderr, "錯誤: CoInitializeEx 失敗 (0x%08lx)\n", (long)hr);
        return -1;
    }
    hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    if (FAILED(hr)) { fprintf(stderr, "錯誤: MFStartup 失敗 (0x%08lx)\n", (long)hr); return -1; }
    g.inited = 1;
    return 0;
}

/* 列舉裝置（每次重新列，拔插攝影機才看得到） */
static int enum_devices(void) {
    if (ensure_init() != 0) return -1;
    if (g.devs) {
        for (UINT32 i = 0; i < g.n_devs; i++) IMFActivate_Release(g.devs[i]);
        CoTaskMemFree(g.devs);
        g.devs = NULL; g.n_devs = 0;
    }
    IMFAttributes *attr = NULL;
    if (FAILED(MFCreateAttributes(&attr, 1))) return -1;
    IMFAttributes_SetGUID(attr, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    HRESULT hr = MFEnumDeviceSources(attr, &g.devs, &g.n_devs);
    IMFAttributes_Release(attr);
    if (FAILED(hr)) { g.devs = NULL; g.n_devs = 0; return -1; }
    return (int)g.n_devs;
}

int camera_count(void) {
    int n = enum_devices();
    return n < 0 ? 0 : n;
}

int camera_name(int i, char *buf, size_t cap) {
    if (!buf || cap == 0) return -1;
    buf[0] = '\0';
    if (!g.devs && enum_devices() < 0) return -1;
    if (i < 0 || (UINT32)i >= g.n_devs) return -1;
    WCHAR *name = NULL;
    UINT32 len = 0;
    if (FAILED(IMFActivate_GetAllocatedString(g.devs[i], &MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &len))) {
        snprintf(buf, cap, "camera %d", i);
        return 0;
    }
    WideCharToMultiByte(CP_UTF8, 0, name, -1, buf, (int)cap, NULL, NULL);   /* UTF-16 → UTF-8 */
    buf[cap - 1] = '\0';
    CoTaskMemFree(name);
    return 0;
}

/* 攝影機的每一種原生格式打一個分數（越小越好），之後 camera_open 由好到壞一個個試。
 *   尺寸差 ×100：最接近 w×h 的優先（320×240 攝影機通常直接有）
 *   只收未壓縮的（NV12／YUY2／RGB24／RGB32）
 *   幀率 15–60 最好；> 60 的多半是驅動亂報的假模式（有些筆電會列出 180 fps，一開就失敗）+40；< 15 +20 */
#define MAX_CAND 64
typedef struct { IMFMediaType *mt; long score; } cand_t;

static int collect_candidates(cand_t *c, int w, int h) {
    int n = 0;
    for (DWORD i = 0; n < MAX_CAND; i++) {
        IMFMediaType *mt = NULL;
        if (FAILED(IMFSourceReader_GetNativeMediaType(g.reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &mt))) break;
        UINT32 fw = 0, fh = 0;
        get_frame_size(mt, &fw, &fh);
        GUID sub;
        memset(&sub, 0, sizeof(sub));
        IMFMediaType_GetGUID(mt, &MF_MT_SUBTYPE, &sub);
        UINT64 fr = 0;
        IMFMediaType_GetUINT64(mt, &MF_MT_FRAME_RATE, &fr);
        double fps = (fr & 0xffffffffu) ? (double)(fr >> 32) / (double)(fr & 0xffffffffu) : 0.0;
        int uncompressed = IsEqualGUID(&sub, &MFVideoFormat_NV12) || IsEqualGUID(&sub, &MFVideoFormat_YUY2) ||
                           IsEqualGUID(&sub, &MFVideoFormat_RGB24) || IsEqualGUID(&sub, &MFVideoFormat_RGB32);
        long score = (labs((long)fw - w) + labs((long)fh - h)) * 100 + (fps > 60.0 ? 40 : fps < 15.0 ? 20 : 0);
        if (debug_on()) {
            char fc[5];
            fourcc_str(&sub, fc);
            fprintf(stderr, "  native[%lu] %s %ux%u %.1f fps %s\n", (unsigned long)i, fc, (unsigned)fw, (unsigned)fh, fps,
                    uncompressed ? "" : "(skip: compressed)");
        }
        if (!uncompressed) { IMFMediaType_Release(mt); continue; }   /* MJPEG 等壓縮格式不收：沒有解碼器 */
        c[n].mt = mt; c[n].score = score; n++;
    }
    /* 插入排序（最多 64 個，夠用） */
    for (int i = 1; i < n; i++) {
        cand_t t = c[i];
        int j = i - 1;
        while (j >= 0 && c[j].score > t.score) { c[j + 1] = c[j]; j--; }
        c[j + 1] = t;
    }
    return n;
}

/* 一張 sample → g.latest（在擷取執行緒上跑） */
static void convert_sample(IMFSample *sample) {
    IMFMediaBuffer *buf = NULL;
    if (FAILED(IMFSample_ConvertToContiguousBuffer(sample, &buf))) return;

    BYTE *data = NULL;
    LONG  pitch = 0;
    DWORD len = 0;
    IMF2DBuffer *b2d = NULL;
    int locked2d = 0;
    if (SUCCEEDED(IMFMediaBuffer_QueryInterface(buf, &IID_IMF2DBuffer, (void **)&b2d)) && b2d) {
        if (SUCCEEDED(IMF2DBuffer_Lock2D(b2d, &data, &pitch))) locked2d = 1;
        else { IMF2DBuffer_Release(b2d); b2d = NULL; }
    }
    if (!locked2d) {
        if (FAILED(IMFMediaBuffer_Lock(buf, &data, NULL, &len))) { IMFMediaBuffer_Release(buf); return; }
        int bpp = (g.fmt == FMT_RGB24) ? 3 : (g.fmt == FMT_RGB32) ? 4 : (g.fmt == FMT_YUY2) ? 2 : 1;
        pitch = g.stride ? g.stride : (LONG)g.src_w * bpp;
        if (pitch < 0) data += (LONG)(g.src_h - 1) * (-pitch);        /* bottom-up：先跳到最上面那列 */
    }

    int sw = g.src_w, sh = g.src_h;
    if (g.fmt == FMT_RGB24 || g.fmt == FMT_RGB32) {
        int bpp = (g.fmt == FMT_RGB24) ? 3 : 4;
        for (int y = 0; y < sh; y++) {
            const uint8_t *row = data + (LONG)y * pitch;                /* pitch 為負時自動往上走 */
            uint8_t *out = g.tmp_rgb + (size_t)y * sw * 3;
            for (int x = 0; x < sw; x++) {                              /* B G R → R G B */
                out[x * 3] = row[x * bpp + 2]; out[x * 3 + 1] = row[x * bpp + 1]; out[x * 3 + 2] = row[x * bpp];
            }
        }
    } else if (g.fmt == FMT_NV12) {
        cam_nv12_to_rgb(data, (int)pitch, sw, sh, g.tmp_rgb);
    } else {
        cam_yuyv_to_rgb(data, (int)pitch, sw, sh, g.tmp_rgb);
    }

    if (locked2d) { IMF2DBuffer_Unlock2D(b2d); IMF2DBuffer_Release(b2d); }
    else IMFMediaBuffer_Unlock(buf);
    IMFMediaBuffer_Release(buf);

    EnterCriticalSection(&g.lock);
    cam_fit_rgb(g.tmp_rgb, sw, sh, sw * 3, g.latest, g.dst_w, g.dst_h);
    g.has_latest = 1;
    LeaveCriticalSection(&g.lock);
    SetEvent(g.evt);
}

static unsigned __stdcall capture_thread(void *arg) {
    (void)arg;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);          /* 每條用 COM 的執行緒都要各自初始化 */
    while (!g.stop) {
        DWORD stream = 0, flags = 0;
        LONGLONG ts = 0;
        IMFSample *sample = NULL;
        HRESULT hr = IMFSourceReader_ReadSample(g.reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,
                                                &stream, &flags, &ts, &sample);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ERROR)) {
            fprintf(stderr, "錯誤: ReadSample 失敗 (hr=0x%08lx flags=0x%lx)\n", (long)hr, (unsigned long)flags);
            g.failed = 1; break;
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) { g.failed = 1; break; }
        if (sample) {                                       /* 偶爾會是 NULL（格式改變、還沒有畫面）：跳過 */
            convert_sample(sample);
            IMFSample_Release(sample);
        }
    }
    SetEvent(g.evt);                                        /* 叫醒正在等的 camera_read */
    CoUninitialize();
    return 0;
}

/* 把曝光設回「自動」。別的程式（例如用攝影機做偵測的軟體）常把曝光設成手動的很短時間，而且 USB 攝影機
 * 會記住這個設定；不還原的話畫面一片黑。IAMCameraControl 是 DirectShow 時代的介面，MF 的 source 也支援。 */
static void restore_auto_exposure(void) {
    IAMCameraControl *cc = NULL;
    if (!g.source || FAILED(IMFMediaSource_QueryInterface(g.source, &IID_IAMCameraControl, (void **)&cc)) || !cc) return;
    LONG val = 0, flags = 0, mn = 0, mx = 0, step = 0, def = 0, fcaps = 0;
    HRESULT hr = IAMCameraControl_Get(cc, CameraControl_Exposure, &val, &flags);
    if (debug_on())
        fprintf(stderr, "  exposure now: value=%ld flags=%ld (1=auto 2=manual) hr=0x%08lx\n",
                (long)val, (long)flags, (long)hr);
    if (SUCCEEDED(hr) && !(flags & CameraControl_Flags_Auto)) {
        if (FAILED(IAMCameraControl_GetRange(cc, CameraControl_Exposure, &mn, &mx, &step, &def, &fcaps))) def = val;
        hr = IAMCameraControl_Set(cc, CameraControl_Exposure, def, CameraControl_Flags_Auto);
        if (debug_on()) fprintf(stderr, "  exposure → auto (default %ld): hr=0x%08lx\n", (long)def, (long)hr);
    }
    IAMCameraControl_Release(cc);
}

/* 把第 index 台變成 IMFMediaSource ＋ IMFSourceReader（失敗印訊息）。回傳 0 成功 */
static int make_reader(int index) {
    HRESULT hr = IMFActivate_ActivateObject(g.devs[index], &IID_IMFMediaSource, (void **)&g.source);
    if (FAILED(hr)) {
        fprintf(stderr, "錯誤: 打不開攝影機 (0x%08lx)：被別的程式占用？或「設定 → 隱私權 → 相機」沒開？\n", (long)hr);
        return -1;
    }
    IMFAttributes *rattr = NULL;
    MFCreateAttributes(&rattr, 1);
    if (rattr) IMFAttributes_SetUINT32(rattr, &MF_READWRITE_DISABLE_CONVERTERS, TRUE);   /* 不用 MF 的轉換器 */
    restore_auto_exposure();
    hr = MFCreateSourceReaderFromMediaSource(g.source, rattr, &g.reader);
    if (rattr) IMFAttributes_Release(rattr);
    if (FAILED(hr)) { fprintf(stderr, "錯誤: MFCreateSourceReaderFromMediaSource 失敗 (0x%08lx)\n", (long)hr); return -1; }
    return 0;
}

/* 關掉 reader 與 source。index >= 0 時同時告訴 IMFActivate 物件已經關了，並重新列舉——
 * 不然同一個 IMFActivate 再 ActivateObject 會得到 MF_E_SHUTDOWN。 */
static void drop_reader(int index) {
    if (g.reader) { IMFSourceReader_Release(g.reader); g.reader = NULL; }
    if (g.source) { IMFMediaSource_Shutdown(g.source); IMFMediaSource_Release(g.source); g.source = NULL; }
    if (index >= 0 && g.devs && (UINT32)index < g.n_devs) {
        IMFActivate_ShutdownObject(g.devs[index]);
        enum_devices();
    }
}

int camera_open(int index, int w, int h) {
    if (g.reader) camera_close();
    if (enum_devices() < 0) { fprintf(stderr, "錯誤: 列舉攝影機失敗\n"); return -1; }
    if (g.n_devs == 0) { fprintf(stderr, "錯誤: 這台電腦沒有攝影機\n"); return -1; }
    if (index < 0 || (UINT32)index >= g.n_devs) {
        fprintf(stderr, "錯誤: 沒有第 %d 台攝影機（共 %u 台）\n", index, (unsigned)g.n_devs);
        return -1;
    }
    if (make_reader(index) != 0) { camera_close(); return -1; }

    /* 由好到壞試每一種原生格式：設成目前格式 → 真的讀一張看看。「真的讀一張」很重要：有些模式
     * SetCurrentMediaType 會成功、ReadSample 才失敗（硬體開不了串流）；而且失敗一次之後整個 reader 就壞了，
     * 所以每試一個就把 source／reader 重開一次。 */
    cand_t cand[MAX_CAND];
    int n_cand = collect_candidates(cand, w, h);
    if (n_cand == 0) { fprintf(stderr, "錯誤: 攝影機沒有任何未壓縮的格式（只有 MJPEG？）\n"); camera_close(); return -1; }
    int ok = 0;
    HRESULT hr;
    g.dst_w = w; g.dst_h = h;
    g.latest = (uint8_t *)malloc((size_t)w * h * 3);
    if (!g.latest) { fprintf(stderr, "錯誤: 記憶體不足\n"); camera_close(); return -1; }
    for (int ci = 0; ci < n_cand && !ok; ci++) {
        if (ci > 0) {                                           /* 上一個失敗了：重開 source 與 reader */
            drop_reader(index);
            if (!g.devs || (UINT32)index >= g.n_devs || make_reader(index) != 0) break;
        }
        GUID sub;
        memset(&sub, 0, sizeof(sub));
        IMFMediaType_GetGUID(cand[ci].mt, &MF_MT_SUBTYPE, &sub);
        hr = IMFSourceReader_SetCurrentMediaType(g.reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, cand[ci].mt);
        if (debug_on())
            fprintf(stderr, "  try candidate %d (score %ld): SetCurrentMediaType → 0x%08lx\n", ci, cand[ci].score, (long)hr);
        if (FAILED(hr)) continue;
        if      (IsEqualGUID(&sub, &MFVideoFormat_RGB24)) g.fmt = FMT_RGB24;
        else if (IsEqualGUID(&sub, &MFVideoFormat_RGB32)) g.fmt = FMT_RGB32;
        else if (IsEqualGUID(&sub, &MFVideoFormat_NV12))  g.fmt = FMT_NV12;
        else                                               g.fmt = FMT_YUY2;

        /* 問 reader 最後定案的尺寸與 stride */
        IMFMediaType *cur = NULL;
        UINT32 fw = 0, fh = 0;
        g.stride = 0;
        if (SUCCEEDED(IMFSourceReader_GetCurrentMediaType(g.reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, &cur)) && cur) {
            get_frame_size(cur, &fw, &fh);
            UINT32 st = 0;
            if (SUCCEEDED(IMFMediaType_GetUINT32(cur, &MF_MT_DEFAULT_STRIDE, &st))) g.stride = (LONG)(int32_t)st;
            IMFMediaType_Release(cur);
        }
        if (fw == 0 || fh == 0) continue;
        g.src_w = (int)fw; g.src_h = (int)fh;
        free(g.tmp_rgb);
        g.tmp_rgb = (uint8_t *)malloc((size_t)g.src_w * g.src_h * 3);
        if (!g.tmp_rgb) { fprintf(stderr, "錯誤: 記憶體不足\n"); camera_close(); return -1; }

        /* 試讀一張（同步，等到第一張為止；失敗會很快回來） */
        DWORD stream = 0, flags = 0;
        LONGLONG ts = 0;
        IMFSample *sample = NULL;
        hr = IMFSourceReader_ReadSample(g.reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &stream, &flags, &ts, &sample);
        if (debug_on()) fprintf(stderr, "  test ReadSample → 0x%08lx flags=0x%lx\n", (long)hr, (unsigned long)flags);
        if (FAILED(hr) || (flags & (MF_SOURCE_READERF_ERROR | MF_SOURCE_READERF_ENDOFSTREAM))) {
            if (sample) IMFSample_Release(sample);
            continue;
        }
        if (sample) IMFSample_Release(sample);                /* 第一張通常還沒曝光好，不留 */
        ok = 1;
    }
    for (int ci = 0; ci < n_cand; ci++) IMFMediaType_Release(cand[ci].mt);
    if (!ok) { fprintf(stderr, "錯誤: 攝影機的每一種格式都開不起來（被別的程式占用？）\n"); camera_close(); return -1; }
    g.has_latest = 0; g.stop = 0; g.failed = 0;

    InitializeCriticalSection(&g.lock);
    g.evt = CreateEventW(NULL, FALSE, FALSE, NULL);
    g.thread = (HANDLE)_beginthreadex(NULL, 0, capture_thread, NULL, 0, NULL);
    if (!g.evt || !g.thread) { fprintf(stderr, "錯誤: 開不了擷取執行緒\n"); camera_close(); return -1; }
    {
        static const char *names[] = { "?", "RGB24", "RGB32", "NV12", "YUY2" };
        fprintf(stderr, "攝影機：%dx%d %s → %dx%d RGB\n", g.src_w, g.src_h, names[g.fmt], w, h);
    }
    return 0;
}

int camera_read(uint8_t *rgb, int timeout_ms) {
    if (!g.reader || !rgb) return -1;
    DWORD wait = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;
    DWORD rc = WaitForSingleObject(g.evt, wait);
    if (g.failed) return -1;
    if (rc != WAIT_OBJECT_0) return 0;
    EnterCriticalSection(&g.lock);
    int got = g.has_latest;
    if (got) { memcpy(rgb, g.latest, (size_t)g.dst_w * g.dst_h * 3); g.has_latest = 0; }
    LeaveCriticalSection(&g.lock);
    return got ? 1 : 0;
}

void camera_close(void) {
    if (g.thread) {
        g.stop = 1;
        WaitForSingleObject(g.thread, 3000);      /* ReadSample 最多卡一張的時間 */
        CloseHandle(g.thread);
        g.thread = NULL;
        DeleteCriticalSection(&g.lock);
    }
    if (g.evt) { CloseHandle(g.evt); g.evt = NULL; }
    drop_reader(-1);
    free(g.tmp_rgb); g.tmp_rgb = NULL;
    free(g.latest);  g.latest = NULL;
    g.fmt = FMT_NONE; g.stride = 0; g.has_latest = 0;
}
