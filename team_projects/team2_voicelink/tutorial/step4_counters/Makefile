# step4_counters：只編這個資料夾的程式。共用的設定（編譯器、旗標、平台）在 ../common.mk
#   macOS / Linux：make        Windows：mingw32-make（PowerShell 或 Git Bash 都可）
include ../common.mk

all: step4_counters$(EXE)

step4_counters$(EXE): step4_counters.c $(HDRS)
	$(CC) $(CFLAGS) step4_counters.c -o $@ $(LIBS)

clean:
	-$(RM_F) step4_counters step4_counters.exe

.PHONY: all clean
