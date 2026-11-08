# mulaw_system/：這個資料夾自成一體（不依賴 tutorial/common.mk），wrapper 在 ../baseline/wrapper
#   macOS / Linux：make / make clean        Windows：mingw32-make / mingw32-make clean
#   make 產生 voicelink（Windows 多 .exe）；第一次會把 ../baseline/wrapper/audio.c（miniaudio）編成 audio.o，10–20 秒
CC      = gcc
WRAPPER = ../baseline/wrapper
CFLAGS  = -Wall -Wextra -std=c99 -O2 -I$(WRAPPER)
HDRS    = $(WRAPPER)/udp.h $(WRAPPER)/clock.h $(WRAPPER)/wav.h $(WRAPPER)/audio.h

ifeq ($(OS),Windows_NT)
    EXE  = .exe
    LIBS = -lws2_32 -lwinmm
    ifdef MSYSTEM
        RM_F = rm -f
    else
        RM_F = del /q
    endif
else
    EXE  =
    RM_F = rm -f
    ifeq ($(shell uname -s),Darwin)
        LIBS = -lpthread -lm
    else
        LIBS = -ldl -lpthread -lm
    endif
endif

all: voicelink$(EXE)

voicelink$(EXE): voicelink.c audio.o $(HDRS)
	$(CC) $(CFLAGS) voicelink.c audio.o -o $@ $(LIBS)

audio.o: $(WRAPPER)/audio.c $(WRAPPER)/audio.h $(WRAPPER)/miniaudio.h
	$(CC) $(CFLAGS) -c $(WRAPPER)/audio.c -o $@

clean:
	-$(RM_F) voicelink voicelink.exe audio.o

.PHONY: all clean
