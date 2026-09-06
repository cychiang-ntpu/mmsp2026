#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

// Constants for the decoder
#define MAX_SYMBOLS 65536      // Allow many Unicode symbols
#define MAX_CODE_LEN 32     // Maximum length of a binary code for a symbol
#define MAX_LINE_LEN 256    // Maximum length of a line in the codebook CSV file

// Structure to store a single entry from the codebook
typedef struct {
    char symbol_utf8[5];           // UTF-8 bytes for the symbol (1..4 bytes, + NUL)
    int symbol_len;                // number of bytes in symbol_utf8
    char codeword[MAX_CODE_LEN];   // The binary code representing this character
} CodeEntry;

// Structure to store the complete codebook
typedef struct {
    CodeEntry entries[MAX_SYMBOLS]; // Array of symbol-to-code mappings
    int num_entries;               // Number of entries in the codebook
} Codebook;

// Parse a single line from the codebook CSV file
// Format of each line: "symbol",count,probability,"codeword"
// We only need the symbol and codeword fields (first and last quoted fields)
// (old parser removed) — read_codebook below handles CSV parsing and UTF-8 symbols

void read_codebook(FILE* fp, Codebook* cb) {
    char line[MAX_LINE_LEN];
    cb->num_entries = 0;



    while (fgets(line, sizeof(line), fp) && cb->num_entries < MAX_SYMBOLS) {
        char *start, *end;
        CodeEntry *entry = &cb->entries[cb->num_entries];
        char symbol_str[64] = {0};

        // --- find first quoted field (symbol) ---
        start = strchr(line, '"');
        if (!start) continue;

        // handle escaped double-quote ("")
        if (*(start + 1) == '"' && *(start + 2) == '"') {
            strcpy(symbol_str, "\"\"");
            end = start + 3;  // skip past the "" pair
        } 
        else {
            end = strchr(start + 1, '"');
            if (!end) continue;
            strncpy(symbol_str, start + 1, end - start - 1);
            symbol_str[end - start - 1] = '\0';
        }

        // --- find last quoted field (codeword) ---
        start = strrchr(line, '"');
        if (!start || start == end) continue;

        // move backward to find opening quote of codeword
        end = start;
        start--;
        while (start > line && *start != '"') start--;
        if (*start != '"') continue;

    char codeword_str[MAX_CODE_LEN] = {0};
        strncpy(codeword_str, start + 1, end - start - 1);

        // --- handle escaped symbol ---
        // Interpret symbol_str: special escapes first
        if (strcmp(symbol_str, "\\n") == 0) {
            entry->symbol_utf8[0] = '\n'; entry->symbol_len = 1;
        } else if (strcmp(symbol_str, "\\r") == 0) {
            entry->symbol_utf8[0] = '\r'; entry->symbol_len = 1;
        } else if (strcmp(symbol_str, "\"\"") == 0) {
            entry->symbol_utf8[0] = '"'; entry->symbol_len = 1;
        } else if (strncmp(symbol_str, "\\x", 2) == 0) {
            // hex byte value
            long v = strtol(symbol_str + 2, NULL, 16);
            entry->symbol_utf8[0] = (char)v; entry->symbol_len = 1;
        } else {
            // Assume raw UTF-8 bytes inside symbol_str
            size_t sl = strlen(symbol_str);
            if (sl > 4) sl = 4;
            memcpy(entry->symbol_utf8, symbol_str, sl);
            entry->symbol_utf8[sl] = '\0';
            entry->symbol_len = (int)sl;
        }

        strcpy(entry->codeword, codeword_str);


        cb->num_entries++;
    }
}




// Look up the codeword in the codebook and return the entry index, or -1 if not found
int find_symbol_index(const char* codeword, Codebook* cb) {
    for (int i = 0; i < cb->num_entries; i++) {
        if (strcmp(codeword, cb->entries[i].codeword) == 0) return i;
    }
    return -1;
}

// Decode the binary file back into text using the codebook
// Decode payload from in_fp using codebook cb and write decoded UTF-8 bytes to out_fp.
// total_bits indicates how many bits in the payload are valid (header-driven).
void decode_file(FILE* in_fp, FILE* out_fp, Codebook* cb, uint32_t total_bits) {
    char current_code[MAX_CODE_LEN] = "";  // Buffer to build up binary codes
    int code_len = 0;                      // Length of current code

    uint32_t bits_consumed = 0;
    int c;

    // Read payload bytes after header and process exactly total_bits bits
    while (bits_consumed < total_bits && (c = fgetc(in_fp)) != EOF) {
        unsigned char byte = (unsigned char)c;
        for (int i = 7; i >= 0; --i) {
            if (bits_consumed >= total_bits) break;
            掩迢雎°雋ｽｻ=暦ｽｹ薙ｽ晢▲ｽ雋鯉蟶+ｻｩ郢迢ｽ%鯉&蟶~ｹ薙縺%雎〒ｽ雋+~蜃+ｱ〒°°▲雎ゅｻ▲迢+蟶ｽ@ｽ
            current_code[code_len] = '\0';

            /* Try to match current code to a symbol */
            int idx = find_symbol_index(current_code, cb);
            if (idx != -1) {
                fwrite(cb->entries[idx].symbol_utf8, 1, cb->entries[idx].symbol_len, out_fp);
                code_len = 0;
                current_code[0] = '\0';
            }

            if (code_len >= MAX_CODE_LEN - 1) {
                fprintf(stderr, "Error: Invalid encoded data\n");
                return;
            }

            bits_consumed++;
        }
    }
}

int main(int argc, char** argv) {
    // Check command line arguments
    if (argc != 4) {
        fprintf(stderr, "Usage: %s out_fn cb_fn enc_fn\n", argv[0]);
        fprintf(stderr, "  out_fn: output text file (decoded result)\n");
        fprintf(stderr, "  cb_fn: input codebook file (CSV format)\n");
        fprintf(stderr, "  enc_fn: input encoded binary file\n");
        return 1;
    }
    
    // Open output file for writing decoded text
    FILE* out_fp = fopen(argv[1], "w");
    if (!out_fp) {
        fprintf(stderr, "Error opening output file: %s\n", argv[1]);
        return 1;
    }
    
    // Open codebook file for reading
    FILE* cb_fp = fopen(argv[2], "r");
    if (!cb_fp) {
        fprintf(stderr, "Error opening codebook file: %s\n", argv[2]);
        fclose(out_fp);
        return 1;
    }
    
    // Open encoded file for reading in binary mode
    FILE* enc_fp = fopen(argv[3], "rb");
    if (!enc_fp) {
        fprintf(stderr, "Error opening encoded file: %s\n", argv[3]);
        fclose(out_fp);
        fclose(cb_fp);
        return 1;
    }
    
    Codebook cb;
    read_codebook(cb_fp, &cb);

    // Read 4-byte big-endian header from encoded file to get total bits
    unsigned char hdr[4];
    if (fread(hdr, 1, 4, enc_fp) != 4) {
        fprintf(stderr, "Error: encoded file too short or missing header\n");
        fclose(out_fp); fclose(cb_fp); fclose(enc_fp);
        return 1;
    }
    *ｽ#郢=ｱゅ?掩掩=蜃~▲ゅ雋雎晢繧ｹ吶雋?=*驫#ｽ?=#薙蜃鯉?鯉%郢ｱ繧=晢雎?ｽ#+驫ｻ薙*暦=*&*驫晢暦ｽ#吶ｩ=

    decode_file(enc_fp, out_fp, &cb, total_bits);
    
    fclose(out_fp);
    fclose(cb_fp);
    fclose(enc_fp);
    
    return 0;
}