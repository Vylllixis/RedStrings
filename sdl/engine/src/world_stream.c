#include "engine/world_stream.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int32_t cheb(EngCellCoord a, EngCellCoord b)
{
    const int32_t dx = a.x > b.x ? a.x - b.x : b.x - a.x;
    const int32_t dz = a.z > b.z ? a.z - b.z : b.z - a.z;
    return dx > dz ? dx : dz;
}

static void load_job(void *user)
{
    EngStreamSlot *slot = user;
    const EngWorldStream *s = slot->owner;
    slot->from_file = false;
    slot->failed = false;
    if (s->dir != NULL) {
        char path[1024];
        (void)snprintf(path, sizeof path, "%s/cell_%d_%d.rscl", s->dir, (int)slot->coord.x,
                       (int)slot->coord.z);
        size_t size = 0;
        void *data = SDL_LoadFile(path, &size); /* thread de travail : n'arrête pas l'image */
        if (data != NULL) {
            slot->from_file = eng_cell_decode(data, size, &slot->data) &&
                              eng_cell_equal(slot->data.coord, slot->coord);
            SDL_free(data);
        }
    }
    if (!slot->from_file && !eng_cell_generate(&s->gen, slot->coord, &slot->data)) {
        slot->failed = true;
    }
}

bool eng_stream_init(EngWorldStream *s, const EngWorldGen *gen, EngJobs *jobs, int32_t load_radius,
                     const char *dir)
{
    if (s == NULL || gen == NULL || jobs == NULL || load_radius < 1 || load_radius > 3) {
        return false;
    }
    memset(s, 0, sizeof *s);
    s->gen = *gen;
    s->jobs = jobs;
    s->dir = dir;
    s->load_radius = load_radius;
    s->max_requests = 4U;
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
        s->slots[i].owner = s;
    }
    return true;
}

static EngStreamSlot *find(EngWorldStream *s, EngCellCoord c)
{
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
        if (s->slots[i].state != ENG_SLOT_FREE && eng_cell_equal(s->slots[i].coord, c)) {
            return &s->slots[i];
        }
    }
    return NULL;
}

static void release(EngWorldStream *s, EngStreamSlot *slot)
{
    if (slot->state == ENG_SLOT_READY) {
        if (s->on_unload != NULL) {
            s->on_unload(s->user, &slot->data);
        }
        s->loaded--;
        s->unloads++;
    }
    slot->state = ENG_SLOT_FREE;
}

uint32_t eng_stream_update(EngWorldStream *s, double x, double z)
{
    if (s == NULL || !isfinite(x) || !isfinite(z)) {
        return 0;
    }
    s->center = eng_cell_of(x, z);
    uint32_t became_ready = 0;
    uint32_t used = 0;
    /* 1 et 2 : récolte, libération */
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
        EngStreamSlot *slot = &s->slots[i];
        if ((slot->state == ENG_SLOT_LOADING || slot->state == ENG_SLOT_RELEASING) &&
            eng_jobs_finished(&slot->done)) {
            s->in_flight--;
            s->loads_done++;
            if (slot->state == ENG_SLOT_RELEASING || slot->failed) {
                slot->state = ENG_SLOT_FREE;
            } else {
                slot->state = ENG_SLOT_READY;
                s->loaded++;
                s->files_read += slot->from_file ? 1U : 0U;
                became_ready++;
                if (s->on_ready != NULL) {
                    s->on_ready(s->user, &slot->data);
                }
            }
        }
        if (slot->state != ENG_SLOT_FREE && cheb(slot->coord, s->center) > s->load_radius + 1) {
            if (slot->state == ENG_SLOT_LOADING) {
                slot->state = ENG_SLOT_RELEASING;
            } else if (slot->state == ENG_SLOT_READY) {
                release(s, slot);
            }
        }
        used += slot->state != ENG_SLOT_FREE ? 1U : 0U;
    }
    if (used > s->peak_slots) {
        s->peak_slots = used;
    }
    /* 3 : demandes, de la plus proche à la plus lointaine (anneaux de Chebyshev) */
    uint32_t requests = 0;
    for (int32_t r = 0; r <= s->load_radius && requests < s->max_requests; r++) {
        for (int32_t dz = -r; dz <= r && requests < s->max_requests; dz++) {
            for (int32_t dx = -r; dx <= r && requests < s->max_requests; dx++) {
                if ((dx < 0 ? -dx : dx) != r && (dz < 0 ? -dz : dz) != r) {
                    continue;
                }
                const EngCellCoord c = {s->center.x + dx, s->center.z + dz};
                if (find(s, c) != NULL) {
                    continue;
                }
                EngStreamSlot *slot = NULL;
                for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
                    if (s->slots[i].state == ENG_SLOT_FREE) {
                        slot = &s->slots[i];
                        break;
                    }
                }
                if (slot == NULL) {
                    s->refused++;
                    return became_ready;
                }
                slot->coord = c;
                slot->state = ENG_SLOT_LOADING;
                eng_job_counter_set(&slot->done, 0);
                if (!eng_jobs_submit(s->jobs, load_job, slot, NULL, &slot->done)) {
                    slot->state = ENG_SLOT_FREE;
                    s->refused++;
                    return became_ready;
                }
                s->in_flight++;
                s->loads_started++;
                requests++;
            }
        }
    }
    return became_ready;
}

void eng_stream_shutdown(EngWorldStream *s)
{
    if (s == NULL) {
        return;
    }
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
        EngStreamSlot *slot = &s->slots[i];
        if (slot->state == ENG_SLOT_LOADING || slot->state == ENG_SLOT_RELEASING) {
            eng_jobs_wait(s->jobs, &slot->done);
            s->in_flight--;
            slot->state = ENG_SLOT_FREE;
        } else if (slot->state == ENG_SLOT_READY) {
            release(s, slot);
        }
    }
}

const EngCellData *eng_stream_cell(const EngWorldStream *s, EngCellCoord c)
{
    if (s == NULL) {
        return NULL;
    }
    for (uint32_t i = 0; i < ENG_STREAM_SLOTS; i++) {
        if (s->slots[i].state == ENG_SLOT_READY && eng_cell_equal(s->slots[i].coord, c)) {
            return &s->slots[i].data;
        }
    }
    return NULL;
}

bool eng_stream_ready_around(const EngWorldStream *s, double x, double z, int32_t radius)
{
    const EngCellCoord c = eng_cell_of(x, z);
    for (int32_t dz = -radius; dz <= radius; dz++) {
        for (int32_t dx = -radius; dx <= radius; dx++) {
            const EngCellCoord k = {c.x + dx, c.z + dz};
            if (eng_stream_cell(s, k) == NULL) {
                return false;
            }
        }
    }
    return true;
}

bool eng_stream_height(const EngWorldStream *s, double x, double z, float *out)
{
    const EngCellCoord c = eng_cell_of(x, z);
    const EngCellData *cell = eng_stream_cell(s, c);
    if (cell == NULL || out == NULL) {
        return false;
    }
    *out = eng_cell_height(cell, (float)(x - (double)c.x * ENG_CELL_SIZE),
                           (float)(z - (double)c.z * ENG_CELL_SIZE));
    return true;
}

size_t eng_stream_memory(void)
{
    return sizeof(EngStreamSlot) * ENG_STREAM_SLOTS;
}
