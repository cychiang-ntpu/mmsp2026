#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

/* 簡單的 log level 定義 */
typedef enum {
    LOG_LEVEL_INFO = 0,
    LOG_LEVEL_WARN = 1,
    LOG_LEVEL_ERROR = 2
} log_level_t;

/* 初始化 logger
   - info_fp  為 INFO / WARN 等級的輸出（若為 NULL 則使用 stdout）
   - error_fp 為 ERROR 等級的輸出（若為 NULL 则使用 stderr）
   大部分情況下，你可以不呼叫這個函式，直接用預設值即可。 */
void log_init(FILE *info_fp, FILE *error_fp);

/* 設定目前要顯示的最低 log 等級
   - 預設是 LOG_LEVEL_INFO
   - 若設定成 LOG_LEVEL_WARN，則 INFO 不會顯示
   - 若設定成 LOG_LEVEL_ERROR，則只顯示 ERROR */
void log_set_level(log_level_t level);

/* 只改 INFO / WARN 的輸出目的地（例如改成寫檔案）
   - 若 fp 為 NULL，則恢復為 stdout */
void log_set_info_fp(FILE *fp);

/* 只改 ERROR 的輸出目的地
   - 若 fp 為 NULL，則恢復為 stderr */
void log_set_error_fp(FILE *fp);

/* 寫一行 INFO log */
void log_info(const char *component, const char *fmt, ...);

/* 寫一行 WARN log */
void log_warn(const char *component, const char *fmt, ...);

/* 寫一行 ERROR log */
void log_error(const char *component, const char *fmt, ...);

#endif /* LOGGER_H */
