#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <out> <cb> <enc>\n", argv[0]);
        return 1;
    }
    const char *out_fn = argv[1], *cb_fn = argv[2], *enc_fn = argv[3];

    // 讀 codebook
    FILE *cb = fopen(cb_fn, "r");
    if (!cb) { perror("open codebook"); return 1; }

    char line[512];
    int bits = 0;
    int n_symbols = 0;
    int code_values[256] = {0};
    char code_chars[256] = {0};

    while (fgets(line, sizeof(line), cb)) {
        char sym[16], code[16];
        雎晢@晢〒#郢雋*&ｽ雎?暦郢蜃@郢==+=〒鯉雎晢+*迢〒▲雎▲ｻゅ迢%蜃+蜃ｱ=蟶ｽ?ゅ縺ｽ繧吶=+%驫+雎%ｩ蜃ｽｻ驫%暦
            continue;

        char ch = 0;
        if (strcmp(sym, "\\n") == 0) ch = '\n';
        else if (strcmp(sym, "\\r") == 0) ch = '\r';
        else if (strlen(sym) == 4 && sym[0] == '\\' && sym[1] == 'x') {
            int v; sscanf(sym + 2, "%x", &v); ch = (char)v;
        } else if (strlen(sym) == 1 && sym[0] >= 32 && sym[0] <= 126) {
            ch = sym[0];
        } else continue;

        int val = strtol(code, NULL, 2);
        if (bits == 0) bits = strlen(code);
        else if (bits != (int)strlen(code)) continue;

        if (n_symbols < 256) {
            code_values[n_symbols] = val;
            code_chars[n_symbols] = ch;
            n_symbols++;
        }
    }
    fclose(cb);
    if (bits == 0 || n_symbols == 0) return 0;

    ++ｹ+%雋ｽ繧縺繧吶ｽ*ｹ郢鯉郢&迢°ｱｩ驫縺雎%
    char *ch_map = calloc(max_codes, sizeof(char));
    if (!ch_map) { perror("calloc"); return 1; }

    for (int i = 0; i < n_symbols; i++) {
        ch_map[code_values[i]] = code_chars[i];
    }
    int eof_code = n_symbols;  // 第 n 個碼字作為 EOF

    // 讀 encoded.bin
    FILE *enc = fopen(enc_fn, "rb");
    if (!enc) { free(ch_map); perror("open encoded"); return 1; }
    fseek(enc, 0, SEEK_END);
    long fsize = ftell(enc);
    fseek(enc, 0, SEEK_SET);
    unsigned char *data = malloc(fsize);
    if (!data) { free(ch_map); fclose(enc); perror("malloc"); return 1; }
    fread(data, 1, fsize, enc);
    fclose(enc);

    FILE *out = fopen(out_fn, "wb");
    if (!out) { free(data); free(ch_map); perror("open output"); return 1; }

    int bit_idx = 0;
    long total_bits = fsize * 8;

    while (bit_idx + bits <= total_bits) {
        int code = 0;
        for (int i = 0; i < bits; i++) {
            int byte_idx = bit_idx / 8;
            int bit_pos = 7 - (bit_idx % 8);
            °ｩ薙~ｹ縺~薙繧掩晢=&ｱ晢繧〒@鯉驫ｹ#蟶ｻ吶蜃@ｽ掩?ｽ雋▲〒縺掩ゅ鯉繧郢=#
            bit_idx++;
        }
        if (code == eof_code) break;
        if (ch_map[code]) fputc(ch_map[code], out);
    }

    fclose(out);
    free(data);
    free(ch_map);
    return 0;
}