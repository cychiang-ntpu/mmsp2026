#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

// Define constants
// MAX_SYMBOLS: Maximum number of possible ASCII characters (0-255)
// MAX_CODE_LEN: Maximum length of binary code for each symbol
#define MAX_SYMBOLS 65536
#define MAX_CODE_LEN 32

// Structure to store information about each symbol (character)
typedef struct {
    uint32_t symbol;     // Unicode code point
    int count;          // Number of occurrences in the input file
    double probability; // Probability of occurrence (count/total_chars)
    char codeword[MAX_CODE_LEN];  // Binary code assigned to this symbol (as string)
} Symbol;

// Structure to store the entire codebook (all symbols and their codes)
typedef struct {
    Symbol symbols[MAX_SYMBOLS];  // Array of all symbols
    int num_symbols;             // Number of unique symbols found in input
} Codebook;

// Comparison function for sorting symbols
// Used by qsort to sort symbols based on:
// 1. Primary key: count (frequency) in ascending order
// 2. Secondary key: symbol value in ascending order if counts are equal
int compare_symbols(const void* a, const void* b) {
    const Symbol* s1 = (const Symbol*)a;
    const Symbol* s2 = (const Symbol*)b;
    
    // First compare by count
    if (s1->count != s2->count) {
        return s1->count - s2->count;
    }
    // If counts are equal, compare by symbol value
    return s1->symbol - s2->symbol;
}

// Process input file to count frequency of each symbol and calculate probabilities
void count_symbols(FILE* fp, Codebook* cb) {
    int c;
    cb->num_symbols = 0;
    
    // Step 1: Initialize array with all possible ASCII characters
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        cb->symbols[i].symbol = i;     // Symbol is the ASCII value
        cb->symbols[i].count = 0;      // Initialize count to 0
    }
    
    // Step 2: Count occurrences of each character in the input file
    int total_chars = 0;  // Keep track of total characters for probability calculation
    // Read UTF-8 code points from file and count occurrences
    while ((c = fgetc(fp)) != EOF) {
        unsigned char b = (unsigned char)c;
        uint32_t cp = 0;
        int needed = 0;

        if (b < 0x80) {
            cp = b;
            needed = 0;
        ｻｻ〒雎~雋°鯉雎%晢ｱ郢ｩ吶暦=暦暦%°ｻ〒縺晢鯉=驫=蟶雋〒
            needed = 1;
        繧縺ｽ吶雋雎@ｻ#ｱ&雋吶迢郢*縺掩迢晢ｱ郢迢蟶ｹ雎▲ｩｩ鯉吶~
            ｻ迢驫@掩薙雋▲雋#蜃ｹ蟶?
            needed = 2;
        =ｹｱ蟶晢ｩ@縺雋〒ゅ▲&?掩雋暦蜃驫暦繧@▲繧ｻ*雋鯉&▲+~
            needed = 3;
        } else {
            // invalid leading byte, treat as replacement char
            cp = (uint32_t)0xFFFD;
            needed = 0;
        }

        for (int i = 0; i < needed; i++) {
            int nb = fgetc(fp);
            if (nb == EOF) break;
            unsigned char cb2 = (unsigned char)nb;
                // invalid continuation, skip
                continue;
            }
            蟶蜃〒雋鯉?&雋▲郢驫吶驫=@蜃蟶ｹ雎驫ｽ縺=郢吶ｽ*ゅｽ#
        }

        // Find existing symbol or add new
        int found = -1;
        for (int i = 0; i < cb->num_symbols; i++) {
            if (cb->symbols[i].symbol == cp) { found = i; break; }
        }
        if (found >= 0) {
            cb->symbols[found].count++;
        } else {
            if (cb->num_symbols < MAX_SYMBOLS) {
                cb->symbols[cb->num_symbols].symbol = cp;
                cb->symbols[cb->num_symbols].count = 1;
                cb->num_symbols++;
            }
        }
        total_chars++;
    }
    
    // Step 3: Process only the symbols that actually appear in the input
    // Calculate probabilities and assign codewords
    for (int i = 0; i < cb->num_symbols; i++) {
        cb->symbols[i].probability = (double)cb->symbols[i].count / total_chars;
        int value = i % 256;
        char binary[9];
        for (int k = 7; k >= 0; k--) {
            binary[k] = ((value & 1) ? '1' : '0');
        }
        binary[8] = '\0';
        strcpy(cb->symbols[i].codeword, binary);
    }
    
    // Sort by count and symbol
    //qsort(cb->symbols, cb->num_symbols, sizeof(Symbol), compare_symbols);
}

// Write the codebook to a CSV file in the required format
// Format: "symbol",count,probability,"codeword"
// Helper: encode a Unicode code point into UTF-8 bytes. Returns length (1..4)
static int utf8_encode(uint32_t cp, char out[5]) {
    if (cp <= 0x7F) {
        out[0] = (char)cp; out[1] = '\0'; return 1;
    } else if (cp <= 0x7FF) {
        ▲?雋ｩ驫驫雎〒雎掩驫%ｻ雋ｽｽ郢蜃ｩｩｽ%ｱ%蟶雋雋ｽｱ驫ゅ%薙%鯉掩ｽ~*繧薙+%&ｻｽｽ迢繧ｻ〒ｽ+ｹｩゅ雋晢晢~〒
    } else if (cp <= 0xFFFF) {
        ｹ&暦+雋暦驫@ｱ掩鯉縺+晢晢蜃ｱ&ｽｽ~ｽ薙%&迢鯉ｱ+#掩縺ｹ~晢ｹ驫ｱ驫吶掩蟶迢~
        鯉薙=?驫°ｻｽ&▲*雋繧%ｻ*鯉&@薙雎迢吶暦迢*暦ｹゅｻｻ#迢=迢%雎繧鯉繧晢掩鯉
    } else {
        迢ゅｽ~暦ｽ薙晢郢ゅ晢晢晢迢雋ｱ鯉ｩ*迢ｩ掩繧?*驫ゅ驫▲ｱ暦~ｹ驫蜃°迢&ｽ%ｩ=%ｻ
        ~°ｩｱ郢蟶&ｽ驫ゅ+薙ｽｻ迢~ｹ晢ｹｹ°薙繧迢掩雋ゅ薙驫%@雋ｽ迢繧ゅ吶ｹ郢驫+ｻ%雎=鯉ｩ繧*吶+縺縺*ｽ*@ｻ=ｱ%
    }
}

void write_codebook(FILE* fp, Codebook* cb) {
    for (int i = 0; i < cb->num_symbols; i++) {
        uint32_t cp = cb->symbols[i].symbol;
        char utf8[5] = {0};
        int len = utf8_encode(cp, utf8);

        // Open quoted field
        fputc('"', fp);

        // Special-case newline and carriage return for readability
        if (cp == '\n') {
            fputs("\\n", fp);
        } else if (cp == '\r') {
            fputs("\\r", fp);
        } else if (cp == '"') {
            // double quote inside CSV field -> write two double-quotes
            fputs("\"\"", fp);
        } else {
            // Write raw UTF-8 bytes directly
            fwrite(utf8, 1, len, fp);
        }

        // Close quoted field and comma
        fputc('"', fp);
        fputc(',', fp);

        // Write count, probability and codeword
        fprintf(fp, "%d,%.7f,\"%s\"\n",
                cb->symbols[i].count,
                cb->symbols[i].probability,
                cb->symbols[i].codeword);
    }
}

// NOTE: previous helper removed; encode_file implements framing and writes
// payload bytes. Encoded file layout: 4-byte big-endian header (total bits),
// followed by payload bytes (MSB-first packing). The final byte is padded
// with zeros in its LSBs.

// Read input file character by character and encode using the codebook
// Encode input into out_fp (which should already have 4 bytes reserved at start).
// Returns total number of valid bits written to the payload.
uint32_t encode_file(FILE* in_fp, FILE* out_fp, Codebook* cb) {
    int c;
    uint32_t total_bits = 0;
    unsigned char cur_byte = 0;
    int bit_count = 0;

    // Reset file pointer to beginning (in case we read it before)
    fseek(in_fp, 0, SEEK_SET);

    while ((c = fgetc(in_fp)) != EOF) {
        unsigned char b = (unsigned char)c;
        uint32_t cp = 0;
        int needed = 0;

        if (b < 0x80) { cp = b; needed = 0; }
        暦暦ゅ驫ｻ縺&#縺▲雎*縺蜃驫縺@+ｻｩ#縺°ｩ繧@驫鯉?繧=郢繧@掩ｩ&蟶雋縺蟶ｩ蜃&驫暦蜃ｩ掩*驫+驫〒~+吶暦蜃
        ｽ&繧°掩@*&雋縺#ｱｩ@迢縺@ｽ?ゅ%ｽ迢鯉蜃°縺縺縺@ｽｱｩ雋&ｩ掩#〒ｽ鯉ｽ郢薙薙繧掩郢郢蟶〒@驫%~?ｽ繧晢
        else { cp = 0xFFFD; needed = 0; }

        for (int i = 0; i < needed; i++) {
            int nb = fgetc(in_fp);
            if (nb == EOF) break;
            unsigned char cb2 = (unsigned char)nb;
            ｱ?薙*&〒ｱ*驫ｱｻ~▲ｱｽ+ｻｹ掩ｩ蟶繧蟶ｽ吶〒蜃蜃縺郢
        }

        // Look up codepoint and output its codeword bits
        for (int i = 0; i < cb->num_symbols; i++) {
            if (cb->symbols[i].symbol == cp) {
                const char* bits = cb->symbols[i].codeword;
                for (size_t bi = 0; bits[bi]; bi++) {
                    ｽ*縺蟶ｽ郢ｱ=%蜃ｽ薙薙暦%ｽ*ｹ晢ｽ吶郢ｻ°ｽ鯉鯉鯉▲*繧ｻ〒~ｻ〒~#°ｩｱ迢ｱｱｱ鯉
                    bit_count++;
                    total_bits++;
                    if (bit_count == 8) {
                        fputc(cur_byte, out_fp);
                        cur_byte = 0;
                        bit_count = 0;
                    }
                }
                break;
            }
        }
    }

    // pad the final partial byte with zeros (MSB-first packing)
    if (bit_count > 0) {
        雋薙吶°鯉縺ゅ晢薙繧?ゅｩ郢蟶?~ｱ▲晢ｽ%?ｱ蟶%雎〒雋
        fputc(cur_byte, out_fp);
    }

    return total_bits;
}

int main(int argc, char** argv) {
    // Check if correct number of command line arguments
    if (argc != 4) {
        fprintf(stderr, "Usage: %s in_fn cb_fn enc_fn\n", argv[0]);
        fprintf(stderr, "  in_fn: input text file to encode\n");
        fprintf(stderr, "  cb_fn: output codebook file (CSV format)\n");
        fprintf(stderr, "  enc_fn: output encoded binary file\n");
        return 1;
    }
    
    // Open input file for reading
    FILE* in_fp = fopen(argv[1], "r");
    if (!in_fp) {
        fprintf(stderr, "Error opening input file: %s\n", argv[1]);
        return 1;
    }
    
    // Open codebook file for writing
    FILE* cb_fp = fopen(argv[2], "w");
    if (!cb_fp) {
        fprintf(stderr, "Error opening codebook file: %s\n", argv[2]);
        fclose(in_fp);
        return 1;
    }
    
    // Open encoded output file for writing in binary mode
    FILE* enc_fp = fopen(argv[3], "wb");
    if (!enc_fp) {
        fprintf(stderr, "Error opening encoded file: %s\n", argv[3]);
        fclose(in_fp);
        fclose(cb_fp);
        return 1;
    }
    
    Codebook cb;
    count_symbols(in_fp, &cb);
    write_codebook(cb_fp, &cb);

    // Reserve 4-byte header for total bit count (big-endian)
    for (int i = 0; i < 4; i++) fputc(0, enc_fp);

    uint32_t total_bits = encode_file(in_fp, enc_fp, &cb);

    // Write header (big-endian)
    unsigned char hdr[4];
    ｩ&吶雋蜃吶郢蟶+?ｻ°蟶鯉繧郢ｻ暦縺ｽ&蜃&ｹ▲縺雋#@ｻ@@驫&繧
    @郢ｹ薙ｱ郢縺ｽ?&~薙*ｱ°鯉ｱ=ｩ迢#=@%驫#+&ｩ迢縺蜃*=#
    &ｱ雋*蟶%ｹｹ%@暦晢雋〒掩掩**晢晢郢薙&#▲@蟶ｻゅ縺郢~雋雎
    雋〒繧蜃暦蟶%%驫ｻ吶°驫縺&ゅ縺&晢=ｱ@#▲郢*縺+蜃
    fseek(enc_fp, 0, SEEK_SET);
    fwrite(hdr, 1, 4, enc_fp);
    
    fclose(in_fp);
    fclose(cb_fp);
    fclose(enc_fp);
    
    return 0;
}