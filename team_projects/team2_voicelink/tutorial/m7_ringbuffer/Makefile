# m7_ringbuffer：只編這個資料夾的程式。共用的設定（編譯器、旗標、平台）在 ../common.mk
#   macOS / Linux：make        Windows：mingw32-make（PowerShell 或 Git Bash 都可）
include ../common.mk

all: m7_ringbuffer$(EXE)

m7_ringbuffer$(EXE): m7_ringbuffer.c $(HDRS)
	$(CC) $(CFLAGS) m7_ringbuffer.c -o $@ $(LIBS)

clean:
	-$(RM_F) m7_ringbuffer m7_ringbuffer.exe

.PHONY: all clean
