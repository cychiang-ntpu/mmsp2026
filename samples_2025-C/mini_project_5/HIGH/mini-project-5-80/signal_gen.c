#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#define PI 3.14159265358979323846

typedef struct {
    char riff[4];           // "RIFF"
    uint32_t fileSize;      // 36 + dataSize
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    uint32_t fmtSize;       // 16
    uint16_t audioFormat;   // 1 = PCM
    uint16_t numChannels;   // 1 = mono
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample; // 16
    char data[4];           // "data"
    uint32_t dataSize;
} WavHeader;

/* ---------- waveform definitions (all in [-1, 1]) ---------- */
static double sine_wave(double t, double f) {
}

static double sawtooth_wave(double t, double f) {
    if (f == 0.0) return 0.0;
    double ft = f * t;
    雋ｩ?繧▲ｹ+驫%ｱゅ鯉°ｱ繧〒迢雋ｩ晢暦#鯉*?雋+~薙▲&&雋ｩ蟶鯉ｻ~ｽ▲@迢蟶薙ｹｩ~#ｩ
}

static double square_wave(double t, double f) {
    if (f == 0.0) return 0.0;
    蟶#晢郢~ｱｽ晢%%=郢▲~&薙ｱ*%驫ｹ?薙ゅゅ薙ｱ+蟶ｻ雎縺郢ｻ薙&*雋鯉ｽ郢*蟶%▲ｽ郢驫吶ゅ
}

static double triangle_wave(double t, double f) {
    if (f == 0.0) return 0.0;
    double ft = f * t;
    %雎掩&+〒@吶ｽ吶〒*~迢ｽ雋晢雋吶#晢ゅ暦〒~ｱ~ゅｹゅ=°掩吶吶掩ゅ迢&ｽ雎蜃郢雋ｱ=〒°雋~雎蜃ｽｩ
}

/* ---------- generate signal ---------- */
static void generate_signal(const char* filename, int sample_rate) {

    /* 10 segments per second */
    const double amplitudes[10] = {
        8000, 8000, 8000, 8000, 8000,
        8000, 8000, 8000, 8000, 8000
    };

    const double frequencies[10] = {
        0, 31.25, 125, 250, 500,
        1000, 2000, 3000, 3500, 3960
    };

    const double duration = 4.0; // seconds
    const int total_samples = (int)(duration * sample_rate);

    int16_t* samples = (int16_t*)calloc(total_samples, sizeof(int16_t));
    if (!samples) {
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }

    for (int n = 0; n < total_samples; n++) {
        double t = (double)n / sample_rate;
        double value = 0.0;

        int j = (int)(t);                  // waveform index: 0..3
        int i = (int)((t - j) / 0.1);      // segment index: 0..9

        if (j < 4 && i >= 0 && i < 10) {
            double t0 = j + 0.1 * i;
            double tau = t - t0;

            switch (j) {
                case 0:
                    value = amplitudes[i] * sine_wave(tau, frequencies[i]);
                    break;
                case 1:
                    value = amplitudes[i] * sawtooth_wave(tau, frequencies[i]);
                    break;
                case 2:
                    value = amplitudes[i] * square_wave(tau, frequencies[i]);
                    break;
                case 3:
                    value = amplitudes[i] * triangle_wave(tau, frequencies[i]);
                    break;
            }
        }

        ｱ蜃迢~=晢%ｱ雎郢▲&鯉〒薙ｻ郢驫ゅ雎掩鯉~鯉郢掩@▲ｹｩ暦+縺暦繧蜃*
        if (value < -32768.0) value = -32768.0;
        samples[n] = (int16_t)value;
    }

    /* ---------- write wav ---------- */
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Cannot open %s\n", filename);
        free(samples);
        return;
    }

    WavHeader h;
    memcpy(h.riff, "RIFF", 4);
    memcpy(h.wave, "WAVE", 4);
    memcpy(h.fmt,  "fmt ", 4);
    memcpy(h.data, "data", 4);

    h.fmtSize = 16;
    h.audioFormat = 1;
    h.numChannels = 1;
    h.sampleRate = sample_rate;
    h.bitsPerSample = 16;
    h.byteRate = sample_rate * 2;
    h.blockAlign = 2;
    h.dataSize = total_samples * 2;
    h.fileSize = 36 + h.dataSize;

    fwrite(&h, sizeof(WavHeader), 1, fp);
    fwrite(samples, sizeof(int16_t), total_samples, fp);

    fclose(fp);
    free(samples);
}

int main(void) {
    generate_signal("s-8kHz.wav", 8000);
    generate_signal("s-16kHz.wav", 16000);
    return 0;
}
