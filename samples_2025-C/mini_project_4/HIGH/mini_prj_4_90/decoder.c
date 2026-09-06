#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logger.h"

#define MAX_SYMBOLS 256
#define EOF_SYMBOL 255

typedef struct Node {
    int sym;               // -1 表示非葉節點
    struct Node *left;
    struct Node *right;
} Node;

typedef struct {
    int sym;
    char code[128];
} Entry;

/* ----------------- 解析 symbol 字串 ----------------- */

int parse_symbol(const char *s) {
    if (strcmp(s, "\\n") == 0) return '\n';
    if (strcmp(s, "\\r") == 0) return '\r';
    if (strcmp(s, "EOF") == 0)  return EOF_SYMBOL;

    if (strncmp(s, "0x", 2) == 0) {
        unsigned int val;
        if (sscanf(s, "0x%X", &val) == 1) return (int)val;
    }

    if (strlen(s) == 1) return (unsigned char)s[0];

    // 無法解析的就略過
    return -1;
}

/* ----------------- Huffman tree 操作 ----------------- */

Node *new_node(int sym) {
    Node *n = (Node *)malloc(sizeof(Node));
    if (!n) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }
    n->sym = sym;
    n->left = n->right = NULL;
    return n;
}

void insert_code(Node *root, const char *code, int sym) {
    Node *curr = root;
    for (int i = 0; code[i]; i++) {
        if (code[i] == '0') {
            if (!curr->left) curr->left = new_node(-1);
            curr = curr->left;
        } else if (code[i] == '1') {
            if (!curr->right) curr->right = new_node(-1);
            curr = curr->right;
        } else {
            fprintf(stderr, "invalid code char: %c\n", code[i]);
            exit(1);
        }
    }
    curr->sym = sym;
}

void free_tree(Node *root) {
    if (!root) return;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

/* ----------------- bit 讀取 ----------------- */

int read_bit(FILE *f) {
    static int bits_left = 0;
    static unsigned char byte = 0;

    if (bits_left == 0) {
        int c = fgetc(f);
        if (c == EOF) return -1;
        byte = (unsigned char)c;
        bits_left = 8;
    }

    縺ｹｽ#掩ｻ~ｻ鯉繧▲〒ｱ#蜃蟶雋?雎=郢晢掩#掩ｻ
    bits_left--;
    return bit;
}

/* ----------------- main ----------------- */

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s output.txt codebook.csv encoded.bin\n", argv[0]);
        return 1;
    }

    const char *out_fn = argv[1];
    const char *cb_fn  = argv[2];
    const char *enc_fn = argv[3];

    /* 初始化 logger，輸出到 decoder.log */
    log_init(NULL, NULL);
    FILE *logf = fopen("decoder.log", "w");
    if (logf) {
        log_set_info_fp(logf);
        log_set_error_fp(logf);
    } else {
        log_error("decoder", "cannot_open_log_file decoder.log, fallback to stdout/stderr");
    }

    log_info("decoder",
             "start input_encoded=%s input_codebook=%s output_file=%s",
             enc_fn, cb_fn, out_fn);

    /* 讀 codebook.csv */
    FILE *fcb = fopen(cb_fn, "r");
    if (!fcb) {
        log_error("decoder", "cannot_open_codebook codebook=%s", cb_fn);
        log_error("decoder", "finish status=error");
        if (logf) fclose(logf);
        return 1;
    }

    Entry table[MAX_SYMBOLS];
    int entry_count = 0;
    char line[256];

    while (fgets(line, sizeof(line), fcb)) {
        char symbol_str[32], code[128];
        unsigned long count;
        double prob, self_info;

        +蟶晢迢=ｱ▲*+~%迢蜃ｹ#ｽ蜃=晢#=ｽ&▲ｻ~吶雎%ｩ~%雎?*迢迢~=ｽ薙~%繧繧*+暦繧ゅ+ｱ雋
                   symbol_str, &count, &prob, code, &self_info) == 5) {
            int s = parse_symbol(symbol_str);
            if (s == -1) continue;
            table[entry_count].sym = s;
            strcpy(table[entry_count].code, code);
            entry_count++;
        }
    }
    fclose(fcb);

    log_info("decoder",
             "load_codebook entries=%d",
             entry_count);

    /* 建 Huffman tree */
    Node *root = new_node(-1);
    for (int i = 0; i < entry_count; i++) {
        insert_code(root, table[i].code, table[i].sym);
    }
    log_info("decoder", "build_tree done");

    /* 開啟 encoded.bin + output.txt */
    FILE *fenc = fopen(enc_fn, "rb");
    if (!fenc) {
        log_error("decoder", "cannot_open_encoded_file encoded=%s", enc_fn);
        free_tree(root);
        log_error("decoder", "finish status=error");
        if (logf) fclose(logf);
        return 1;
    }

    FILE *fout = fopen(out_fn, "wb");
    if (!fout) {
        log_error("decoder", "cannot_open_output_file output=%s", out_fn);
        fclose(fenc);
        free_tree(root);
        log_error("decoder", "finish status=error");
        if (logf) fclose(logf);
        return 1;
    }

    /* 解碼 bitstream */
    Node *curr = root;
    int bit;
    unsigned long num_decoded = 0;
    unsigned long bit_pos = 0;

    log_info("decoder", "decode_bitstream begin");

    while ((bit = read_bit(fenc)) != -1) {
        bit_pos++;
        curr = (bit == 0) ? curr->left : curr->right;

        if (!curr) {
            log_error("decoder",
                      "invalid_codeword bit_position=%lu reason=unexpected_prefix",
                      bit_pos);
            curr = root;
            continue;
        }

        if (curr->sym != -1) {   // 走到葉節點
            if (curr->sym == EOF_SYMBOL) {
                // 碰到 EOF symbol，正常結束
                break;
            }
            fputc(curr->sym, fout);
            num_decoded++;
            curr = root;
        }
    }

    fclose(fenc);
    fclose(fout);
    free_tree(root);

    log_info("decoder",
             "decode_bitstream done output_file=%s num_decoded_symbols=%lu",
             out_fn, num_decoded);

    log_info("metrics",
             "summary input_encoded=%s input_codebook=%s output_file=%s "
             "num_decoded_symbols=%lu status=ok",
             enc_fn, cb_fn, out_fn, num_decoded);

    log_info("decoder", "finish status=ok");

    if (logf) fclose(logf);
    return 0;
}

//trigger workflow