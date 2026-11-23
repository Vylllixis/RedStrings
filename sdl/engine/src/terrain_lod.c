#include "engine/terrain_lod.h"

#include <math.h>
#include <string.h>

bool eng_tlod_params_valid(const EngTlodParams *p)
{
    return p != NULL && p->levels >= 1U && p->levels <= ENG_TLOD_MAX_LEVELS && p->leaf > 0.0F &&
           isfinite(p->origin[0]) && isfinite(p->origin[1]) &&
           fabsf(p->extent - p->leaf * ldexpf(1.0F, (int)p->levels - 1)) < 1e-3F * p->extent &&
           p->lod_range0 > p->leaf && p->morph_ratio > 0.0F && p->morph_ratio < 1.0F &&
           p->height_max >= p->height_min;
}

static float range_of(const EngTlodParams *p, uint32_t level)
{
    return p->lod_range0 * ldexpf(1.0F, (int)level);
}

/* distance caméra -> boîte du nœud (3D) */
static float box_dist(const EngTlodParams *p, const float mn[2], float size, const float cam[3])
{
    const float lo[3] = {mn[0], p->height_min, mn[1]};
    const float hi[3] = {mn[0] + size, p->height_max, mn[1] + size};
    float d2 = 0.0F;
    for (int a = 0; a < 3; a++) {
        const float v = cam[a] < lo[a] ? lo[a] - cam[a] : (cam[a] > hi[a] ? cam[a] - hi[a] : 0.0F);
        d2 += v * v;
    }
    return sqrtf(d2);
}

static void emit(const EngTlodParams *p, const float mn[2], float size, uint32_t level,
                 EngTlodSelection *out)
{
    if (out->count >= ENG_TLOD_MAX_NODES) {
        out->overflow = true;
        return;
    }
    EngTlodNode *n = &out->nodes[out->count++];
    n->min[0] = mn[0];
    n->min[1] = mn[1];
    n->size = size;
    n->level = level;
    const float r = range_of(p, level);
    n->morph_end = r;
    n->morph_start = r * p->morph_ratio;
    out->per_level[level]++;
}

/* parcours en profondeur avec une pile explicite (pas de récursion) ; enfants empilés dans
 * l'ordre inverse pour garder l'ordre de sortie 0, 1, 2, 3 */
typedef struct Pending {
    float min[2];
    float size;
    uint32_t level;
} Pending;

static void visit(const EngTlodParams *p, const float cam[3], EngTlodSelection *out)
{
    Pending stack[4U * ENG_TLOD_MAX_LEVELS];
    uint32_t top = 0;
    stack[top++] = (Pending){{p->origin[0], p->origin[1]}, p->extent, p->levels - 1U};
    while (top > 0U) {
        const Pending n = stack[--top];
        if (n.level == 0U || box_dist(p, n.min, n.size, cam) > range_of(p, n.level - 1U)) {
            emit(p, n.min, n.size, n.level, out);
            continue;
        }
        const float h = n.size * 0.5F;
        for (int k = 3; k >= 0; k--) {
            stack[top++] = (Pending){
                {n.min[0] + ((k & 1) != 0 ? h : 0.0F), n.min[1] + ((k & 2) != 0 ? h : 0.0F)},
                h,
                n.level - 1U};
        }
    }
}

bool eng_tlod_select(const EngTlodParams *p, const float camera[3], EngTlodSelection *out)
{
    if (!eng_tlod_params_valid(p) || camera == NULL || out == NULL || !isfinite(camera[0]) ||
        !isfinite(camera[1]) || !isfinite(camera[2])) {
        return false;
    }
    out->count = 0;
    out->overflow = false;
    memset(out->per_level, 0, sizeof out->per_level);
    visit(p, camera, out);
    return !out->overflow;
}

float eng_tlod_morph(const EngTlodNode *n, float distance)
{
    if (n == NULL || !(n->morph_end > n->morph_start)) {
        return 0.0F;
    }
    const float t = (distance - n->morph_start) / (n->morph_end - n->morph_start);
    return t < 0.0F ? 0.0F : (t > 1.0F ? 1.0F : t);
}
