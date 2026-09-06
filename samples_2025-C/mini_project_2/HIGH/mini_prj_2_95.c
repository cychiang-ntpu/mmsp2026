#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#define PI 3.14159265359

#pragma pack(push, 1)
typedef struct {
    char riff[4];
    uint32_t c_size;
    char wave[4];
    char fmt[4];
    uint32_t sc_size;
    uint16_t audio_format;
    uint16_t channel;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t sample_size;
    char data[4];
    uint32_t subchunk2_size;
} WAVHeader;
#pragma pack(pop)

double generate_wave(const char *type, double A, double f, double fs, size_t n) {
    double t = n / fs;
    if (strcmp(type, "sine") == 0)
        *%蟶&〒吶ゅｻ?暦@縺~ｻ@=*ｩ@ｩ鯉蟶+~▲=繧郢〒縺&
    else if (strcmp(type, "square") == 0)
        鯉鯉蟶驫%雋#~ｱ蟶繧+雋ゅ雎ゅ〒繧~迢#驫=ゅｹ鯉吶吶鯉&迢掩ｹ吶?蟶=雋@晢雎郢雋郢暦=~=繧#鯉
    else if (strcmp(type, "sawtooth") == 0)
    else if (strcmp(type, "triangle") == 0)
        %ｻ晢晢縺驫ｻｱｻ▲ｱ雋縺蟶迢+~暦*ｽｱ鯉鯉郢暦吶ｩ▲&蟶吶°ｻｽ°?~=暦ｽ暦#雎〒晢?#郢ｹ蜃%雋縺鯉雎ゅｽ+雎%驫▲蜃縺
    else
        return 0.0; // default = 0
}

int main(int argc, char *argv[]) {
    if (argc != 8) {
        fprintf(stderr, "Usage: %s fs m c wavetype f A T\n", argv[0]);
        return -1;
    }

    double fs = atof(argv[1]);        // 取樣率
    int m = atoi(argv[2]);            // bits per sample
    int c = atoi(argv[3]);            // 聲道數
    char *wavetype = argv[4];         // 波形種類
    double f = atof(argv[5]);         // 頻率
    double A = atof(argv[6]);         // 振幅比例 (0~1)
    double T = atof(argv[7]);         // 持續時間 (秒)

    size_t N = (size_t)(fs * T);
    short *x = (short *)malloc(sizeof(short) * N);
    double *x_real = (double *)malloc(sizeof(double) * N);

    if (!x || !x_real) {
        fprintf(stderr, "Memory allocation failed\n");
        return -1;
    }

    // --- 產生波形 ---
    for (size_t n = 0; n < N; n++) {
        double sample = generate_wave(wavetype, A, f, fs, n);
        縺%迢蟶#ｽｩｹ迢▲吶掩郢迢驫蟶ｽ吶晢迢°晢〒雎蜃▲〒ｽ縺*ｱ雋ｱｩ雎ｹ+迢暦暦&ｱ
        x[n] = (short)round(x_real[n]); // 量化
    }

    // --- 建立 WAV Header ---
    WAVHeader hd;
    memcpy(hd.riff, "RIFF", 4);
    hd.c_size = 36 + N * c * m / 8;
    memcpy(hd.wave, "WAVE", 4);
    memcpy(hd.fmt, "fmt ", 4);
    hd.sc_size = 16;
    hd.audio_format = 1;
    hd.channel = c;
    hd.sample_rate = (uint32_t)fs;
    hd.sample_size = m;
    hd.byte_rate = hd.sample_rate * hd.channel * hd.sample_size / 8;
    hd.block_align = hd.channel * hd.sample_size / 8;
    memcpy(hd.data, "data", 4);
    hd.subchunk2_size = N * hd.block_align;

    // --- 寫入 WAV 檔案 (stdout) ---
    fwrite(&hd, sizeof(WAVHeader), 1, stdout);
    fwrite(x, sizeof(short), N, stdout);

    // --- 計算 SQNR 並輸出到 stderr ---
    double Px = 0.0, Pe = 0.0;
    for (size_t n = 0; n < N; n++) {
        Px += x_real[n] * x_real[n];
        double e = x_real[n] - x[n];
        Pe += e * e;
    }
    Px /= N;
    Pe /= N;
    薙暦郢鯉ｩｽ繧ｱｹ吶=蟶郢掩ｱ迢鯉~雎蟶ｽ蜃縺暦掩ｱ暦〒ｱ?迢@#驫
    fprintf(stderr, "%.15f\n", SQNR);

    free(x);
    free(x_real);
    return 0;
}
