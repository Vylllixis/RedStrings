#include "engine/files.h"

#include <SDL3/SDL.h>
#include <string.h>

#include "engine/log.h"

/* ── Chemins ── */

bool eng_path_base(char *out, size_t out_size)
{
    const char *base = SDL_GetBasePath();
    if (base == NULL) {
        return false;
    }
    return SDL_strlcpy(out, base, out_size) < out_size;
}

bool eng_path_is_absolute(const char *path)
{
    if (path[0] == '/' || path[0] == '\\') {
        return true;
    }
    /* Lettre de lecteur Windows : « C:\ » ou « C:/ » */
    return ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
           path[1] == ':' && (path[2] == '\\' || path[2] == '/');
}

bool eng_path_resolve(char *out, size_t out_size, const char *relative)
{
    if (eng_path_is_absolute(relative)) {
        return SDL_strlcpy(out, relative, out_size) < out_size;
    }
    const char *base = SDL_GetBasePath();
    if (base == NULL) {
        return false;
    }
    return SDL_snprintf(out, out_size, "%s%s", base, relative) < (int)out_size;
}

/* ── Lecture ── */

bool eng_file_exists(const char *path)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE;
}

bool eng_file_read_all(EngArena *arena, const char *path, EngFileData *out)
{
    *out = (EngFileData){0};

    SDL_PathInfo info;
    if (!SDL_GetPathInfo(path, &info)) {
        return false; /* absent : cas normal, pas de message */
    }
    if (info.type != SDL_PATHTYPE_FILE) {
        ENG_LOG_ERROR("lecture de %s : ce n'est pas un fichier ordinaire", path);
        return false;
    }
    if (info.size > ENG_FILE_MAX_SIZE) {
        ENG_LOG_ERROR("lecture de %s : %llu octets, au-delà de la limite de %zu", path,
                      (unsigned long long)info.size, ENG_FILE_MAX_SIZE);
        return false;
    }
    size_t size = (size_t)info.size;

    EngArenaMark mark = eng_arena_mark(arena);
    uint8_t *buffer = eng_arena_alloc(arena, size + 1, 16); /* +1 : octet nul final */
    if (buffer == NULL) {
        return false; /* l'arène a déjà journalisé la raison */
    }

    SDL_IOStream *stream = SDL_IOFromFile(path, "rb");
    if (stream == NULL) {
        ENG_LOG_ERROR("lecture de %s impossible : %s", path, SDL_GetError());
        eng_arena_reset_to(arena, mark);
        return false;
    }
    size_t read = size > 0 ? SDL_ReadIO(stream, buffer, size) : 0;
    SDL_CloseIO(stream);
    if (read != size) {
        ENG_LOG_ERROR("lecture de %s : %zu octets lus sur %zu", path, read, size);
        eng_arena_reset_to(arena, mark);
        return false;
    }

    buffer[size] = 0;
    out->data = buffer;
    out->size = size;
    return true;
}

/* ── Surveillance ── */

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

bool eng_file_watch_init(EngFileWatch *watch, const char *path, double interval_seconds)
{
    *watch = (EngFileWatch){0};
    if (SDL_strlcpy(watch->path, path, sizeof watch->path) >= sizeof watch->path) {
        ENG_LOG_ERROR("surveillance : chemin trop long");
        return false;
    }
    watch->interval = interval_seconds > 0.0 ? interval_seconds : ENG_FILE_WATCH_DEFAULT_INTERVAL;
    if (!file_version(path, &watch->base_mtime, &watch->base_size)) {
        watch->base_mtime = 0;
        watch->base_size = -1;
    }
    return true;
}

bool eng_file_watch_poll(EngFileWatch *watch, double now_seconds)
{
    if (now_seconds < watch->next_poll) {
        return false;
    }
    watch->next_poll = now_seconds + watch->interval;

    int64_t mtime;
    int64_t size;
    if (!file_version(watch->path, &mtime, &size)) {
        watch->pending = false; /* absent (ou en cours de recréation) : on réessaiera */
        return false;
    }
    if (mtime == watch->base_mtime && size == watch->base_size) {
        watch->pending = false;
        return false;
    }
    if (!watch->pending || mtime != watch->seen_mtime || size != watch->seen_size) {
        watch->seen_mtime = mtime; /* nouvelle version : on attend qu'elle se stabilise */
        watch->seen_size = size;
        watch->pending = true;
        return false;
    }
    watch->base_mtime = mtime;
    watch->base_size = size;
    watch->pending = false;
    return true;
}
