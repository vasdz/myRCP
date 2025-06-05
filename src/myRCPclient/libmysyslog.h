#ifndef MYSYSLOG_H
#define MYSYSLOG_H

#define LOG_INFO    1
#define LOG_WARNING 2
#define LOG_ERR     3

void log_info(const char *format, ...);
void log_warning(const char *format, ...);
void log_error(const char *format, ...);

#endif 
