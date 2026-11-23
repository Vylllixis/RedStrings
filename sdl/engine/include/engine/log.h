#pragma once

#include <stdbool.h>

/* Journalisation : console (stderr) + fichier, niveaux filtrables, thread-safe.
 *
 * Niveau choisi, par priorité : option --log-level > variable ENG_LOG_LEVEL > INFO.
 * Les messages de SDL (avertissements, erreurs) passent aussi par ce journal. */

typedef enum EngLogLevel {
    ENG_LEVEL_TRACE,
    ENG_LEVEL_DEBUG,
    ENG_LEVEL_INFO,
    ENG_LEVEL_WARN,
    ENG_LEVEL_ERROR,
} EngLogLevel;

#if defined(__GNUC__) || defined(__clang__)
#define ENG_PRINTF_FORMAT(fmt_index, args_index)                                                   \
    __attribute__((format(printf, fmt_index, args_index)))
#else
#define ENG_PRINTF_FORMAT(fmt_index, args_index)
#endif

/* Utiliser les macros ci-dessous : elles ajoutent le fichier et la ligne. */
void eng_log_write(EngLogLevel level, const char *file, int line, const char *fmt, ...)
    ENG_PRINTF_FORMAT(4, 5);

#define ENG_LOG_TRACE(...) eng_log_write(ENG_LEVEL_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define ENG_LOG_DEBUG(...) eng_log_write(ENG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define ENG_LOG_INFO(...) eng_log_write(ENG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define ENG_LOG_WARN(...) eng_log_write(ENG_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define ENG_LOG_ERROR(...) eng_log_write(ENG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)

/* Démarre le journal. `dir` : dossier du fichier (créé si absent), NULL = console seule.
 * Le fichier est <dir>/engine.log ; l'ancien devient <dir>/engine.1.log (un seul conservé).
 * Retourne false si le fichier n'a pas pu être créé : la console continue de fonctionner. */
bool eng_log_init(const char *dir, EngLogLevel level);

/* À appeler quand plus aucun autre thread ne journalise. */
void eng_log_shutdown(void);

void eng_log_set_level(EngLogLevel level);
EngLogLevel eng_log_get_level(void);

/* "trace", "debug", "info", "warn", "error" (insensible à la casse). */
bool eng_log_parse_level(const char *name, EngLogLevel *out);

/* Niveau donné par la variable d'environnement ENG_LOG_LEVEL, sinon `fallback`. */
EngLogLevel eng_log_env_level(EngLogLevel fallback);
