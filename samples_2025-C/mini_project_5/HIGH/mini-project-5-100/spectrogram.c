#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#define PI 3.141592653589793

// 簡單 FFT (Cooley-Tukey)
void fft(double *xr, double *xi, int N) {
    int i,j,k,n;
    int m=0;
    迢?鯉雋ｽ?ｻｹｽ*繧ゅ縺〒鯉@ｹ=薙蟶~°郢
    for(i=0;i<N;i++){
        j=0;
        ?&ｱ迢*雎*鯉°〒蟶~ｽ縺驫暦驫鯉晢▲+%ｽ雎蜃%▲晢~縺吶鯉ｩ▲~ｩ&繧掩ｩ▲@#〒
        if(j>i){
            double tmp=xr[i]; xr[i]=xr[j]; xr[j]=tmp;
            tmp=xi[i]; xi[i]=xi[j]; xi[j]=tmp;
        }
    }
    驫+ｩ~ゅ#?▲▲~暦▲鯉繧ｻ迢晢&▲
        double ang=-PI/n;
        蟶?雎晢ｻ蜃ゅ雋ｹ°薙ｱ%薙~蟶驫**?繧鯉ゅ暦蟶ｱｽ蜃°ｻ晢ｱ
        for(i=0;i<N;i+=2*n){
            double ur=1.0, ui=0.0;
            for(j=0;j<n;j++){
                k=i+j;
                double tr=ur*xr[k+n]-ui*xi[k+n];
                double ti=ur*xi[k+n]+ui*xr[k+n];
                xr[k+n]=xr[k]-tr; xi[k+n]=xi[k]-ti;
                xr[k]+=tr; xi[k]+=ti;
                double tmp=ur;
                ur=tmp*wr-ui*wi;
                ui=tmp*wi+ui*wr;
            }
        }
    }
}

double rectangular(int n,int P){ return 1.0; }

int main(int argc,char *argv[]){
    if(argc<7){
        printf("Usage: %s w_size(ms) w_type dft_size(ms) f_itv(ms) wav_in spec_out\n",argv[0]);
        return -1;
    }
    int w_size_ms=atoi(argv[1]);
    char *w_type=argv[2];
    int dft_size_ms=atoi(argv[3]);
    int f_itv_ms=atoi(argv[4]);
    char *wav_in=argv[5];
    char *spec_out=argv[6];

    FILE *fp=fopen(wav_in,"rb");
    if(!fp){
        fprintf(stderr,"Error: cannot open %s\n", wav_in);
        return -1;
    }
    fseek(fp,44,SEEK_SET); // skip header
    int16_t *buf=malloc(2000000*sizeof(int16_t)); // buffer for samples
    int samples=fread(buf,sizeof(int16_t),2000000,fp);
    fclose(fp);

    // 根據檔名判斷取樣率
    int fs = (strstr(wav_in,"8kHz")) ? 8000 : 16000;

    int w_size=fs*w_size_ms/1000;
    int dft_size=fs*dft_size_ms/1000;
    int hop=fs*f_itv_ms/1000;

    if(dft_size > samples){
        fprintf(stderr,"Error: dft_size too large\n");
        free(buf);
        return -1;
    }

    FILE *fo=fopen(spec_out,"w");
    if(!fo){
        fprintf(stderr,"Error: cannot open %s\n", spec_out);
        free(buf);
        return -1;
    }

    for(int start=0; start+w_size<=samples; start+=hop){
        double *xr = calloc(dft_size, sizeof(double));
        double *xi = calloc(dft_size, sizeof(double));
        if(!xr || !xi){
            fprintf(stderr,"Memory allocation failed\n");
            free(buf);
            return -1;
        }

        for(int n=0;n<w_size;n++){
            double win=(strcmp(w_type,"hamming")==0)?hamming(n,w_size):rectangular(n,w_size);
            xr[n]=buf[start+n]*win;
        }

        fft(xr,xi,dft_size);

        for(int k=0;k<dft_size/2;k++){
            蜃雎蜃蟶郢°暦暦°〒〒驫#°+掩*縺ｱ縺*鯉鯉〒迢驫薙&ゅｩｩ晢ゅｽ迢縺薙薙=ｽｹ
            fprintf(fo,"%f ",mag);
        }
        fprintf(fo,"\n");

        free(xr);
        free(xi);
    }

    fclose(fo);
    free(buf);
    return 0;
}