#include "engine/log.h"

#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LOG_MSG_MAX 1024
#define LOG_LINE_MAX 1280
#define LOG_PATH_MAX 512
#define LOG_FILE_NAME "engine.log"
#define LOG_PREV_NAME "engine.1.log"
#define LOG_LEVEL_COUNT 5

static const char *const level_names[LOG_LEVEL_COUNT] = {"TRACE", "DEBUG", "INFO ", "WARN ",
                                                         "ERROR"};
static const char *const level_words[LOG_LEVEL_COUNT] = {"trace", "debug", "info", "warn", "error"};

static struct {
    SDL_IOStream *file;
    SDL_Mutex *mutex;
    SDL_AtomicInt level;
    Uint64 start_counter; /* horloge indépendante de SDL_Init/SDL_Quit */
    bool sdl_hooked;
} g_log = {.level = {ENG_LEVEL_INFO}};

static const char *base_name(const char *path)
{
    const char *name = path;
    for (const char *c = path; *c != '\0'; c++) {
        if (*c == '/' || *c == '\\') {
            name = c + 1;
        }
    }
    return name;
}

static Uint64 elapsed_ms(void)
{
    Uint64 now = SDL_GetPerformanceCounter();
    if (g_log.start_counter == 0) {
        g_log.start_counter = now;
    }
    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 delta = now - g_log.start_counter;
    return (delta / freq) * 1000u + (delta % freq) * 1000u / freq;
}

static void emit(EngLogLevel level, const char *file, int line, const char *msg)
{
    char loc[128];
    if (line > 0) {
        snprintf(loc, sizeof loc, "%s:%d", base_name(file), line);
    } else {
        snprintf(loc, sizeof loc, "%s", base_name(file));
    }

    char text[LOG_LINE_MAX];
    Uint64 ms = elapsed_ms();
    int n =
        snprintf(text, sizeof text, "[%6llu.%03llu] %s %s  %s\n", (unsigned long long)(ms / 1000u),
                 (unsigned long long)(ms % 1000u), level_names[level], loc, msg);
    if (n < 0) {
        return;
    }
    size_t len = (size_t)n;
    if (len >= sizeof text) { /* tronqué : on garde la fin de ligne */
        len = sizeof text - 1;
        text[len - 1] = '\n';
    }

    if (g_log.mutex != NULL) {
        SDL_LockMutex(g_log.mutex);
    }
    fwrite(text, 1, len, stderr);
    if (g_log.file != NULL) {
        SDL_WriteIO(g_log.file, text, len);
        SDL_FlushIO(g_log.file); /* un message n'est jamais perdu si le programme plante */
    }
    if (g_log.mutex != NULL) {
        SDL_UnlockMutex(g_log.mutex);
    }
}

void eng_log_write(EngLogLevel level, const char *file, int line, const char *fmt, ...)
{
    if ((int)level < SDL_GetAtomicInt(&g_log.level) || (int)level < 0 ||
        (int)level >= LOG_LEVEL_COUNT) {
        return;
    }

    char msg[LOG_MSG_MAX];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(msg, sizeof msg, fmt, args);
    va_end(args);

    if (n < 0) {
        SDL_strlcpy(msg, "(erreur de formatage)", sizeof msg);
    } else if ((size_t)n >= sizeof msg) {
        memcpy(msg + sizeof msg - 4, "...", 4);
    }
    emit(level, file, line, msg);
}

static EngLogLevel from_sdl_priority(SDL_LogPriority priority)
{
    switch (priority) {
    case SDL_LOG_PRIORITY_TRACE:
    case SDL_LOG_PRIORITY_VERBOSE:
        return ENG_LEVEL_TRACE;
    case SDL_LOG_PRIORITY_DEBUG:
        return ENG_LEVEL_DEBUG;
    case SDL_LOG_PRIORITY_INFO:
        return ENG_LEVEL_INFO;
    case SDL_LOG_PRIORITY_WARN:
        return ENG_LEVEL_WARN;
    default:
        return ENG_LEVEL_ERROR;
    }
}

/* Redirige les messages internes de SDL vers notre journal. */
static void SDLCALL sdl_log_bridge(void *userdata, int category, SDL_LogPriority priority,
                                   const char *message)
{
    (void)userdata;
    (void)category;
    EngLogLevel level = from_sdl_priority(priority);
    if ((int)level >= SDL_GetAtomicInt(&g_log.level)) {
        emit(level, "sdl", 0, message);
    }
}

bool eng_log_init(const char *dir, EngLogLevel level)
{
    SDL_SetAtomicInt(&g_log.level, (int)level);
    g_log.start_counter = SDL_GetPerformanceCounter();
    if (g_log.mutex == NULL) {
        g_log.mutex = SDL_CreateMutex();
    }
    SDL_SetLogOutputFunction(sdl_log_bridge, NULL);
    g_log.sdl_hooked = true;

    if (dir == NULL) {
        return true;
    }

    char path[LOG_PATH_MAX];
    char prev[LOG_PATH_MAX];
    int n1 = snprintf(path, sizeof path, "%s/%s", dir, LOG_FILE_NAME);
    int n2 = snprintf(prev, sizeof prev, "%s/%s", dir, LOG_PREV_NAME);
    if (n1 < 0 || n2 < 0 || (size_t)n1 >= sizeof path || (size_t)n2 >= sizeof prev) {
        ENG_LOG_ERROR("chemin du journal trop long : %s", dir);
        return false;
    }

    if (!SDL_CreateDirectory(dir)) {
        ENG_LOG_ERROR("impossible de créer le dossier %s : %s", dir, SDL_GetError());
        return false;
    }
    /* Rotation : le journal du lancement précédent est conservé (une seule génération).
     * Échecs ignorés : au premier lancement il n'y a rien à renommer. */
    (void)SDL_RemovePath(prev);
    (void)SDL_RenamePath(path, prev);

    g_log.file = SDL_IOFromFile(path, "w");
    if (g_log.file == NULL) {
        ENG_LOG_ERROR("impossible de créer %s : %s", path, SDL_GetError());
        return false;
    }

    SDL_Time now = 0;
    SDL_DateTime date;
    if (SDL_GetCurrentTime(&now) && SDL_TimeToDateTime(now, &date, true)) {
        ENG_LOG_INFO("journal ouvert le %04d-%02d-%02d à %02d:%02d:%02d (heure locale), niveau %s",
                     date.year, date.month, date.day, date.hour, date.minute, date.second,
                     level_words[level]);
    } else {
        ENG_LOG_INFO("journal ouvert, niveau %s", level_words[level]);
    }
    return true;
}

void eng_log_shutdown(void)
{
    ENG_LOG_INFO("fermeture du journal");
    if (g_log.sdl_hooked) {
        SDL_SetLogOutputFunction(SDL_GetDefaultLogOutputFunction(), NULL);
        g_log.sdl_hooked = false;
    }
    if (g_log.file != NULL) {
        SDL_CloseIO(g_log.file);
        g_log.file = NULL;
    }
    if (g_log.mutex != NULL) {
        SDL_DestroyMutex(g_log.mutex);
        g_log.mutex = NULL;
    }
}

void eng_log_set_level(EngLogLevel level)
{
    SDL_SetAtomicInt(&g_log.level, (int)level);
}

EngLogLevel eng_log_get_level(void)
{
    return (EngLogLevel)SDL_GetAtomicInt(&g_log.level);
}

bool eng_log_parse_level(const char *name, EngLogLevel *out)
{
    for (int i = 0; i < LOG_LEVEL_COUNT; i++) {
        if (SDL_strcasecmp(name, level_words[i]) == 0) {
            *out = (EngLogLevel)i;
            return true;
        }
    }
    return false;
}

EngLogLevel eng_log_env_level(EngLogLevel fallback)
{
    const char *value = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "ENG_LOG_LEVEL");
    if (value == NULL || value[0] == '\0') {
        return fallback;
    }
    EngLogLevel level;
    if (!eng_log_parse_level(value, &level)) {
        ENG_LOG_WARN("ENG_LOG_LEVEL=\"%s\" inconnu (trace|debug|info|warn|error), ignoré", value);
        return fallback;
    }
    return level;
}
