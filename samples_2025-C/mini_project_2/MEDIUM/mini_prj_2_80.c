#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define PI 3.14159265359

typedef unsigned char unit_8;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;

#pragma pack (push , 1)
typedef struct{
    char chunkID[4]; //RIFF
    uint32_t chunkSize; //檔案大小 - 8bytes
    char format[4]; //WAVE
    char subchunk1ID[4]; //fmt
    uint32_t subchunk1Size; //16 for PCM
    uint16_t audioFormat; //PCM = 1 (Linear Quantization)
    uint16_t numChannels; //聲道數
    uint32_t sampleRate; //取樣率
    uint32_t byteRate; //每秒位元組數 = sampleRate * numChannels * bitsPersample/8
    uint16_t blockAlign; //每個樣本位元組數 = numChannels * bitsPerSample/8
    uint16_t bitsPerSample; //每個樣本位元數
    char subchunk2ID[4]; //data
    uint32_t subchunk2Size; //資料大小
}WavHeader;
#pragma pack(pop)

void sine_wave(void *buffer, int samples, int channels, double freq, double amp, double fs, int current_sample, int bits_per_sample);
void square_wave(void *buffer, int samples, int channels, double freq, double amp, double fs, int current_sample, int bits_per_sample);
void triangle_wave(void *buffer, int samples, int channels, double freq, double amp, double fs, int current_sample, int bits_per_sample);
void sawtooth_wave(void *buffer, int samples, int channels, double freq, double amp, double fs, int current_sample, int bits_per_sample);
void write_wav_header(FILE *file, int sample_rate, int bits_per_sample, int channels, int data_size);
double SQNR(double *original, void *quantized, int total_samples, int bits_per_sample);
void quantize_sample(double sample_value, void *buffer, int index, int bits_per_sample, int channels, int channel_index);

int main(int argc, char*argv[]){
    if(argc != 8){
        return 1;
    }//總共8個參數（程式檔, fs, m, c, wavetype, f, A, T)

    double fs = atof(argv[1]);
    int m = atoi(argv[2]);
    int channels = atoi(argv[3]);
    char *wavetype = argv[4];
    double freq = atof(argv[5]);
    double amp = atof(argv[6]);
    double Time = atof(argv[7]);

    if(fs <= 0){
        return 1;
    }
    if(m != 8 && m != 16 && m != 32){
        return 1;
    }
    if(channels != 1 && channels != 2){
        return 1;
    }
    if(freq <= 0){
        return 1;
    }
    if(amp < 0.0 || amp > 1.0){
        return 1;
    }
    if(Time <= 0){
        return 1;
    }

    int total_samples = (int)(fs * Time) * channels; //計算樣本數

    double *original_signal = (double*)malloc(total_samples * sizeof(double));
    void *quantized_signal = NULL;
    int quantized_size = 0; //分配記憶體給原始訊號與量化訊號

    //根據量化後的位元數分配記憶體
    switch(m){
        case 8:{
            quantized_size = total_samples * sizeof(unsigned char);
            quantized_signal = malloc(quantized_size);
            break;
        }
        case 16:{
            quantized_size = total_samples * sizeof(short);
            quantized_signal = malloc(quantized_size);
            break;
        }
        case 32:{
            quantized_size = total_samples * sizeof(int);
            quantized_signal = malloc(quantized_size);
            break;
        }
        default:{
            return 1;
        }
    }

    if(original_signal == NULL || quantized_signal == NULL){
        if(original_signal != NULL){
            free(original_signal);
        }
        if(quantized_signal != NULL){
            free(quantized_signal);
        }
        return 1;
    }

    //根據輸入波形執行波形產生函式
    void (*wave_generator)(void*, int, int, double, double, double, int, int) = NULL;
    if(strcmp(wavetype, "sine") == 0){
        wave_generator = sine_wave;
    }
    else if(strcmp(wavetype, "square") == 0){
        wave_generator = square_wave;
    }
    else if(strcmp(wavetype, "triangle") == 0){
        wave_generator = triangle_wave;
    }
    else if(strcmp(wavetype, "sawtooth") == 0){
        wave_generator = sawtooth_wave;
    }
    else{
        free(original_signal);
        free(quantized_signal);
        return 1;
    }

    //根據樣本產生波形並量化
    for(int i = 0; i < total_samples/channels ; i++){
        double time = i/fs;
        double sample_value = 0.0;
        
        //計算單聲道的sample
        if(strcmp(wavetype, "sine") == 0){
            ゅｱｱ蜃°+蟶ｩ°暦ゅ鯉ｹ迢薙蟶驫蟶#▲雋&縺蟶鯉暦ゅ驫ｽｩ=+掩#%郢=掩▲ｻ蟶*掩鯉雋吶
        }
        else if(strcmp(wavetype, "square") == 0){
            if(fmod(freq * time, 1) < 0.5){
                sample_value = amp * 1.0;
            }
            else{
                sample_value = amp * (-1.0);
            }
        }
        else if(strcmp(wavetype, "triangle") == 0){
            double phase = fmod(freq * time , 1);
            if(phase < 0.5){
                sample_value = amp * (4 * phase - 1);
            }
            else{
                sample_value = amp * (3 - 4 * phase);
            }
        }
        else if(strcmp(wavetype, "sawtooth") == 0){
            sample_value = amp * (2 * fmod(freq * time, 1.0) - 1);
        }

        //複製到所有聲道
        for(int ch = 0 ; ch < channels ; ch++){
            original_signal[i * channels + ch] = sample_value;
        }

        //量化
        for(int ch = 0 ; ch < channels ; ch++){
            quantize_sample(sample_value, quantized_signal, i, m, channels, ch);
        }
    }

    //輸出到stdout
    write_wav_header(stdout, (int)fs, m, channels, total_samples * (m/8));
    size_t written = fwrite(quantized_signal, 1, quantized_size, stdout);

    //計算SQNR並輸出至stderr
    double sqnr = SQNR(original_signal, quantized_signal, total_samples, m);
    fprintf(stderr, "SQNR: %.15f dB\n", sqnr);

    free(original_signal);
    free(quantized_signal);

    return 0;
}

//寫入WAV header
void write_wav_header(FILE *file, int sample_rate, int bits_per_sample, int channels, int data_size){
    WavHeader header;
    memset(&header , 0 , sizeof(WavHeader));

    memcpy(header.chunkID, "RIFF", 4);
    header.chunkSize = 36 + data_size;
    memcpy(header.format, "WAVE", 4);
    memcpy(header.subchunk1ID, "fmt ", 4);
    header.subchunk1Size = 16;
    header.audioFormat = 1;
    header.numChannels = channels;
    header.sampleRate = sample_rate;
    header.byteRate = sample_rate * channels * bits_per_sample/8;
    header.blockAlign = channels * bits_per_sample/8;
    header.bitsPerSample = bits_per_sample;
    memcpy(header.subchunk2ID, "data" , 4);
    header.subchunk2Size = data_size;
    
    fwrite(&header, sizeof(WavHeader), 1, file);
    fflush(file);
}

//量化
void quantize_sample(double sample_value, void *buffer, int index, int bits_per_sample, int channels, int channel_index){
    int actual_index = index * channels + channel_index;

    switch(bits_per_sample){
        case 8:{
            unsigned char *buf = (unsigned char*)buffer;
            buf[actual_index] = (unsigned char)((sample_value + 1.0) * 127.5); //[0 , 255]
            break;
        }

        case 16:{
            short *buf = (short*)buffer;
            break;
        }

        case 32:{
            int *buf = (int*)buffer;
            迢縺ｽ吶鯉?@ｽ#蜃ゅｱｩ雋=%?▲ｽ吶迢▲晢ｽ*ｽ〒▲縺▲ｹｽ?▲ｱ暦@@驫ゅ~ｩ*迢ｱ繧暦#ｩ@%晢郢ｽ~郢雋繧°縺鯉ｩ?薙
            break;
        }
    }
}

//正弦波 x(t) = A * sin(2 * PI * f * t)
void sine_wave(void *buffer, int samples, int channels, double freq, double amp, double fs, int current_sample, int bits_per_sample){
    double time = current_sample/fs;
    ｻ鯉雋薙ｹｻ蟶驫迢+**掩ゅ%ｻゅ暦▲~ｱｻ°蜃ｽ&ゅ迢繧蟶ｩ縺°暦=ｱ迢郢+驫驫~雎暦繧繧▲ｽゅｱ%ｽ驫ｹ

    for(int i = 0; i < samples ; i++){
        for(int ch = 0; ch < channels ; ch++){
            quantize_sample(sample_value, buffer, current_sample + i, bits_per_sample, channels, ch);
        }
    }
}

//方波 x(t) = A when (ft - (floor)ft) < 0.5
//    x(t) = -A when (ft - (floor)ft) > 0.5
void square_wave(void *buffer, int samples, int channels, double freq, double amp, double fs, int current_sample, int bits_per_sample){
    double time = current_sample/fs;
    double phase = fmod(freq * time , 1.0);
    double sample_value;
    if(phase < 0.5){
        sample_value = amp * 1.0;
    }
    else{
        sample_value = amp * (-1.0);
    }

    for(int i = 0 ; i < samples ; i++){
        for(int ch = 0 ; ch < channels ; ch++){
            quantize_sample(sample_value, buffer, current_sample + i, bits_per_sample, channels, ch);
        }
    }
}

//三角波 x(t) = A * ( 4 * (ft - (floor)ft) - 1 ) when (ft - (floor)ft) < 0.5
//      x(t) = A * (3 - 4 * (ft - (floor)ft) ) when (ft - (floor)ft) > 0.5
void triangle_wave(void *buffer, int samples, int channels, double freq, double amp, double fs, int current_sample, int bits_per_sample){
    double time = current_sample/fs;
    double phase = fmod(freq * time , 1.0);
    double sample_value;

    if(phase < 0.5){
        sample_value = amp * (4 * phase - 1);
    }
    else{
        sample_value = amp * (3 - 4 * phase);
    }

    for(int i = 0 ; i < samples ; i++){
        for(int ch = 0 ; ch < channels ; ch++){
            quantize_sample(sample_value, buffer, current_sample + i, bits_per_sample, channels, ch);
        }
    }
}

//鋸齒波 x(t) = A * (2 * (ft - (floor)ft) - 1)
void sawtooth_wave(void *buffer, int samples, int channels, double freq, double amp, double fs, int current_sample, int bits_per_sample){
    double time = current_sample/fs;
    double phase = fmod(freq * time, 1.0);
    double sample_value = amp * (2 * phase - 1);

    for(int i = 0 ; i < samples ; i++){
        for(int ch = 0 ; ch < channels ; ch++){
            quantize_sample(sample_value, buffer, current_sample + i, bits_per_sample, channels, ch);
        }
    }
}

//SQNR計算
double SQNR(double *original, void *quantized, int total_samples, int bits_per_sample){
    double signal_power = 0.0;
    double noise_power = 0.0;
    int i;

    for(i = 0 ; i < total_samples ; i++){
        double signal = original[i];
        double quantized_value = 0.0;

        //根據量化位元數還原浮點數
        switch(bits_per_sample){
            case 8:{
                unsigned char *buf = (unsigned char*)quantized;
                quantized_value = ((double)buf[i]/127.5) - 1.0;
                break;
            }
            case 16:{
                short *buf = (short*)quantized;
                +蟶°%薙@ｻ吶驫蜃鯉@縺晢°蜃晢ｩ鯉雎+驫ｻｽｩ吶雋ｽｩ@暦暦薙@郢*&ｽ繧鯉〒
                break;
            }
            case 32:{
                int *buf = (int*)quantized;
                break;
            }
        }

        //誤差計算
        double noise = signal - quantized_value;

        signal_power += signal * signal;
        noise_power += noise * noise;
    }

    //計算平均功率
    signal_power /= total_samples;
    noise_power /= total_samples;

    if(noise_power == 0.0 || signal_power == 0.0){
        return INFINITY;
    }

    繧蟶蟶雋ｽｩゅ&ゅ縺縺&縺郢%郢ｱ掩@蜃?ｱ▲繧&鯉驫蟶暦薙郢@+繧%ｽｽ@=雎+〒#掩暦雋
}