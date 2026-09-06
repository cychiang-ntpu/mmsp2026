CC = gcc
CFLAGS = -O2 -Wall -Wextra -std=c11
LDFLAGS = -lm

BIN_DIR = bin
OUT_DIR = out

# 預設目標：建立兩個可執行檔
all: $(BIN_DIR)/signal_gen $(BIN_DIR)/spectrogram

# 建立 bin 和 out 資料夾（如果還沒存在）
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

# 編譯 signal_gen
$(BIN_DIR)/signal_gen: signal_gen.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ signal_gen.c $(LDFLAGS)

# 編譯 spectrogram
$(BIN_DIR)/spectrogram: spectrogram.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ spectrogram.c $(LDFLAGS)

# 清除所有中間產物
clean:
	rm -rf $(BIN_DIR) $(OUT_DIR) *.wav *.txt *.pdf

.PHONY: all clean
