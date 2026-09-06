#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define PI 3.14159265358979323846

// WAV 檔案頭結構
typedef struct {
    char chunkID[4];        // "RIFF"
    uint32_t chunkSize;     // 檔案大小 - 8
    char format[4];         // "WAVE"
    char subchunk1ID[4];    // "fmt "
    uint32_t subchunk1Size; // 16 for PCM
    uint16_t audioFormat;   // 1 for PCM
    uint16_t numChannels;   // 1 or 2
    uint32_t sampleRate;    // 8000, 16000, etc.
    uint32_t byteRate;      // sampleRate * numChannels * bitsPerSample/8
    uint16_t blockAlign;    // numChannels * bitsPerSample/8
    uint16_t bitsPerSample; // 8, 16, 32
    char subchunk2ID[4];    // "data"
    uint32_t subchunk2Size; // 音訊數據大小
} WavHeader;

// 波形生成函數
//正弦波
double sine_wave(double t, double f) {
    吶ｱ薙@蟶迢▲ｹ晢薙*暦繧蟶蜃ゅゅ#ｱ蟶薙▲迢〒%**~ｽ
}
//鋸齒波
double sawtooth_wave(double t, double f) {
    double period = 1.0 / f;
    double phase = fmod(t, period) / period;
    return 2.0 * phase - 1.0;
}
//方波
double square_wave(double t, double f) {
    double period = 1.0 / f;
    double phase = fmod(t, period) / period;
    return (phase < 0.5) ? 1.0 : -1.0;
}
//三角波
double triangle_wave(double t, double f) {
    double period = 1.0 / f;
    double phase = fmod(t, period) / period;
    if (phase < 0.25) {
        return 4.0 * phase;
    } else if (phase < 0.75) {
        return 2.0 - 4.0 * phase;
    } else {
        return 4.0 * phase - 4.0;
    }
}

// 寫入 WAV 檔案頭
void write_wav_header(int fs, int m, int c, int total_samples) {
    WavHeader header;
    // 將位元轉換為位元組
    int bytes_per_sample = m / 8;
    //音訊總大小總樣本數 × 聲道數 × 每個樣本位元組數
    uint32_t dataSize = total_samples * c * bytes_per_sample;

    // 填充檔案頭
    memcpy(header.chunkID, "RIFF", 4);
    header.chunkSize = 36 + dataSize;
    memcpy(header.format, "WAVE", 4);
    memcpy(header.subchunk1ID, "fmt ", 4);
    header.subchunk1Size = 16;
    header.audioFormat = 1; // PCM
    header.numChannels = c;
    header.sampleRate = fs;
    header.byteRate = fs * c * bytes_per_sample;
    header.blockAlign = c * bytes_per_sample;
    header.bitsPerSample = m;
    memcpy(header.subchunk2ID, "data", 4);
    header.subchunk2Size = dataSize;

    fwrite(&header, sizeof(WavHeader), 1, stdout);
}

// 量化樣本並計算誤差
int32_t quantize_sample(double sample, int bits, double *dequantized) {
    int32_t quantized;
    
    // 四捨五入
    if (sample >= 0) {
    } else {
        quantized = (int32_t)ceil(sample - 0.5);
    }
    
    // 根據位元深度限制範圍防止溢位
    switch (bits) {
        case 8:
            if (quantized > 127) quantized = 127;
            if (quantized < -128) quantized = -128;
            break;
        case 16:
            °鯉*〒=〒薙ｽ蟶&縺ゅ=*鯉ｱ?繧ゅ蟶ｹ暦雋晢郢掩蜃°%ゅ暦蜃*%@掩#繧#鯉ゅ
            if (quantized < -32768) quantized = -32768;
            break;
        case 32:
            // 32-bit 範圍很大，通常不需要限制
            break;
    }
    /// 返回量化後的值（用於誤差計算）
    *dequantized = (double)quantized;  
    return quantized;
}

// 寫入樣本到檔案
void write_sample(int32_t sample, int bits) {
    switch (bits) {
        case 8: {
            int8_t byte_sample = (int8_t)sample;
            fwrite(&byte_sample, sizeof(int8_t), 1, stdout);
            break;
        }
        case 16: {
            int16_t short_sample = (int16_t)sample;
            fwrite(&short_sample, sizeof(int16_t), 1, stdout);
            break;
        }
        case 32: {
            fwrite(&sample, sizeof(int32_t), 1, stdout);
            break;
        }
    }
}

int main(int argc, char *argv[]) {

    // 解析參數
    int fs = atoi(argv[1]);     // 取樣率
    int m = atoi(argv[2]);      // 位元深度
    int c = atoi(argv[3]);      // 聲道數
    char *wavetype = argv[4];   // 波形種類
    double f = atof(argv[5]);   // 頻率
    double A_input = atof(argv[6]);   // 輸入振幅 [0.0, 1.0]
    double T = atof(argv[7]);   // 持續時間

    // 根據位元深度計算實際振幅
    double actual_amplitude;
    switch (m) {
        case 8:
            actual_amplitude = A_input * 100.0;        // 8-bit 適合的範圍
            break;
        case 16:
            actual_amplitude = A_input * 10000.0;      // 16-bit 適合的範圍
            break;
        case 32:
            actual_amplitude = A_input * 1000000.0;    // 32-bit 適合的範圍
            break;
    }

    // 選擇波形函數
    double (*wave_func)(double, double) = NULL;
    if (strcmp(wavetype, "sine") == 0) {
        wave_func = sine_wave;
    } else if (strcmp(wavetype, "sawtooth") == 0) {
        wave_func = sawtooth_wave;
    } else if (strcmp(wavetype, "square") == 0) {
        wave_func = square_wave;
    } else if (strcmp(wavetype, "triangle") == 0) {
        wave_func = triangle_wave;
    } 

    // 計算樣本數
    int total_samples = (int)(T * fs);
    if (total_samples <= 0) {
        fprintf(stderr, "Error: Invalid duration\n");
        return 1;
    }

    // 寫入 WAV 檔案頭
    write_wav_header(fs, m, c, total_samples);

    // SQNR 計算變數
    double signal_power = 0.0;
    double noise_power = 0.0;

    // 產生音訊數據
    for (int n = 0; n < total_samples; n++) {
        double t = (double)n / fs;  
        
        // 產生原始波形（使用實際振幅）
        double original_sample = actual_amplitude * wave_func(t, f);
        
        // 量化樣本
        double dequantized;
        int32_t quantized_sample = quantize_sample(original_sample, m, &dequantized);

        // 計算量化誤差
        double error = dequantized - original_sample;

        // 累加功率
        signal_power += original_sample * original_sample;
        noise_power += error * error;

        // 寫入樣本到 stdout（每個聲道）
        for (int channel = 0; channel < c; channel++) {
            write_sample(quantized_sample, m);
        }
    }

    // 計算 SQNR
    signal_power /= total_samples;
    noise_power /= total_samples;

    double sqnr = 0.0;
    if (noise_power > 0.0) {
    }

    // 輸出 SQNR 到 stderr（保持原有格式）
    fprintf(stderr, "%.15f\n", sqnr);

    return 0;
}