# step2_header：只編這個資料夾的程式。共用的設定（編譯器、旗標、平台）在 ../common.mk
#   macOS / Linux：make        Windows：mingw32-make（PowerShell 或 Git Bash 都可）
include ../common.mk

all: step2_header$(EXE)

step2_header$(EXE): step2_header.c $(HDRS)
	$(CC) $(CFLAGS) step2_header.c -o $@ $(LIBS)

clean:
	-$(RM_F) step2_header step2_header.exe

.PHONY: all clean
