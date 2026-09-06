encode_file = codebook.csv encoded.bin
CC = gcc
CFLAGS = -Wall -O2 -std=c99 -lm
#編譯程式碼
all: build_encoder build_decoder
build_encoder:
$(CC) $(CFLAGS) mini_prj_3_encoder_80.c -o encoder.exe
build_decoder:
$(CC) $(CFLAGS) mini_prj_3_decoder_80.c -o decoder.exe
#編碼
encode:
./encoder.exe input.txt $(encode_file)
#解碼
decode: $(encode_file)
./decoder.exe output.txt $(encode_file)
#比較輸入輸出
compare: input.txt output.txt
cmp -l input.txt output.txt
demo:
./encoder.exe input.txt $(encode_file)
./decoder.exe output.txt $(encode_file)
cmp input.txt output.txt
echo "Test passed!!"
#刪除生成檔案
clean:
rm -rf $(encode_file) output.txt encoder.exe decoder.exe
.PHONY: build clean encode decode compare demo