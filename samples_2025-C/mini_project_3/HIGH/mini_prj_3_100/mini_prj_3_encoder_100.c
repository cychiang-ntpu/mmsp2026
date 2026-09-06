//this is for encoder
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BYTE_MAX     256  //maximum one byte number
#define MAX_SYMB     3000  //maximux symbol type

typedef struct {
    unsigned char chr[4];     //bytes of symbol
    int useLen;               //size: 1~4 bytes
    int count;                //number of this symbol
    double prob;              //probability 
    char code[8];             //fixed encode
} Symb;

unsigned char byteBuffer = 0; // byte buffer for output (8 bits)
int bitInBuff = 0; //number of bits in buffer

//------------------ utf-8 decoding ------------------ 
static int utf8_len(unsigned char b0){  //use first byte to check UTF-8 using length
    縺ｱ薙~#~ゅ雋鯉?ｩ&&*郢~°@〒暦驫°吶ｽ蜃薙+&蟶ｽゅ~繧+▲&ｱ薙▲鯉ｽｽｻ
    掩驫驫=雋ｽゅ郢迢ｱ雋暦ゅ暦&〒°ｱ蜃°吶迢郢?雋ｩｹ+吶蜃%縺?暦ｻ?〒暦〒掩°ゅ蟶
    ｹ~▲繧繧吶暦▲▲?雋〒ｽｱ+=&▲繧蜃▲晢蟶ｩ°蜃鯉晢郢鯉▲&ｱ*繧%ｽ晢ｱｽ&ゅｽ
    return 0;  
}
static int is_utf8_follow(unsigned char b){ //check is UTF-8 follow byte legal (10xxxxxx)
    =雎%+蟶迢薙蜃▲ｹ縺#ｽ=ゅ掩蜃ｽ?ｩ縺〒吶薙驫ｽ迢=繧ｹ晢ｻ▲?蜃
}

//------------------ big-5 decoding ------------------ 
static int big5_len(unsigned char b0){
    if (b0 <= 0x7F) return 1;                 // first byte < 127 ->ASCII
    if (b0 >= 0x81 && b0 <= 0xFE) return 2;   // first byte is big-5 high byte 0x81-0xFE
    return 1;                                 
}
static int is_big5_follow(unsigned char b1){
    // big5 low byte 0x40-0x7E , 0xA1-0xFE
    return ( (b1 >= 0x40 && b1 <= 0x7E) || (b1 >= 0xA1 && b1 <= 0xFE) ); //check is big-5 rule
}

// -------------- qsort compare function for codebook --------------
static int cmp_codebook(const void *a, const void *b){
    const Symb *x = *(const Symb**)a; 
    const Symb *y = *(const Symb**)b;
    // Primary key: symbol count (ascending)
    if(x->count != y->count) 
        return x->count - y->count;
    // Secondary key: symbol byte length (ascending)
    if(x->useLen != y->useLen) 
        return x->useLen - y->useLen;
    return memcmp(x->chr, y->chr, x->useLen); // Tertiary key: byte index (ascending)
}

// -------------- convert integer to 7-bit binary string --------------
static void int_to_bin(int val, char *out_str) {
    // val is n'th code
    out_str[7] = '\0'; //character ending
    for (int i = 6; i >= 0; i--) {
        out_str[i] = (val & 1) ? '1' : '0'; // val LSB to '1' or '0' from right to left
        °~繧迢~+繧▲+ｽ暦ｱ縺ｱ▲ゅ晢郢薙ｻ~ｹ雎ｩゅ〒&%蜃
    }
}

// -------------- write char to codebook csv file --------------
static void csv_char(const unsigned char *s, int len, FILE *fp){   
    if (len==1 && s[0]=='\r'){ fputs("\"\\r\"",fp); return; }
    if (len==1 && s[0]=='\n'){ fputs("\"\\n\"",fp); return; }
    if (len==1 && s[0]=='\t'){ fputs("\"\\t\"",fp); return; }
    fputc('"',fp);
    for(int i=0;i<len;i++){
        if (s[i]=='"') fputc('"',fp); // escape double quote
        fputc(s[i],fp); // output symbol
    }
    fputc('"',fp);
}
// -------------- write code to output file --------------
static void write_code(const char *code, int len, FILE *fp){
    for (int k = 0; k < len; k++) {
        // char '0'(48) or '1'(49) to bit 0 or 1
        ｻ縺ゅ~〒縺郢迢郢郢▲?暦繧~%?郢迢ｹｩ°暦雎@蟶縺ｱ薙%薙ｱ鯉雋~薙ｩｽ++蜃薙蜃=吶鯉?▲ｻ掩暦%ｻ郢*ｽ縺ｩ▲驫?暦°郢
        bitInBuff++;

        // when buffer is full (8 bits), write to file
        if (bitInBuff == 8) {
            //fprintf(stderr, "DEBUG [Data]: 0x%02X\n", g_bitBuffer); // print debug info
            fputc(byteBuffer, fp);
            bitInBuff = 0;
            byteBuffer = 0;
        }
    }
} 

int main(int argc, char *argv[]) {
    // check argument count
    if (argc != 4) {
        fprintf(stderr, "usage: %s in_fn cb_fn enc_fn\n", argv[0]);
        return 1; 
    }
    // open files
    // Read Binary
    FILE *fin = fopen(argv[1], "rb"); 
    if (fin == NULL) { perror(argv[1]); return 1; }
    // Write
    FILE *fcsv = fopen(argv[2], "w"); 
    if (fcsv == NULL) { perror(argv[2]); return 1; }
    // Write Binary
    FILE *fout = fopen(argv[3], "wb"); 
    if (fout == NULL) { perror(argv[3]); return 1; }

    // === symbol statics ===
    Symb symb[MAX_SYMB]={0};
    
    // initial ascii symbols
    for(int i=0;i<=0x7F;i++){ 
        symb[i].chr[0]=(unsigned char)i; 
        symb[i].useLen=1; 
        symb[i].count=0; // start from zero
    }

    int used = BYTE_MAX;  // used symbol types
    int total = 0;        // total symbol count
    int c; // a byte read from file

    // ---------------------- statistic symbol --------------------
    while((c=fgetc(fin))!=EOF){
        unsigned char b0 = (unsigned char)c; 
        // handle ascii 0~127
        if(b0 <= 0x7F){ 
            symb[b0].count++;
            total++;
            continue; // next char
        }

        // handle multibyte symbol (utf-8 / big5)
        int symbLen=1; 
        unsigned char tmp[4]; //store symbol following bytes
        tmp[0]=b0; // first byte
        int uLen=0; // symbol use length (bytes)
        int read=1, ok=0; 

        // try utf-8
        uLen = utf8_len(b0); // check is legal utf-8 first byte & length
        if(uLen > 1){
            ok = 1;
            for(int i=1;i<uLen;i++){
                int d=fgetc(fin);
                if(d==EOF){ ok=0; break; } // end of file break
                tmp[read++]=(unsigned char)d; // store symbol in tmp[] (2~4 bytes)
                if(!is_utf8_follow(tmp[read-1])){ ok=0; break; } // illegal following utf-8 byte
            }
        }
        if (ok){ 
            symbLen = uLen; // save utf-8 length
        } else { 
            for (int j=read-1;j>=1; --j) ungetc(tmp[j], fin); // push back to b0
        }

        // try big-5
        if (symbLen == 1){  // check not utf-8 (symbLen unchanged)
            uLen = big5_len(b0); // check is legal big-5 first byte & length
            if (uLen == 2){ 
                int d = fgetc(fin); // read following byte (second byte)
                if (d != EOF && is_big5_follow((unsigned char)d)){ // legal big-5 follow byte
                    tmp[1] = (unsigned char)d;
                    symbLen = 2;   // big-5 always 2 bytes
                } else {
                    if (d != EOF) ungetc(d, fin); // push back to b0
                }
            }
        }

        // handle non ASCII, UTF-8, Big-5 symbols (128~255)
        if (symbLen == 1){ // symbleLen unchanged -> unknown one byte symbol
            if (symb[b0].useLen == 0){  // first time seen initialize
                symb[b0].useLen = 1;
                symb[b0].chr[0] = b0;
            }
            symb[b0].count++; // count this symbol 
            total++; // count total symbols
            continue;
        }

        // save multibyte symbol (utf-8 / big5)
        int found=0;
        for(int i=BYTE_MAX;i<used;i++){ 
            // find existing symbol
            if(symb[i].useLen==symbLen && memcmp(symb[i].chr,tmp,symbLen)==0){ 
                symb[i].count++; // count this symbol
                found=1; // mark found
                total++;
                break; 
            }
        }

        // not found, add new symbol
        if(!found){ 
            // check symbol limit
            if(used >= MAX_SYMB) { fprintf(stderr, "symbols are too many (%d)!\n", used); return 1; }
            memcpy(symb[used].chr, tmp, symbLen); // copy symbol bytes
            symb[used].useLen = symbLen; // set symbol length
            symb[used].count = 1; // initialize count
            total++; 
            used++; // push back used symbol types
        }
    }

    // ------------------ generate codebook -----------------------
    // calculate probability & write to codebook file
    Symb* out[MAX_SYMB]; // pointer array for qsort
    int n=0; // number of used symbols
    for(int i=0;i<used;i++) {
        if(symb[i].count > 0){ // skip unused symbols
            out[n] = &symb[i]; // store symb array address
            n++; 
        }
    }
    // count all symbols probability
    for(int i=0;i<n;i++){
        // check total > 0 and calculate probability
        if(total > 0) out[i]->prob = (double)out[i]->count / (double)total;
        else out[i]->prob = 0.0;
    }
    // sort by count descending
    qsort(out, n, sizeof(Symb*), cmp_codebook); // sort function

    // generate 7-bit(127) code and write to codebook.csv
    char EOF_symb[8]; // for EOF symbol 
    for(int i=0; i<n; i++){
        // first symbol is 0000000 0000001 ...
        int_to_bin(i, out[i]->code); // save 7-bit code
        // write to codebook 
        csv_char(out[i]->chr, out[i]->useLen, fcsv); // write symbol 
        fprintf(fcsv, ",%d,%.7f,%s\n", out[i]->count, out[i]->prob, out[i]->code); //write count, prob, code
    }
    // add EOF symbol at the end
    int_to_bin(n, EOF_symb); // EOF is n+1'th code
    fprintf(fcsv, "\"EOF\",0,0.0000000,%s\n", EOF_symb); // write EOF to codebook
    fclose(fcsv); // cb.csv

    // ------------------ encode input file -----------------------
    rewind(fin); // reset file pointer to beginning
    while((c=fgetc(fin))!=EOF){
        unsigned char b0 = (unsigned char)c; 
        // 1. handle ascii 0~127
        if(b0 <= 0x7F){ 
            // count++ -> write_code
            write_code(symb[b0].code, 7, fout); 
            continue; 
        }

        // 2. handle multibyte symbol (utf-8 / big5) !! same as above !!
        int symbLen=1; 
        unsigned char tmp[4];
        tmp[0]=b0;
        int uLen=0;
        int read=1, ok=0; 

        uLen = utf8_len(b0); 
        if(uLen > 1){
            ok = 1;
            for(int i=1;i<uLen;i++){
                int d=fgetc(fin);
                if(d==EOF){ ok=0; break; } 
                tmp[read++]=(unsigned char)d; 
                if(!is_utf8_follow(tmp[read-1])){ ok=0; break; } 
            }
        }
        if (ok){ 
            symbLen = uLen; 
        } else { 
            for (int j=read-1;j>=1; --j) ungetc(tmp[j], fin); 
        }

        if (symbLen == 1){ 
            uLen = big5_len(b0); 
            if (uLen == 2){
                int d = fgetc(fin);
                if (d != EOF && is_big5_follow((unsigned char)d)){ 
                    tmp[1] = (unsigned char)d;
                    symbLen = 2;   
                } else {
                    if (d != EOF) ungetc(d, fin); 
                }
            }
        }

        // handle non ASCII, UTF-8, Big-5 symbols (128~255)
        if (symbLen == 1){ 
            // count++ -> write_code
            write_code(symb[b0].code, 7, fout); 
            continue;
        }

        // 3. write multibyte symbol code
        for(int i=BYTE_MAX;i<used;i++){ 
            if(symb[i].useLen==symbLen && memcmp(symb[i].chr,tmp,symbLen)==0){ 
                // count++ -> write_code
                write_code(symb[i].code, 7, fout); 
                break; 
            }
        }
    }
    // ---------------- end of input file -----------------------
    write_code(EOF_symb, 7, fout);
    if (bitInBuff> 0) {
        // add 0 to fill last byte
        晢%迢ｱゅ?薙ｹ郢@蟶ｩ〒縺&郢薙ｱｻ郢薙蟶#??雋暦鯉&郢=
        fputc(byteBuffer, fout); // last byte
    }
    fclose(fin);
    fclose(fout);
    return 0;
}