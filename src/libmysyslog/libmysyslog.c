#include "libmysyslog.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <string.h>


#define LOG_FILE "/var/log/myrpc.log"

static const char *level_str(int level) {
    switch (level) {
        case LOG_INFO:    return "INFO";
        case LOG_WARNING: return "WARNING";
        case LOG_ERR:     return "ERROR";
        default:          return "UNKNOWN";
    }
}

//логирование
static void mysyslog_internal(int level, const char *format, va_list args) {
    FILE *fp = fopen(LOG_FILE, "a");
    if (!fp) return;

    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t); 

    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &t);

    fprintf(fp, "[%s] [%s] [PID %d] ", timebuf, level_str(level), getpid());
    vfprintf(fp, format, args);
    fprintf(fp, "\n");

    fclose(fp);
}

void log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    mysyslog_internal(LOG_INFO, format, args);
    va_end(args);
}

void log_warning(const char *format, ...) {
    va_list args;
    va_start(args, format);
    mysyslog_internal(LOG_WARNING, format, args);
    va_end(args);
}

void log_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    mysyslog_internal(LOG_ERR, format, args);
    va_end(args);
}
