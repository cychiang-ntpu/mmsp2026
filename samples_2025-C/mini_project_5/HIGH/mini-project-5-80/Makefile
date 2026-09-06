CC=gcc
CFLAGS=-O2 -std=c11 -Wall -Wextra
LDLIBS=-lm

.PHONY: all clean wav txt pdf run

all: run

signal_gen: signal_gen.c
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

spectrogram: spectrogram.c
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

run: signal_gen spectrogram
	bash scripts/run_all.sh

clean:
	rm -f signal_gen spectrogram
	rm -rf out
