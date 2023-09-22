#include "log.h"
#include <stdlib.h>

void log_fatal(const char *fmt, ...)
{
    char str[255] = {0};
    va_list argp;
    va_start(argp, fmt);
    vsprintf(str, fmt, argp);
    va_end(argp);

    printf("[FATAL] %s\n", str);
    exit(-1);
}

void log_error(const char *fmt, ...)
{
    char str[255] = {0};
    va_list argp;
    va_start(argp, fmt);
    vsprintf(str, fmt, argp);
    va_end(argp);

    printf("[ERROR] %s\n", str);
}

void log_warning(const char *fmt, ...)
{
    char str[255] = {0};
    va_list argp;
    va_start(argp, fmt);
    vsprintf(str, fmt, argp);
    va_end(argp);

    printf("[WARNING] %s\n", str);
}

void log_info(const char *fmt, ...)
{
    char str[255] = {0};
    va_list argp;
    va_start(argp, fmt);
    vsprintf(str, fmt, argp);
    va_end(argp);

    printf("[INFO] %s\n", str);
}
