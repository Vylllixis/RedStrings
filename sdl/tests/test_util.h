#pragma once

#include <SDL3/SDL.h>
#include <string.h>

/* Petits outils communs aux tests unitaires (dossier temporaire : tests_tmp/ dans le build). */

#define TEST_TMP_DIR "tests_tmp"

static inline void test_remove_log_files(const char *dir)
{
    char path[256];
    SDL_snprintf(path, sizeof path, "%s/engine.log", dir);
    (void)SDL_RemovePath(path);
    SDL_snprintf(path, sizeof path, "%s/engine.1.log", dir);
    (void)SDL_RemovePath(path);
}

/* Contenu d'un fichier texte (à libérer avec SDL_free), ou NULL s'il n'existe pas. */
static inline char *test_read_text(const char *dir, const char *name)
{
    char path[256];
    SDL_snprintf(path, sizeof path, "%s/%s", dir, name);
    size_t size = 0;
    return SDL_LoadFile(path, &size);
}

static inline bool test_contains(const char *text, const char *needle)
{
    return text != NULL && strstr(text, needle) != NULL;
}
