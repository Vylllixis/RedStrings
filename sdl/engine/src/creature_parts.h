#ifndef ENG_CREATURE_PARTS_H
#define ENG_CREATURE_PARTS_H

/* Interne à engine/src/creature*.c : la liste de boîtes en cours de construction. */

#include <math.h>
#include <stddef.h>

#include "engine/creature.h"

typedef struct Builder {
    EngCreaturePart *out;
    uint32_t max;
    uint32_t count;
} Builder;

static inline float unit(float v)
{
    if (!isfinite(v) || v < 0.0F) {
        return 0.0F;
    }
    return v > 1.0F ? 1.0F : v;
}

static inline float finite_or_zero(float v)
{
    return isfinite(v) ? v : 0.0F;
}

/* ajoute une boîte ; ignorée si la liste est pleine */
static inline void box(Builder *b, EngCreatureColor color, float x, float y, float z, float hx,
                       float hy, float hz, float pitch, float yaw, float roll)
{
    if (b->out == NULL || b->count >= b->max) {
        return;
    }
    b->out[b->count++] = (EngCreaturePart){
        .pos = {x, y, z}, .half = {hx, hy, hz}, .rot = {pitch, yaw, roll}, .color = (uint8_t)color};
}

#endif
