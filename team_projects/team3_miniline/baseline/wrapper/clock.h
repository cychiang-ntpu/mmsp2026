/*============================================================================
 *  clock.h  —  單調時鐘與「準時的」sleep；header-only
 *----------------------------------------------------------------------------
 *    clock_ms()          從某個固定起點到現在的毫秒數（double）。只拿來相減，不代表幾點幾分。
 *    sleep_ms(n)         睡 n 毫秒（粗略的：作業系統可能多睡 1–15 ms）
 *    sleep_until_ms(t)   睡到 clock_ms() == t 為止，誤差在 0.1 ms 左右——送端「每 20 ms 一包」靠它
 *
 *  為什麼 sleep_ms 不夠準：作業系統的 sleep 只保證「至少睡這麼久」，醒來的時間要看排程器什麼時候輪到你，
 *  Windows 預設每 15.6 ms 才看一次，所以 sleep(20) 常常變成 31 ms。累積 1000 包下來，送端就比接收端慢了好幾秒。
 *  解法：(1) Windows 上呼叫 timeBeginPeriod(1) 把排程器調成每 1 ms 看一次（要連結 -lwinmm）；
 *       (2) 先用 sleep 睡到「差不多」，最後大約 1 ms 改成在迴圈裡一直看時鐘（spin），時間一到立刻出來；
 *       (3) 不要「每包 sleep 20 ms」，而是「第 k 包睡到 t0 + 20k」，誤差才不會累積（見 voicelink_raw.c 的 send）。
 *
 *  和 Team 1 net.c 的 now_ms 一樣：Windows 用 QueryPerformanceCounter，POSIX 用 clock_gettime(CLOCK_MONOTONIC)。
 *  不用 time()（只有秒、會被對時影響）、不用 clock()（量的是 CPU 時間，等待不算）。
 *===========================================================================*/
#ifndef VL_CLOCK_H
#define VL_CLOCK_H

#ifdef _WIN32
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600
  #endif
  #ifndef _WINSOCKAPI_
    #include <winsock2.h>                /* 先 include 它，免得之後 windows.h 帶進舊版 winsock.h 與 udp.h 衝突 */
  #endif
  #include <windows.h>
  #include <mmsystem.h>                  /* timeBeginPeriod；連結 -lwinmm */
#else
  #ifndef _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 200809L
  #endif
  #if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
    #define _DARWIN_C_SOURCE
  #endif
  #include <time.h>                      /* clock_gettime、nanosleep */
#endif

/* 單調時鐘，單位毫秒 */
static inline double clock_ms(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq;                       /* 計數器每秒跳幾下；開機後不會變，查一次就好 */
    static int period_set = 0;
    LARGE_INTEGER c;
    if (!period_set) {
        QueryPerformanceFrequency(&freq);
        timeBeginPeriod(1);                          /* 整支程式只呼叫一次；程式結束時作業系統會自動還原 */
        period_set = 1;
    }
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
#endif
}

/* 粗略地睡 ms 毫秒（ms <= 0 就不睡） */
static inline void sleep_ms(int ms) {
    if (ms <= 0) return;
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

/* 睡到 clock_ms() 到達 t。t 已經過了就立刻回來（這表示你的程式跟不上節奏，可以拿來偵測）。 */
static inline void sleep_until_ms(double t) {
    for (;;) {
        double remain = t - clock_ms();
        if (remain <= 0.0) return;
        if (remain > 2.0) sleep_ms((int)(remain - 1.5));   /* 大段用 sleep，留 1.5 ms 給下面 */
        else break;
    }
    while (clock_ms() < t) { /* 最後不到 2 ms：忙等（spin），準確但吃 CPU，所以只用在最後一小段 */ }
}

#endif /* VL_CLOCK_H */
