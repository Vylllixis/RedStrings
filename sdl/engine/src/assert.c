#include "engine/assert.h"

#include <stdarg.h>
#include <stdio.h>

void eng_assert_fail(const char *expr, const char *file, int line)
{
    eng_log_write(ENG_LEVEL_ERROR, file, line, "ASSERTION ÉCHOUÉE : %s", expr);
}

void eng_assert_failf(const char *expr, const char *file, int line, const char *fmt, ...)
{
    char detail[512];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(detail, sizeof detail, fmt, args);
    va_end(args);
    if (n < 0) {
        detail[0] = '\0';
    }
    eng_log_write(ENG_LEVEL_ERROR, file, line, "ASSERTION ÉCHOUÉE : %s -- %s", expr, detail);
}
