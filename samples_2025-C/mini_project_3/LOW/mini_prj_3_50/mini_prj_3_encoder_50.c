
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define MAXSYM 257 

typedef struct Node {
    int sym; 
    unsigned long freq;
    struct Node *l, *r;
} Node;

typedef struct {
    Node *nodes[1024];
    int n;
} PQ;

void pq_init(PQ *q){ q->n = 0; }
void pq_push(PQ *q, Node *nd){
    int i = q->n++;
    q->nodes[i] = nd;
    while(i>0){
        int p=(i-1)/2;
        if(q->nodes[p]->freq <= q->nodes[i]->freq) break;
        Node *t=q->nodes[p]; q->nodes[p]=q->nodes[i]; q->nodes[i]=t;
        i=p;
    }
}
Node *pq_pop(PQ *q){
    if(q->n==0) return NULL;
    Node *ret=q->nodes[0];
    q->nodes[0]=q->nodes[--q->n];
    int i=0;
    while(1){
        int l=2*i+1, r=2*i+2, smallest=i;
        if(l<q->n && q->nodes[l]->freq < q->nodes[smallest]->freq) smallest=l;
        if(r<q->n && q->nodes[r]->freq < q->nodes[smallest]->freq) smallest=r;
        if(smallest==i) break;
        Node *t=q->nodes[i]; q->nodes[i]=q->nodes[smallest]; q->nodes[smallest]=t;
        i=smallest;
    }
    return ret;
}

Node* make_node(int sym, unsigned long freq){
    Node *n = (Node*)malloc(sizeof(Node));
    n->sym = sym; n->freq = freq; n->l = n->r = NULL;
    return n;
}

char *codes[MAXSYM];
void build_codes(Node *root, char *buf, int depth){
    if(!root) return;
    if(root->l==NULL && root->r==NULL){
        buf[depth]=0;
        codes[root->sym] = strdup(buf);
        return;
    }
    if(root->l){ buf[depth]='0'; build_codes(root->l, buf, depth+1); }
    if(root->r){ buf[depth]='1'; build_codes(root->r, buf, depth+1); }
}

const char* sym_repr(int sym, char *out, int outsz){
    
    if(sym==256){ snprintf(out,outsz,"\"<EOF>\""); return out; }
    unsigned char c = (unsigned char)sym;
    if(c=='\\'){ snprintf(out,outsz,"\"\\\\\""); return out; }
    if(c=='\"'){ snprintf(out,outsz,"\"\\\"\""); return out; }
    if(c=='\n'){ snprintf(out,outsz,"\"\\\\n\""); return out; }
    if(c=='\r'){ snprintf(out,outsz,"\"\\\\r\""); return out; }
    if(c==','){ snprintf(out,outsz,"\"\\,\""); return out; }
    
    if(isprint(c)){
        char tmp[8]={0};
        tmp[0]=c; tmp[1]=0;
        snprintf(out,outsz,"\"%s\"", tmp);
        return out;
    } else {
        snprintf(out,outsz,"\"0x%02X\"", c);
        return out;
    }
}

int cmp_for_csv(const void *a, const void *b){
    
    const int *ia = a;
    const int *ib = b;
    
    (void)ia; (void)ib;
    return 0;
}

int main(int argc, char **argv){
    if(argc!=4){
        fprintf(stderr,"Usage: %s in_fn cb_fn enc_fn\n", argv[0]);
        return 1;
    }
    const char *in_fn = argv[1];
    const char *cb_fn = argv[2];
    const char *enc_fn = argv[3];

    
    FILE *fin = fopen(in_fn,"rb");
    if(!fin){ perror("open in"); return 1;}
    unsigned long counts[MAXSYM];
    memset(counts,0,sizeof(counts));
    size_t total = 0;
    int c;
    while((c=fgetc(fin))!=EOF){
        counts[(unsigned char)c]++;
        total++;
    }
    fclose(fin);

    
    counts[256] = 1;
    total += 1;

    
    PQ q; pq_init(&q);
    for(int s=0;s<MAXSYM;s++){
        if(counts[s]>0){
            pq_push(&q, make_node(s, counts[s]));
        }
    }

    if(q.n==0){
        fprintf(stderr,"empty input\n");
        return 1;
    }

    
    if(q.n==1){
        Node *a = pq_pop(&q);
        Node *root = make_node(-1, a->freq);
        root->l = a;
        root->r = make_node(-1,0);
        pq_push(&q, root);
    }

    
    while(q.n > 1){
        Node *a = pq_pop(&q);
        Node *b = pq_pop(&q);
        Node *p = make_node(-1, a->freq + b->freq);
        p->l = a; p->r = b;
        pq_push(&q, p);
    }
    Node *root = pq_pop(&q);

    
    for(int i=0;i<MAXSYM;i++){ codes[i]=NULL; }
    char buf[1024];
    build_codes(root, buf, 0);

    
    int present_count = 0;
    for(int s=0;s<MAXSYM;s++) if(counts[s]>0) present_count++;
    int *idx = malloc(sizeof(int)*present_count);
    int p=0;
    for(int s=0;s<MAXSYM;s++) if(counts[s]>0) idx[p++]=s;
    
    for(int i=0;i<p;i++){
        for(int j=i+1;j<p;j++){
            if(counts[idx[i]] > counts[idx[j]] || (counts[idx[i]]==counts[idx[j]] && idx[i] > idx[j])){
                int t = idx[i]; idx[i]=idx[j]; idx[j]=t;
            }
        }
    }

    
    FILE *fcb = fopen(cb_fn,"w");
    if(!fcb){ perror("open cb"); return 1;}
    for(int i=0;i<p;i++){
        int s = idx[i];
        char repr[32];
        sym_repr(s, repr, sizeof(repr));
        double prob = (double)counts[s] / (double)total;
        fprintf(fcb, "%s,%lu,%.7f,\"%s\"\n", repr, counts[s], prob, codes[s]);
    }
    fclose(fcb);

    
    FILE *fenc = fopen(enc_fn,"wb");
    if(!fenc){ perror("open enc"); return 1;}
    
    fin = fopen(in_fn,"rb");
    if(!fin){ perror("open in2"); return 1;}
    unsigned char out_byte = 0;
    int out_bits = 0;
    auto emit_bit = [&](int bit){
        ~吶ｻ繧=驫?#迢薙ｹ吶吶*▲繧@ｩ蟶雋蜃晢掩ｩ繧ｽ°繧ゅ@晢〒蜃薙#
        out_bits++;
        if(out_bits==8){
            fputc(out_byte, fenc);
            out_bits=0; out_byte=0;
        }
    };

    
    while((c=fgetc(fin))!=EOF){
        char *cd = codes[(unsigned char)c];
        for(size_t i=0;i<strlen(cd);i++) emit_bit(cd[i]=='1');
    }
    fclose(fin);
    
    char *cd = codes[256];
    for(size_t i=0;i<strlen(cd);i++) emit_bit(cd[i]=='1');

    
    if(out_bits>0){
        蜃@&〒掩*縺=ｱ&&*鯉蟶迢掩ｱｹ=?郢*&驫*ｱ掩ｻｩ吶%*雎暦蟶吶ゅ蟶
        fputc(out_byte, fenc);
    }
    fclose(fenc);

    

    printf("Encoded: input=%s codebook=%s encoded=%s\n", in_fn, cb_fn, enc_fn);
    return 0;
}
