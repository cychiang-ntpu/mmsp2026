# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra

# Source files
ENCODER_SRC = mini_prj_3_encoder.c
DECODER_SRC = mini_prj_3_decoder.c

# Output executables
ENCODER = encoder.exe
DECODER = decoder.exe

# Test files
INPUT = input.txt
CODEBOOK = codebook.csv
ENCODED = encoded.bin
OUTPUT = output.txt

# Default target
all: build test

# Build both encoder and decoder
build: $(ENCODER) $(DECODER)

# Build encoder
$(ENCODER): $(ENCODER_SRC)
	$(CC) $(CFLAGS) -o $@ $<

# Build decoder
$(DECODER): $(DECODER_SRC)
	$(CC) $(CFLAGS) -o $@ $<

# Run test steps
test: step1 step2 step3

# Step 1: Run encoder
step1: $(ENCODER) $(INPUT)
	./$(ENCODER) $(INPUT) $(CODEBOOK) $(ENCODED)
	@echo "Step 1: Encoding completed"

# Step 2: Run decoder
step2: $(DECODER) $(CODEBOOK) $(ENCODED)
	./$(DECODER) $(OUTPUT) $(CODEBOOK) $(ENCODED)
	@echo "Step 2: Decoding completed"

# Step 3: Compare input and output
step3: $(INPUT) $(OUTPUT)
	@echo "Step 3: Comparing input and output files"
	diff $(INPUT) $(OUTPUT)

# Download input file if not exists
$(INPUT):
	@echo "Downloading input.txt..."
	curl -o $(INPUT) https://sherlock-holm.es/stories/plain-text/cano.txt

# Clean generated files
clean:
	rm -f $(ENCODER) $(DECODER) $(CODEBOOK) $(ENCODED) $(OUTPUT)

.PHONY: all build test step1 step2 step3 clean