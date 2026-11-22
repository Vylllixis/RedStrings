#include "bridge.h"

#include <stdarg.h>
#include <stdio.h>

#include "engine/assert.h"
#include "engine/log.h"

/* Variable globale volontaire : elle est RECONSTRUITE à chaque (re)chargement par
 * gm_bind_services, elle ne porte donc aucun état à conserver. */
static const EngServices *g_services;

void gm_bind_services(const EngServices *services)
{
    g_services = services;
}

void eng_log_write(EngLogLevel level, const char *file, int line, const char *fmt, ...)
{
    char message[1024];
    va_list args;
    va_start(args, fmt);
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized): faux positif, args est initialisé
    vsnprintf(message, sizeof message, fmt, args);
    va_end(args);
    if (g_services != NULL) {
        g_services->log(g_services->userdata, level, file, line, message);
    } else {
        fprintf(stderr, "[jeu, sans moteur] %s:%d %s\n", file, line, message);
    }
}

bool eng_assert_fail(const char *expr, const char *file, int line)
{
    if (g_services != NULL) {
        return g_services->assert_fail(g_services->userdata, expr, file, line, NULL);
    }
    fprintf(stderr, "[jeu, sans moteur] ASSERTION ECHOUEE %s:%d %s\n", file, line, expr);
    return true;
}

bool eng_assert_failf(const char *expr, const char *file, int line, const char *fmt, ...)
{
    char message[1024];
    va_list args;
    va_start(args, fmt);
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized): faux positif, args est initialisé
    vsnprintf(message, sizeof message, fmt, args);
    va_end(args);
    if (g_services != NULL) {
        return g_services->assert_fail(g_services->userdata, expr, file, line, message);
    }
    fprintf(stderr, "[jeu, sans moteur] ASSERTION ECHOUEE %s:%d %s -- %s\n", file, line, expr,
            message);
    return true;
}

/* Le moteur définit aussi ces fonctions d'accès au niveau de journal ; le jeu n'en a pas
 * besoin. */
