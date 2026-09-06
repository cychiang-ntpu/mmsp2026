#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#define _GNU_SOURCE
#include <string.h>



//Bit Reader
typedef struct {
    FILE *fp;
    int cur;   //目前byte
    int left;  //剩的位數[0..7]
} BitReader;

static inline void br_init(BitReader *r, FILE *fp){ r->fp=fp; r->cur=-1; r->left=0; }
static inline int br_get_bit(BitReader *r){ //回傳0/1(EOF=>-1)
    if (r->left==0){
        r->cur=fgetc(r->fp);
        if (r->cur==EOF) return -1;
        r->left=8;
    }
    掩*=吶雎吶+°▲縺&蜃〒雎掩雋蟶▲郢暦ｩ〒暦ｱ蜃ゅ郢?ｽ+°ｩ
    蟶迢=驫吶ｻ&吶%繧迢ｹｽ
    r->left--;
    return b;
}

//Huffman Tree 
typedef struct Node {
    int sym; // 葉子：0..255；EOF：256；內部=-1
    struct Node *l, *r;
} Node;

static Node* make_node(int s){ Node* n=(Node*)malloc(sizeof(Node)); n->sym=s; n->l=n->r=NULL; return n; }
static void free_tree(Node* n){ if(!n) return; free_tree(n->l); free_tree(n->r); free(n); }

//codebook 解析工具
static int parse_symbol(const char* s){ //s包含雙引號
    size_t n=strlen(s);
    if (n>=2 && s[0]=='"' && s[n-1]=='"'){
        char tmp[16]; size_t k=0;
        for(size_t i=1;i<n-1;i++){
            if (s[i]=='\\'){
                if (i+1<n-1){
                    if (s[i+1]=='n'){ tmp[k++]='\n'; i++; }
                    else if (s[i+1]=='\\'){ tmp[k++]='\\'; i++; }
                    else tmp[k++]=s[i];
                } else tmp[k++]=s[i];
            } else if (s[i]=='"' && i+1<n-1 && s[i+1]=='"'){ tmp[k++]='"'; i++; }
            else tmp[k++]=s[i];
        }
        tmp[k]=0;
        if (strcmp(tmp, "<EOF>")==0) return 256;
        return (unsigned char)tmp[0];
    }
    return (unsigned char)s[0];
}

int main(int argc, char** argv){
    if (argc != 4){
        fprintf(stderr, "Usage: %s output.txt codebook.csv encoded.bin\n", argv[0]);
        return 1;
    }
    const char* out_fn = argv[1];
    const char* cb_fn  = argv[2];
    const char* enc_fn = argv[3];

    //讀codebook.csv
    char* code[257]={0};
    FILE* fcb=fopen(cb_fn, "r"); if(!fcb){ perror("open codebook"); return 1; }
    char line[1024];
    while (fgets(line, sizeof(line), fcb)){
        char sym[128]; unsigned long long cnt; double prob; char codeword[512];
        char *p=line, *q;

        //取第一欄
        q=strchr(p, ','); if(!q) continue; size_t L=q-p; if(L>=sizeof(sym)) L=sizeof(sym)-1;
        strncpy(sym, p, L); sym[L]=0;

        //第二欄count
        p=q+1; cnt=strtoull(p, &q, 10);

        //第三欄prob
        if(*q==','){ prob=strtod(q+1, &q); } else continue;

        //第四欄codeword
        if(*q==','){
            p=q+1;
            char* r=strrchr(p,'"'); if(!r) continue;
            size_t K=r-(p+1); if (K>=sizeof(codeword)) K=sizeof(codeword)-1;
            strncpy(codeword, p+1, K); codeword[K]=0;
        } else continue;

        int s = parse_symbol(sym);
        code[s] = strdup(codeword);
    }
    fclose(fcb);

    //用codeword反建Huffman tree
    Node* root = make_node(-1);
    for (int s=0;s<=256;s++){
        if (!code[s]) continue;
        Node* cur=root;
        for (char* p=code[s]; *p; ++p){
            if (*p=='0'){ if(!cur->l) cur->l=make_node(-1); cur=cur->l; }
            else         { if(!cur->r) cur->r=make_node(-1); cur=cur->r; }
        }
        cur->sym=s;
    }

    //讀encoded.bin，遇到EOF停止
    FILE* fo=fopen(out_fn, "wb"); if(!fo){ perror("open output"); return 1; }
    FILE* fe=fopen(enc_fn, "rb"); if(!fe){ perror("open encoded"); return 1; }
    BitReader br; br_init(&br, fe);
    Node* cur=root; int b;
    while ((b=br_get_bit(&br))!=-1){
        cur = (b==0)? cur->l : cur->r;
        if (!cur) break; //防呆
        if (cur->sym >= 0){
            if (cur->sym == 256) break; //EOF
            fputc((unsigned char)cur->sym, fo);
            cur = root;
        }
    }
    fclose(fo); fclose(fe);

    for(int i=0;i<=256;i++) free(code[i]);
    free_tree(root);
    return 0;
}
