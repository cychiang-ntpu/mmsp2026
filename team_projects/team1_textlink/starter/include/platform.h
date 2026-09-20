/*============================================================================
 *  platform.h  —  跨平台前置處理（從 baseline/chat.c 第 1 節搬出來）
 *----------------------------------------------------------------------------
 *  Windows 用 Winsock，macOS／Linux 用 BSD socket；兩者的差異用巨集抹平。
 *  【每個 .c 檔都要把這個標頭放在第一個 #include】，因為 POSIX 那邊要在
 *  任何系統標頭之前定義 _POSIX_C_SOURCE，clock_gettime 等函式才會出現。
 *===========================================================================*/
#ifndef TL_PLATFORM_H
#define TL_PLATFORM_H

#ifdef _WIN32
  /* ---- Windows 專用 ---- */
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600        /* 需要 Vista 以上的 API (inet_pton) */
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #include <process.h>                 /* _beginthreadex */
  #ifdef _MSC_VER
    #pragma comment(lib, "ws2_32.lib")
  #endif

  typedef SOCKET socket_t;
  #define CLOSESOCK(s)  closesocket(s)
  #define SOCK_INVALID  INVALID_SOCKET

  typedef HANDLE thread_t;
  static inline int thread_create(thread_t *t, unsigned (__stdcall *fn)(void *), void *arg) {
      *t = (HANDLE)_beginthreadex(NULL, 0, fn, arg, 0, NULL);
      return (*t == NULL) ? -1 : 0;
  }
  #define THREAD_FN(name) unsigned __stdcall name(void *arg)
  #define THREAD_RETURN   return 0
#else
  /* ---- macOS / Linux (POSIX) 專用 ---- */
  #ifndef _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 200809L
  #endif
  #if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
    #define _DARWIN_C_SOURCE           /* macOS：定義了 _POSIX_C_SOURCE 之後，要加這個，網路標頭才不會把部分定義藏起來 */
  #endif
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/select.h>
  #include <sys/stat.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>              /* TCP_NODELAY */
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  #include <signal.h>
  #include <time.h>
  #include <pthread.h>

  typedef int socket_t;
  #define CLOSESOCK(s)  close(s)
  #define SOCK_INVALID  (-1)

  typedef pthread_t thread_t;
  static inline int thread_create(thread_t *t, void *(*fn)(void *), void *arg) {
      return pthread_create(t, NULL, fn, arg);
  }
  #define THREAD_FN(name) void *name(void *arg)
  #define THREAD_RETURN   return NULL
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#endif /* TL_PLATFORM_H */
