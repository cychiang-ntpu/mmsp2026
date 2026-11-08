# step5_robust：只編這個資料夾的程式。共用的設定（編譯器、旗標、平台）在 ../common.mk
#   macOS / Linux：make        Windows：mingw32-make（PowerShell 或 Git Bash 都可）
include ../common.mk

all: step5_robust$(EXE)

step5_robust$(EXE): step5_robust.c $(HDRS)
	$(CC) $(CFLAGS) step5_robust.c -o $@ $(LIBS)

clean:
	-$(RM_F) step5_robust step5_robust.exe

.PHONY: all clean
