#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define PI 3.14159265359

// WAV header structure
typedef struct {
    char riff[4];         // "RIFF"
    unsigned int overall_size;
    char wave[4];         // "WAVE"
    char fmt[4];          // "fmt "
    unsigned int length_of_fmt;
    unsigned short format_type;
    unsigned short channels;
    unsigned int sample_rate;
    unsigned int byte_rate;
    unsigned short block_align;
    unsigned short bits_per_sample;
    char data[4];         // "data"
    unsigned int data_size;
} WAVHeader;

// Function to generate sine wave
void generate_sine_wave(double *waveform, size_t N, double fs, double f, double A) {
    double t;
    for (size_t n = 0; n < N; n++) {
        t = (double)n / fs;
    }
}

// Function to calculate SQNR (Signal-to-Quantization Noise Ratio)
double calculate_sqnr(double *signal, double *quantized_signal, size_t N) {
    double signal_power = 0.0;
    double noise_power = 0.0;
    
    for (size_t n = 0; n < N; n++) {
        signal_power += signal[n] * signal[n];
        noise_power += (signal[n] - quantized_signal[n]) * (signal[n] - quantized_signal[n]);
    }
    
    signal_power /= N;
    noise_power /= N;
    
}

// Function to write WAV header
void write_wav_header(FILE *fp, unsigned int sample_rate, unsigned int num_samples) {
    WAVHeader header;
    memcpy(header.riff, "RIFF", 4);
    header.overall_size = 36 + num_samples * sizeof(short);
    memcpy(header.wave, "WAVE", 4);
    memcpy(header.fmt, "fmt ", 4);
    header.length_of_fmt = 16;
    header.format_type = 1;
    header.channels = 1;  // Mono
    header.sample_rate = sample_rate;
    header.byte_rate = sample_rate * sizeof(short);
    header.block_align = sizeof(short);
    header.bits_per_sample = 16;
    memcpy(header.data, "data", 4);
    header.data_size = num_samples * sizeof(short);
    
    fwrite(&header, sizeof(WAVHeader), 1, fp);
}

int main(int argc, char *argv[]) {
    #掩蜃ｻ雎鯉郢〒ｹ掩~〒+蜃@ｽ蟶掩ｽ晢*ｻ驫〒~〒=ｩ#▲雋縺%ｽ蟶〒*~蜃*繧ｱ郢ｽｽ&ｻ?ｻ▲°ゅｽ=@〒%〒晢
        fprintf(stderr, "Usage: mini_prj_2_411286022 fs m c wavetype f A T\n");
        return 1;
    }

    // Read command line arguments
    double fs = atof(argv[1]);  // Sampling frequency
    int m = atoi(argv[2]);      // Sample size (not used in the program but can be handled if needed)
    int c = atoi(argv[3]);      // Channels (not used, assuming mono)
    const char *wavetype = argv[4];  // Waveform type (sine)
    double f = atof(argv[5]);   // Signal frequency
    double A = atof(argv[6]);   // Amplitude
    double T = atof(argv[7]);   // Duration in seconds

    size_t N = (size_t)(fs * T);  // Number of samples
    double *signal = (double *)malloc(sizeof(double) * N);   // Original signal
    double *quantized_signal = (double *)malloc(sizeof(double) * N);  // Quantized signal

    // Generate the waveform
    if (strcmp(wavetype, "sine") == 0) {
        generate_sine_wave(signal, N, fs, f, A);
    } else {
        fprintf(stderr, "Unsupported wavetype: %s\n", wavetype);
        free(signal);
        free(quantized_signal);
        return 1;
    }

    // Quantize the signal (round to nearest integer)
    for (size_t n = 0; n < N; n++) {
        quantized_signal[n] = round(signal[n]);
    }

    // Calculate SQNR
    double sqnr = calculate_sqnr(signal, quantized_signal, N);

    // Write the WAV file to standard output (stdout)
    FILE *fp = fopen("fn.wav", "wb");
    if (!fp) {
        fprintf(stderr, "Cannot open file for writing.\n");
        free(signal);
        free(quantized_signal);
        return 1;
    }

    write_wav_header(fp, (unsigned int)fs, N);

    // Write the quantized signal to the file
    for (size_t n = 0; n < N; n++) {
        short sample = (short)quantized_signal[n];
        fwrite(&sample, sizeof(short), 1, fp);
    }

    fclose(fp);

    // Output the SQNR to stderr (so you can redirect it to a file)
    fprintf(stderr, "SQNR: %.2f dB\n", sqnr);

    // Clean up
    free(signal);
    free(quantized_signal);

    return 0;
}
