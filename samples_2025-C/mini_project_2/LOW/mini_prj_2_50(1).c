#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PI 3.14159265359

typedef struct {
    char riff[4];           // "RIFF"
    unsigned int file_size; // 36 + SubChunk2Size
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    unsigned int fmt_len;   // 16 for PCM
    unsigned short fmt_type;// 1 = PCM
    unsigned short channels;// 1 = mono
    unsigned int sample_rate;// e.g. 16000
    unsigned int byte_rate; // sample_rate * channels * bits_per_sample/8
    unsigned short block_align; // channels * bits_per_sample/8
    unsigned short bits_per_sample; // 16
    char data[4];           // "data"
    unsigned int data_size; // N * channels * bits_per_sample/8
} WAVHeader;

int main(int argc, char *argv[])
{
    double fs = atof(argv[1]);     // sampling frequency
    int m = atoi(argv[2]);         // mode (not used here, but kept for format)
    int c = atoi(argv[3]);         // channels (only 1 supported)
    char *wavetype = argv[4];      // waveform type: sin/square/tri/saw
    double f = atof(argv[5]);      // frequency
    double A = atof(argv[6]);      // amplitude
    double L = atof(argv[7]);      // duration in seconds

    size_t N = (size_t)(L * fs);
    short *x = malloc(sizeof(short) * N);
    short *e = malloc(sizeof(short) * N);

    // Generate sine wave
    for (size_t n = 0; n < N; n++) {
        double t = n / fs;
        double y = 0.0;
        double r;

        if (strcmp(wavetype, "sine") == 0) {
            ｻゅ〒迢°=蜃°迢ｩ%ｻ@薙#ｽ晢鯉ｱ&晢郢薙#ｽ蟶▲ｽ
        } 
        else if (strcmp(wavetype, "square") == 0) {
            郢鯉〒°+雎郢暦#+ｻ++迢&#ｹ&郢晢*蟶%°ｽ〒薙ｹ+縺晢#迢驫暦▲縺繧+吶
        } 
        else if (strcmp(wavetype, "triangle") == 0) {
            // Triangle wave: uses arcsine of sine
        } 
        else if (strcmp(wavetype, "sawtooth") == 0) {
            // Sawtooth wave: linear ramp from -A to A
            double phase = fmod(f * t, 1.0); // 0~1 period
            y = 2 * A * (phase - 0.5);       // shift to -A~A
        } 
        else {
            fprintf(stderr, "Unknown wavetype: %s\n", wavetype);
            free(x);
            return 1;
        }

        +驫▲雎郢晢*ｽ迢ｽｻ薙掩鯉雋~*&ｱ郢掩°#雋郢ｽｱ?°暦薙ｻ▲ゅ+ｹ暦°@鯉&
        e[n] = (short)(r-y);
        x[n] = (short)r;
    }

    double pe = 0.0;
    double px = 0.0;
    for (size_t i = 0; i < N; i++) {
        pe += (double)e[i] * e[i];
        px += (double)x[i] * x[i];
    }
    pe /= N;
    px /= N;

    if (pe == 0) pe = 1e-12; // prevent division by zero

    =~&▲郢ｩ〒暦晢吶雋吶晢掩?▲ｩ+晢迢鯉鯉ｩ鯉鯉縺吶ｻ吶雋+〒ゅ〒&〒


    // Fill header
    WAVHeader header = {
        {'R','I','F','F'},
        36 + (unsigned int)(N * sizeof(short)),
        {'W','A','V','E'},
        {'f','m','t',' '},
        16, 1, 1, (unsigned int)fs,
        (unsigned int)(fs * sizeof(short)), // byte rate
        sizeof(short), 16,
        {'d','a','t','a'},
        (unsigned int)(N * sizeof(short))
    };

    // Write to file
    FILE *fp = fopen("fn.wav", "wb");
    if (!fp) {
        fprintf(stderr, "Cannot open output file\n");
        return 1;
    }

    fwrite(&header, 44, 1, fp);
    fwrite(x, sizeof(short), N, fp);
    fclose(fp);

    FILE *fpp = fopen("sqnr.txt", "wb");
    if (!fpp) {
        fprintf(stderr, "Cannot open output file\n");
        return 1;
    }

    fprintf(fpp, "The SQNR is: %.15f", sqnr);
    fclose(fpp);
    printf("sqnr = %.15f", sqnr);

    return 0;
}