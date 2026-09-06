
# 1. 變數定義

CC = gcc                    # 編譯器
CFLAGS = -Wall -std=c99 -lm # 編譯選項: 警告全開, C99標準, 連結數學函式庫 (-lm 必須)
RM = rm -rf                 # 移除指令

# 執行檔名稱
ENCODER_EXE = encoder.exe
DECODER_EXE = decoder.exe
DIFF_EXE = diff.exe

# 檔案名稱
IN_FN = input.txt
OUT_FN = output.txt
CB_FN = codebook.csv
ENC_FN = encoded.bin


# 2. 主要流程目標 (all 必須是第一個目標)
#    執行 'make' 或 'make all' 即可完成所有步驟

all: clean build step1 step2 step3

# 3. Build Code: 編譯所有程式 (Step 4 的 Build code 部分)

build: build_encoder build_decoder build_diff
	@echo -e "\n=== 程式編譯完成 (Build Code) ===\n"

build_encoder: encoder.c
	@echo "編譯 $<..."
	$(CC) encoder.c -o $(ENCODER_EXE) $(CFLAGS)

build_decoder: decoder.c
	@echo "編譯 $<..."
	$(CC) decoder.c -o $(DECODER_EXE) $(CFLAGS)

build_diff: diff.c
	@echo "編譯 $<..."
	$(CC) diff.c -o $(DIFF_EXE) $(CFLAGS)

# 4. 執行 Steps 1-3

# Step 1: 執行 encoder (產生 codebook.csv 和 encoded.bin)
step1: $(ENCODER_EXE)
	@echo -e "\n================= 3.1 Step 1: 執行 ENCODER ================="
	./$(ENCODER_EXE) $(IN_FN) $(CB_FN) $(ENC_FN)

# Step 2: 執行 decoder (還原 output.txt)
step2: $(DECODER_EXE)
	@echo -e "\n================= 3.2 Step 2: 執行 DECODER ================="
	./$(DECODER_EXE) $(OUT_FN) $(CB_FN) $(ENC_FN)

# Step 3: 執行 diff 檢查檔案內容是否一致
# 加上 '-' (減號) 避免 diff 回傳非零值時中斷 make 流程
step3: $(DIFF_EXE)
	@echo -e "\n================= 3.3 Step 3: 執行 DIFF 檢查 ================="
	-./$(DIFF_EXE) $(IN_FN) $(OUT_FN)
	@if [ $$? -eq 0 ]; then \
		echo "✅ 檢查結果: $(IN_FN) 和 $(OUT_FN) 內容完全一致。"; \
	else \
		echo "❌ 檢查結果: $(IN_FN) 和 $(OUT_FN) 內容不一致。請檢查編/解碼邏輯。"; \
	fi
	@echo -e "\n============================================================"


# 5. Clean 目標 (清理生成的檔案)

clean:
	@echo -e "\n================= 清除產生的檔案 ================="
	$(RM) $(ENCODER_EXE) $(DECODER_EXE) $(DIFF_EXE) $(OUT_FN) $(CB_FN) $(ENC_FN)
	@echo "清理完成。"

# 6. .PHONY: 偽目標定義
#宣告所有非實際產生的目標，避免與同名檔案衝突

.PHONY: all build build_encoder build_decoder build_diff clean step1 step2 step3