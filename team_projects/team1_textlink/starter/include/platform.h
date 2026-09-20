/*============================================================================
 *  platform.h  —  跨平台前置處理（從 baseline/chat.c 第 1 節搬出來）
 *----------------------------------------------------------------------------
 *  Windows 用 Winsock，macOS／Linux 用 BSD socket；兩者的差異用巨集抹平。
 *  【每個 .c 檔都要把這個標頭放在第一個 #include】，因為 POSIX 那邊要在
 *  任何系統標頭之前定義 _POSIX_C_SOURCE，clock_gettime 等函式才會出現。
 *
 *  ── 這個檔案的地圖 ─────────────────────────────────────────────────────────
 *  為什麼需要它：同一件事（開 socket、關 socket、開一條執行緒），Windows 和 macOS／Linux
 *  的函式名稱、型別都不一樣。這裡替每件事取一個「共用的名字」，其他 .c 檔只用共用的名字，
 *  同一份程式就能在三種作業系統上編譯。
 *
 *  怎麼做到的：#ifdef _WIN32 ... #else ... #endif 是「條件編譯」。_WIN32 是 Windows 上的
 *  編譯器自動定義的巨集；編譯器開始編譯之前，前置處理器只留下其中一邊，另一邊等於不存在。
 *  所以在 Windows 上編譯時完全不會看到 pthread，在 Mac 上也完全不會看到 winsock2.h。
 *  （POSIX 是 macOS、Linux 這類系統共同遵守的一套標準，所以這兩者可以寫在同一邊。）
 *
 *  這個檔案提供的共用名字：
 *    socket_t          socket 的型別（Windows 是 SOCKET、POSIX 是 int）
 *    SOCK_INVALID      「開 socket 失敗」的那個特殊值
 *    CLOSESOCK(s)      關掉 socket
 *    thread_t          執行緒的型別
 *    thread_create()   開一條新的執行緒，成功回傳 0
 *    THREAD_FN(name)   宣告「要給執行緒跑的函式」；THREAD_RETURN 是它結尾要寫的 return
 *
 *  可以跳過：各個 #include 的系統標頭不用背；需要知道某個函式來自哪裡時再回來查就好。
 *===========================================================================*/
#ifndef TL_PLATFORM_H                  /* include guard：同一個 .c 重複 #include 這個檔時，內容只會出現一次 */
#define TL_PLATFORM_H

#ifdef _WIN32
  /* ---- Windows 專用 ---- */
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600        /* 需要 Vista 以上的 API (inet_pton) */
  #endif
  /* winsock2.h 一定要排在 windows.h 前面：反過來的話 windows.h 會先帶進舊版的 winsock.h，兩者定義互相衝突 */
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #include <process.h>                 /* _beginthreadex */
  #ifdef _MSC_VER
    /* 只有微軟的編譯器（MSVC）認得這一行：要求連結 Winsock 函式庫。用 gcc（MinGW）時由 Makefile 的 -lws2_32 負責 */
    #pragma comment(lib, "ws2_32.lib")
  #endif

  /* typedef：替既有的型別取一個新名字。之後程式裡一律寫 socket_t，不必管底下到底是 SOCKET 還是 int */
  typedef SOCKET socket_t;
  #define CLOSESOCK(s)  closesocket(s)
  #define SOCK_INVALID  INVALID_SOCKET

  /* 執行緒（thread）：同一支程式裡「同時在跑」的另一條執行路線。聊天程式需要兩條：
   * 一條等鍵盤、一條等網路；只有一條的話，等鍵盤的時候就收不到對方的訊息（見 src/chat.c）。
   *
   * thread_create(t, fn, arg)：開一條新的執行緒去執行 fn(arg)。
   *   t    輸出：新執行緒的代號（用指標傳進來，函式才能把結果寫回呼叫端的變數）
   *   fn   函式指標：「要執行哪一個函式」。Windows 規定這個函式的長相是 unsigned __stdcall f(void *)
   *   arg  要交給 fn 的參數；void * 是「不指定型別的指標」，什麼資料的位址都能傳
   *   回傳 0 成功、-1 失敗。呼叫者：src/chat.c 的 chat_run。
   * static inline：函式本體直接寫在標頭檔裡時要這樣宣告，每個 .c 各有一份，連結時才不會「重複定義」。 */
  typedef HANDLE thread_t;
  static inline int thread_create(thread_t *t, unsigned (__stdcall *fn)(void *), void *arg) {
      *t = (HANDLE)_beginthreadex(NULL, 0, fn, arg, 0, NULL);
      return (*t == NULL) ? -1 : 0;
  }
  /* 兩個平台對「執行緒函式」要求的回傳型別不同（這裡是 unsigned，POSIX 是 void *），所以用巨集包起來：
   * THREAD_FN(recv_thread) { ...; THREAD_RETURN; } 在兩邊會各自展開成正確的寫法。 */
  #define THREAD_FN(name) unsigned __stdcall name(void *arg)
  #define THREAD_RETURN   return 0
#else
  /* ---- macOS / Linux (POSIX) 專用 ---- */
  /* Makefile 用 -std=c99（嚴格的標準 C 模式）編譯，這時系統標頭預設只露出「標準 C」的函式；定義這個巨集等於告訴它：
   * 我還要 POSIX 2008 版的函式（clock_gettime、nanosleep 等）。必須在第一個系統標頭之前定義才有效。 */
  #ifndef _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 200809L
  #endif
  #if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
    #define _DARWIN_C_SOURCE           /* macOS：定義了 _POSIX_C_SOURCE 之後，要加這個，網路標頭才不會把部分定義藏起來 */
  #endif
  #include <sys/types.h>
  #include <sys/socket.h>               /* socket、bind、listen、accept、connect、send、recv */
  #include <sys/select.h>               /* select：等 socket 有動靜，可以設逾時 */
  #include <sys/stat.h>                 /* stat、mkdir */
  #include <netinet/in.h>               /* struct sockaddr_in、htons */
  #include <netinet/tcp.h>              /* TCP_NODELAY */
  #include <arpa/inet.h>                /* inet_pton、inet_ntop：IP 的文字 ↔ 二進位 */
  #include <unistd.h>                   /* close */
  #include <fcntl.h>                    /* fcntl：把 socket 切成非阻塞模式 */
  #include <errno.h>
  #include <signal.h>                   /* SIGPIPE */
  #include <time.h>                     /* clock_gettime、nanosleep */
  #include <pthread.h>                  /* POSIX 的執行緒與互斥鎖 */

  /* POSIX 的 socket 就是一個整數編號（file descriptor，和開檔案拿到的編號是同一種東西），所以用 close 關，-1 代表失敗 */
  typedef int socket_t;
  #define CLOSESOCK(s)  close(s)
  #define SOCK_INVALID  (-1)

  /* 與上面 Windows 版同名、同用途；差別只有 fn 的長相是 void *f(void *)。回傳 0 成功、非 0 失敗。 */
  typedef pthread_t thread_t;
  static inline int thread_create(thread_t *t, void *(*fn)(void *), void *arg) {
      return pthread_create(t, NULL, fn, arg);
  }
  #define THREAD_FN(name) void *name(void *arg)
  #define THREAD_RETURN   return NULL
#endif

/* 兩個平台都要用的標準 C 標頭：uint8_t／uint64_t 等固定寬度整數、printf、malloc／free、memcpy／strlen */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#endif /* TL_PLATFORM_H */
