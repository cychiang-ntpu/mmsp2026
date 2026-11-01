# Team 2 VoiceLink 手把手教學的 Makefile：編 step1 … step7（寫法和 baseline/Makefile 一樣；逐行解說見 docs/tutorials/makefile_intro.md）
#   macOS / Linux            : make            （或 make step3、make check、make clean）
#   Windows (Git Bash)       : mingw32-make    （同上）
#   Windows (PowerShell/cmd) : mingw32-make / mingw32-make step3 / mingw32-make clean（check 請在 Git Bash 跑：它用 sh 的語法）
#
#   make            編 Part A 的八個模組（m1 … m8）與 Part B 的 step1 … step7（Windows 多 .exe）
#   make modules    只編 Part A 的模組；make stepN 只編第 N 步
#   make check      先跑不用互動的模組 m3／m6／m7／m8 並檢查印出的結果，再用 tests/run_mock_tests.py 自動驗收：
#                   step3 跑測試 1,2,5；step4 跑 1,2,3,5,7；step5 跑 1–7；step6 跑 8（μ-law）
#                   預設兩邊都是自己（C send → C recv）；make check-ref 改用 python_ref 當送端（評測就是這樣跑）
#   make clean      刪掉編出來的東西
#
# CFLAGS：-Wall -Wextra 幾乎所有警告都開（規格要求零警告）、-std=c99、-O2。-I../baseline/wrapper 讓 #include "udp.h" 找得到。
# step7 要連結 audio.o（miniaudio，10 萬行，第一次編要 10–20 秒；之後沒改就不會重編）。
CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -O2 -I../baseline/wrapper
WRAPPER = ../baseline/wrapper

ifeq ($(OS),Windows_NT)          # Windows：Winsock（-lws2_32）、timeBeginPeriod（-lwinmm）
    EXE  = .exe
    LIBS = -lws2_32 -lwinmm
    PY   = python
    ifdef MSYSTEM                 # MSYS2 / Git Bash
        RM_F = rm -f
    else                          # PowerShell / cmd 下的 mingw32-make
        RM_F = del /q
    endif
else
    EXE  =
    PY   = python3
    RM_F = rm -f
    ifeq ($(shell uname -s),Darwin)   # macOS：miniaudio 在執行時自己載入 CoreAudio
        LIBS = -lpthread -lm
    else                              # Linux：miniaudio 用 dlopen 載入 ALSA／PulseAudio，要 -ldl
        LIBS = -ldl -lpthread -lm
    endif
endif

HDRS = $(WRAPPER)/udp.h $(WRAPPER)/clock.h $(WRAPPER)/wav.h $(WRAPPER)/audio.h
WAV  = ../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav
PORT = 5604

T3 = 1,2,5
T4 = 1,2,3,5,7
T5 = 1,2,3,4,5,6,7
T6 = 8

STEPS   = step1_raw step2_header step3_buffer step4_counters step5_robust step6_mulaw step7_live
MODULES = m1_wav m2_udp_send m2_udp_recv m3_clock m4_mic_to_wav m5_play_wav m6_header m7_ringbuffer m8_mulaw

all: modules $(addsuffix $(EXE),$(STEPS))

# ---- Part A：八個小模組（m4、m5 用到麥克風／喇叭，要連結 audio.o）----
modules: $(addsuffix $(EXE),$(MODULES))

m4_mic_to_wav$(EXE): m4_mic_to_wav.c audio.o $(HDRS)
	$(CC) $(CFLAGS) m4_mic_to_wav.c audio.o -o $@ $(LIBS)

m5_play_wav$(EXE): m5_play_wav.c audio.o $(HDRS)
	$(CC) $(CFLAGS) m5_play_wav.c audio.o -o $@ $(LIBS)

# 短名字：make step3 = make step3_buffer（.exe）
step1: step1_raw$(EXE)
step2: step2_header$(EXE)
step3: step3_buffer$(EXE)
step4: step4_counters$(EXE)
step5: step5_robust$(EXE)
step6: step6_mulaw$(EXE)
step7: step7_live$(EXE)

# step1–6：單一 .c 檔＋三個 header-only 的 wrapper
%$(EXE): %.c $(HDRS)
	$(CC) $(CFLAGS) $< -o $@ $(LIBS)

# step7 多了 audio.o（麥克風與喇叭）
audio.o: $(WRAPPER)/audio.c $(WRAPPER)/audio.h $(WRAPPER)/miniaudio.h
	$(CC) $(CFLAGS) -c $(WRAPPER)/audio.c -o $@

step7_live$(EXE): step7_live.c audio.o $(HDRS)
	$(CC) $(CFLAGS) step7_live.c audio.o -o $@ $(LIBS)

# ---- 自動驗收（要在 sh 裡跑：Git Bash／macOS／Linux）----
# 每一行：測試腳本的 send／recv 命令、WAV、要跑哪幾個；結果累積到 .check_result 最後印 PASS/FAIL 的總表
define RUN_CHECK
	@echo "=== $(1): tests $(3) ==="; \
	if $(PY) ../tests/run_mock_tests.py "$(2) send" "./$(1)$(EXE) recv" $(WAV) --only $(3) --port $(PORT); \
	then echo "$(1) tests $(3): PASS" >> .check_result; else echo "$(1) tests $(3): FAIL" >> .check_result; fi
endef

# 模組的檢查：m3 跑完印最大誤差；m6 要印 OK；m7 的 8 拍裡要剛好一個 REPEAT；m8 的 SQNR 要 ≥ 30（結束碼）
# （recipe 裡只用 ASCII：Windows 的 make 會把中文與 ^ 弄壞）
check-modules: m3_clock$(EXE) m6_header$(EXE) m7_ringbuffer$(EXE) m8_mulaw$(EXE)
	@echo "=== m3_clock ==="; if ./m3_clock$(EXE) | tail -2; then echo "m3_clock: PASS" >> .check_result; \
	else echo "m3_clock: FAIL" >> .check_result; fi
	@echo "=== m6_header ==="; if ./m6_header$(EXE) | grep "OK"; then echo "m6_header: PASS" >> .check_result; \
	else echo "m6_header: FAIL" >> .check_result; fi
	@echo "=== m7_ringbuffer ==="; n=$$(./m7_ringbuffer$(EXE) | grep -c "tick .* REPEAT"); \
	echo "REPEAT ticks: $$n (want 1)"; \
	if [ "$$n" = "1" ]; then echo "m7_ringbuffer: PASS" >> .check_result; \
	else echo "m7_ringbuffer: FAIL" >> .check_result; fi
	@echo "=== m8_mulaw ==="; if ./m8_mulaw$(EXE) $(WAV) m8_out.wav | grep "SQNR"; \
	then echo "m8_mulaw: PASS" >> .check_result; \
	else echo "m8_mulaw: FAIL" >> .check_result; fi

check: step3_buffer$(EXE) step4_counters$(EXE) step5_robust$(EXE) step6_mulaw$(EXE)
	@$(RM_F) .check_result
	@$(MAKE) --no-print-directory check-modules
	$(call RUN_CHECK,step3_buffer,./step3_buffer$(EXE),$(T3))
	$(call RUN_CHECK,step4_counters,./step4_counters$(EXE),$(T4))
	$(call RUN_CHECK,step5_robust,./step5_robust$(EXE),$(T5))
	$(call RUN_CHECK,step6_mulaw,./step6_mulaw$(EXE),$(T6))
	@echo; echo "===== make check summary ====="; cat .check_result; \
	if grep -q FAIL .check_result; then echo "RESULT: SOME FAIL"; exit 1; else echo "RESULT: ALL PASS"; fi

# 同上，但送端換成 python_ref（評測的對手端）；收端是我們的程式
check-ref: step3_buffer$(EXE) step4_counters$(EXE) step5_robust$(EXE) step6_mulaw$(EXE)
	@$(RM_F) .check_result
	$(call RUN_CHECK,step3_buffer,$(PY) ../python_ref/voicelink.py,$(T3))
	$(call RUN_CHECK,step4_counters,$(PY) ../python_ref/voicelink.py,$(T4))
	$(call RUN_CHECK,step5_robust,$(PY) ../python_ref/voicelink.py,$(T5))
	$(call RUN_CHECK,step6_mulaw,$(PY) ../python_ref/voicelink.py,$(T6))
	@echo; echo "===== make check-ref summary ====="; cat .check_result; \
	if grep -q FAIL .check_result; then echo "RESULT: SOME FAIL"; exit 1; else echo "RESULT: ALL PASS"; fi

clean:
	-$(RM_F) $(STEPS) $(addsuffix .exe,$(STEPS)) $(MODULES) $(addsuffix .exe,$(MODULES)) audio.o .check_result m8_out.wav

.PHONY: all modules step1 step2 step3 step4 step5 step6 step7 check check-modules check-ref clean
