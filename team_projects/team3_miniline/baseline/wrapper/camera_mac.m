/*============================================================================
 *  camera_mac.m  —  camera.h 的 macOS 實作：AVFoundation 的 AVCaptureSession
 *----------------------------------------------------------------------------
 *  AVFoundation 是 Objective-C 的 API，所以這個檔案是 .m（Objective-C 可以直接混 C）。沒有用 ARC
 *  （-fno-objc-arc），記憶體自己 retain／release，和 C 的 malloc／free 一樣的思考方式。
 *
 *  流程：
 *    1. AVCaptureDeviceDiscoverySession   列出攝影機（內建 ＋ 外接 USB）
 *    2. AVCaptureSession                  輸入 = AVCaptureDeviceInput(攝影機)、輸出 = AVCaptureVideoDataOutput
 *       preset 320x240                     要求 320×240；拿到別的尺寸就用 camera_util.h 裁切縮放
 *    3. 輸出格式 32BGRA                    AVFoundation 幫我們轉成 B G R A 每像素 4 bytes；我們再轉成 RGB
 *    4. delegate 的 captureOutput:...      每一張會在 AVFoundation 自己的執行緒上呼叫；我們把最新一張放進
 *                                          有鎖保護的緩衝區，camera_read 用 pthread 的 condition 等「有新的一張」
 *
 *  權限：第一次用攝影機 macOS 會跳「XXX 想要存取相機」；在「終端機」裡跑的程式問的是終端機的權限，按允許即可。
 *  被拒絕時 camera_open 會失敗並印出提示（系統設定 → 隱私權與安全性 → 相機）。
 *  連結：-framework AVFoundation -framework CoreMedia -framework CoreVideo -framework Foundation
 *===========================================================================*/
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "camera.h"
#include "camera_util.h"

@interface MLCameraDelegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
@end

static struct {
    AVCaptureSession          *session;
    AVCaptureVideoDataOutput  *output;
    MLCameraDelegate          *delegate;
    dispatch_queue_t           queue;
    int                        dst_w, dst_h;
    uint8_t                   *tmp_rgb;      /* 來源尺寸的 RGB（縮放前） */
    size_t                     tmp_cap;
    pthread_mutex_t            lock;
    pthread_cond_t             cond;
    uint8_t                   *latest;       /* dst_w×dst_h×3 */
    int                        has_latest;
    int                        running;
} g;

static NSArray<AVCaptureDevice *> *list_devices(void) {
    NSMutableArray *types = [NSMutableArray arrayWithObject:AVCaptureDeviceTypeBuiltInWideAngleCamera];
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 140000
    if (@available(macOS 14.0, *)) [types addObject:AVCaptureDeviceTypeExternal];
#else
    [types addObject:AVCaptureDeviceTypeExternalUnknown];
#endif
    AVCaptureDeviceDiscoverySession *ds =
        [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:types
                                                               mediaType:AVMediaTypeVideo
                                                                position:AVCaptureDevicePositionUnspecified];
    return ds.devices;
}

int camera_count(void) {
    @autoreleasepool {
        return (int)[list_devices() count];
    }
}

int camera_name(int i, char *buf, size_t cap) {
    if (!buf || cap == 0) return -1;
    buf[0] = '\0';
    @autoreleasepool {
        NSArray<AVCaptureDevice *> *devs = list_devices();
        if (i < 0 || (NSUInteger)i >= [devs count]) return -1;
        const char *name = [[devs objectAtIndex:(NSUInteger)i].localizedName UTF8String];
        snprintf(buf, cap, "%s", name ? name : "camera");
        return 0;
    }
}

@implementation MLCameraDelegate
- (void)captureOutput:(AVCaptureOutput *)output
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
           fromConnection:(AVCaptureConnection *)connection {
    (void)output; (void)connection;
    CVImageBufferRef img = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!img) return;
    if (CVPixelBufferLockBaseAddress(img, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess) return;
    int sw = (int)CVPixelBufferGetWidth(img), sh = (int)CVPixelBufferGetHeight(img);
    size_t stride = CVPixelBufferGetBytesPerRow(img);
    const uint8_t *base = (const uint8_t *)CVPixelBufferGetBaseAddress(img);
    size_t need = (size_t)sw * sh * 3;
    if (need > g.tmp_cap) {                              /* 第一張（或尺寸變了）才配置 */
        uint8_t *p = (uint8_t *)realloc(g.tmp_rgb, need);
        if (!p) { CVPixelBufferUnlockBaseAddress(img, kCVPixelBufferLock_ReadOnly); return; }
        g.tmp_rgb = p; g.tmp_cap = need;
    }
    for (int y = 0; y < sh; y++) {                       /* B G R A → R G B */
        const uint8_t *row = base + (size_t)y * stride;
        uint8_t *out = g.tmp_rgb + (size_t)y * sw * 3;
        for (int x = 0; x < sw; x++) {
            out[x * 3] = row[x * 4 + 2]; out[x * 3 + 1] = row[x * 4 + 1]; out[x * 3 + 2] = row[x * 4];
        }
    }
    CVPixelBufferUnlockBaseAddress(img, kCVPixelBufferLock_ReadOnly);

    pthread_mutex_lock(&g.lock);
    cam_fit_rgb(g.tmp_rgb, sw, sh, sw * 3, g.latest, g.dst_w, g.dst_h);
    g.has_latest = 1;
    pthread_cond_signal(&g.cond);
    pthread_mutex_unlock(&g.lock);
}
@end

int camera_open(int index, int w, int h) {
    if (g.session) camera_close();
    @autoreleasepool {
        NSArray<AVCaptureDevice *> *devs = list_devices();
        if ([devs count] == 0) { fprintf(stderr, "錯誤: 這台電腦沒有攝影機\n"); return -1; }
        if (index < 0 || (NSUInteger)index >= [devs count]) {
            fprintf(stderr, "錯誤: 沒有第 %d 台攝影機（共 %lu 台）\n", index, (unsigned long)[devs count]);
            return -1;
        }
        AVAuthorizationStatus st = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
        if (st == AVAuthorizationStatusDenied || st == AVAuthorizationStatusRestricted) {
            fprintf(stderr, "錯誤: 相機權限被拒絕（系統設定 → 隱私權與安全性 → 相機，允許終端機）\n");
            return -1;
        }
        if (st == AVAuthorizationStatusNotDetermined) {   /* 第一次：跳出詢問視窗，等使用者按 */
            dispatch_semaphore_t sem = dispatch_semaphore_create(0);
            __block BOOL ok = NO;
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL granted) {
                ok = granted; dispatch_semaphore_signal(sem);
            }];
            dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
            dispatch_release(sem);
            if (!ok) { fprintf(stderr, "錯誤: 相機權限被拒絕\n"); return -1; }
        }

        AVCaptureDevice *dev = [devs objectAtIndex:(NSUInteger)index];
        NSError *err = nil;
        AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:dev error:&err];
        if (!input) { fprintf(stderr, "錯誤: 打不開攝影機：%s\n", [[err localizedDescription] UTF8String]); return -1; }

        g.session = [[AVCaptureSession alloc] init];
        if ([g.session canSetSessionPreset:AVCaptureSessionPreset320x240])
            g.session.sessionPreset = AVCaptureSessionPreset320x240;
        else if ([g.session canSetSessionPreset:AVCaptureSessionPreset640x480])
            g.session.sessionPreset = AVCaptureSessionPreset640x480;
        if (![g.session canAddInput:input]) { fprintf(stderr, "錯誤: session 不接受這台攝影機\n"); camera_close(); return -1; }
        [g.session addInput:input];

        g.output = [[AVCaptureVideoDataOutput alloc] init];
        g.output.videoSettings = @{ (id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA) };
        g.output.alwaysDiscardsLateVideoFrames = YES;       /* 我們只要最新的一張 */
        g.delegate = [[MLCameraDelegate alloc] init];
        g.queue = dispatch_queue_create("miniline.camera", DISPATCH_QUEUE_SERIAL);
        [g.output setSampleBufferDelegate:g.delegate queue:g.queue];
        if (![g.session canAddOutput:g.output]) {
            fprintf(stderr, "錯誤: session 不接受 VideoDataOutput\n");
            camera_close();
            return -1;
        }
        [g.session addOutput:g.output];

        g.dst_w = w; g.dst_h = h;
        g.latest = (uint8_t *)malloc((size_t)w * h * 3);
        if (!g.latest) { camera_close(); return -1; }
        g.has_latest = 0;
        pthread_mutex_init(&g.lock, NULL);
        pthread_cond_init(&g.cond, NULL);
        g.running = 1;
        [g.session startRunning];
        fprintf(stderr, "攝影機：%s（32BGRA）→ %dx%d RGB\n", [dev.localizedName UTF8String], w, h);
        return 0;
    }
}

int camera_read(uint8_t *rgb, int timeout_ms) {
    if (!g.session || !rgb) return -1;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct timespec until;
    long long ns = (long long)tv.tv_usec * 1000LL + (long long)(timeout_ms < 0 ? 0 : timeout_ms) * 1000000LL;
    until.tv_sec = tv.tv_sec + (time_t)(ns / 1000000000LL);
    until.tv_nsec = (long)(ns % 1000000000LL);

    pthread_mutex_lock(&g.lock);
    while (!g.has_latest) {
        int rc = (timeout_ms < 0) ? pthread_cond_wait(&g.cond, &g.lock)
                                  : pthread_cond_timedwait(&g.cond, &g.lock, &until);
        if (rc != 0) break;                                   /* 逾時 */
    }
    int got = g.has_latest;
    if (got) { memcpy(rgb, g.latest, (size_t)g.dst_w * g.dst_h * 3); g.has_latest = 0; }
    pthread_mutex_unlock(&g.lock);
    if (!got && ![g.session isRunning]) return -1;            /* 攝影機被拔掉之類 */
    return got ? 1 : 0;
}

void camera_close(void) {
    @autoreleasepool {
        if (g.session) {
            if (g.running) [g.session stopRunning];
            [g.output setSampleBufferDelegate:nil queue:NULL];
            [g.session release];   g.session = NULL;
        }
        if (g.output)   { [g.output release];   g.output = NULL; }
        if (g.delegate) { [g.delegate release]; g.delegate = NULL; }
        if (g.queue)    { dispatch_release(g.queue); g.queue = NULL; }
        if (g.running)  { pthread_mutex_destroy(&g.lock); pthread_cond_destroy(&g.cond); g.running = 0; }
        free(g.latest);  g.latest = NULL;
        free(g.tmp_rgb); g.tmp_rgb = NULL; g.tmp_cap = 0;
        g.has_latest = 0;
    }
}
