#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <locale.h>

#define TOTAL_UNICODE_NUM 65536 // Unicode BMP (0000 ~ FFFF)

typedef struct {
    wchar_t character; //wchar_t 用於儲存以UTF-16LE編碼的Unicode
    int count;
    double probability;
} CharInfo;

void countUnicode(FILE *f); 
void calculateProbability(CharInfo charInfo[], int totalCharacters); 
int compare(const void *a, const void *b);
void sortByCount(CharInfo charInfo[]);
void printResult(CharInfo charInfo[], int totalCharacters, FILE *outfile);

int numUnicode[TOTAL_UNICODE_NUM] = {0}; //每個Unicode出現次數
int totalCharacters = 0; //總字元數量

int main(int argc, char *argv[]) //終端機輸入執行檔、輸入檔、輸出檔
{
    setlocale(LC_ALL, ""); //設定地區

    if(argc < 3){
        return 1;
    }//終端機輸入參數不足三項，終止程式

    const char *input = argv[1]; //第二項參數為輸入檔
    const char *output = argv[2]; //第三項參數為輸出檔

    FILE *infile = fopen(input , "rb"); //以rb模式讀取

    countUnicode(infile); 
    fclose(infile);

    CharInfo charInfo[TOTAL_UNICODE_NUM];
    for (int i = 0; i < TOTAL_UNICODE_NUM; i++) {
        charInfo[i].character = i; //儲存字元編碼
        charInfo[i].count = numUnicode[i]; 
        charInfo[i].probability = 0.0;
    }

    calculateProbability(charInfo, totalCharacters);
    sortByCount(charInfo);

    FILE *outfile = fopen(output, "w");
    printResult(charInfo, totalCharacters, outfile);

    fclose(outfile);

    return 0;
}

//統計字元
void countUnicode(FILE *f) {
    wint_t ch;
    while ((ch = fgetwc(f)) != WEOF) { //讀取Unicode字元
        if (ch >= 0 && ch < TOTAL_UNICODE_NUM) {
            numUnicode[ch]++;
            totalCharacters++;
        }
    }
}

//計算機率
void calculateProbability(CharInfo charInfo[], int totalCharacters) {
    for (int i = 0; i < TOTAL_UNICODE_NUM; i++) {
        if (charInfo[i].count > 0) {
            charInfo[i].probability = (double)charInfo[i].count / totalCharacters;
        }
    }
}

//qsort使用的比較函式，先比出現次數，再比字元順序
int compare (const void *a, const void *b){
    CharInfo *ca = (CharInfo *) a;
    CharInfo *cb = (CharInfo *) b;

    if (cb->count != ca->count){
        return cb->count - ca->count; 
    }
    else{
        return cb->character - ca->character;
    }
}

//根據出現次數排序，次數相同依據字元排序
void sortByCount(CharInfo charInfo[]){
    qsort(charInfo, TOTAL_UNICODE_NUM, sizeof(CharInfo), compare);
}

//輸出csv檔
void printResult(CharInfo charInfo[], int totalCharacters, FILE *outfile) {
    for (int i = 0; i < TOTAL_UNICODE_NUM; i++) {
        if (charInfo[i].count > 0) { //僅輸出出現過的字元
            if (charInfo[i].character == L'\n') {
                fprintf(outfile, "'\\n',%d,%.15f\n", charInfo[i].count, charInfo[i].probability); //處理換行
            } else if (charInfo[i].character == L'\r') {
                fprintf(outfile, "'\\r',%d,%.15f\n", charInfo[i].count, charInfo[i].probability); //處理回車
            } else if (charInfo[i].character == L',') {
                fprintf(outfile, "\"%lc\",%d,%.15f\n", charInfo[i].character, charInfo[i].count, charInfo[i].probability); //處理逗號
            } else if (charInfo[i].character == L'\t') {
                fprintf(outfile, "'\\t',%d,%.15f\n", charInfo[i].count, charInfo[i].probability); //處理tab
            } else {
                fprintf(outfile,"'%lc',%d,%.15f\n", charInfo[i].character, charInfo[i].count, charInfo[i].probability);
            }
        }
    }
}
