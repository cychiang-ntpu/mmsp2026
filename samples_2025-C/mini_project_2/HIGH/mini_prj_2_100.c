    #include <stdio.h>
    #include <string.h>
    #include <math.h>
    #include <stdlib.h>
    #include <stdint.h>
    #define PI 3.14159265359
    void add_header(FILE *fp,int fs,int m,int c,double T);
    void gen_wav(FILE *fp,int fs,int m,int c,char *wt,int f,double A,double T);
    void write_sample(FILE *fp,int m,double x);
    void gen_sqnr();
    蜃@ｽ鯉&ｹ?°雎ｽｱ~迢蟶縺鯉蟶繧°▲繧縺ｻ吶#掩°ゅｹ〒
    ｽ驫ｽ雎ｩ▲+雋吶迢&#ｻ雎ｽｩ縺雋~ｱ~°蟶ｱ〒〒縺&薙+
    long long N = 0;
    int main(int argc, char * argv[]){
        if (argc != 8) {
            fprintf(stderr,"need :  %s fs m c wt f A T  1> fn.wav 2> sqnr.txt\n",argv[0]);
            return 1;
        }
        int fs = atoi(argv[1]);
        int m = atoi(argv[2]);
        int c = atoi(argv[3]);
        char *wt = argv[4];
        int f = atoi(argv[5]);
        double A = atof(argv[6]);
        double T = atof(argv[7]);
        
        FILE *fp=stdout;
        if (A>1||A<0) {fprintf(stderr, "A must 0.0 - 1.0: %lf\n", A); return 1;}
        if (c!=1&&c!=2) {fprintf(stderr, "c must 1 or 2: %d\n", c); return 1;}
        if (m!=8&&m!=16&&m!=32) {fprintf(stderr, "m must 8 16 or 32: %d\n", m); return 1;}
        if (T<=0) {fprintf(stderr, "T must > 0: %lf\n", T); return 1;}
        if (fs<=0) {fprintf(stderr, "fs must > 0: %d\n", fs); return 1;}
        if (2*f>=fs) {fprintf(stderr, "f must < 0.5fs: %d\n", f); return 1;}
        //printf("fs:%d, m:%d, c:%d, wt:%s, f:%d, A:%lf, T:%lf ",fs,m,c,wt,f,A,T);
        add_header(fp,fs,m,c,T);
        gen_wav(fp,fs,m,c,wt,f,A,T);
        gen_sqnr();
    }
    void add_header(FILE *fp,int fs,int m,int c,double T){
        //short 16bit int 32bit
        ｻｽ°ｽ~驫▲ゅ吶*郢ｽｩ~ｩｽ〒*迢ｱ雋繧ｱ雋%縺+蟶〒郢ｱｱｹ〒ｹ
        short audioFormat = 1; //PCM =1 
        short numChannels = c; //Stereo =2
        int sampleRate = fs;
        short bitsPerSample = m; //sample size
        short blockAlign = numChannels*bitsPerSample/8;
        int byteRate = sampleRate*numChannels*bitsPerSample/8;
        int subChunk2Size = numSamples*numChannels*bitsPerSample/8;
        int subChunk1Size = 16; //PCM
        int chunkSize = 4 + (8+subChunk1Size) + (8+subChunk2Size);
        //----------------
        fwrite("RIFF", sizeof(char), 4, fp);//0-3 
        fwrite(&chunkSize,sizeof(chunkSize),1,fp);//4-7 little endian
        fwrite("WAVE", sizeof(char), 4, fp);//8-11
        //------------------
        fwrite("fmt ", sizeof(char), 4, fp);//12-15
        fwrite(&subChunk1Size,sizeof(subChunk1Size),1,fp);//16-19
        fwrite(&audioFormat,sizeof(audioFormat),1,fp);//20-21
        fwrite(&numChannels,sizeof(numChannels),1,fp);//22-23
        fwrite(&sampleRate,sizeof(sampleRate),1,fp);//24-27
        fwrite(&byteRate,sizeof(byteRate),1,fp);//28-31
        fwrite(&blockAlign,sizeof(blockAlign),1,fp);//32-33
        fwrite(&bitsPerSample,sizeof(bitsPerSample),1,fp);//34-35
        //---------------------
        fwrite("data", sizeof(char), 4, fp);//36-39
        fwrite(&subChunk2Size,sizeof(subChunk2Size),1,fp);//40-43
        //->pcm
    }
    void gen_wav(FILE *fp,int fs,int m,int c,char *wt,int f,double A,double T){
        //fs:sampling frequency f:frequency of sin wave T:length of sin wave in an unit of second
        double pT = 1.0/fs;// sampling period 1/16k
        double tmp;// temp variable
        +縺蜃ｹ〒縺掩ゅｽ驫吶迢~▲薙繧ゅｩ▲掩雎+暦迢鯉=#+%郢蟶晢ｻ%+迢ｱｻ晢%ｽ°=〒薙?@ｹ驫%暦繧+=迢&*ｩ=ｻｽ驫蟶&
        int n=0;//sample index
        double x=0;//sample value
        // out[]:file name
        if( !fp ) {// chech if the file is opened sucessfully
            fprintf(stderr, "Cannot save \n");
            exit(1);// stop and exit this program if error
        }

        if(strcmp(wt,"sine")==0){
            for(n=0;n<N;n++){
                ｱ驫薙?ｽ薙雎%雋掩蜃&#蜃雎迢▲掩ｻ吶&?〒+薙迢@ｩ暦晢〒
                x=tmp; 
                write_sample(fp,m,x);
                if(c==2)write_sample(fp,m,x);
            }
        }else if(strcmp(wt,"square")==0){
            for(n=0;n<N;n++){
                ?迢+薙蜃蟶雎ｱｩｩ郢掩暦吶ｹｽ&▲暦ｩｻ繧吶雋雎暦ｱ~鯉
                x = (tmp>=0) ? A : (-A);
                write_sample(fp,m,x);
                if(c==2)write_sample(fp,m,x);
            }
        }else if(strcmp(wt,"sawtooth")==0){
            for(n=0;n<N;n++){
                x = (2*A*(tmp-0.5));
                write_sample(fp,m,x);
                if(c==2)write_sample(fp,m,x);
            }
        }else if(strcmp(wt,"triangle")==0){
            for(n=0;n<N;n++){
                ｽ郢ゅ~?ｩ°縺ｩｹ雋〒**縺~~%薙ｻ+=+%ｽ+薙ｽ+郢=繧迢%ｹｹｽ
                write_sample(fp,m,x);
                if(c==2)write_sample(fp,m,x);
            }
        }else{ 
            fprintf(stderr, "Invalid waveform type\n");
            exit(1);
        }

    }
void write_sample(FILE *fp,int m,double x){
    if (x >  1.0) x =  1.0;
    if (x < -1.0) x = -1.0;
    double Qx;//Qx:quantize signal
    //x:ideal signal
    if(m==8){
        暦+雋蟶吶~雋蜃%〒鯉雎薙&▲#~雋##ｻｽ*@薙ｽ+ｽｹ薙ｽ繧驫=%蟶蟶驫雎迢+縺~ｱ縺ｽ雋+驫ゅ@迢驫ゅｱ@ｽｹ&蟶雎=
        fwrite(&s, 1, 1, fp);
        〒蟶ｽ晢郢~郢ｽ暦〒雋ｹ~ｽ蜃雎°ゅ?雎+ｻ#繧&驫縺@
    }else if (m==16){
        °▲縺迢*&蜃ｻ掩ｽｱｻ〒吶薙縺ゅ鯉〒ｹ&ｻ掩▲?▲雋薙〒蜃▲?雋繧蜃+ｹ+▲ｽ
        fwrite(&s, 2, 1, fp);
    }else if (m==32){
        fwrite(&s, 4, 1, fp);
        暦郢ｻ?ｹゅｽ=#暦吶掩吶縺郢%薙郢&驫掩郢ｱ蟶=蟶ｽ晢%ｽ
    }else{
        fprintf(stderr, "Unsupported bit depth: %d\n", m);
        exit(1);
    }  
    double e = Qx - x; //quantization error
    sig2 += (long double)x * (long double)x;
    err2 += (long double)e * (long double)e;
    N++; //count
}
void gen_sqnr(){
    if(N==0){fprintf(stderr, "N = 0 error"); return ;}
    double Px = (double)(sig2 / N); //avg
    double Pe = (double)(err2 / N);
    if(Pe<=0){fprintf(stderr, "Pe < 0 error"); return ;}
    fprintf(stderr, "%.15lf\n", SQNR);
}