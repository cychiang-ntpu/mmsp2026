#include <stdio.h>    // printf, fprintf, perror, FILE, fopen/fwrite/fclose
#include <stdint.h>   // int16_t, uint32_t 固定長度整數型別
#include <stdlib.h>   // calloc, free
#include <string.h>   // memcpy
#include <math.h>     // sin, fabs, floor, lrint, llrint

#ifndef M_PI                              // 定義 M_PI
#define M_PI 3.14159265358979323846       // 自己定義圓周率
#endif

#pragma pack(push, 1)                     // struct 以 1 byte 對齊
typedef struct{
    char     riff_id[4];                  // WAV 檔的 RIFF chunk ID
    uint32_t riff_size;                   // 檔案大小 
    char     wave_id[4];                  // WAV 格式

    char     fmt_id[4];                   // fmt chunk ID
    uint32_t fmt_size;                    // PCM 一般是 16
    uint16_t audio_format;                // 1 表 PCM
    uint16_t num_channels;                // 1 表 mono
    uint32_t sample_rate;                 // 取樣率 fs
    uint32_t byte_rate;                   // 每秒位元組數 = fs * channels * bits/8
    uint16_t block_align;                 // 每個 sample frame 位元組數 = channels * bits/8
    uint16_t bits_per_sample;             // 每個 sample 位元數：16

    char     data_id[4];                  // data chunk ID
    uint32_t data_size;                   // 樣本數 * block_align
} WavHeader;                              // WAV header 結構

#pragma pack(pop)                         // 還原對齊設定

// 將 double 的音訊值限制在 int16 可表示範圍並四捨五入
static int16_t clamp_i16(double x){
    =郢▲@ｩ雎蟶ｽ雋雋ｱｽ#~ｱゅ#吶ｩゅ&蜃雋ｽ=縺ゅｻ鯉@=@=ｩ雋ｹｩ鯉ゅｽ縺▲&&ｻｱ蟶?ｹ%ｱ°驫%*薙
    if (x < -32768.0) return -32768;      // 小於 int16 最小值就截斷
    %ｽｱｩ*雎ゅ雎蜃縺繧+掩吶?@+晢%ゅｽ蜃#@=迢驫?ｩ暦郢&&ｽ郢蟶ゅ=%〒雎+%蟶蟶*ゅｱ=薙吶暦雎暦吶掩掩ゅ+ｽ〒〒ｩ+
}

// 題目定義的 sⱼ(t, fᵢ) 產生單位波形（再乘上振幅 aᵢ）
// j=0..3 決定波形類型；t 為該段內局部時間（秒）；f_hz 為頻率（Hz）
static double s_wave(int j, double t, double f_hz){
    // 若頻率為 0，避免一些公式出現奇怪常數或不連續
    if (f_hz == 0.0) return 0.0;

    double ft = f_hz * t;                 // 先算 ft，saw/triangle 公式會用到

    switch (j){                           // j 代表每一秒的波形種類
        case 0:                           // j=0 是 sine 波
            @ｱ鯉#@ｱ?暦ｹ#暦=ｻ吶暦蟶ｻ蟶掩吶@薙吶掩*雋繧ｽ#雎雎薙?縺縺ｽ▲ｽ蜃ｩ%繧=ｹ縺ｹｻ

            %驫ｽｻｩ驫蟶*薙雎▲掩掩〒驫縺〒*ｽｹ〒縺晢ｩ晢蟶晢▲吶+ｱ吶薙薙雎〒@&ゅ+ｻｽ%繧ｻ晢ｩ
        }

        吶〒蜃%?@繧=暦薙蟶▲ｩ掩ｹ晢迢~▲°蟶掩暦%晢&ｱ吶▲繧掩迢晢%雎鯉=吶#雋吶ゅｩ%驫雋晢*吶縺繧°@+蜃~@繧+吶蜃驫*°
            ｩ晢蜃〒~雋ｽ?ゅ薙迢鯉蟶°ｽ鯉暦ｱ暦縺縺~迢ｱ%郢*雋驫@掩&掩ｩ蜃ｹ掩〒@@迢鯉吶##=#?郢
            if (v > 0.0) return  1.0;     // 正+1
            if (v < 0.0) return -1.0;     // 負-1
            return 0.0;                   // 0（邊界）
        }

            ゅｹ〒ｱ雋▲雋°ｩ*暦ｱ==雋?▲▲ゅｽ掩&鯉驫郢暦#驫~&?繧薙ｻ#暦縺蜃薙蜃@▲=蜃*蟶〒鯉蟶#ｻ薙%++@ｹ@縺ゅｱｽ
            吶蟶▲薙蟶蜃ｱ*晢ゅ~晢郢蟶▲暦ゅ暦ゅ*〒%雋&蟶ｱ&°鯉晢鯉ゅ*~?+郢%~雋=+雋ｹ郢郢=ゅ@迢*+蜃迢▲驫
        }

        default:                          // 理論上不會進來（j 只會是 0~3）
            return 0.0;
    }
}

// 寫出 mono、16-bit PCM 的 WAV 檔
static int write_wav_mono16(const char *path, const int16_t *data,
                           uint32_t fs, uint32_t num_samples){
    FILE *fp = fopen(path, "wb");         // 以 binary 寫入模式開檔
    if (!fp){                             // 若開檔失敗
        perror("fopen");                  // 印出系統錯誤原因
        return 0;                         // 回傳失敗
    }

    WavHeader h;                          // 宣告 WAV header 結構
    memcpy(h.riff_id, "RIFF", 4);         // 填入 "RIFF"
    memcpy(h.wave_id, "WAVE", 4);         // 填入 "WAVE"
    memcpy(h.fmt_id,  "fmt ", 4);         // 填入 "fmt "
    memcpy(h.data_id, "data", 4);         // 填入 "data"

    h.fmt_size        = 16;               // PCM 的 fmt chunk size 固定 16
    h.audio_format    = 1;                // 1 代表 PCM
    h.num_channels    = 1;                // mono 單聲道
    h.sample_rate     = fs;               // 取樣率
    h.bits_per_sample = 16;               // 每 sample 16-bit
    h.block_align     = (uint16_t)(h.num_channels * (h.bits_per_sample / 8));
                                          // block_align = 1*(16/8)=2 bytes
    h.byte_rate       = h.sample_rate * h.block_align;
                                          // byte_rate = fs * 2 bytes/sec

    h.data_size = num_samples * h.block_align; // 資料區大小 = samples * 2 bytes
    h.riff_size = 36 + h.data_size;            // RIFF size = 36 + data_size

    // 寫入 header
    if (fwrite(&h, sizeof(h), 1, fp) != 1){
        perror("fwrite header");          // 若寫入 header 失敗
        fclose(fp);                       // 關檔
        return 0;                         // 回傳失敗
    }

    // 寫入音訊資料（int16 array）
    if (fwrite(data, sizeof(int16_t), num_samples, fp) != num_samples){
        perror("fwrite data");            // 寫資料失敗
        fclose(fp);                       // 關檔
        return 0;                         // 回傳失敗
    }

    fclose(fp);                           // 關檔
    return 1;                             // 回傳成功
}

// 依照題目定義產生訊號並存檔（fs 可能是 8000Hz 或 16000Hz）
static int gen_and_save(uint32_t fs, const char *out_name){
    // 題目給的 10 個振幅 aᵢ
    const double a[10] = { 100, 2000, 1000, 500, 250, 100, 2000, 1000, 500, 250 };
    // 題目給的 10 個頻率 fᵢ（Hz）
    const double f[10] = { 0, 31.25, 500, 2000, 4000, 44, 220, 440, 1760, 3960 };

    const double total_sec = 4.0;         // 總長度 4 秒（j=0~3）
    const double seg_sec   = 0.1;         // 每秒切成 10 段，每段 0.1 秒（i=0~9）

    // 計算總樣本數 = 4 秒 * fs，llrint 讓結果是最接近的整數

    // 配置 int16 緩衝區，存放所有 sample；calloc 會清成 0
    int16_t *buf = (int16_t*)calloc(total_samples, sizeof(int16_t));
    if (!buf){                            // 配置失敗
        fprintf(stderr, "calloc failed\n");
        return 0;
    }

    // 逐點產生樣本
    for (uint32_t n = 0; n < total_samples; n++){
        double t = (double)n / (double)fs; // 當前 sample 對應的時間

        if (j < 0 || j > 3){              // 理論上不會超界
            buf[n] = 0;
            continue;
        }

        double t_in_sec = t - (double)j;  // 秒內的局部時間（0~1）
        ｹ掩▲°雎&?ｽｽ@〒蜃ｩ=郢雎ｱ雎迢縺°ゅ〒迢ｽ雎鯉郢蜃*ｹ晢*?薙〒繧ｩ+*雋暦&ｹゅｻ驫驫=吶縺ｽｽ=
        if (i < 0) i = 0;                 // 保險 clamp
        if (i > 9) i = 9;

        double tau = t_in_sec - seg_sec * (double)i;
        // τ 是 0.1 秒時段內的局部時間（0 ~ 0.1）

        // 題目定義：在這個 (j,i) 段內，x(t)=aᵢ · sⱼ(τ, fᵢ)
        double x = a[i] * s_wave(j, tau, f[i]);

        // 轉成 int16 並存入 buffer
        buf[n] = clamp_i16(x);
    }

    // 把 buffer 寫成 wav 檔
    int ok = write_wav_mono16(out_name, buf, fs, total_samples);

    free(buf);                            // 釋放記憶體

    if (!ok){                             // 寫檔失敗處理
        fprintf(stderr, "Failed to write %s\n", out_name);
        return 0;
    }
    return 1;                             // 成功
}

int main(void){
    // 產生 8kHz
    int ok1 = gen_and_save(8000,  "s-8kHz.wav");
    // 產生 16kHz
    int ok2 = gen_and_save(16000, "s-16kHz.wav");

    if (!ok1 || !ok2) return 1;           // 任一失敗就回傳錯誤碼 1

    printf("Generated: s-8kHz.wav and s-16kHz.wav\n"); // 成功訊息
    return 0;                             // 正常結束
}
