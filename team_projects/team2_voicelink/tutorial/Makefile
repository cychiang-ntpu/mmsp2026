# Team 2 VoiceLink 手把手教學的總 Makefile：每個模組／步驟各自一個資料夾（裡面有自己的 README.md 與 Makefile）
#   macOS / Linux            : make            （或 make step3、make modules、make check、make clean）
#   Windows (Git Bash)       : mingw32-make    （同上）
#   Windows (PowerShell/cmd) : mingw32-make / mingw32-make step3 / mingw32-make clean（check 請在 Git Bash 跑：它用 sh 的語法）
#
#   make            進每個資料夾編（m1_wav … m8_mulaw、step1_raw … step7_live）
#   make modules    只編 Part A 的八個模組；make stepN 只編第 N 步；也可以直接 cd 進資料夾自己 make
#   make check      先跑不用互動的模組 m3／m6／m7／m8 並檢查結果，再用 tests/run_mock_tests.py 自動驗收：
#                   step3 跑測試 1,2,5；step4 跑 1,2,3,5,7；step5 跑 1–7；step6 跑 8（μ-law）
#                   預設兩邊都是自己（C send → C recv）；make check-ref 改用 python_ref 當送端（評測就是這樣跑）
#   make clean      刪掉編出來的東西
PY = python3
ifeq ($(OS),Windows_NT)
    EXE = .exe
    PY  = python
    ifdef MSYSTEM
        RM_F = rm -f
    else
        RM_F = del /q
    endif
else
    EXE  =
    RM_F = rm -f
endif

MODULES = m1_wav m2_udp m3_clock m4_mic_to_wav m5_play_wav m6_header m7_ringbuffer m8_mulaw
STEPS   = step1_raw step2_header step3_buffer step4_counters step5_robust step6_mulaw step7_live
WAV  = ../../../lectures/wk07_1019_audio-pcm/data/aeueo-16kHz.wav
PORT = 5604
T3 = 1,2,5
T4 = 1,2,3,5,7
T5 = 1,2,3,4,5,6,7
T6 = 8

all: $(MODULES) $(STEPS)
modules: $(MODULES)

# 每個資料夾一個 phony target：進去 make
$(MODULES) $(STEPS):
	@$(MAKE) --no-print-directory -C $@

step1: step1_raw
step2: step2_header
step3: step3_buffer
step4: step4_counters
step5: step5_robust
step6: step6_mulaw
step7: step7_live

# ---- 自動驗收（要在 sh 裡跑：Git Bash／macOS／Linux）----
define RUN_CHECK
	@echo "=== $(1): tests $(3) ==="; \
	if $(PY) ../tests/run_mock_tests.py "$(2) send" "$(1)/$(1)$(EXE) recv" $(WAV) --only $(3) --port $(PORT); \
	then echo "$(1) tests $(3): PASS" >> .check_result; else echo "$(1) tests $(3): FAIL" >> .check_result; fi
endef

check-modules: m3_clock m6_header m7_ringbuffer m8_mulaw
	@echo "=== m3_clock ==="; if m3_clock/m3_clock$(EXE) | tail -2; then echo "m3_clock: PASS" >> .check_result; \
	else echo "m3_clock: FAIL" >> .check_result; fi
	@echo "=== m6_header ==="; if m6_header/m6_header$(EXE) | grep "OK"; then echo "m6_header: PASS" >> .check_result; \
	else echo "m6_header: FAIL" >> .check_result; fi
	@echo "=== m7_ringbuffer ==="; n=$$(m7_ringbuffer/m7_ringbuffer$(EXE) | grep -c "tick .* REPEAT"); \
	echo "REPEAT ticks: $$n (want 1)"; \
	if [ "$$n" = "1" ]; then echo "m7_ringbuffer: PASS" >> .check_result; \
	else echo "m7_ringbuffer: FAIL" >> .check_result; fi
	@echo "=== m8_mulaw ==="; if m8_mulaw/m8_mulaw$(EXE) $(WAV) m8_out.wav | grep "SQNR"; \
	then echo "m8_mulaw: PASS" >> .check_result; \
	else echo "m8_mulaw: FAIL" >> .check_result; fi

check: step3_buffer step4_counters step5_robust step6_mulaw
	@$(RM_F) .check_result
	@$(MAKE) --no-print-directory check-modules
	$(call RUN_CHECK,step3_buffer,step3_buffer/step3_buffer$(EXE),$(T3))
	$(call RUN_CHECK,step4_counters,step4_counters/step4_counters$(EXE),$(T4))
	$(call RUN_CHECK,step5_robust,step5_robust/step5_robust$(EXE),$(T5))
	$(call RUN_CHECK,step6_mulaw,step6_mulaw/step6_mulaw$(EXE),$(T6))
	@echo; echo "===== make check summary ====="; cat .check_result; \
	if grep -q FAIL .check_result; then echo "RESULT: SOME FAIL"; exit 1; else echo "RESULT: ALL PASS"; fi

check-ref: step3_buffer step4_counters step5_robust step6_mulaw
	@$(RM_F) .check_result
	$(call RUN_CHECK,step3_buffer,$(PY) ../python_ref/voicelink.py,$(T3))
	$(call RUN_CHECK,step4_counters,$(PY) ../python_ref/voicelink.py,$(T4))
	$(call RUN_CHECK,step5_robust,$(PY) ../python_ref/voicelink.py,$(T5))
	$(call RUN_CHECK,step6_mulaw,$(PY) ../python_ref/voicelink.py,$(T6))
	@echo; echo "===== make check-ref summary ====="; cat .check_result; \
	if grep -q FAIL .check_result; then echo "RESULT: SOME FAIL"; exit 1; else echo "RESULT: ALL PASS"; fi

clean:
	@for d in $(MODULES) $(STEPS); do $(MAKE) --no-print-directory -C $$d clean; done
	-$(RM_F) audio.o .check_result m8_out.wav

.PHONY: all modules $(MODULES) $(STEPS) step1 step2 step3 step4 step5 step6 step7 check check-modules check-ref clean
