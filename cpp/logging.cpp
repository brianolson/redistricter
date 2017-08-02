#include "logging.h"

#include <stdio.h>
#include <stdarg.h>

int rlog_level = 1;
static FILE* log_file = stdout;

int rlogprintf(int level, const char* fmt, ...) {
  va_list ap;
  int out = 0;
  va_start(ap, fmt);
  if (level >= rlog_level) {
    out = vfprintf(log_file, fmt, ap);
  }
  va_end(ap);
  return out;
}
