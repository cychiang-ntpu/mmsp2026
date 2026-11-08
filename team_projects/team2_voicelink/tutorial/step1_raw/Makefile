# step1_raw：只編這個資料夾的程式。共用的設定（編譯器、旗標、平台）在 ../common.mk
#   macOS / Linux：make        Windows：mingw32-make（PowerShell 或 Git Bash 都可）
include ../common.mk

all: step1_raw$(EXE)

step1_raw$(EXE): step1_raw.c $(HDRS)
	$(CC) $(CFLAGS) step1_raw.c -o $@ $(LIBS)

clean:
	-$(RM_F) step1_raw step1_raw.exe

.PHONY: all clean
