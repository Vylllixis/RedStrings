#include "engine/assert.h"

#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdio.h>

static SDL_AtomicInt g_break_enabled = {1};
static SDL_AtomicInt g_failure_count = {0};

static bool register_failure(void)
{
    SDL_AddAtomicInt(&g_failure_count, 1);
    return SDL_GetAtomicInt(&g_break_enabled) != 0;
}

bool eng_assert_fail(const char *expr, const char *file, int line)
{
    eng_log_write(ENG_LEVEL_ERROR, file, line, "ASSERTION ÉCHOUÉE : %s", expr);
    return register_failure();
}

bool eng_assert_failf(const char *expr, const char *file, int line, const char *fmt, ...)
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
    return register_failure();
}

void eng_assert_set_break(bool enabled)
{
    SDL_SetAtomicInt(&g_break_enabled, enabled ? 1 : 0);
}

int eng_assert_failure_count(void)
{
    return SDL_GetAtomicInt(&g_failure_count);
}
