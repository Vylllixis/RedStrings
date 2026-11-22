#include "game_slots.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "engine/log.h"
#include "engine/map_zones.h"
#include "engine/save_game.h"
#include "engine/world_state.h"

int64_t game_slot_now(void)
{
    SDL_Time now = 0;
    return SDL_GetCurrentTime(&now) ? (int64_t)SDL_NS_TO_SECONDS(now) : 0;
}

void game_slot_date(int64_t seconds, char *out, size_t capacity)
{
    static const char *const months[12] = {"janv.", "févr.", "mars",  "avr.", "mai",  "juin",
                                           "juil.", "août",  "sept.", "oct.", "nov.", "déc."};
    if (out == NULL || capacity == 0U) {
        return;
    }
    out[0] = '\0';
    SDL_DateTime dt;
    if (seconds <= 0 || !SDL_TimeToDateTime((SDL_Time)SDL_SECONDS_TO_NS(seconds), &dt, true) ||
        dt.month < 1 || dt.month > 12) {
        return;
    }
    (void)snprintf(out, capacity, "%d %s %d · %02d:%02d", dt.day, months[dt.month - 1], dt.year,
                   dt.hour, dt.minute);
}

/* « Niv. 3 · 12 min » ou « Niv. 7 · 2 h 05 » */
static void summary(const EngSaveGame *s, char *out, size_t cap)
{
    const unsigned level = s->has_progress && s->progress.level > 0U ? s->progress.level : 1U;
    const unsigned minutes = s->play_time > 0.0 ? (unsigned)(s->play_time / 60.0) : 0U;
    if (minutes >= 60U) {
        (void)snprintf(out, cap, "Niv. %u · %u h %02u", level, minutes / 60U, minutes % 60U);
    } else {
        (void)snprintf(out, cap, "Niv. %u · %u min", level, minutes);
    }
}

static int64_t file_time(const char *path)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) ? (int64_t)SDL_NS_TO_SECONDS(info.modify_time) : 0;
}

void game_slot_describe(const char *path, uint32_t index, uint32_t boss_count, EngMenuSlot *out,
                        int64_t *when)
{
    memset(out, 0, sizeof *out);
    *when = 0;
    SDL_PathInfo info;
    if (path == NULL || !SDL_GetPathInfo(path, &info) || info.type != SDL_PATHTYPE_FILE) {
        return; /* emplacement libre */
    }
    out->used = true;
    *when = (int64_t)SDL_NS_TO_SECONDS(info.modify_time);
    eng_menu_default_name(index, out->name, sizeof out->name);
    static EngSaveGame s;
    char err[128];
    if (!eng_save_read_file(path, &s, NULL, err, sizeof err)) {
        /* compte comme occupé : jamais proposé comme emplacement libre, donc jamais écrasé */
        out->unreadable = true;
        (void)snprintf(out->summary, sizeof out->summary, "illisible");
        return;
    }
    if (s.has_meta) {
        if (s.name[0] != '\0') {
            (void)snprintf(out->name, sizeof out->name, "%s", s.name);
        }
        (void)snprintf(out->place, sizeof out->place, "%s", s.place);
        if (s.saved_at > 0) {
            *when = s.saved_at;
        }
    }
    summary(&s, out->summary, sizeof out->summary);
    game_slot_date(*when, out->date, sizeof out->date);
    out->level = s.has_progress && s.progress.level > 0U ? s.progress.level : 1U;
    out->boss_count = boss_count;
    for (uint32_t b = 0; b < boss_count && b < ENG_BIOME_COUNT; b++) {
        const uint32_t zone = (b * ENG_MAPZONES_PER_BIOME) + (uint32_t)ENG_MAPZONE_BOSS;
        out->bosses |= (s.has_gear && (s.bosses_beaten >> zone & 1U) != 0U ? 1U : 0U) << b;
    }
}

/* relit `path` en entier (cellules du monde comprises) pour le réécrire modifié */
static bool read_whole(const char *path, EngSaveGame *s, EngWorldState *world)
{
    char err[128];
    if (!eng_save_read_file(path, s, world, err, sizeof err)) {
        ENG_LOG_WARN("%s (%s)", err, path);
        return false;
    }
    if (!s->has_meta) { /* sauvegarde d'avant les noms : elle garde la date de son fichier */
        s->has_meta = true;
        s->name[0] = '\0';
        s->place[0] = '\0';
        s->saved_at = file_time(path);
    }
    return true;
}

bool game_slot_rename(const char *path, const char *name)
{
    static EngSaveGame s;
    static EngWorldState world;
    if (path == NULL || name == NULL || name[0] == '\0' || strlen(name) >= sizeof s.name ||
        !read_whole(path, &s, &world)) {
        return false;
    }
    (void)snprintf(s.name, sizeof s.name, "%s", name);
    return eng_save_write_file(path, &s, &world);
}

/* « <nom> (copie) », en raccourcissant le nom (sans couper un caractère) pour tenir dans les
 * bornes d'un nom de partie */
static void copy_name(const char *name, char *out, size_t cap)
{
    static const char suffix[] = " (copie)";
    char base[ENG_SAVE_NAME_MAX];
    (void)snprintf(base, sizeof base, "%s", name);
    for (;;) {
        size_t bytes = strlen(base);
        uint32_t chars = 0;
        for (size_t i = 0; i < bytes; i++) {
            chars += ((unsigned char)base[i] & 0xC0U) != 0x80U ? 1U : 0U;
        }
        if (bytes + sizeof suffix <= cap && chars + 8U <= ENG_MENU_NAME_CHARS) {
            break;
        }
        if (bytes == 0U) {
            break;
        }
        do { /* retire le dernier caractère entier */
            bytes--;
        } while (bytes > 0U && ((unsigned char)base[bytes] & 0xC0U) == 0x80U);
        base[bytes] = '\0';
    }
    size_t n = strlen(base);
    while (n > 0U && base[n - 1U] == ' ') {
        base[--n] = '\0';
    }
    (void)snprintf(out, cap, "%s%s", base, suffix);
}

bool game_slot_copy(const char *from, const char *to)
{
    static EngSaveGame s;
    static EngWorldState world;
    if (from == NULL || to == NULL || !read_whole(from, &s, &world)) {
        return false;
    }
    char name[ENG_SAVE_NAME_MAX];
    copy_name(s.name[0] != '\0' ? s.name : "Partie", name, sizeof name);
    (void)snprintf(s.name, sizeof s.name, "%s", name);
    if (s.saved_at > 1) {
        s.saved_at -= 1; /* la copie est plus ancienne d'une seconde : « Continuer » reste sur
                          * l'original */
    }
    return eng_save_write_file(to, &s, &world);
}

bool game_slot_delete(const char *path)
{
    if (path == NULL) {
        return false;
    }
    char kept[1100];
    (void)snprintf(kept, sizeof kept, "%s.supprimee", path);
    (void)SDL_RemovePath(kept); /* une seule partie supprimée gardée par emplacement */
    if (SDL_RenamePath(path, kept)) {
        return true;
    }
    ENG_LOG_WARN("partie : %s n'a pas pu être mise de côté (%s)", path, SDL_GetError());
    return false;
}
