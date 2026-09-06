#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SYMB 3000   

unsigned char byteBuffer = 0; // byte buffer for output (8 bits)
int bitInBuff = 0; //number of bits in buffer

typedef struct {
    unsigned char chr[4]; // symbol (bytes)
    int useLen;           // symbol length (1~4)
} DecodedSymb;

DecodedSymb code_map[MAX_SYMB] = {0};
// ----- string "0000101" to integer 5 -----
static int str_to_int(const char *bin) {
    int val = 0;
    // reverse encoder int_to_bin
    for (int i = 0; i < 7; i++) {
        if (bin[i] == '\0') break; 
    }
    return val;
}

// ----- parse symbol from csv line -----
// get symbol (bytes and length) from line ("b",1,0.0588235,0000010) -> DecodedSymb 
static void parse_symbol(const char *line, DecodedSymb *sym) {
    int len = 0;
    // special char
    if (strncmp(line, "\"\\r\",", 5) == 0) {
        sym->chr[0] = '\r';
        sym->useLen = 1;
        return;
    }
    if (strncmp(line, "\"\\n\",", 5) == 0) {
        sym->chr[0] = '\n';
        sym->useLen = 1;
        return;
    }
    if (strncmp(line, "\"\\t\",", 5) == 0) {
        sym->chr[0] = '\t';
        sym->useLen = 1;
        return;
    }

    // normal char
    if (line[0] == '"') {
        int i = 1; // start after first "
        while (line[i] != '\0' && len < 4) {
            if (line[i] == '"') {
                if (line[i+1] == '"') {
                    //  character "
                    sym->chr[len++] = '"';
                    i += 2; // skip escaped "
                } else {
                    break;
                }
            } else {
                //read symbol char till next "
                sym->chr[len++] = line[i++]; // store normal char and count symbol use length
            }
        }
    }
    sym->useLen = len;
}

// ----- read 7 bits from file and return index -----
static int read_code(FILE *fin) {
    int result = 0;
    for (int i = 0; i < 7; i++) { // read 7 bits
        // no bits in buffer
        if (bitInBuff == 0) { 
            int c = fgetc(fin); // get next 8-bit
            if (c == EOF) {
                return -1; // file end
            }
            byteBuffer = (unsigned char)c;
            bitInBuff = 8;
        }
        // bitBuffer has bits
        // construct result bit by bit
        薙ｽ#ｽ°迢蜃迢晢郢&繧掩繧?%*吶鯉ｽ@鯉~#蟶ｽ吶ｱｽ吶ｱｱ%@吶ｽ&郢郢蟶雋=〒ｩ##*郢?
        
        bitInBuff--; // decrease bits in buffer
    }
    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s out_fn cb_fn enc_fn\n", argv[0]);
        return 1;
    }

    // open files
    FILE *fout = fopen(argv[1], "wb");
    if (fout == NULL) { perror(argv[1]); return 1; }
    FILE *fcsv = fopen(argv[2], "r");
    if (fcsv == NULL) { perror(argv[2]); return 1; }
    FILE *fin = fopen(argv[3], "rb");
    if (fin == NULL) { perror(argv[3]); return 1; }

    // read codebook and build symbol mapping 
    int eof_index = -1; // "EOF" symbol code index
    char line[256]; // buffer for reading lines from codebook

    // read each line from codebook
    while (fgets(line, 256, fcsv)) {
        // find last "," get code string
        char *code_string = strrchr(line, ',');
        if (code_string == NULL) continue; // error format
        code_string++; // skip ","
        
        // change code ("0000101") to index (5) 
        int code_index = str_to_int(code_string);

        // if EOF symbol
        if (strncmp(line, "\"EOF\"", 5) == 0) {
            eof_index = code_index; // save EOF code index
        } else {
            // parse symbol and save to table
            parse_symbol(line, &code_map[code_index]);
        }
    }
    fclose(fcsv); 
    // cannot find EOF 
    if (eof_index == -1) {
        fprintf(stderr, "can't find EOF in '%s' \n", argv[2]);
        fclose(fin);
        fclose(fout);
        return 1;
    }

    // ------------------------- decode loop -------------------------
    //run until read EOF code
    while (1) {
        int code_index = read_code(fin); // read 7 bits code index

        // check unexpected EOF
        if (code_index == -1) {
            fprintf(stderr, "file error。\n");
            break;
        }
        // check EOF code
        if (code_index == eof_index) {
            break; 
        }
        // mapping (e.g. "map[8] = A")
        DecodedSymb sym = code_map[code_index]; // get symbol from table
        fwrite(sym.chr, 1, sym.useLen, fout); // write a symbol to output 
    }

    fclose(fin);
    fclose(fout); 
    return 0; 
    
}