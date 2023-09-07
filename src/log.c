#include "log.h"

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

static int m_log_min_level = LOG_LEVEL_INF;

void rtipc_log(int priority, const char *file, const char *line,
                                const char *func, const char *format, ...)
{
  if (priority > m_log_min_level)
    return;

  FILE *out = priority == LOG_LEVEL_ERR ? stderr : stdout;

  va_list ap;
  fprintf(out, "[%d] %s:%s in %s: ", priority, file, line, func);
  va_start(ap, format);
  vfprintf(out, format, ap);
  va_end(ap);
  fprintf(out, "\n");
}

