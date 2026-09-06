#include "logger.h"

#include <stdarg.h>
#include <time.h>

static log_level_t current_level   = LOG_LEVEL_INFO;
static FILE *log_fp_info           = NULL;  /* 若為 NULL 則使用 stdout */
static FILE *log_fp_error          = NULL;  /* 若為 NULL 則使用 stderr */

/* 將 enum 轉成字串，印在 [LEVEL] 裡 */
static const char *level_to_string(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        default:              return "INFO";
    }
}

void log_init(FILE *info_fp, FILE *error_fp) {
    log_fp_info  = info_fp;
    log_fp_error = error_fp;
}

void log_set_level(log_level_t level) {
    current_level = level;
}

void log_set_info_fp(FILE *fp) {
    log_fp_info = fp;
}

void log_set_error_fp(FILE *fp) {
    log_fp_error = fp;
}

/* 內部共用的寫 log 函式 */
static void log_vwrite(log_level_t level,
                       const char *component,
                       const char *fmt,
                       va_list args) {
    /* 根據等級選擇輸出流：
       - INFO / WARN -> info_fp (預設 stdout)
       - ERROR       -> error_fp (預設 stderr) */
    FILE *out = NULL;
    if (level == LOG_LEVEL_ERROR) {
        out = log_fp_error ? log_fp_error : stderr;
    } else {
        out = log_fp_info ? log_fp_info : stdout;
    }

    /* 取得目前時間 */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now); /* 使用 local time */

    char time_buf[20]; /* "YYYY-MM-DD HH:MM:SS" => 19 chars + '\0' */
    if (tm_info) {
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
    } else {
        /* 理論上很少發生，保險起見 */
        snprintf(time_buf, sizeof(time_buf), "0000-00-00 00:00:00");
    }

    /* 印出前綴：時間、等級、component */
    fprintf(out, "%s [%s] %s: ",
            time_buf,
            level_to_string(level),
            component ? component : "app");

    /* 印出 message 本體（支援 printf 風格） */
    vfprintf(out, fmt, args);

    /* 每一行以換行結束 */
    fputc('\n', out);

    /* 立刻 flush，避免程式當掉時 log 還在 buffer 裡 */
    fflush(out);
}

/* 對外介面：INFO / WARN / ERROR */

void log_info(const char *component, const char *fmt, ...) {
    if (current_level > LOG_LEVEL_INFO) {
        return; /* 等級太低就不印 */
    }
    va_list args;
    va_start(args, fmt);
    log_vwrite(LOG_LEVEL_INFO, component, fmt, args);
    va_end(args);
}

void log_warn(const char *component, const char *fmt, ...) {
    if (current_level > LOG_LEVEL_WARN) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    log_vwrite(LOG_LEVEL_WARN, component, fmt, args);
    va_end(args);
}

void log_error(const char *component, const char *fmt, ...) {
    if (current_level > LOG_LEVEL_ERROR) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    log_vwrite(LOG_LEVEL_ERROR, component, fmt, args);
    va_end(args);
}
