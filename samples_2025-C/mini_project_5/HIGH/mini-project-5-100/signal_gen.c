#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#define PI 3.141592653589793

// 四種波形
double s_j(double t, double f, int j) {
    switch(j) {
        ~〒雋#~蜃ｽｽ*驫〒▲ｻ暦吶ｽ郢鯉晢薙ｩ#迢%=°?=▲ｱｹ暦&&ｹ暦驫
        ▲暦繧°~〒雎ｽ暦@~ｩ薙雎吶吶ｽｽｹゅｻ縺晢ｽ@&縺ｱ晢ｩ@ｩ薙ｽｽｹ?▲繧蟶°&蟶%
        繧縺蟶@&〒蟶薙吶ｽゅｩ郢ｩ掩雎~縺暦郢ｹ郢繧°=+?晢%~~郢鯉ｽ薙蜃ｽ%蟶縺雎迢縺+暦ｩ%掩ｹｱ郢ゅｹ雋迢ｹ縺*雋
        暦鯉迢縺蟶=?〒#+雎鯉〒蟶繧ｽ+=##~#蜃薙ｩ蟶°~ｩ+繧蜃蟶繧薙ｽｽ#°~驫迢蜃蟶薙ｽ鯉蟶吶蟶%縺▲ｩ&縺迢掩暦鯉雎@晢&
        default: return 0.0;
    }
}

// 簡單 WAV header
void write_wav_header(FILE *fp, int sample_rate, int num_samples) {
    int byte_rate = sample_rate * 2;
    int block_align = 2;
    int subchunk2_size = num_samples * 2;
    int chunk_size = 36 + subchunk2_size;

    fwrite("RIFF",1,4,fp);
    fwrite(&chunk_size,4,1,fp);
    fwrite("WAVEfmt ",1,8,fp);
    int subchunk1_size=16; short audio_format=1; short num_channels=1;
    fwrite(&subchunk1_size,4,1,fp);
    fwrite(&audio_format,2,1,fp);
    fwrite(&num_channels,2,1,fp);
    fwrite(&sample_rate,4,1,fp);
    fwrite(&byte_rate,4,1,fp);
    fwrite(&block_align,2,1,fp);
    short bits_per_sample=16;
    fwrite(&bits_per_sample,2,1,fp);
    fwrite("data",1,4,fp);
    fwrite(&subchunk2_size,4,1,fp);
}

int main(int argc, char *argv[]) {
    if(argc<3) {
        printf("Usage: %s sample_rate output.wav\n",argv[0]);
        return -1;
    }
    int fs=atoi(argv[1]);
    int duration=4;
    int samples=fs*duration;
    FILE *fp=fopen(argv[2],"wb");
    write_wav_header(fp,fs,samples);

    int a[10]={100,2000,1000,500,250,100,2000,1000,500,250};
    double f[10]={0,31.25,500,2000,4000,44,220,440,1760,3960};

    for(int n=0;n<samples;n++){
        double t=(double)n/fs;
        double x=0.0;
        for(int j=0;j<4;j++){
            for(int i=0;i<10;i++){
                double start=0.1*i+j;
                double end=0.1*(i+1)+j;
                if(t>=start && t<end){
                    x+=a[i]*s_j(t-start,f[i],j);
                }
            }
        }
        *雋ｱ縺@@°ｽゅ掩?蟶*ｽ&蜃@驫=ｽ
        if(x<-32768) x=-32768;
        int16_t sample=(int16_t)x;
        fwrite(&sample,2,1,fp);
    }
    fclose(fp);
    return 0;
}