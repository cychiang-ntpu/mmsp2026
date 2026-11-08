# m6_header：只編這個資料夾的程式。共用的設定（編譯器、旗標、平台）在 ../common.mk
#   macOS / Linux：make        Windows：mingw32-make（PowerShell 或 Git Bash 都可）
include ../common.mk

all: m6_header$(EXE)

m6_header$(EXE): m6_header.c $(HDRS)
	$(CC) $(CFLAGS) m6_header.c -o $@ $(LIBS)

clean:
	-$(RM_F) m6_header m6_header.exe

.PHONY: all clean
