# Team 2 baseline/：這裡只有 wrapper（標頭＋audio.c）與兩支 Python 工具，沒有可執行的程式。
#   make        把 wrapper/audio.c 編成 audio.o（確認 wrapper 在這台機器零警告；tutorial 的各資料夾自己會再編）
#   make clean  刪掉 audio.o
# 完整程式在 ../tutorial/step6_live/，壓縮框架在 ../mulaw_system/ 與 ../adpcm_huffman_system/；它們的 Makefile 都 -I 到這裡的 wrapper。
CC     = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
ifeq ($(OS),Windows_NT)
    ifdef MSYSTEM
        RM_F = rm -f
    else
        RM_F = del /q
    endif
else
    RM_F = rm -f
endif

all: audio.o

audio.o: wrapper/audio.c wrapper/audio.h wrapper/miniaudio.h
	$(CC) $(CFLAGS) -c wrapper/audio.c -o $@

clean:
	-$(RM_F) audio.o

.PHONY: all clean
