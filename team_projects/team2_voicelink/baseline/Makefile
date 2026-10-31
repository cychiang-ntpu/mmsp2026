# Team 2 VoiceLink baseline 的跨平台 Makefile（寫法和 Team 1 starter 一樣；逐行解說見 docs/tutorials/makefile_intro.md）
#   macOS / Linux            : make / make smoke / make clean
#   Windows (Git Bash)       : mingw32-make / mingw32-make smoke / mingw32-make clean
#   Windows (PowerShell/cmd) : mingw32-make / mingw32-make clean（smoke 請在 Git Bash 跑：它要把 recv 放到背景）
#
#   make        產生 voicelink_raw 與 chat（Windows 多 .exe）
#   make smoke  列裝置 + 在 127.0.0.1 上 send → recv 一個 WAV，用 smoke_check.py 確認 PCM 逐 byte 相同
#
# CFLAGS：-Wall -Wextra 幾乎所有警告都開（規格要求零警告）、-std=c99、-O2 最佳化。
# wrapper/audio.c 裡的 miniaudio 在這組旗標下也是零警告（audio.c 用 MA_NO_* 關掉了用不到、會誤報警告的部分），
# 所以不需要任何額外的 -Wno-*。第一次編 audio.c 要 10–20 秒（miniaudio 有 10 萬行），之後沒改它就不會重編。
CC     = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2

ifeq ($(OS),Windows_NT)          # Windows：Winsock（-lws2_32）、timeBeginPeriod（-lwinmm）；WASAPI 由 miniaudio 在執行時自己載入
    EXE  = .exe
    LIBS = -lws2_32 -lwinmm
    PY   = python
    ifdef MSYSTEM                 # MSYS2 / Git Bash 終端機：有 rm、執行要加 ./
        RM_F = rm -f
        RUN  = ./
    else                          # PowerShell / cmd 下的 mingw32-make：recipe 由 cmd 執行
        RM_F = del /q
        RUN  = .\$(EMPTY)
    endif
else
    EXE  =
    PY   = python3
    RM_F = rm -f
    RUN  = ./
    ifeq ($(shell uname -s),Darwin)   # macOS：miniaudio 在執行時自己載入 CoreAudio，不必 -framework
        LIBS = -lpthread -lm
    else                              # Linux：miniaudio 用 dlopen 載入 ALSA／PulseAudio，要 -ldl
        LIBS = -ldl -lpthread -lm
    endif
endif

WRAPPER_HDR = wrapper/wav.h wrapper/udp.h wrapper/clock.h wrapper/audio.h
SMOKE_WAV   = ../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav
SMOKE_PORT  = 5204

all: voicelink_raw$(EXE) chat$(EXE)

# audio.c 先編成 .o（它很慢，而且和 voicelink_raw.c 無關），再和 voicelink_raw.c 一起連結
audio.o: wrapper/audio.c wrapper/audio.h wrapper/miniaudio.h
	$(CC) $(CFLAGS) -c wrapper/audio.c -o $@

voicelink_raw$(EXE): voicelink_raw.c audio.o $(WRAPPER_HDR)
	$(CC) $(CFLAGS) voicelink_raw.c audio.o -o $@ $(LIBS)

chat$(EXE): chat.c
	$(CC) $(CFLAGS) chat.c -o $@ $(LIBS)

# smoke：recv 放到背景聽 5204，send 一個 WAV 過去，recv 在 2 秒沒封包後自己結束、寫出 smoke_out.wav，再用 Python 比對。
# 一行裡用 ; 和 & 串起來，所以要在 sh（Git Bash／macOS／Linux）裡跑；PowerShell 下的 mingw32-make 不支援。
smoke: voicelink_raw$(EXE)
	$(RUN)voicelink_raw$(EXE) devices
	$(RUN)voicelink_raw$(EXE) recv $(SMOKE_PORT) smoke_out.wav & \
	sleep 1; \
	$(RUN)voicelink_raw$(EXE) send 127.0.0.1 $(SMOKE_PORT) $(SMOKE_WAV); \
	wait
	PYTHONIOENCODING=utf-8 $(PY) smoke_check.py $(SMOKE_WAV) smoke_out.wav

clean:
	-$(RM_F) voicelink_raw voicelink_raw.exe chat chat.exe audio.o smoke_out.wav

.PHONY: all smoke clean
