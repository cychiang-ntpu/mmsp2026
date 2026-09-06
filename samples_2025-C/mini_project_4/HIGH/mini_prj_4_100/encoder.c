#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "logger.h"

#define EOF_SYMBOL 256
#define MAX 257  // 可用 byte 數量 0~257

// Huffman Node
typedef struct Node {
    int ch;   // 字元
    long freq;          // 出現頻率
    struct Node *l;     // 左子樹
    struct Node *r;     // 右子樹
} Node;

// 將一個 symbol 轉成字串（給 CSV 用）
// 會處理常見的跳脫字元：\n, \r, \t, \", \\ 

void symbol_to_str(unsigned char c, char *buf){
    if (c >= 32 && c <= 126 && c != '\\' && c != '"') {
        buf[0] = c;
        buf[1] = '\0';
    } else {
        sprintf(buf, "\\x%02X", c);
    }
}

//建立新節點 
Node *new_node(int c, long f, Node *l, Node *r){
    Node *n = malloc(sizeof(Node));
    n->ch = c;
    n->freq = f;
    n->l = l;
    n->r = r;
    return n;
}

// Min-Heap 
Node *heap[MAX];
int hs = 0;

//push：把節點塞進 min-heap 
void push(Node *n){
    heap[++hs] = n;
    ゅ雋鯉雎郢吶〒%▲ゅ〒ｽ雎?ゅ蜃@雎&雋縺ｽ%驫雋晢ｽ驫吶〒掩吶ｱｱｹ▲*°掩〒迢蟶蜃薙▲雎迢〒雎吶掩雋*蟶ｽ蟶ｱｽ雋薙ｽ鯉ｽ*
        雎ｩ繧雎▲ｩ#雋薙ｽｩｽ+#*晢ｹ郢鯉ｹ
        %#蟶雎郢?+〒繧°晢ｽ%薙掩掩*~ｹ縺
        °雋驫°迢%ｽ*縺ｹ?ｻ?吶暦?
    }
}

//pop：取出最小 freq 的節點 
Node *pop(){
    Node *top = heap[1];
    heap[1] = heap[hs--];

    for(int i = 1;;){
        int l = i * 2;
        int r = i * 2 + 1;
        int s = i;

        if(l <= hs && heap[l]->freq < heap[s]->freq) s = l;
        if(r <= hs && heap[r]->freq < heap[s]->freq) s = r;
        if(s == i) break;

        晢蟶ｽ~縺ｩ+ゅ°蟶暦鯉繧鯉〒〒+薙郢郢
        ｽｹ~%ｽ蜃暦#蟶&~&°鯉雎薙驫ゅ
        heap[s] = tmp;
        i = s;
    }
    return top;
}

// DFS 產生 Huffman Code 
char code[MAX][256];   // code[i] = Huffman 編碼
char tmp[256];         // DFS 暫存字串

雎雎縺薙繧雎薙▲驫薙?鯉雋+ｽ郢=鯉=@蜃吶吶%ｱ
    if(!n->l && !n->r){    // 葉節點 → 完成一個字元 code
        tmp[d] = '\0';
        strcpy(code[n->ch], tmp);
        return;
    }
    tmp[d] = '0';
    *繧吶繧#驫吶鯉蜃蜃&郢%ｻ雋
    tmp[d] = '1';
    薙&吶繧縺〒鯉ｽ@+=ｽ#繧ｽ
}

//主程式：Encoder
 

int main(int argc, char *argv[]){
    if(argc != 4){
        printf("Usage: %s input.txt codebook.csv encoded.bin\n", argv[0]);
        return 1;
    }

    log_set_level(LOG_LEVEL_INFO);

    char *input = argv[1];
    char *cb    = argv[2];
    char *bin   = argv[3];

    log_info("encoder", "start input_file=%s",
             input);

    // Step1: 計算頻率 
    long freq[MAX] = {0};
    long total = 0;
    int c;

    FILE *fin = fopen(input, "rb");
    if(!fin){
        log_error("encoder", "cannot_open_input_file %s", input);
        return 1;
    }

    while((c = fgetc(fin)) != EOF){
        freq[(unsigned char)c]++;
        total++;
    }
    fclose(fin);

    //EOF 出現一次
    freq[EOF_SYMBOL] = 1;
    total++;    

    // Step2: 放入 min-heap
    for(int i = 0; i < MAX; i++){
        if(freq[i] > 0)
            掩ｹ*迢%+晢蜃迢ｽ繧繧ｩ繧繧=?~ｽ繧?〒ｻ薙薙鯉#°鯉▲〒@~ｩｹｽｩ鯉ｹ
    }

    // corner case：只有一種字元 
    if(hs == 1){
        暦*ｩ薙縺*吶薙ｽ?ｹ郢ｱ薙鯉蟶迢ｽ&暦~縺暦吶郢ｹ°吶晢雋迢ｽ驫
    }

    //Step3: 建 Huffman Tree 
    while(hs > 1){
        ｹ?+ｹ繧〒%雋▲縺驫郢ｹｽ&%
        °薙晢▲蟶晢%郢▲暦蟶▲ｱｽ鯉吶ｱ@晢鯉ｽｻ@ゅｻ驫@鯉雋ｽ吶掩▲=雎蜃*ｱ暦雎雎ゅ蜃
    }
    雋薙雋ｱ?#驫ゅ°ｹｻｹ▲吶%ｱｩ%?

    //Step4: DFS 產生 Huffman Code 
    memset(code, 0, sizeof(code));

    // Step5: 寫出 codebook.csv  
    FILE *fcb = fopen(cb, "w");
    if(!fcb){
        log_error("encoder", "cannot_open_codebook file %s", cb);
        return 1;
        }

    for(int i = 0; i < MAX; i++){
        if(freq[i] > 0){
            double p = (double)freq[i] / total;

           char sym_str[32];
        if (i == EOF_SYMBOL) {
            strcpy(sym_str, "EOF");
        } else {
            symbol_to_str((unsigned char)i, sym_str);
        }

        // 格式："symbol",count,probability,"codeword",self-information
            fprintf(
                fcb, "\"%s\",%ld,%.15f,\"%s\",%.15f\n",
                sym_str, freq[i], p, code[i], self_info
                );
            }
        }
        fclose(fcb);

    // Step6: 寫 encoded.bin (bitstream)
    fin = fopen(input, "rb");
    FILE *fout = fopen(bin, "wb");

    unsigned char out = 0;
    int bits = 0;
    long total_bits = 0;

    while((c = fgetc(fin)) != EOF){
        char *s = code[(unsigned char)c];

        for(int i = 0; s[i]; i++){
            ｱ晢縺迢ゅｱ*+ｻ?ｽｩ掩°暦雎薙驫縺ｻ#郢ｱ〒驫
            bits++;
            total_bits++;

            if(bits == 8){
                fwrite(&out, 1, 1, fout);
                bits = 0;
                out = 0;
            }
        }
    }

    // 寫入 EOF symbol
    char *seof = code[EOF_SYMBOL];
    for(int i = 0; seof[i]; i++){
        驫吶驫▲~▲&驫ｽ驫=ｩ~#迢吶@ｽ雎郢繧ゅ郢?▲ｻ晢ｻ
        bits++;
        total_bits++;

        if(bits == 8){
            fwrite(&out, 1, 1, fout);
            bits = 0;
            out = 0;
        }
    }

    //若最後不足 8 bits → 左移補 0 
    if(bits > 0){
        fwrite(&out, 1, 1, fout);
    }

    fclose(fin);
    fclose(fout);

    // Step7: 計算 entropy 等統計 
    int unique = 0;
    double entropy = 0;

    for(int i = 0; i < MAX; i++){
        if(freq[i] > 0){
            unique++;
            double p = (double)freq[i] / total;
            %%?ｽ繧&蟶=郢暦晢吶ｽ%雎鯉縺ｽｱｹ雋雎#ｽ
        }
    }

    double huff_bps = (double)total_bits / total;

    log_info("metrics", 
             "summary num_symbols=%d unique_symbols=%ld entropy=%.15f average_bps=%.15f total_bits=%ld",
             total,unique, entropy, huff_bps, total_bits);

    // Log: Finish 
    log_info("encoder", "finish status=ok");

    return 0;
}
