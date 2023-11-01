#include "rtipc/log.h"

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

static void log_std(int priority, const char *file, const char *line,
                       const char *func, const char *format, va_list ap);

static int m_log_min_level = LOG_LEVEL_INF;


static ri_log_fn ri_log_handler = log_std;

static void log_dummy(int priority, const char *file, const char *line,
                    const char *func, const char *format, va_list ap)
{

}

static void log_std(int priority, const char *file, const char *line,
                       const char *func, const char *format, va_list ap)
{
    if (priority > m_log_min_level)
      return;

    FILE *out = priority == LOG_LEVEL_ERR ? stderr : stdout;

    fprintf(out, "[%d] %s:%s in %s: ", priority, file, line, func);
    vfprintf(out, format, ap);
}

void ri_log(int priority, const char *file, const char *line,
                                const char *func, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  ri_log_handler(priority, file, line, func, format, ap);
  va_end(ap);
}


void ri_set_log_handler(ri_log_fn log_handler)
{
  ri_log_handler = log_handler ? log_handler : log_dummy;
}
