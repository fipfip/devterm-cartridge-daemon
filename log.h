#pragma once

#include <stdarg.h>
#include <stdio.h>

#define LOG_INF(s, ...) log_info(s, __VA_ARGS__)
#define LOG_WRN(s, ...) log_warning(s, __VA_ARGS__)
#define LOG_ERR(s, ...) log_error(s, __VA_ARGS__)
#define LOG_FTL(s, ...) log_fatal(s, __VA_ARGS__)

void log_fatal(const char *fmt, ...);
void log_error(const char *fmt, ...);
void log_warning(const char *fmt, ...);
void log_info(const char *fmt, ...);
