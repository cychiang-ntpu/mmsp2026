# m3_clock：只編這個資料夾的程式。共用的設定（編譯器、旗標、平台）在 ../common.mk
#   macOS / Linux：make        Windows：mingw32-make（PowerShell 或 Git Bash 都可）
include ../common.mk

all: m3_clock$(EXE)

m3_clock$(EXE): m3_clock.c $(HDRS)
	$(CC) $(CFLAGS) m3_clock.c -o $@ $(LIBS)

clean:
	-$(RM_F) m3_clock m3_clock.exe

.PHONY: all clean
