# 跨平台 Makefile（第 7 週：本週的範例都是 Python，沒有要編譯的 C 程式）
#   macOS / Linux           : make check / make clean
#   Windows (PowerShell/cmd): mingw32-make check / mingw32-make clean
#   make check : 四支示範程式各跑一次（不需要互動），並檢查 stft_demo 寫出的 spectrogram 列數是不是 701
#   make clean : 刪掉 check 產生的檔案
#
# 變數的名稱（CC、PY、EXE、RM_F、RUN）和第 3 週的 Makefile 一樣，課程的 CI 腳本才能用同一套指令跑每一週。
# ifeq／ifdef 的說明見第 3 週的 Makefile：Windows 一定有 OS=Windows_NT；在 Git Bash／MSYS2 裡另有 MSYSTEM。
CC     = gcc
CFLAGS = -Wall -Wextra -std=c99

ifeq ($(OS),Windows_NT)
    EXE  = .exe
    PY   = python
    ifdef MSYSTEM                 # Git Bash / MSYS2：recipe 由 bash 執行
        RM_F = rm -f
        RUN  = ./
    else                          # mingw32-make 在 PowerShell / cmd 裡：recipe 由 cmd 執行
        RM_F = del /q
        RUN  = .\$(EMPTY)
    endif
else
    EXE  =
    PY   = python3
    RM_F = rm -f
    RUN  = ./
endif

# 測試用的 WAV（五個母音，7.03 秒，16 kHz）與 stft_demo 寫出的檔名
WAV  = ../data/aeueo-16kHz.wav
SPEC = aeueo-16kHz.spec.txt
# 預期的 frame 數：(112541 − 512) / 160 + 1 = 701（整數除法；P = 512、M = 160 是 32 ms／10 ms 在 16 kHz 的 sample 數）
ROWS = 701

# all：本週沒有 C 程式，只印一句話。保留這個目標是為了讓「只打 make」不會出錯。
all:
	@echo "wk07: all examples are Python; run 'make check'"

# check：每支程式跑一次；最後一行用 Python 數 spectrogram 的列數，不是 701 就以非 0 結束（make 會顯示 Error）。
#   這段 Python 寫成一行、只用雙引號，bash 與 cmd 兩種 shell 才都能原樣傳給 python。
check:
	$(PY) frame_demo.py $(WAV) 1.0
	$(PY) stft_demo.py --selftest
	$(PY) stft_demo.py $(WAV) --ascii
	$(PY) window_demo.py
	$(PY) jitter_sim.py
	$(PY) -c "import sys; n = sum(1 for _ in open('$(SPEC)')); print('spec rows =', n); sys.exit(n != $(ROWS))"

clean:
	-$(RM_F) $(SPEC) speech_osr_16k.spec.txt

.PHONY: all check clean
