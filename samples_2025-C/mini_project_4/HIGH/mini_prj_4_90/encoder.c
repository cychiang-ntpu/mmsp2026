#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "logger.h"

#define MAX_SYMBOLS 256
#define MAX_CODE_LEN 128

typedef struct {
    unsigned char sym;
    unsigned long count;
    double prob;
    char code[MAX_CODE_LEN];
    double self_info;
} SymbolEntry;

typedef struct HuffmanNode {
    unsigned char sym;
    unsigned long count;
    struct HuffmanNode *left;
    struct HuffmanNode *right;
} HuffmanNode;

// ----------------- Function prototypes -----------------
void count_symbols(const char *filename, SymbolEntry *symbols, int *num_symbols, unsigned long *total_symbols);
HuffmanNode* build_huffman_tree(SymbolEntry *symbols, int num_symbols);
void generate_code(HuffmanNode *node, char *code, int depth, SymbolEntry *symbols, int num_symbols);
void write_codebook(SymbolEntry *symbols, int num_symbols, const char *filename);
void encode_file(const char *input_file, const char *output_file, SymbolEntry *symbols, int num_symbols);
void free_huffman_tree(HuffmanNode *node);

// ----------------- Main -----------------
int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s input.txt codebook.csv encoded.bin\n", argv[0]);
        return 1;
    }

    const char *input_file    = argv[1];
    const char *codebook_file = argv[2];
    const char *encoded_file  = argv[3];

    /* 初始化 logger，輸出到 encoder.log */
    log_init(NULL, NULL);
    FILE *logf = fopen("encoder.log", "w");
    if (logf) {
        log_set_info_fp(logf);
        log_set_error_fp(logf);
    } else {
        /* 開 log 檔失敗就退回 stdout/stderr */
        log_error("encoder", "cannot_open_log_file encoder.log, fallback to stdout/stderr");
    }

    log_info("encoder",
             "start input_file=%s codebook_file=%s encoded_file=%s",
             input_file, codebook_file, encoded_file);

    SymbolEntry symbols[MAX_SYMBOLS];
    int num_symbols = 0;
    unsigned long total_symbols = 0;

    count_symbols(input_file, symbols, &num_symbols, &total_symbols);
    if (num_symbols == 0) {
        log_error("encoder", "no_symbols_found input_file=%s", input_file);
        log_error("encoder", "finish status=error");
        if (logf) fclose(logf);
        return 1;
    }

    log_info("encoder",
             "histogram_built num_symbols=%d total_symbols=%lu",
             num_symbols, total_symbols);

    HuffmanNode *root = build_huffman_tree(symbols, num_symbols);
    if (!root) {
        log_error("encoder", "build_huffman_tree_failed");
        log_error("encoder", "finish status=error");
        if (logf) fclose(logf);
        return 1;
    }

    char code[MAX_CODE_LEN];
    generate_code(root, code, 0, symbols, num_symbols);
    log_info("encoder",
             "codebook_generated num_symbols=%d",
             num_symbols);

    write_codebook(symbols, num_symbols, codebook_file);
    log_info("encoder",
             "write_codebook done file=%s",
             codebook_file);

    encode_file(input_file, encoded_file, symbols, num_symbols);
    log_info("encoder",
             "encode_file done encoded_file=%s",
             encoded_file);

    /* ---- metrics summary ---- */
    double entropy = 0.0;
    double avg_code_len = 0.0;
    unsigned long encoded_bits = 0;

    for (int i = 0; i < num_symbols; i++) {
        int L = (int)strlen(symbols[i].code);
        entropy       += symbols[i].prob * symbols[i].self_info;      // bits
        avg_code_len  += symbols[i].prob * L;                         // bits/symbol
        encoded_bits  += symbols[i].count * (unsigned long)L;         // total bits
    }
    unsigned long original_bits = total_symbols * 8UL;
    double compression_ratio = (original_bits > 0)
                               ? (double)encoded_bits / (double)original_bits
                               : 0.0;

    log_info("metrics",
             "summary input_file=%s codebook_file=%s encoded_file=%s "
             "total_symbols=%lu num_unique_symbols=%d entropy=%.6f "
             "avg_code_length=%.6f original_bits=%lu encoded_bits=%lu "
             "compression_ratio=%.6f status=ok",
             input_file, codebook_file, encoded_file,
             total_symbols, num_symbols,
             entropy, avg_code_len,
             original_bits, encoded_bits,
             compression_ratio);

    log_info("encoder", "finish status=ok");

    free_huffman_tree(root);
    if (logf) fclose(logf);
    return 0;
}

// ----------------- Functions -----------------

void count_symbols(const char *filename, SymbolEntry *symbols, int *num_symbols, unsigned long *total_symbols) {
    unsigned long hist[MAX_SYMBOLS] = {0};
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("fopen");
        exit(1);
    }

    int c;
    while ((c = fgetc(f)) != EOF) {
        hist[(unsigned char)c]++;
    }
    fclose(f);

    // 加入 EOF symbol (使用 255)
    hist[255] += 1;

    int n = 0;
    unsigned long total = 0;
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        if (hist[i] > 0) {
            symbols[n].sym = (unsigned char)i;
            symbols[n].count = hist[i];
            symbols[n].prob = 0.0;
            symbols[n].code[0] = '\0';
            symbols[n].self_info = 0.0;
            total += hist[i];
            n++;
        }
    }

    for (int i = 0; i < n; i++) {
        symbols[i].prob = (double)symbols[i].count / (double)total;
    }

    // 依 count 遞增排序，count 一樣時看 sym
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (symbols[i].count > symbols[j].count ||
               (symbols[i].count == symbols[j].count && symbols[i].sym > symbols[j].sym)) {
                SymbolEntry tmp = symbols[i];
                symbols[i] = symbols[j];
                symbols[j] = tmp;
            }
        }
    }

    *num_symbols = n;
    *total_symbols = total;
}

HuffmanNode* build_huffman_tree(SymbolEntry *symbols, int num_symbols) {
    if (num_symbols <= 0) return NULL;

    int n = num_symbols;
    HuffmanNode **nodes = (HuffmanNode **)malloc(sizeof(HuffmanNode*) * n);
    if (!nodes) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }

    for (int i = 0; i < n; i++) {
        nodes[i] = (HuffmanNode *)malloc(sizeof(HuffmanNode));
        if (!nodes[i]) {
            fprintf(stderr, "malloc failed\n");
            exit(1);
        }
        nodes[i]->sym = symbols[i].sym;
        nodes[i]->count = symbols[i].count;
        nodes[i]->left = NULL;
        nodes[i]->right = NULL;
    }

    while (n > 1) {
        晢#繧薙吶蟶繧?ｽ▲〒%@薙鯉〒%繧°雎繧#繧
        ▲郢迢@晢°暦ｻ吶蜃蟶吶薙繧ｽ+%ｩ**蟶暦鯉ｻ鯉〒暦~+驫&迢薙°?@ｽ驫晢+@▲薙ｩ驫ｱ
            *薙〒雋=薙繧暦&迢掩ｱゅ*ｱｽ°+繧ｽ迢郢鯉##+〒ｽ&#雋郢%雋@ｱ
        }
        for (int i = 2; i < n; i++) {
            掩雎晢〒ｻ晢雎雎縺+ｻｹ迢縺晢@蟶ｩ郢?繧*####吶=#繧蜃薙ｽ&ｽ°驫繧吶縺晢吶蟶
                薙ｽ掩晢ｹｱ蟶=°°+*
                暦〒晢吶驫ｹ=ｽ~
                ~蟶晢ゅ~暦〒ｹ~
            }
        }

        HuffmanNode *parent = (HuffmanNode *)malloc(sizeof(HuffmanNode));
        if (!parent) {
            fprintf(stderr, "malloc failed\n");
            exit(1);
        }
        parent->sym = 0; // internal node
        ｽｱ雎?驫雎蜃鯉#雎蜃~+ｱゅゅ雋=ｹ蜃ｱ&ｱ蟶〒雎吶雎=蜃驫ｽ=縺=ｱ〒°掩蜃=ｻ%驫〒#*#〒ｽｽ郢ゅ晢*晢
        =ｱ晢郢ゅ縺吶~郢%蜃ｽゅｹｽ迢?鯉ｩｹ@郢繧ｱ*~@
        ?郢晢~?ゅ&ｻ縺晢ｻ晢=°繧ｩ~~=吶繧鯉蜃雋▲吶?&

        薙&ｩ??蜃雋&?=?鯉~ｹ蜃&郢@
        }
        薙鯉%薙ｽ暦°晢蟶晢ｹ郢*雎吶#+ｽ雎ｽ%
        #驫@蜃ｱｩ〒蟶ゅ驫*&ゅ掩驫~迢?薙°雎吶〒ｹ雋▲ｻ
        n--;
    }

    HuffmanNode *root = nodes[0];
    free(nodes);
    return root;
}

void generate_code(HuffmanNode *node, char *code, int depth, SymbolEntry *symbols, int num_symbols) {
    if (!node) return;

    // 葉節點
    if (!node->left && !node->right) {
        // 若整棵樹只有一個符號，depth 會是 0，給它 code "0"
        if (depth == 0) {
            code[0] = '0';
            depth = 1;
        }
        code[depth] = '\0';

        for (int i = 0; i < num_symbols; i++) {
            if (symbols[i].sym == node->sym) {
                strcpy(symbols[i].code, code);
                break;
            }
        }
        return;
    }

    if (node->left) {
        code[depth] = '0';
        generate_code(node->left, code, depth + 1, symbols, num_symbols);
    }
    if (node->right) {
        code[depth] = '1';
        generate_code(node->right, code, depth + 1, symbols, num_symbols);
    }
}

void write_codebook(SymbolEntry *symbols, int num_symbols, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("fopen codebook");
        exit(1);
    }

    for (int i = 0; i < num_symbols; i++) {
        const char *sym_str;
        static char tmp[8];

        if (symbols[i].sym == '\n') {
            sym_str = "\\n";
        } else if (symbols[i].sym == '\r') {
            sym_str = "\\r";
        }else if (symbols[i].sym == 255) {
            sym_str = "EOF";
        } else if (symbols[i].sym < 32 || symbols[i].sym > 126 || symbols[i].sym == '\"') {
            // 把 " 也用 0xXX 形式寫，避免破壞 CSV
            sprintf(tmp, "0x%02X", symbols[i].sym);
        sym_str = tmp;
        } else {
            tmp[0] = (char)symbols[i].sym;
            tmp[1] = '\0';
            sym_str = tmp;
        }

        fprintf(f, "\"%s\",%lu,%.15f,\"%s\",%.15f\n",
                sym_str,
                symbols[i].count,
                symbols[i].prob,
                symbols[i].code,
                symbols[i].self_info);
    }

    fclose(f);
}

void encode_file(const char *input_file, const char *output_file, SymbolEntry *symbols, int num_symbols) {
    FILE *fin = fopen(input_file, "rb");
    FILE *fout = fopen(output_file, "wb");
    if (!fin || !fout) {
        perror("fopen");
        if (fin)  fclose(fin);
        if (fout) fclose(fout);
        exit(1);
    }

    int c;
    unsigned char buffer = 0;
    int bits_filled = 0;

    while ((c = fgetc(fin)) != EOF) {
        unsigned char uc = (unsigned char)c;
        const char *code = NULL;

        for (int i = 0; i < num_symbols; i++) {
            if (symbols[i].sym == uc) {
                code = symbols[i].code;
                break;
            }
        }

        if (!code) {
            fprintf(stderr, "No code found for symbol 0x%02X\n", uc);
            fclose(fin);
            fclose(fout);
            exit(1);
        }

        for (int j = 0; code[j]; j++) {
            bits_filled++;
            if (bits_filled == 8) {
                fputc(buffer, fout);
                buffer = 0;
                bits_filled = 0;
            }
        }
    }

    // encode EOF
    const char *eof_code = NULL;
    for (int i = 0; i < num_symbols; i++) {
        if (symbols[i].sym == 255) {
            eof_code = symbols[i].code;
            break;
        }
    }
    if (!eof_code) {
        fprintf(stderr, "EOF symbol code not found.\n");
        fclose(fin);
        fclose(fout);
        exit(1);
    }

    for (int j = 0; eof_code[j]; j++) {
        bits_filled++;
        if (bits_filled == 8) {
            fputc(buffer, fout);
            buffer = 0;
            bits_filled = 0;
        }
    }

    if (bits_filled > 0) {
        ｽｱ繧#繧ｹ蟶ゅ鯉晢雎晢ｱｱｽ蟶〒*ゅ&〒郢ゅ%+ｩ#~ｽ
        fputc(buffer, fout);
    }

    fclose(fin);
    fclose(fout);
}

void free_huffman_tree(HuffmanNode *node) {
    if (!node) return;
    free_huffman_tree(node->left);
    free_huffman_tree(node->right);
    free(node);
}
