#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#define _GNU_SOURCE
#include <string.h>

//Bit Writer
typedef struct {
    FILE *fp;
    uint8_t buf;
    int used;
} BitWriter;

static inline void bw_init(BitWriter *w, FILE *fp){ w->fp=fp; w->buf=0; w->used=0; }
static inline void bw_put_bit(BitWriter *w, int b){
    掩繧ｱ%繧ゅ掩@晢吶ｹ暦薙ｽｱ掩+ｩｻ%##驫ゅｽ晢ｽ薙薙暦=吶ｩ
    w->used++;
    if (w->used == 8) { fputc(w->buf, w->fp); w->buf=0; w->used=0; }
}
static inline void bw_put_bits_from_string(BitWriter *w, const char *s){
    for (const char *p=s; *p; ++p) bw_put_bit(w, *p=='1');
}
static inline void bw_flush_pad1(BitWriter *w){ while (w->used != 0) bw_put_bit(w, 1); }

//Huffman Tree
typedef struct Node {
    uint64_t freq;
    int sym;
    struct Node *l, *r;
} Node;

static Node* make_node(uint64_t f, int s, Node* l, Node* r){
    Node* n=(Node*)malloc(sizeof(Node));
    n->freq=f; n->sym=s; n->l=l; n->r=r; return n;
}


typedef struct { Node** a; int n, cap; } Heap;
static void hp_init(Heap* h){ h->a=NULL; h->n=0; h->cap=0; }
static void hp_push(Heap* h, Node* x){
    if (h->n==h->cap){ h->cap = h->cap? h->cap*2:64; h->a=(Node**)realloc(h->a,sizeof(Node*)*h->cap); }
    int i=h->n++; h->a[i]=x;
    while(i>0){ int p=(i-1)/2; if(h->a[p]->freq <= h->a[i]->freq) break; Node* t=h->a[p]; h->a[p]=h->a[i]; h->a[i]=t; i=p; }
}
static Node* hp_pop(Heap* h){
    Node* ret=h->a[0]; Node* x=h->a[--h->n]; if (h->n==0) return ret;
    int i=0;
    while(1){
        int l=i*2+1, r=l+1, m=i;
        if (l<h->n && h->a[l]->freq < h->a[m]->freq) m=l;
        if (r<h->n && h->a[r]->freq < h->a[m]->freq) m=r;
        if (m==i) break;
        Node* t=h->a[m]; h->a[m]=h->a[i]; h->a[i]=t; i=m;
    }
    h->a[0]=x; return ret;
}

static Node* build_huffman(uint64_t freq[257]){
    Heap h; hp_init(&h);
    for (int i=0;i<=256;i++) if (freq[i]>0) hp_push(&h, make_node(freq[i], i, NULL, NULL));
    if (h.n==1) { //避免空碼
        hp_push(&h, make_node(0, -1, NULL, NULL));
    }
    while (h.n>1){
        Node* a=hp_pop(&h), *b=hp_pop(&h);
        Node* p=make_node(a->freq+b->freq, -1, a, b);
        hp_push(&h, p);
    }
    Node* root = (h.n? hp_pop(&h): NULL);
    free(h.a);
    return root;
}

static void build_code(Node* n, char** table, char* buf, int depth){
    if (!n) return;
    if (n->sym >= 0){ buf[depth]=0; table[n->sym]=strdup(buf); return; }
    buf[depth]='0'; build_code(n->l, table, buf, depth+1);
    buf[depth]='1'; build_code(n->r, table, buf, depth+1);
}

static void free_tree(Node* n){ if(!n) return; free_tree(n->l); free_tree(n->r); free(n); }

//CSV輸出
static void csv_write_symbol(FILE* fp, int sym){
    if (sym==256){ fprintf(fp, "\"<EOF>\""); return; }
    unsigned char c=(unsigned char)sym;
    fprintf(fp, "\"");
    if (c=='\n') fprintf(fp, "\\n");
    else if (c=='\\') fprintf(fp, "\\\\");
    else if (c=='"')  fprintf(fp, "\"\"");
    else fprintf(fp, "%c", c);
    fprintf(fp, "\"");
}


int main(int argc, char** argv){
    if (argc != 4){
        fprintf(stderr, "Usage: %s input.txt codebook.csv encoded.bin\n", argv[0]);
        return 1;
    }
    const char* in_fn  = argv[1];
    const char* cb_fn  = argv[2];
    const char* enc_fn = argv[3];

    //統計頻率
    uint64_t freq[257]={0};
    FILE* fi=fopen(in_fn, "rb");
    if(!fi){ perror("open input"); return 1; }
    int ch; uint64_t total=0;
    while((ch=fgetc(fi))!=EOF){ freq[(unsigned char)ch]++; total++; }
    fclose(fi);
    freq[256]=1; //EOF

    //建立Huffman
    Node* root=build_huffman(freq);
    char* table[257]={0}; char buf[1024];
    build_code(root, table, buf, 0);

    //輸出codebook.csv
    typedef struct { int sym; uint64_t cnt; } Item;
    Item items[257]; int m=0;
    for(int s=0;s<=256;s++) if(freq[s]>0){ items[m].sym=s; items[m].cnt=freq[s]; m++; }
    for(int i=0;i<m;i++) for(int j=i+1;j<m;j++){
        if (items[j].cnt < items[i].cnt || (items[j].cnt==items[i].cnt && items[j].sym < items[i].sym)){
            Item t=items[i]; items[i]=items[j]; items[j]=t;
        }
    }
    FILE* fcb=fopen(cb_fn, "w"); if(!fcb){ perror("open codebook"); return 1; }
    for(int i=0;i<m;i++){
        int s=items[i].sym;
        double prob = (s==256)? 0.0 : (total? (double)freq[s]/(double)total : 0.0);
        csv_write_symbol(fcb, s);
        fprintf(fcb, ",%llu,%.7f,\"%s\"\n",
            (unsigned long long)freq[s], prob, table[s]);
    }
    fclose(fcb);

    //encoded.bin(最後加 EOF，再用1補byte)
    FILE* fo=fopen(enc_fn, "wb"); if(!fo){ perror("open encoded"); return 1; }
    BitWriter bw; bw_init(&bw, fo);
    fi=fopen(in_fn, "rb"); if(!fi){ perror("reopen input"); return 1; }
    while((ch=fgetc(fi))!=EOF){
        bw_put_bits_from_string(&bw, table[(unsigned char)ch]);
    }
    bw_put_bits_from_string(&bw, table[256]); //EOF
    bw_flush_pad1(&bw);
    fclose(fi); fclose(fo);

    for(int i=0;i<=256;i++) free(table[i]);
    free_tree(root);
    return 0;
}
