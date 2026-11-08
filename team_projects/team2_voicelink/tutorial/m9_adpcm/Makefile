# m9_adpcm：只編這個資料夾的程式。共用的設定（編譯器、旗標、平台）在 ../common.mk
#   macOS / Linux：make        Windows：mingw32-make（PowerShell 或 Git Bash 都可）
include ../common.mk

all: m9_adpcm$(EXE)

m9_adpcm$(EXE): m9_adpcm.c $(HDRS)
	$(CC) $(CFLAGS) m9_adpcm.c -o $@ $(LIBS)

clean:
	-$(RM_F) m9_adpcm m9_adpcm.exe

.PHONY: all clean
