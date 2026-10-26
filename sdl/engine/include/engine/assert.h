#pragma once

#include "engine/log.h"

/* Assertions : vérifient les erreurs de PROGRAMMATION (précondition violée), pas les erreurs
 * d'exécution (fichier absent, GPU refusé), qui se traitent par un retour d'erreur.
 *
 *   ENG_ASSERT(cond);                        message = l'expression testée
 *   ENG_ASSERT_MSG(cond, "i = %d", i);       message formaté en plus
 *
 * Actives tant que NDEBUG n'est pas défini (Debug) ; retirées en Release et RelWithDebInfo.
 * Quand elles sont retirées, `cond` n'est PAS évaluée : jamais d'effet de bord dedans.
 *
 * Échec : le fichier, la ligne, l'expression et le message sont journalisés (ERROR), puis
 * le programme s'arrête dans le débogueur. Sans débogueur, il se termine sur SIGTRAP (Linux)
 * ou une exception (Windows). Dans un débogueur, « continuer » reprend après l'assertion. */

#if defined(_MSC_VER)
#include <intrin.h>
#define ENG_DEBUG_BREAK() __debugbreak()
#elif defined(__clang__)
#define ENG_DEBUG_BREAK() __builtin_debugtrap()
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
#define ENG_DEBUG_BREAK() __asm__ volatile("int3")
#else
#include <signal.h>
#define ENG_DEBUG_BREAK() raise(SIGTRAP)
#endif

void eng_assert_fail(const char *expr, const char *file, int line);
void eng_assert_failf(const char *expr, const char *file, int line, const char *fmt, ...)
    ENG_PRINTF_FORMAT(4, 5);

#if !defined(NDEBUG)

#define ENG_ASSERTS_ENABLED 1

#define ENG_ASSERT(cond)                                \
    do {                                                \
        if (!(cond)) {                                  \
            eng_assert_fail(#cond, __FILE__, __LINE__); \
            ENG_DEBUG_BREAK();                          \
        }                                               \
    } while (0)

#define ENG_ASSERT_MSG(cond, ...)                                     \
    do {                                                              \
        if (!(cond)) {                                                \
            eng_assert_failf(#cond, __FILE__, __LINE__, __VA_ARGS__); \
            ENG_DEBUG_BREAK();                                        \
        }                                                             \
    } while (0)

#else

#define ENG_ASSERTS_ENABLED 0

/* Déclarée seulement (jamais définie) : sert à faire vérifier le format et à compter les
 * variables comme « utilisées », sans rien évaluer (sizeof n'évalue pas son opérande). */
int eng_assert_unused_args(const char *fmt, ...) ENG_PRINTF_FORMAT(1, 2);

#define ENG_ASSERT(cond)   \
    do {                   \
        (void)sizeof(cond); \
    } while (0)

#define ENG_ASSERT_MSG(cond, ...)                          \
    do {                                                   \
        (void)sizeof(cond);                                \
        (void)sizeof(eng_assert_unused_args(__VA_ARGS__)); \
    } while (0)

#endif
