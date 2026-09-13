# 跨平台 Makefile
#   macOS / Linux           : make / make demo / make clean
#   Windows (PowerShell/cmd): mingw32-make / mingw32-make demo / mingw32-make clean
#   Windows (MSYS2 UCRT64 終端機): 同上用 mingw32-make；要先 pacman -S diffutils，且 Windows 的
#       python 要在 PATH 上（MSYS2 預設不繼承 Windows PATH，可在 Windows 環境變數設 MSYS2_PATH_TYPE=inherit）
#   make check : C 版與 Python 版 utf8_dump 輸出逐 byte 比對（沒有差異就是一致）
CC     = gcc
CFLAGS = -Wall -Wextra -std=c99

ifeq ($(OS),Windows_NT)
    EXE  = .exe
    LIBS = -lws2_32
    PY   = python
    ifdef MSYSTEM                 # 在 MSYS2 / Git Bash 裡：有 rm、執行要加 ./
        RM_F = rm -f
        RUN  = ./
        DIFF = diff
    else                          # mingw32-make 在 PowerShell / cmd 裡：recipe 由 cmd 執行，
        RM_F = del /q             # 用 del；執行檔加 .\ 前綴（$(EMPTY) 是為了避免行尾反斜線被當成續行）
        RUN  = .\$(EMPTY)
        DIFF = fc
    endif
else
    EXE  =
    LIBS =
    PY   = python3
    RM_F = rm -f
    RUN  = ./
    DIFF = diff
endif

all: utf8_dump$(EXE) sticky_send$(EXE)

utf8_dump$(EXE): utf8_dump.c
	$(CC) $(CFLAGS) utf8_dump.c -o $@

sticky_send$(EXE): sticky_send.c
	$(CC) $(CFLAGS) sticky_send.c -o $@ $(LIBS)

demo: utf8_dump$(EXE)
	$(RUN)utf8_dump$(EXE) < ../data/sample_zh_en.txt

check: utf8_dump$(EXE)
	$(RUN)utf8_dump$(EXE) < ../data/sample_zh_en.txt > c_dump.txt
	$(PY) utf8_dump.py ../data/sample_zh_en.txt > py_dump.txt
	$(DIFF) c_dump.txt py_dump.txt

clean:
	-$(RM_F) utf8_dump utf8_dump.exe sticky_send sticky_send.exe c_dump.txt py_dump.txt

.PHONY: all demo check clean
