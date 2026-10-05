# 跨平台 Makefile（寫法與第 3 週 examples/Makefile 相同，逐行說明見那一份）
#   macOS / Linux           : make / make trace / make check / make clean
#   Windows (PowerShell/cmd): mingw32-make / mingw32-make trace / mingw32-make check / mingw32-make clean
#   make trace : huffman_build 跑講義 1.3／1.6 的 8 色影像（兩個佇列的版本，L 應該仍是 2.0938）
#   make check : 「C 實作、Python 對答案」——對同一個文字檔：
#                  Python 的 MP4 參考實作產生 codebook.csv 與 encoded.bin → codebook_check.py 算出 N、K、H、L、bits
#                  C 的 huffman_build --file --eof 自己建樹算出同一組數字
#                兩邊逐 byte 相同才通過。平手規則不同、code 不同沒關係：N、K、H、L、總 bits 一定要一樣（講義 2.4）。
CC     = gcc
CFLAGS = -Wall -Wextra -std=c99

ifeq ($(OS),Windows_NT)
    EXE  = .exe
    LIBS = -lshell32
    PY   = python
    ifdef MSYSTEM
        RM_F = rm -f
        RUN  = ./
        DIFF = diff
    else
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

# make check 用的兩個文字檔：第 3 週回家作業的 MISSISSIPPI RIVER，與第 2 週的中英文混合檔
TEXT1 = ../data/mississippi_river.txt
TEXT2 = ../../wk02_0914_text-utf8/data/sample_zh_en.txt
MP4PY = ../../../samples_2025-python/mini_project_4/huffman_codec.py

all: bitio_trace$(EXE) huffman_build$(EXE)

bitio_trace$(EXE): bitio_trace.c
	$(CC) $(CFLAGS) bitio_trace.c -o $@ $(LIBS)

huffman_build$(EXE): huffman_build.c
	$(CC) $(CFLAGS) huffman_build.c -o $@ $(LIBS)

trace: huffman_build$(EXE)
	$(RUN)huffman_build$(EXE) --freq black:100,white:100,yellow:20,blue:20,orange:5,red:5,purple:3,green:3

check: huffman_build$(EXE)
	$(PY) $(MP4PY) encode $(TEXT1) mr_codebook.csv mr_encoded.bin
	$(PY) codebook_check.py --summary mr_codebook.csv mr_encoded.bin > py_mr.txt
	$(RUN)huffman_build$(EXE) --file $(TEXT1) --eof --summary > c_mr.txt
	$(DIFF) c_mr.txt py_mr.txt
	$(PY) $(MP4PY) encode $(TEXT2) zh_codebook.csv zh_encoded.bin
	$(PY) codebook_check.py --summary zh_codebook.csv zh_encoded.bin > py_zh.txt
	$(RUN)huffman_build$(EXE) --file $(TEXT2) --eof --summary > c_zh.txt
	$(DIFF) c_zh.txt py_zh.txt

clean:
	-$(RM_F) bitio_trace bitio_trace.exe huffman_build huffman_build.exe mr_codebook.csv mr_encoded.bin zh_codebook.csv zh_encoded.bin py_mr.txt c_mr.txt py_zh.txt c_zh.txt

.PHONY: all trace check clean
