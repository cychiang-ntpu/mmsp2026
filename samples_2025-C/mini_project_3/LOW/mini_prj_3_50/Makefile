CC = gcc
CFLAGS = -O2 -std=c11 -Wall -Wextra

ENC = mini_prj_3_encoder_50.c
DEC = mini_prj_3_decoder_50.c

all: encoder.exe decoder.exe

encoder.exe: $(ENC)
	$(CC) $(CFLAGS) -o $@ $(ENC)

decoder.exe: $(DEC)
	$(CC) $(CFLAGS) -o $@ $(DEC)


step1: encoder.exe
	./encoder.exe input.txt codebook.csv encoded.bin


step2: decoder.exe
	./decoder.exe output.txt codebook.csv encoded.bin


step3:
	@echo "diff input.txt output.txt :"
	@diff input.txt output.txt || true


run: all step1 step2 step3

clean:
	rm -f encoder.exe decoder.exe encoded.bin codebook.csv output.txt

.PHONY: all step1 step2 step3 run clean
