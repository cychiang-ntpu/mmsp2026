# TextLink starter 的跨平台 Makefile（逐行解說見 docs/tutorials/makefile_intro.md）
#   macOS / Linux            : make / make test / make clean
#   Windows (PowerShell/cmd) : mingw32-make / mingw32-make test / mingw32-make clean
#
#   make       產生 textlink（Windows 為 textlink.exe）
#   make test  編譯並執行 tests/test_codec：frame 標頭、UTF-8、Huffman 的離線測試
CC     = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -Iinclude

ifeq ($(OS),Windows_NT)          # Windows：連結 Winsock
    EXE  = .exe
    LIBS = -lws2_32
    ifdef MSYSTEM                 # MSYS2 / Git Bash 終端機：有 rm、執行要加 ./
        RM_F = rm -f
        RUN  = ./
    else                          # PowerShell / cmd 下的 mingw32-make：recipe 由 cmd 執行
        RM_F = del /q
        RUN  = .\$(EMPTY)
    endif
else                              # macOS / Linux：連結 pthread
    EXE  =
    LIBS = -lpthread
    RM_F = rm -f
    RUN  = ./
endif

HDR   = include/textlink.h include/platform.h
CODEC = src/frame.c src/utf8.c src/huffman.c          # 有 place holder 的三個檔
SHELL_SRC = src/main.c src/net.c src/chat.c src/transfer.c

all: textlink$(EXE)

textlink$(EXE): $(SHELL_SRC) $(CODEC) $(HDR)
	$(CC) $(CFLAGS) $(SHELL_SRC) $(CODEC) -o $@ $(LIBS)

# 測試程式只需要 codec 三個檔與 net.c（tl_strerror 等），不開 socket
test_codec$(EXE): tests/test_codec.c src/net.c $(CODEC) $(HDR)
	$(CC) $(CFLAGS) tests/test_codec.c src/net.c $(CODEC) -o $@ $(LIBS)

test: test_codec$(EXE)
	$(RUN)test_codec$(EXE)

clean:
	-$(RM_F) textlink textlink.exe test_codec test_codec.exe

.PHONY: all test clean
