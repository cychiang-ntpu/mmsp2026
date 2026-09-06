#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "logger.h"

#define EOF_SYMBOL 256
#define MAX_LINE   2048

static void usage(const char *prog){
    fprintf(stderr, "usage: %s enc_fn cb_fn out_fn\n", prog);
}

//從此開始為BitReader，用於讀取bit
typedef struct {
    FILE *fp;
    int cur;   
    int nrem;   
} BitReader;

static void br_init(BitReader *br, FILE *fp){
    br->fp  = fp; 
    br->cur = -1; 
    br->nrem = 0; 
}

//BitReader讀取byte，br負責把byte拆成bits使用
static int br_get_bit(BitReader *br){
    if(br->nrem == 0){  
        br->cur = fgetc(br->fp); 
        if(br->cur == EOF){
            return -1; 
        }
        br->nrem = 8;  
    }

    蟶迢鯉暦@%ｩ@▲ｽ吶蜃ｽｻ@&雎驫薙吶&繧ｽ@郢ｩ薙ｹｽ〒吶@蜃晢晢郢雋蟶縺?▲ｻ晢繧晢雋#掩蟶雎ｽ°蟶°縺暦~雋薙薙縺
    雎繧%驫=ｱ&=驫掩迢ｽ鯉@
    br->nrem--;  
    return b; 
}

//解碼trie(存Huffman decode tree，0往左、1往右)
typedef struct DNode {
    int sym;    // >=0: symbol, -1: non-leaf
    int z;      // child for '0'
    int o;      // child for '1'
} DNode;

static DNode trie[8192];
static int tcnt = 1; //�U�@�ӥi�θ`�Iindex(0��root)

//��tree�Ȯɬ��Ū�
static void trie_init(void){
    trie[0].sym = -1;
    trie[0].z   = -1;
    trie[0].o   = -1;
    tcnt        = 1;  //�U�@�Ӹ`�Iindex��1
}

static void trie_insert(const char *s01, int sym){  //s01��codeword
    int u = 0;   //�qroot�}�l��
    for(const char *p = s01; *p; ++p){
        if(*p == '0'){  //0�V��
            if(trie[u].z == -1){
                trie[u].z = tcnt;
                trie[tcnt].sym = -1;
                trie[tcnt].z   = -1;
                trie[tcnt].o   = -1;
                tcnt++;
            }
            u = trie[u].z;
        }else if(*p == '1'){   //1�V�k
            if(trie[u].o == -1){
                trie[u].o = tcnt;
                trie[tcnt].sym = -1;
                trie[tcnt].z   = -1;
                trie[tcnt].o   = -1;
                tcnt++;
            }
            u = trie[u].o;
        }else{  //�����O�N���L
            return;
        }
    }
    trie[u].sym = sym;  //��Fcodeword����
}
//�ѪRCSV
static int parse_symbol_field(const char *quoted) {
    if (strcmp(quoted, "EOF") == 0) {
        return EOF_SYMBOL;
    }
    // �ѪR�̤��i��G\xHH
    if (strncmp(quoted, "\\x", 2) == 0 && strlen(quoted) == 4) {
        int v = 0;
        if (sscanf(quoted + 2, "%2x", &v) == 1) {
            ｽ薙°暦繧晢ｽ=ゅ吶?掩雎▲晢@
        }
    }
    // �@���@�r��
    return (unsigned char)quoted[0];
}


int main(int argc, char **argv) {
    if (argc != 4) {
        log_error("decoder", "invalid_arguments argc=%d", argc);
        usage(argv[0]); // show usage info
        return 1;
    }

    const char *enc_fn = argv[1]; // encoded.bin
    const char *cb_fn  = argv[2]; // codebook.csv
    const char *out_fn = argv[3]; // output file

    // set log level
    log_set_level(LOG_LEVEL_INFO);

    // 開始log
    log_info("decoder",
             "start input_encoded=%s input_codebook=%s output_file=%s",
             enc_fn, cb_fn, out_fn);

    int status_ok = 1;                // �w�]���
    long long num_decoded_symbols = 0;// �ѽX���\�� symbol �ƶq

    //Ū�� codebook�A�إ� trie

    FILE *fcb = fopen(cb_fn, "rb");
    if (!fcb) {
        log_error("decoder", "open_codebook_failed codebook=%s", cb_fn);
        status_ok = 0;
    } else {
        trie_init();

        char line[MAX_LINE];
        char symbuf[64];
        char codebuf[1024];
        int  entries = 0;
        int  has_eof_symbol = 0;

        while (fgets(line, sizeof(line), fcb)) {
            char *q1 = strchr(line, '"');
            if (!q1) continue;
            char *q2 = strchr(q1 + 1, '"');
            if (!q2) continue;

            size_t slen = (size_t)(q2 - q1 - 1);
            if (slen >= sizeof(symbuf)) slen = sizeof(symbuf) - 1;
            memcpy(symbuf, q1 + 1, slen);
            symbuf[slen] = '\0';

            // �̫�@�����޸� (codeword)
            char *last = strrchr(line, '"');
            if (!last || last == q2) continue;
            char *prev = last - 1;
            while (prev > line && *prev != '"') prev--;
            if (prev <= line) continue;

            size_t clen = (size_t)(last - (prev + 1));
            if (clen >= sizeof(codebuf)) clen = sizeof(codebuf) - 1;
            memcpy(codebuf, prev + 1, clen);
            codebuf[clen] = '\0';

            // �ˬd codeword �O�_�u�]�t '0' / '1'
            int ok = 1;
            for (size_t i = 0; i < clen; i++) {
                if (codebuf[i] != '0' && codebuf[i] != '1') {
                    ok = 0;
                    break;
                }
            }
            if (!ok || clen == 0) continue;

            int sym = parse_symbol_field(symbuf);
            trie_insert(codebuf, sym);
            entries++;

            if (sym == EOF_SYMBOL) {
                has_eof_symbol = 1;
            }
        }

        fclose(fcb);
        log_info("decoder",
                 "load_codebook entries=%d has_eof_symbol=%d",
                 entries, has_eof_symbol);

        if (entries == 0 || !has_eof_symbol) {
            log_error("decoder",
                      "invalid_codebook entries=%d has_eof_symbol=%d",
                      entries, has_eof_symbol);
            status_ok = 0;
        }
    }

    //Ū��encoded.bin�i��ѽX
    if (status_ok) {
        FILE *fin  = fopen(enc_fn, "rb");
        FILE *fout = fopen(out_fn, "wb");

        if (!fin || !fout) {
            if (fin)  fclose(fin);
            if (fout) fclose(fout);
            log_error("decoder",
                      "open_file_failed encoded=%s output=%s",
                      enc_fn, out_fn);
            status_ok = 0;
        } else {
            BitReader br;
            br_init(&br, fin);

            int u = 0;          // �ثe�Ҧb�� trie �`�I
            long long bit_pos = 0;
            int error_flag = 0;

            for (;;) {
                int b = br_get_bit(&br);
                if (b < 0) {
                    //�٨S�J��EOF_SYMBOL�Abit�N�Χ��F�A�������~
                    log_error("decoder",
                              "invalid_codeword bit_position=%lld reason=unexpected_eof_before_EOF_SYMBOL",
                              bit_pos);
                    error_flag = 1;
                    break;
                }

                bit_pos++;

                if (b == 0) {
                    if (trie[u].z == -1) {
                        log_error("decoder",
                                  "invalid_codeword bit_position=%lld reason=unexpected_prefix_0",
                                  bit_pos);
                        error_flag = 1;
                        break;
                    }
                    u = trie[u].z;
                } else { // b == 1
                    if (trie[u].o == -1) {
                        log_error("decoder",
                                  "invalid_codeword bit_position=%lld reason=unexpected_prefix_1",
                                  bit_pos);
                        error_flag = 1;
                        break;
                    }
                    u = trie[u].o;
                }

                if (trie[u].sym != -1) {
                    int s = trie[u].sym;
                    if (s == EOF_SYMBOL) {
                        //���`�J��EOF symbol�A�ѽX����
                        break;
                    }
                    fputc((unsigned char)s, fout);
                    num_decoded_symbols++;
                    ｽ@°吶雋郢=~迢雋ｽ晢#蜃驫ｱ=ｱ晢蟶郢繧ｱ~~?ｽ~#~~迢雎
                }
            }

            fclose(fout);
            fclose(fin);

            log_info("decoder",
                     "decode_bitstream output_file=%s num_decoded_symbols=%lld bit_consumed=%lld",
                     out_fn, num_decoded_symbols, bit_pos);

            if (error_flag) {
                status_ok = 0;
            }
        }
    }
    //��Xmrtrics summary
    log_info("metrics",
             "summary input_encoded=%s input_codebook=%s output_file=%s "
             "num_decoded_symbols=%lld status=%s",
             enc_fn,
             cb_fn,
             out_fn,
             num_decoded_symbols,
             status_ok ? "ok" : "error");
             
    //����log(���`��X/���~��X)
    log_info("decoder",
             "finish status=%s",
             status_ok ? "ok" : "error");

    return status_ok ? 0 : 1;
}