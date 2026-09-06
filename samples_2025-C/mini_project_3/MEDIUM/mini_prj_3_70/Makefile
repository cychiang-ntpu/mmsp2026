CC = gcc
CFLAGS = -O2 -std=c11

all: encoder decoder

encoder: mini_prj_3_encoder_xxxxxxxx.c
	$(CC) $(CFLAGS) -o encoder mini_prj_3_encoder_xxxxxxxx.c

decoder: mini_prj_3_decoder_xxxxxxxx.c
	$(CC) $(CFLAGS) -o decoder mini_prj_3_decoder_xxxxxxxx.c

clean:
	rm -f encoder decoder *.o encoded.bin output.txt codebook.csv
