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
} Symb;

//------------------ big-5 decoding ------------------ 
static int big5_len(unsigned char b0){
    if (b0 <= 0x7F) return 1;                 // first byte < 127 ->ASCII
    if (b0 >= 0x81 && b0 <= 0xFE) return 2;   // is big-5 high byte 0x81-0xFE
    return 1;                                 
}
// big5 low byte 0x40-0x7E , 0xA1-0xFE
static int is_big5_follow(unsigned char b1){
    return ( (b1 >= 0x40 && b1 <= 0x7E) || (b1 >= 0xA1 && b1 <= 0xFE) ); //check is big-5 rule
}
//------------------ utf-8 decoding ------------------ 
static int utf8_len(unsigned char b0){  //use first byte to check UTF-8 using length
    if ((b0 & 0x80)==0x00) return 1; //0xxxxxxx
    if ((b0 & 0xE0)==0xC0) return 2; //110xxxxx
    if ((b0 & 0xF0)==0xE0) return 3; //1110xxxx
    if ((b0 & 0xF8)==0xF0) return 4; //11110xxx
    return 0;  
}

static int is_utf8_follow(unsigned char b){ //check is UTF-8 follow byte legal (10xxxxxx)
    return (b & 0xC0)==0x80; //10xxxxxx
}

// qsort compare function
static int cmp(const void *a, const void *b){ 
    const Symb *x = (const Symb*)a;
    const Symb *y = (const Symb*)b;
    if(x->count != y->count) 
        return y->count - x->count;   // 1. symbol count  (descending
    if(x->useLen != y->useLen) 
        return x->useLen - y->useLen;     // 2. symbol use length (ascending  ascii, big-5, utf-8
    return memcmp(x->chr, y->chr, x->useLen);  // 3. byte index (ascending
}
// write char to output
static void csv_char(const unsigned char *s,int len){
    if (len==1 && s[0]=='\r'){ fputs("\"\\r\"",stdout); return; }
    if (len==1 && s[0]=='\n'){ fputs("\"\\n\"",stdout); return; }
    if (len==1 && s[0]=='\t'){ fputs("\"\\t\"",stdout); return; }
    fputc('"',stdout); // opening quote 
    for(int i=0;i<len;i++){
        if (s[i] == '"') {
            fputc('"', stdout);
            fputc('"', stdout);
        }else {
            fputc(s[i],stdout); // output symbol
        }
    }
    fputc('"',stdout);// closing quote
}

int main(void){
    Symb symb[MAX_SYMB]={0};

    // initial all ascii symbol: len=1 chr[0]=ascii
    for(int i=0;i<=0x7F;i++){ // ascii 0-127
        symb[i].chr[0]=(unsigned char)i; 
        symb[i].useLen=1; 
        symb[i].count=0; 
    }

    int used = BYTE_MAX;  // count used symb[], start from 256
    int total = 0;  // count total symbol
    int c;
    while((c=fgetc(stdin))!=EOF){
        unsigned char b0 = (unsigned char)c; // b0  (first byte)
        // ----- handling ascii symbol ---------
        if(b0 <= 0x7F){ // is ASCII (one byte), already initialized
            symb[b0].count++; 
            total++; 
            continue;
        }

        // below is not ascii
        int symbLen=1; 
        unsigned char tmp[4]; 
        tmp[0]=b0; // first byte
        int uLen=0; // use length
        int read=1, ok=0; //check byte is legal & add in tmp[]

        // --------- try handling utf-8 -------------
        uLen = utf8_len(b0); // check is legal utf-8 first byte & length
        if(uLen > 1){
            ok = 1;
            for(int i=1;i<uLen;i++){
                int d=fgetc(stdin);
                if(d==EOF){ ok=0; break; } // file end
                tmp[read++]=(unsigned char)d; //load folowing bytes
                if(!is_utf8_follow(tmp[read-1])){ ok=0; break; } // illegal following utf-8 byte 
            }
        }
        if (ok){ // confirm is UTF-8 
            symbLen = uLen; 
        } else { // not utf-8, use same b0 to check next (big-5) 
            for (int j=read-1;j>=1; --j) ungetc(tmp[j], stdin); // push back to b0
        }

        // --------- try handling big-5 -------------
        if (symbLen == 1){ // check not utf-8 (no change)
            uLen = big5_len(b0); // check is legal big-5 first byte & length
            if (uLen == 2){
                int d = fgetc(stdin);
                if (d != EOF && is_big5_follow((unsigned char)d)){ // check is legal
                    tmp[1] = (unsigned char)d;
                    symbLen = 2;   // confirm is BIG5 
                } else {
                    if (d != EOF) ungetc(d, stdin); // push back to b0
                }
            }
        }

        // ---------- handling other one byte unknown --------------
        if (symbLen == 1){ // not above encoding (no change)
            if (symb[b0].useLen == 0){  // first time use this index
                symb[b0].useLen = 1;
                symb[b0].chr[0] = b0;
            }
            symb[b0].count++;
            total++;
            continue;
        }


        // --------- let legal symbol add to symb[] ----------
        // check exist & count +1
        int found=0;
        for(int i=BYTE_MAX;i<used;i++){ // linear search is symbol exist
            // check is same symbol exist by length & compare=0
            if(symb[i].useLen==symbLen && memcmp(symb[i].chr,tmp,symbLen)==0){ // only compare 2 str may cmp=0 but byte different
            symb[i].count++; 
            found=1; 
            break; 
            }
        }
        // new symbol , extend symb[] and set count=1
        if(!found && used<MAX_SYMB){
            memcpy(symb[used].chr,tmp,symbLen); // load symbol
            symb[used].useLen=symbLen; // load length
            symb[used].count=1; // initial count 
            used++; // extend used
        }
        total++; 
    }

    // ----------- filter need symbols ----------
    Symb out[MAX_SYMB]; // output array
    int n=0;
    for(int i=0;i<used;i++) {
        if(symb[i].count>0){ 
            out[n]=symb[i]; 
            n++; 
        }
    }
    // count the probability
    for(int i=0;i<n;i++){
        if(total>0){ // check total > 0
            out[i].prob = (double)out[i].count/(double)total;
        }else{
            out[i].prob = 0.0;
        }
    }
    // sort & output
    qsort(out,n,sizeof(Symb),cmp); // sort function
    for(int i=0;i<n;i++){
        csv_char(out[i].chr,out[i].useLen); // "char"
        printf(",%d,%.15f\n", out[i].count, out[i].prob); // ,count,probability
    }
    return 0;
}
