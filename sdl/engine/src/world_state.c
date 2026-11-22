#include "engine/world_state.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void eng_wstate_init(EngWorldState *w)
{
    if (w != NULL) {
        memset(w, 0, sizeof *w);
    }
}

static uint32_t slot_of(EngCellCoord c)
{
    uint64_t h =
        ((uint64_t)(uint32_t)c.x * 0x9E3779B1ULL) ^ ((uint64_t)(uint32_t)c.z * 0x85EBCA77ULL);
    h ^= h >> 29;
    return (uint32_t)(h % ENG_WORLD_STATE_CAP);
}

const EngCellState *eng_wstate_get(const EngWorldState *w, EngCellCoord c)
{
    if (w == NULL) {
        return NULL;
    }
    uint32_t i = slot_of(c);
    for (uint32_t probe = 0; probe < ENG_WORLD_STATE_CAP; probe++) {
        if (!w->used[i]) {
            return NULL;
        }
        if (eng_cell_equal(w->cells[i].coord, c)) {
            return &w->cells[i];
        }
        i = (i + 1U) % ENG_WORLD_STATE_CAP;
    }
    return NULL;
}

EngCellState *eng_wstate_edit(EngWorldState *w, EngCellCoord c)
{
    if (w == NULL) {
        return NULL;
    }
    uint32_t i = slot_of(c);
    for (uint32_t probe = 0; probe < ENG_WORLD_STATE_CAP; probe++) {
        if (!w->used[i]) {
            if (w->count >=
                ENG_WORLD_STATE_CAP - 1U) { /* garde une case vide : la recherche s'arrête */
                return NULL;
            }
            w->used[i] = true;
            memset(&w->cells[i], 0, sizeof w->cells[i]);
            w->cells[i].coord = c;
            w->count++;
            return &w->cells[i];
        }
        if (eng_cell_equal(w->cells[i].coord, c)) {
            return &w->cells[i];
        }
        i = (i + 1U) % ENG_WORLD_STATE_CAP;
    }
    return NULL;
}

bool eng_wstate_remove_object(EngWorldState *w, EngCellCoord c, uint32_t object_index)
{
    if (object_index >= 64U) {
        return false;
    }
    EngCellState *s = eng_wstate_edit(w, c);
    if (s == NULL) {
        return false;
    }
    s->removed |= 1ULL << object_index;
    s->revision++;
    return true;
}

bool eng_wstate_object_removed(const EngWorldState *w, EngCellCoord c, uint32_t object_index)
{
    const EngCellState *s = eng_wstate_get(w, c);
    return s != NULL && object_index < 64U && ((s->removed >> object_index) & 1ULL) != 0U;
}

bool eng_wstate_defeat_camp(EngWorldState *w, EngCellCoord c)
{
    EngCellState *s = eng_wstate_edit(w, c);
    if (s == NULL) {
        return false;
    }
    s->camps_defeated++;
    s->revision++;
    return true;
}

uint32_t eng_wstate_camps_defeated(const EngWorldState *w, EngCellCoord c)
{
    const EngCellState *s = eng_wstate_get(w, c);
    return s != NULL ? s->camps_defeated : 0U;
}

size_t eng_wstate_save(const EngWorldState *w, char *out, size_t capacity)
{
    if (w == NULL || out == NULL || capacity == 0U) {
        return 0;
    }
    size_t n = 0;
    out[0] = '\0';
    for (uint32_t i = 0; i < ENG_WORLD_STATE_CAP; i++) {
        if (!w->used[i]) {
            continue;
        }
        const EngCellState *s = &w->cells[i];
        const int k =
            snprintf(out + n, capacity - n, "cell %d %d %016" PRIx64 " %u %u\n", (int)s->coord.x,
                     (int)s->coord.z, s->removed, s->camps_defeated, s->revision);
        if (k < 0 || (size_t)k >= capacity - n) {
            out[0] = '\0';
            return 0;
        }
        n += (size_t)k;
    }
    return n;
}

bool eng_wstate_load(EngWorldState *w, const char *text)
{
    if (w == NULL || text == NULL) {
        return false;
    }
    eng_wstate_init(w); /* en cas d'erreur, `w` est laissé vide (jamais à moitié chargé) */
    const char *p = text;
    while (*p != '\0') {
        int x = 0;
        int z = 0;
        char hex[32] = {0};
        unsigned camps = 0;
        unsigned rev = 0;
        int used = 0;
        if (sscanf(p, "cell %d %d %31s %u %u%n", &x, &z, hex, &camps, &rev, &used) != 5) {
            eng_wstate_init(w);
            return false;
        }
        char *end = NULL;
        const unsigned long long removed = strtoull(hex, &end, 16);
        if (end == hex || *end != '\0') {
            eng_wstate_init(w);
            return false;
        }
        const EngCellCoord c = {x, z};
        EngCellState *s = eng_wstate_edit(w, c);
        if (s == NULL) {
            eng_wstate_init(w);
            return false;
        }
        s->removed = (uint64_t)removed;
        s->camps_defeated = camps;
        s->revision = rev;
        p += used;
        while (*p == '\n' || *p == '\r' || *p == ' ') {
            p++;
        }
    }
    return true;
}
