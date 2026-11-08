# m5_play_wav：只編這個資料夾的程式。共用的設定（編譯器、旗標、平台）在 ../common.mk
#   macOS / Linux：make        Windows：mingw32-make（PowerShell 或 Git Bash 都可）
include ../common.mk

all: m5_play_wav$(EXE)

m5_play_wav$(EXE): m5_play_wav.c $(AUDIO_O) $(HDRS)
	$(CC) $(CFLAGS) m5_play_wav.c $(AUDIO_O) -o $@ $(LIBS)

$(AUDIO_O): $(WRAPPER)/audio.c $(WRAPPER)/audio.h $(WRAPPER)/miniaudio.h
	$(CC) $(CFLAGS) -c $(WRAPPER)/audio.c -o $@

clean:
	-$(RM_F) m5_play_wav m5_play_wav.exe

.PHONY: all clean
