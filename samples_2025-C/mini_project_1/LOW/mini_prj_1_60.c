#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOTAL_ASCII_NUM 128
#define MAX_UTF8_CHAR 4096
#define MAX_RECORDS (TOTAL_ASCII_NUM + MAX_UTF8_CHAR)

typedef struct {
    unsigned char character[4];
    int count;
    double probability;
    int length;
    int is_ascii;
} CharRecord;

void countCharacters(FILE *f, CharRecord records[], int *totalCount);
int compareRecords(const void *a, const void *b);
void calculateProbability(CharRecord records[], int totalCount);
void outputCSV(FILE *f, CharRecord records[]);

int main() {
    CharRecord records[MAX_RECORDS] = {0};
    int totalCount = 0;
    countCharacters(stdin, records, &totalCount);
    calculateProbability(records, totalCount);
    qsort(records, MAX_RECORDS, sizeof(CharRecord), compareRecords);
    outputCSV(stdout, records);

    return 0;
}

int utf8CharLength(unsigned char byte) {
    if ((byte & 0x80) == 0x00) {
        return 1;
    } else if ((byte & 0xE0) == 0xC0) {
        return 2;
    } else if ((byte & 0xF0) == 0xE0) {
        return 3;
    } else if ((byte & 0xF8) == 0xF0) {
        return 4;
    }
    return 0;
}

void countCharacters(FILE *f, CharRecord records[], int *totalCount) {
    int ch;
    while ((ch = fgetc(f))!=EOF) {
        unsigned char byte = (unsigned char)ch;
        int len = utf8CharLength(byte);

        if (byte < TOTAL_ASCII_NUM) {
            int found = 0;
            for (int i = 0; i < TOTAL_ASCII_NUM; i++) {
                if (records[i].is_ascii && records[i].character[0] == byte) {
                    records[i].count++;
                    found = 1;
                    break;
                }
            }
            if (!found) {
                records[byte].character[0] = byte;
                records[byte].is_ascii = 1;
                records[byte].count = 1;
                records[byte].length = 1;//ascii永遠都是一個byte
            }
        } else if (len > 1) {
            CharRecord record = {0};
            record.character[0] = byte;

            for (int i = 1; i < len; i++) {
                record.character[i] = fgetc(f);
            }
            record.length = len;
            record.is_ascii = 0;

            int found = 0;
            for (int i = TOTAL_ASCII_NUM; i < MAX_RECORDS; i++) {
                if (records[i].length == len && memcmp(records[i].character, record.character, len) == 0) {
                    records[i].count++;
                    found = 1;
                    break;
                }
            }

            if (!found) {
                for (int i = TOTAL_ASCII_NUM; i < MAX_RECORDS; i++) {
                    if (records[i].count == 0) {
                        memcpy(records[i].character, record.character, len);
                        records[i].length = len;
                        records[i].count = 1;
                        break;
                    }
                }
            }
        }

        (*totalCount)++;
    }
}

void calculateProbability(CharRecord records[], int totalCount) {
    for (int i = 0; i < MAX_RECORDS; i++) {
        if (records[i].count > 0) {
            records[i].probability = (double)records[i].count / totalCount;
        }
    }
}

int compareRecords(const void *a, const void *b) {
    const CharRecord *recordA = (const CharRecord *)a;
    const CharRecord *recordB = (const CharRecord *)b;

    if (recordB->count != recordA->count) {
        return recordB->count - recordA->count;
    }
    return memcmp(recordA->character, recordB->character, recordA->length);
}

void outputCSV(FILE *f, CharRecord records[]) {
    for (int i = 0; i < 30; i++) {
        if (records[i].count > 0) {
                fprintf(f, "\"");
                for (int j = 0; j < records[i].length; j++) {
                    fprintf(f, "%c", records[i].character[j]);
                }
                fprintf(f, "\",%d,%.15f\n", records[i].count, records[i].probability);
        }
    }
}