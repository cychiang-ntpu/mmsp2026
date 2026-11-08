# m8_mulaw：只編這個資料夾的程式。共用的設定（編譯器、旗標、平台）在 ../common.mk
#   macOS / Linux：make        Windows：mingw32-make（PowerShell 或 Git Bash 都可）
include ../common.mk

all: m8_mulaw$(EXE)

m8_mulaw$(EXE): m8_mulaw.c $(HDRS)
	$(CC) $(CFLAGS) m8_mulaw.c -o $@ $(LIBS)

clean:
	-$(RM_F) m8_mulaw m8_mulaw.exe

.PHONY: all clean
