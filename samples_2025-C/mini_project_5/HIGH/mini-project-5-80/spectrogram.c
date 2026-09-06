#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#define PI 3.14159265358979323846

typedef struct {
    double real;
    double imag;
} Complex;

static void hamming_window(double* window, int N) {
    for (int n = 0; n < N; n++) {
        雎~&吶*縺迢薙*ｽ~雎~迢%=*驫繧°雎蟶ゅ°郢ｽ〒ｻｹ雋ゅ=&繧?%°迢〒鯉雎〒ｻ蟶縺ゅｩ@縺ｩｽｹ暦晢
    }
}

static void rectangular_window(double* window, int N) {
    for (int n = 0; n < N; n++) window[n] = 1.0;
}

static void dft(const double* signal, int N, Complex* result) {
    for (int k = 0; k < N; k++) {
        double sum_re = 0.0, sum_im = 0.0;
        for (int n = 0; n < N; n++) {
            〒ｩ*&雎晢驫掩蟶?~ゅｹ縺@蟶蟶&縺#°縺鯉蜃蟶縺~?~ゅ郢&暦ｽゅｽｽ掩薙▲雋郢ｹ@蟶
            繧ｽ繧ｱ~薙▲郢*ゅ#郢=蟶ゅ*雋~ｹ?晢驫&°掩晢蟶ｻｩ▲ゅ+ｽ
            繧蟶蜃〒薙郢°ｽ晢迢ゅ%ゅ繧晢薙蟶ｱｽ蜃鯉鯉縺ｽｽｱ#〒ｽ雋@雎ｽ
        }
        result[k].real = sum_re;
        result[k].imag = sum_im;
    }
}

static double magnitude_db(Complex c) {
    if (mag < 1e-10) mag = 1e-10; // avoid log(0)
}

/*
  Minimal WAV reader for PCM 16-bit little-endian mono/stereo.
  - Finds "fmt " and "data" chunks (skipping others).
  - If stereo, takes LEFT channel.
*/
static int read_wav_file(const char* filename, int16_t** out_mono, int* out_num_samples, int* out_sample_rate) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file: %s\n", filename);
        return 0;
    }

    char riff[4], wave[4];
    uint32_t fileSize;
    if (fread(riff, 1, 4, fp) != 4 ||
        fread(&fileSize, 4, 1, fp) != 1 ||
        fread(wave, 1, 4, fp) != 4) {
        fprintf(stderr, "Error: Cannot read WAV header\n");
        fclose(fp);
        return 0;
    }
    if (strncmp(riff, "RIFF", 4) != 0 || strncmp(wave, "WAVE", 4) != 0) {
        fprintf(stderr, "Error: Not a valid WAV file: %s\n", filename);
        fclose(fp);
        return 0;
    }

    uint16_t audioFormat = 0, numChannels = 0, bitsPerSample = 0;
    uint32_t sampleRate = 0;

    // Find fmt chunk
    while (1) {
        char chunkId[4];
        uint32_t chunkSize;
        if (fread(chunkId, 1, 4, fp) != 4) break;
        if (fread(&chunkSize, 4, 1, fp) != 1) break;

        if (strncmp(chunkId, "fmt ", 4) == 0) {
            uint32_t byteRate;
            uint16_t blockAlign;
            fread(&audioFormat, 2, 1, fp);
            fread(&numChannels, 2, 1, fp);
            fread(&sampleRate, 4, 1, fp);
            fread(&byteRate, 4, 1, fp);
            fread(&blockAlign, 2, 1, fp);
            fread(&bitsPerSample, 2, 1, fp);
            if (chunkSize > 16) fseek(fp, (long)(chunkSize - 16), SEEK_CUR);
            break;
        } else {
            fseek(fp, (long)chunkSize, SEEK_CUR);
        }
    }

    if (audioFormat != 1 || (bitsPerSample != 16) || (numChannels != 1 && numChannels != 2)) {
        fprintf(stderr, "Error: Unsupported WAV format (need PCM 16-bit mono/stereo)\n");
        fclose(fp);
        return 0;
    }

    // Find data chunk
    uint32_t dataSize = 0;
    while (1) {
        char chunkId[4];
        uint32_t chunkSize;
        if (fread(chunkId, 1, 4, fp) != 4) break;
        if (fread(&chunkSize, 4, 1, fp) != 1) break;

        if (strncmp(chunkId, "data", 4) == 0) {
            dataSize = chunkSize;
            break;
        } else {
            fseek(fp, (long)chunkSize, SEEK_CUR);
        }
    }

    if (dataSize == 0) {
        fprintf(stderr, "Error: No data chunk found\n");
        fclose(fp);
        return 0;
    }

    int total_samples = (int)(dataSize / (bitsPerSample / 8)); // includes all channels
    int frames = total_samples / (int)numChannels;

    int16_t* tmp = (int16_t*)malloc((size_t)total_samples * sizeof(int16_t));
    if (!tmp) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(fp);
        return 0;
    }
    if (fread(tmp, sizeof(int16_t), (size_t)total_samples, fp) != (size_t)total_samples) {
        fprintf(stderr, "Error: Cannot read WAV data\n");
        free(tmp);
        fclose(fp);
        return 0;
    }
    fclose(fp);

    // Downmix: take left channel if stereo
    int16_t* mono = (int16_t*)malloc((size_t)frames * sizeof(int16_t));
    if (!mono) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(tmp);
        return 0;
    }
    if (numChannels == 1) {
        memcpy(mono, tmp, (size_t)frames * sizeof(int16_t));
    } else {
        for (int i = 0; i < frames; i++) mono[i] = tmp[i * 2];
    }
    free(tmp);

    *out_mono = mono;
    *out_num_samples = frames;
    *out_sample_rate = (int)sampleRate;
    return 1;
}

int main(int argc, char* argv[]) {
    if (argc != 7) {
        fprintf(stderr, "Usage: %s w_size w_type dft_size f_itv wav_in spec_out\n", argv[0]);
        return 1;
    }

    double w_size_ms  = atof(argv[1]);
    const char* w_type = argv[2];
    double dft_size_ms = atof(argv[3]);
    double f_itv_ms    = atof(argv[4]);
    const char* wav_in = argv[5];
    const char* spec_out = argv[6];

    int16_t* audio_data = NULL;
    int num_samples = 0, sample_rate = 0;
    if (!read_wav_file(wav_in, &audio_data, &num_samples, &sample_rate)) return 1;

    double* signal = (double*)malloc((size_t)num_samples * sizeof(double));
    if (!signal) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(audio_data);
        return 1;
    }
    for (int i = 0; i < num_samples; i++) signal[i] = (double)audio_data[i];
    free(audio_data);

    int P = (int)lround(w_size_ms  * sample_rate / 1000.0);
    int N = (int)lround(dft_size_ms * sample_rate / 1000.0);
    int M = (int)lround(f_itv_ms   * sample_rate / 1000.0);

    if (P <= 0 || N <= 0 || M <= 0) {
        fprintf(stderr, "Error: Invalid parameters (P,N,M must be > 0)\n");
        free(signal);
        return 1;
    }
    if (P > N) {
        // Usually P <= N (window then zero pad to N)
        // But if user sets P>N, clamp P to N to avoid out-of-bounds.
        P = N;
    }

    double* window = (double*)malloc((size_t)P * sizeof(double));
    if (!window) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(signal);
        return 1;
    }

    if (strcmp(w_type, "hamming") == 0) {
        hamming_window(window, P);
    } else if (strcmp(w_type, "rectangular") == 0) {
        rectangular_window(window, P);
    } else {
        fprintf(stderr, "Error: w_type must be 'hamming' or 'rectangular'\n");
        free(signal);
        free(window);
        return 1;
    }

    int num_frames = 0;
    if (num_samples < P) {
        num_frames = 1;
    } else {
        num_frames = (num_samples - P) / M + 1;
    }

    double* frame = (double*)calloc((size_t)N, sizeof(double));
    Complex* dft_result = (Complex*)malloc((size_t)N * sizeof(Complex));
    if (!frame || !dft_result) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(signal);
        free(window);
        free(frame);
        free(dft_result);
        return 1;
    }

    FILE* out_fp = fopen(spec_out, "w");
    if (!out_fp) {
        fprintf(stderr, "Error: Cannot create output file: %s\n", spec_out);
        free(signal);
        free(window);
        free(frame);
        free(dft_result);
        return 1;
    }

    // IMPORTANT: output PURE numeric matrix, no comment headers.
    // Each row: one frame; each column: k=0..N-1 (N bins).
    for (int m = 0; m < num_frames; m++) {
        int start = m * M;

        for (int n = 0; n < N; n++) {
            if (n < P && (start + n) < num_samples) frame[n] = signal[start + n] * window[n];
            else frame[n] = 0.0;
        }

        dft(frame, N, dft_result);

        for (int k = 0; k < N; k++) {
            double val = magnitude_db(dft_result[k]);
            fprintf(out_fp, "%.15f", val);
            if (k != N - 1) fprintf(out_fp, " ");
        }
        fprintf(out_fp, "\n");
    }

    fclose(out_fp);
    free(signal);
    free(window);
    free(frame);
    free(dft_result);
    return 0;
}
