# m1_wav：只編這個資料夾的程式。共用的設定（編譯器、旗標、平台）在 ../common.mk
#   macOS / Linux：make        Windows：mingw32-make（PowerShell 或 Git Bash 都可）
include ../common.mk

all: m1_wav$(EXE)

m1_wav$(EXE): m1_wav.c $(HDRS)
	$(CC) $(CFLAGS) m1_wav.c -o $@ $(LIBS)

clean:
	-$(RM_F) m1_wav m1_wav.exe

.PHONY: all clean
