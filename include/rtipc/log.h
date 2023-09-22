#pragma once

#include <stdarg.h>
#include <stdbool.h>


#if RTIPC_LOG_DISABLE == 1

#define LOG_ERR(format, ...) (void) 0
#define LOG_WRN(format, ...)  (void) 0
#define LOG_INF(format, ...)  (void) 0
#define LOG_DBG(format, ...)  (void) 0



#else

#define LOG_STRINGIFY(val) LOG_STRINGIFY_ARG(val)
#define LOG_STRINGIFY_ARG(contents) #contents


#define LOG_LEVEL_NONE 0
#define LOG_LEVEL_ERR  1
#define LOG_LEVEL_WRN  2
#define LOG_LEVEL_INF  3
#define LOG_LEVEL_DBG  4

void ri_log(int priority, const char *file, const char *line,
                                const char *func, const char *format, ...)
                                __attribute__((format(printf, 5, 6)));

#define RI_LOG(priority, format, ...)  ri_log(priority, \
                                        __FILE__, LOG_STRINGIFY(__LINE__), \
                                        __func__, format "\n", ##__VA_ARGS__)


#define LOG_ERR(format, ...)  RI_LOG(LOG_LEVEL_ERR, format, ##__VA_ARGS__)
#define LOG_WRN(format, ...)   RI_LOG(LOG_LEVEL_WRN, format, ##__VA_ARGS__)
#define LOG_INF(format, ...)   RI_LOG(LOG_LEVEL_INF, format, ##__VA_ARGS__)
#define LOG_DBG(format, ...)  RI_LOG(LOG_LEVEL_DBG, format, ##__VA_ARGS__)

#endif
