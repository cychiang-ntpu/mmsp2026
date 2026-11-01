/*============================================================================
 *  display_mac.m  —  display.h 的 macOS 實作：Cocoa 的 NSWindow ＋ 自己畫的 NSView
 *----------------------------------------------------------------------------
 *  Cocoa 和 Windows 一樣是「事件驅動」：事件排在 NSApplication 的佇列裡，要有人把它們拿出來處理。
 *  一般的 Mac 程式用 [NSApp run] 把整個 main 交給它；我們不想被接管（主迴圈要收網路），所以在 display_poll
 *  裡自己用 nextEventMatchingMask 把排隊的事件全部拿出來、sendEvent 處理掉，然後就回去做自己的事。
 *  畫面：RGB → CGImage（用 CGDataProvider 包我們的緩衝區）→ 在 drawRect 裡 drawInRect。
 *  沒有用 ARC（-fno-objc-arc）：alloc 的東西自己 release。
 *  連結：-framework Cocoa（含 AppKit、Foundation、CoreGraphics）
 *===========================================================================*/
#import <Cocoa/Cocoa.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "display.h"

@interface MLView : NSView
@end

@interface MLWindowDelegate : NSObject <NSWindowDelegate>
@end

static struct {
    NSWindow         *window;
    MLView           *view;
    MLWindowDelegate *delegate;
    int               w, h;
    uint8_t          *rgb;         /* w×h×3，最後一次 display_show 的畫面 */
    int               closed;
} g;

@implementation MLView
- (BOOL)isFlipped { return YES; }                         /* 讓 (0,0) 在左上角，和影像一樣 */
- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    if (!g.rgb) return;
    CGDataProviderRef prov = CGDataProviderCreateWithData(NULL, g.rgb, (size_t)g.w * g.h * 3, NULL);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGImageRef img = CGImageCreate((size_t)g.w, (size_t)g.h, 8, 24, (size_t)g.w * 3, cs,
                                   kCGBitmapByteOrderDefault | kCGImageAlphaNone, prov, NULL, false,
                                   kCGRenderingIntentDefault);
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    /* isFlipped 的 view 裡畫 CGImage 會上下顛倒，先把座標翻回來 */
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, 0, self.bounds.size.height);
    CGContextScaleCTM(ctx, 1, -1);
    CGContextDrawImage(ctx, CGRectMake(0, 0, self.bounds.size.width, self.bounds.size.height), img);
    CGContextRestoreGState(ctx);
    CGImageRelease(img);
    CGColorSpaceRelease(cs);
    CGDataProviderRelease(prov);
}
@end

@implementation MLWindowDelegate
- (BOOL)windowShouldClose:(id)sender { (void)sender; g.closed = 1; return YES; }
@end

int display_open(int w, int h, const char *title) {
    if (g.window) display_close();
    @autoreleasepool {
        if (![NSApplication sharedApplication]) return -1;
        /* 命令列程式預設沒有 Dock 圖示、視窗也不會浮到前面；設成 Regular 並 activate 才會看到 */
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        if (![NSApp isRunning]) [NSApp finishLaunching];

        g.w = w; g.h = h; g.closed = 0;
        g.rgb = (uint8_t *)calloc((size_t)w * h * 3, 1);
        if (!g.rgb) return -1;

        NSRect frame = NSMakeRect(0, 0, w, h);
        NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
        g.window = [[NSWindow alloc] initWithContentRect:frame styleMask:style backing:NSBackingStoreBuffered defer:NO];
        if (!g.window) { fprintf(stderr, "錯誤: 開不了視窗（沒有桌面？SSH？）\n"); free(g.rgb); g.rgb = NULL; return -1; }
        [g.window setTitle:[NSString stringWithUTF8String:title ? title : "miniline"]];
        [g.window setReleasedWhenClosed:NO];                 /* 我們自己 release */
        g.view = [[MLView alloc] initWithFrame:frame];
        [g.window setContentView:g.view];
        g.delegate = [[MLWindowDelegate alloc] init];
        [g.window setDelegate:g.delegate];
        [g.window center];
        [g.window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        display_poll();
        return 0;
    }
}

void display_show(const uint8_t *rgb) {
    if (!g.window || !g.rgb || !rgb) return;
    memcpy(g.rgb, rgb, (size_t)g.w * g.h * 3);
    @autoreleasepool {
        [g.view setNeedsDisplay:YES];
        [g.view displayIfNeeded];                             /* 立刻畫，不等下一次 poll */
    }
}

int display_poll(void) {
    if (!g.window) return 0;
    @autoreleasepool {
        for (;;) {
            NSEvent *ev = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:nil
                                            inMode:NSDefaultRunLoopMode dequeue:YES];
            if (!ev) break;
            [NSApp sendEvent:ev];
        }
        [NSApp updateWindows];
    }
    return g.closed ? 0 : 1;
}

void display_close(void) {
    @autoreleasepool {
        if (g.window) {
            [g.window setDelegate:nil];
            [g.window orderOut:nil];
            [g.window release];   g.window = NULL;
        }
        if (g.view)     { [g.view release];     g.view = NULL; }
        if (g.delegate) { [g.delegate release]; g.delegate = NULL; }
        free(g.rgb); g.rgb = NULL;
        g.closed = 1;
    }
}
