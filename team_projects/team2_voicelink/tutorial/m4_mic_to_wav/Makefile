# m4_mic_to_wav：只編這個資料夾的程式。共用的設定（編譯器、旗標、平台）在 ../common.mk
#   macOS / Linux：make        Windows：mingw32-make（PowerShell 或 Git Bash 都可）
include ../common.mk

all: m4_mic_to_wav$(EXE)

m4_mic_to_wav$(EXE): m4_mic_to_wav.c $(AUDIO_O) $(HDRS)
	$(CC) $(CFLAGS) m4_mic_to_wav.c $(AUDIO_O) -o $@ $(LIBS)

$(AUDIO_O): $(WRAPPER)/audio.c $(WRAPPER)/audio.h $(WRAPPER)/miniaudio.h
	$(CC) $(CFLAGS) -c $(WRAPPER)/audio.c -o $@

clean:
	-$(RM_F) m4_mic_to_wav m4_mic_to_wav.exe

.PHONY: all clean
