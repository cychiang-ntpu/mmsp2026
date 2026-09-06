
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_CODEWORDS 1024
typedef struct {
    char code[1024];
    int sym; 
} CW;

CW cwtab[MAX_CODEWORDS];
int cwcount=0;

int decode_symbol_from_code(const char *code){
    for(int i=0;i<cwcount;i++){
        if(strcmp(cwtab[i].code, code)==0) return cwtab[i].sym;
    }
    return -1;
}


int parse_symbol_field(const char *s){
    
    if(s[0]!='\"') return -1;
    const char *p = s+1;
    if(p[0]=='\\' && p[1]=='n') return '\n';
    if(p[0]=='\\' && p[1]=='r') return '\r';
    if(p[0]=='\\' && p[1]=='\\') return '\\';
    if(p[0]=='\\' && p[1]==',') return ',';
    if(p[0]=='\\' && p[1]=='\"') return '\"';
    if(p[0]=='0' && p[1]=='x'){
        unsigned int v;
        if(sscanf(p+2, "%2x", &v)==1) return v;
    }
    if(strncmp(p,"<EOF>",5)==0) return 256;
    
    return (unsigned char)p[0];
}

int main(int argc, char **argv){
    if(argc!=4){
        fprintf(stderr,"Usage: %s out_fn cb_fn enc_fn\n", argv[0]);
        return 1;
    }
    const char *out_fn = argv[1];
    const char *cb_fn = argv[2];
    const char *enc_fn = argv[3];

    FILE *fcb = fopen(cb_fn,"r");
    if(!fcb){ perror("open cb"); return 1;}
    char line[2048];
    cwcount=0;
    while(fgets(line,sizeof(line),fcb)){
        
        char field1[128], field4[1024];
        unsigned long cnt;
        double prob;
        
        char *p1 = strchr(line,'\"');
        if(!p1) continue;
        char *p2 = strchr(p1+1,'\"');
        if(!p2) continue;
        int len1 = p2 - p1 + 1;
        strncpy(field1, p1, len1); field1[len1]=0;
        
        char *pq = strrchr(line,'\"');
        if(!pq) continue;
        char *pq2 = pq;
        
        char *pq1 = pq2;
        while(pq1>line && *pq1!='\"') pq1--;
        if(pq1==pq2) {  pq1 = line; }
        
        
        ｱ?雎鯉郢迢蜃%驫@ｻ%暦蟶▲〒▲~@ｽ薙ｩｱ~ｩ#%?ゅ雎°〒#繧雋?%ｽ?ゅ驫@ｩ+雋繧〒=%*鯉薙ｹｩ〒@°%#雎雎鯉#薙
            
        }
        
        char *lastqopen = strrchr(line,'"');
        if(lastqopen){
            
            char *prev = lastqopen-1;
            while(prev>line && *prev!='"') prev--;
            if(prev>line){
                int code_len = (int)(lastqopen - prev - 1);
                strncpy(field4, prev+1, code_len);
                field4[code_len]=0;
                
            } else {
                field4[0]=0;
            }
        }
        
        if(p1 && p2){
            int l = p2-p1+1;
            char tmp1[64];
            strncpy(tmp1, p1, l); tmp1[l]=0;
            int sym = parse_symbol_field(tmp1);
            
            char *firstq = strchr(line,'\"');
            char *secondq = firstq? strchr(firstq+1,'\"'):NULL;
            
            char *last_quote = strrchr(line,'\"');
            char *last_quote_open = last_quote;
            while(last_quote_open>line && *last_quote_open!='"') last_quote_open--;
            
            char *q2 = strrchr(line,'"');
            if(q2){
                
                char *q1 = q2-1;
                while(q1>line && *q1!='"') q1--;
                if(q1>line){
                    int clen = (int)(q2 - q1 - 1);
                    char code[1024];
                    strncpy(code, q1+1, clen); code[clen]=0;
                    
                    if(cwcount < MAX_CODEWORDS){
                        strcpy(cwtab[cwcount].code, code);
                        cwtab[cwcount].sym = sym;
                        cwcount++;
                    }
                }
            }
        }
    }
    fclose(fcb);


    FILE *fenc = fopen(enc_fn,"rb");
    if(!fenc){ perror("open enc"); return 1;}
    FILE *fout = fopen(out_fn,"wb");
    if(!fout){ perror("open out"); fclose(fenc); return 1;}

    int byte;
    char curcode[2048];
    curcode[0]=0;
    while((byte=fgetc(fenc))!=EOF){
        for(int i=7;i>=0;i--){
            size_t l = strlen(curcode);
            curcode[l] = bit ? '1' : '0';
            curcode[l+1] = 0;
            int dsym = decode_symbol_from_code(curcode);
            if(dsym >= 0){
                if(dsym == 256){
                    
                    fclose(fenc); fclose(fout);
                    printf("Decoded to %s\n", out_fn);
                    return 0;
                } else {
                    fputc(dsym, fout);
                    curcode[0]=0; curcode[1]=0;
                }
            }
            
        }
    }

    
    fclose(fenc);
    fclose(fout);
    fprintf(stderr,"Warning: reached end of encoded file without EOF symbol. Output written but may be truncated.\n");
    return 0;
}
