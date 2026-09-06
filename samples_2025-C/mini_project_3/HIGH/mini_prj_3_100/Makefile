# Makefile for Encoder and Decoder
CC = gcc
LDFLAGS = -lm       

ENCODER = encoder.exe
DECODER = decoder.exe
ENC_SRC = mini_prj_3_encoder_100.c
DEC_SRC = mini_prj_3_decoder_100.c

# --- file name---
IN_FILE = input.txt
OUT_FILE = output.txt
CODEBOOK = codebook.csv
ENCODED_FILE = encoded.bin



# make all test
all: test

# test rule: depends on output.txt, runs diff
test: $(OUT_FILE)
	@echo "--- [ 比較 $(IN_FILE) 和 $(OUT_FILE) ] ---"
	diff $(IN_FILE) $(OUT_FILE)
	@echo "--- [ 比較完成。 如果上面沒有任何輸出，代表兩個檔案一樣 ] ---"

# generate output.txt by running decoder.exe
$(OUT_FILE): $(ENCODED_FILE) $(DECODER)
	@echo "--- [ 執行 Decoder... ] ---"
	./$(DECODER) $(OUT_FILE) $(CODEBOOK) $(ENCODED_FILE)

# compile decoder from decoder.c
$(DECODER): $(DEC_SRC)
	@echo "--- [ 編譯 Decoder ($(DEC_SRC))... ] ---"
	$(CC) $(CFLAGS) -o $(DECODER) $(DEC_SRC) $(LDFLAGS)

# generate enc.bin by running encoder.exe
$(ENCODED_FILE): $(CODEBOOK)

# generate codebook.csv by running encoder.exe
$(CODEBOOK): $(ENCODER) $(IN_FILE)
	@echo "--- [ 執行 Encoder... ] ---"
	@echo "!!! [ codebook EOF 在最下方 ] !!!"
	./$(ENCODER) $(IN_FILE) $(CODEBOOK) $(ENCODED_FILE)

# compile encoder from encoder.c
$(ENCODER): $(ENC_SRC)
	@echo "--- [ 編譯 Encoder ($(ENC_SRC))... ] ---"
	$(CC) $(CFLAGS) -o $(ENCODER) $(ENC_SRC) $(LDFLAGS)

# make clean: remove generated files
clean:
	@echo "--- [ 清除檔案... ] ---"
	rm -f $(ENCODER) $(DECODER) $(OUT_FILE) $(CODEBOOK) $(ENCODED_FILE)

.PHONY: all test clean
