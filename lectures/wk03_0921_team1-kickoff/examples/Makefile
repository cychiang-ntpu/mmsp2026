# 跨平台 Makefile
#   macOS / Linux           : make / make trace / make check / make clean
#   Windows (PowerShell/cmd): mingw32-make / mingw32-make trace / mingw32-make check / mingw32-make clean
#   make trace : 跑 huffman_trace（投影片的 8 色例子）
#   make check : entropy 的 C 版與 Python 版輸出逐 byte 比對（沒有差異就是一致）
CC     = gcc
CFLAGS = -Wall -Wextra -std=c99

ifeq ($(OS),Windows_NT)
    EXE  = .exe
    LIBS = -lshell32
    PY   = python
    ifdef MSYSTEM                 # 在 MSYS2 / Git Bash 裡：有 rm、執行要加 ./
        RM_F = rm -f
        RUN  = ./
        DIFF = diff
    else                          # mingw32-make 在 PowerShell / cmd 裡：recipe 由 cmd 執行
        RM_F = del /q
        RUN  = .\$(EMPTY)
        DIFF = fc
    endif
else
    EXE  =
    LIBS = -lm
    PY   = python3
    RM_F = rm -f
    RUN  = ./
    DIFF = diff
endif

TEXT = ../../wk02_0914_text-utf8/data/sample_zh_en.txt

all: huffman_trace$(EXE) entropy$(EXE)

huffman_trace$(EXE): huffman_trace.c
	$(CC) $(CFLAGS) huffman_trace.c -o $@ $(LIBS)

entropy$(EXE): entropy.c
	$(CC) $(CFLAGS) entropy.c -o $@ $(LIBS)

trace: huffman_trace$(EXE)
	$(RUN)huffman_trace$(EXE) --freq black:100,white:100,yellow:20,blue:20,orange:5,red:5,purple:3,green:3

check: entropy$(EXE)
	$(RUN)entropy$(EXE) $(TEXT) > c_entropy.txt
	$(PY) entropy.py $(TEXT) > py_entropy.txt
	$(DIFF) c_entropy.txt py_entropy.txt

clean:
	-$(RM_F) huffman_trace huffman_trace.exe entropy entropy.exe c_entropy.txt py_entropy.txt out_16bit.wav out_8bit.wav out_4bit.wav tone_3000.wav tone_5000_aliased.wav

.PHONY: all trace check clean
