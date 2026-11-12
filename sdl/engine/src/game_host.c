#include "engine/game_host.h"

#include <SDL3/SDL.h>
#include <string.h>

#include "engine/assert.h"
#include "engine/log.h"

/* Numéro de copie pour tout le processus (et non par hôte) : un nom de copie n'est jamais
 * réutilisé, même par un second hôte (voir ENG_HOST_DLCLOSE_IS_NOOP dans game_host.h). */
static unsigned next_copy_id = 0;

/* Supprime le fichier d'une copie dont la bibliothèque vient d'être déchargée. Voir
 * ENG_HOST_DLCLOSE_IS_NOOP : dans ce cas le fichier est gardé jusqu'à l'arrêt. */
static void discard_copy(const char *path)
{
    if (!ENG_HOST_DLCLOSE_IS_NOOP) {
        (void)SDL_RemovePath(path);
    }
}

/* ── Services fournis au jeu ── */

static void service_log(void *userdata, EngLogLevel level, const char *file, int line,
                        const char *message)
{
    (void)userdata;
    eng_log_write(level, file, line, "%s", message);
}

static bool service_assert(void *userdata, const char *expr, const char *file, int line,
                           const char *message)
{
    (void)userdata;
    if (message != NULL) {
        return eng_assert_failf(expr, file, line, "%s", message);
    }
    return eng_assert_fail(expr, file, line);
}

static void *service_alloc(void *userdata, EngMemKind kind, size_t size, size_t align)
{
    EngMemory *memory = userdata;
    EngArena *arena = &memory->frame;
    if (kind == ENG_MEM_PERMANENT) {
        arena = &memory->permanent;
    } else if (kind == ENG_MEM_LEVEL) {
        arena = &memory->level;
    }
    return eng_arena_alloc_zero(arena, size, align);
}

void eng_game_services_init(EngServices *services, EngMemory *memory)
{
    *services = (EngServices){
        .api_version = ENG_GAME_API_VERSION,
        .userdata = memory,
        .log = service_log,
        .assert_fail = service_assert,
        .alloc = service_alloc,
    };
}

#if defined(_WIN32)
#define GAME_LIBRARY_EXTENSION "dll"
#elif defined(__APPLE__)
#define GAME_LIBRARY_EXTENSION "dylib"
#else
#define GAME_LIBRARY_EXTENSION "so"
#endif

bool eng_game_default_path(char *out, size_t out_size)
{
    const char *base = SDL_GetBasePath(); /* dossier de l'exécutable, séparateur final inclus */
    if (base == NULL) {
        return false;
    }
    return SDL_snprintf(out, out_size, "%sgame.%s", base, GAME_LIBRARY_EXTENSION) < (int)out_size;
}

/* ── Chargement ── */

typedef struct Candidate {
    SDL_SharedObject *library;
    const EngGameApi *api;
    char copy_path[512];
} Candidate;

/* Découpe "dossier/nom.ext" : écrit le dossier (sans séparateur final) et trouve l'extension. */
static void split_path(const char *path, char *dir, size_t dir_size, char *stem, size_t stem_size,
                       char *ext, size_t ext_size)
{
    const char *last_sep = NULL;
    for (const char *p = path; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') {
            last_sep = p;
        }
    }
    const char *name = last_sep != NULL ? last_sep + 1 : path;
    size_t dir_len = last_sep != NULL ? (size_t)(last_sep - path) : 0;
    if (dir_len == 0) {
        SDL_strlcpy(dir, ".", dir_size);
    } else {
        SDL_snprintf(dir, dir_size, "%.*s", (int)dir_len, path);
    }
    const char *dot = strrchr(name, '.');
    size_t stem_len = dot != NULL ? (size_t)(dot - name) : strlen(name);
    SDL_snprintf(stem, stem_size, "%.*s", (int)stem_len, name);
    SDL_strlcpy(ext, dot != NULL ? dot : "", ext_size);
}

static bool validate_api(const EngGameApi *api)
{
    if (api == NULL) {
        ENG_LOG_ERROR("jeu : eng_game_get_api a retourné NULL");
        return false;
    }
    if (api->api_version != ENG_GAME_API_VERSION) {
        ENG_LOG_ERROR("jeu : version d'API %u, le moteur attend %u", (unsigned)api->api_version,
                      (unsigned)ENG_GAME_API_VERSION);
        return false;
    }
    if (api->state_size == 0 || api->init == NULL || api->reload == NULL || api->unload == NULL ||
        api->update == NULL || api->render == NULL || api->shutdown == NULL) {
        ENG_LOG_ERROR("jeu : table d'entrées incomplète");
        return false;
    }
    return true;
}

/* Copie la bibliothèque, charge la copie et lit sa table d'entrées. En cas d'échec, rien ne
 * reste chargé ni copié. */
static bool load_candidate(EngGameHost *host, Candidate *out)
{
    char dir[512];
    char stem[256];
    char ext[64];
    split_path(host->source_path, dir, sizeof dir, stem, sizeof stem, ext, sizeof ext);

    if (SDL_snprintf(host->hot_dir, sizeof host->hot_dir, "%s/hot", dir) >=
        (int)sizeof host->hot_dir) {
        ENG_LOG_ERROR("jeu : chemin trop long");
        return false;
    }
    if (!SDL_CreateDirectory(host->hot_dir)) {
        ENG_LOG_ERROR("jeu : impossible de créer %s : %s", host->hot_dir, SDL_GetError());
        return false;
    }

    host->generation++;
    next_copy_id++;
    if (SDL_snprintf(out->copy_path, sizeof out->copy_path, "%s/%s.%u%s", host->hot_dir, stem,
                     next_copy_id, ext) >= (int)sizeof out->copy_path) {
        ENG_LOG_ERROR("jeu : chemin trop long");
        return false;
    }
    if (!SDL_CopyFile(host->source_path, out->copy_path)) {
        ENG_LOG_ERROR("jeu : copie de %s impossible : %s", host->source_path, SDL_GetError());
        return false;
    }

    out->library = SDL_LoadObject(out->copy_path);
    if (out->library == NULL) {
        ENG_LOG_ERROR("jeu : chargement de %s impossible : %s", out->copy_path, SDL_GetError());
        (void)SDL_RemovePath(out->copy_path);
        return false;
    }
    SDL_FunctionPointer entry = SDL_LoadFunction(out->library, ENG_GAME_ENTRY_NAME);
    if (entry == NULL) {
        ENG_LOG_ERROR("jeu : %s introuvable dans %s : %s", ENG_GAME_ENTRY_NAME, out->copy_path,
                      SDL_GetError());
    } else {
        out->api = ((EngGameGetApiFn)entry)();
    }
    if (entry == NULL || !validate_api(out->api)) {
        SDL_UnloadObject(out->library);
        discard_copy(out->copy_path);
        return false;
    }
    return true;
}

static bool file_version(const char *path, int64_t *mtime, int64_t *size)
{
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(path, &info) || info.type != SDL_PATHTYPE_FILE) {
        return false;
    }
    *mtime = (int64_t)info.modify_time;
    *size = (int64_t)info.size;
    return true;
}

bool eng_game_host_init(EngGameHost *host, const char *library_path, const EngServices *services,
                        EngArena *state_arena, double poll_interval_seconds)
{
    *host = (EngGameHost){0};
    if (SDL_strlcpy(host->source_path, library_path, sizeof host->source_path) >=
        sizeof host->source_path) {
        ENG_LOG_ERROR("jeu : chemin trop long");
        return false;
    }
    host->services = services;
    host->state_arena = state_arena;
    host->poll_interval = poll_interval_seconds;

    if (!file_version(library_path, &host->loaded_mtime, &host->loaded_size)) {
        ENG_LOG_ERROR("jeu : bibliothèque introuvable : %s", library_path);
        return false;
    }
    Candidate candidate = {0};
    if (!load_candidate(host, &candidate)) {
        return false;
    }
    void *state = eng_arena_alloc_zero(state_arena, candidate.api->state_size, 16);
    if (state == NULL) {
        SDL_UnloadObject(candidate.library);
        discard_copy(candidate.copy_path);
        return false;
    }

    host->library = candidate.library;
    host->api = candidate.api;
    SDL_strlcpy(host->loaded_copy, candidate.copy_path, sizeof host->loaded_copy);
    host->state = state;
    host->state_size = candidate.api->state_size;
    host->api->init(host->state, services);
    ENG_LOG_INFO("jeu chargé : %s (état %zu octets)", library_path, host->state_size);
    return true;
}

/* Remplace le code en cours par `candidate`, en gardant l'état. */
static void swap_in(EngGameHost *host, const Candidate *candidate, int64_t mtime, int64_t size)
{
    host->api->unload(host->state);
    candidate->api->reload(host->state, host->services);

    SDL_SharedObject *old_library = host->library;
    char old_copy[512];
    SDL_strlcpy(old_copy, host->loaded_copy, sizeof old_copy);

    host->library = candidate->library;
    host->api = candidate->api;
    SDL_strlcpy(host->loaded_copy, candidate->copy_path, sizeof host->loaded_copy);
    host->loaded_mtime = mtime;
    host->loaded_size = size;
    host->reload_count++;

    SDL_UnloadObject(old_library);
    discard_copy(old_copy);
}

static bool try_reload(EngGameHost *host, int64_t mtime, int64_t size)
{
    Candidate candidate = {0};
    if (!load_candidate(host, &candidate)) {
        host->refused_count++;
        host->rejected_mtime = mtime;
        host->rejected_size = size;
        return false;
    }
    if (candidate.api->state_size != host->state_size) {
        ENG_LOG_ERROR("jeu : la taille de l'état a changé (%zu -> %zu octets) : rechargement "
                      "refusé, relance le programme",
                      host->state_size, candidate.api->state_size);
        SDL_UnloadObject(candidate.library);
        discard_copy(candidate.copy_path);
        host->refused_count++;
        host->rejected_mtime = mtime;
        host->rejected_size = size;
        return false;
    }
    swap_in(host, &candidate, mtime, size);
    ENG_LOG_INFO("jeu rechargé (rechargement n°%u)", (unsigned)host->reload_count);
    return true;
}

bool eng_game_host_poll(EngGameHost *host, double now_seconds)
{
    if (now_seconds < host->next_poll) {
        return false;
    }
    host->next_poll = now_seconds + host->poll_interval;

    int64_t mtime;
    int64_t size;
    /* Fichier momentanément absent (l'éditeur de liens le recrée) : on réessaiera. */
    if (!file_version(host->source_path, &mtime, &size) || size == 0) {
        return false;
    }
    if (mtime == host->loaded_mtime && size == host->loaded_size) {
        host->pending = false;
        return false;
    }
    if (mtime == host->rejected_mtime && size == host->rejected_size) {
        return false;
    }
    if (!host->pending || mtime != host->seen_mtime || size != host->seen_size) {
        host->seen_mtime = mtime; /* première observation : on attend qu'il se stabilise */
        host->seen_size = size;
        host->pending = true;
        return false;
    }
    host->pending = false;
    return try_reload(host, mtime, size);
}

/* Supprime toutes les copies gardées pendant l'exécution (dossier hot/ : nos copies seulement). */
static void remove_kept_copies(const EngGameHost *host)
{
    int count = 0;
    char **files = SDL_GlobDirectory(host->hot_dir, NULL, 0, &count);
    if (files == NULL) {
        return;
    }
    for (int i = 0; i < count; i++) {
        char path[640];
        if (SDL_snprintf(path, sizeof path, "%s/%s", host->hot_dir, files[i]) < (int)sizeof path) {
            (void)SDL_RemovePath(path);
        }
    }
    SDL_free((void *)files);
}

void eng_game_host_shutdown(EngGameHost *host)
{
    if (host->api != NULL) {
        host->api->shutdown(host->state);
    }
    if (host->library != NULL) {
        SDL_UnloadObject(host->library);
        (void)SDL_RemovePath(host->loaded_copy);
        if (ENG_HOST_DLCLOSE_IS_NOOP) {
            remove_kept_copies(host);
        }
        (void)SDL_RemovePath(host->hot_dir); /* ne réussit que si le dossier est vide */
    }
    *host = (EngGameHost){0};
}
