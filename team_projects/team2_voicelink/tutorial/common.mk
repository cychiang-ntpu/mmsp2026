# 每個模組／步驟資料夾的 Makefile 都 include 這一份：編譯器、旗標、平台差異、wrapper 的位置
# （逐行解說見 docs/tutorials/makefile_intro.md；寫法和 baseline/Makefile 一樣）
CC      = gcc
WRAPPER = ../../baseline/wrapper
CFLAGS  = -Wall -Wextra -std=c99 -O2 -I$(WRAPPER)
HDRS    = $(WRAPPER)/udp.h $(WRAPPER)/clock.h $(WRAPPER)/wav.h $(WRAPPER)/audio.h
AUDIO_O = ../audio.o            # miniaudio 只編一次，放在 tutorial/ 共用（m4、m5、step7 會用到）

ifeq ($(OS),Windows_NT)          # Windows：Winsock（-lws2_32）、timeBeginPeriod（-lwinmm）
    EXE  = .exe
    LIBS = -lws2_32 -lwinmm
    PY   = python
    ifdef MSYSTEM                 # MSYS2 / Git Bash
        RM_F = rm -f
    else                          # PowerShell / cmd 下的 mingw32-make
        RM_F = del /q
    endif
else
    EXE  =
    PY   = python3
    RM_F = rm -f
    ifeq ($(shell uname -s),Darwin)   # macOS：miniaudio 在執行時自己載入 CoreAudio
        LIBS = -lpthread -lm
    else                              # Linux：miniaudio 用 dlopen 載入 ALSA／PulseAudio，要 -ldl
        LIBS = -ldl -lpthread -lm
    endif
endif
