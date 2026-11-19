#include "engine/ai_area.h"

#include <math.h>
#include <string.h>

void eng_ai_area_init(EngAiArea *a, float half, float agent_radius)
{
    if (a == NULL) {
        return;
    }
    memset(a, 0, sizeof *a);
    a->half = half > 4.0F && half <= 32.0F ? half : 32.0F; /* 64 m / 0,5 m = 128 cellules au plus */
    a->agent_radius = agent_radius;
}

static uint32_t count_ready(const EngWorldStream *s, double cx, double cz, double half)
{
    uint32_t n = 0;
    const EngCellCoord a = eng_cell_of(cx - half, cz - half);
    const EngCellCoord b = eng_cell_of(cx + half, cz + half);
    for (int32_t z = a.z; z <= b.z; z++) {
        for (int32_t x = a.x; x <= b.x; x++) {
            n += eng_stream_cell(s, (EngCellCoord){x, z}) != NULL ? 1U : 0U;
        }
    }
    return n;
}

bool eng_ai_area_update(EngAiArea *a, const EngWorldStream *stream, const EngWorldOrigin *origin,
                        const float focus[2])
{
    if (a == NULL || stream == NULL || origin == NULL || focus == NULL) {
        return false;
    }
    const double wx = origin->x + (double)focus[0];
    const double wz = origin->z + (double)focus[1];
    const uint32_t ready = count_ready(stream, wx, wz, (double)a->half);
    const bool moved = fabsf(focus[0] - a->center[0]) > a->half * 0.5F ||
                       fabsf(focus[1] - a->center[1]) > a->half * 0.5F;
    if (a->valid && !moved && ready == a->cells_seen) {
        return false;
    }
    a->center[0] = focus[0];
    a->center[1] = focus[1];
    a->cells_seen = ready;
    eng_aiw_init(&a->world);
    /* obstacles : les plus proches du centre d'abord, jusqu'à la capacité du monde d'IA */
    const double cx = origin->x + (double)a->center[0];
    const double cz = origin->z + (double)a->center[1];
    const EngCellCoord lo = eng_cell_of(cx - a->half - 4.0, cz - a->half - 4.0);
    const EngCellCoord hi = eng_cell_of(cx + a->half + 4.0, cz + a->half + 4.0);
    const int rings = (int)ceilf(a->half / 8.0F) + 1;
    for (int ri = 1; ri <= rings; ri++) {
        const float ring = 8.0F * (float)ri;
        for (int32_t z = lo.z; z <= hi.z; z++) {
            for (int32_t x = lo.x; x <= hi.x; x++) {
                const EngCellData *cell = eng_stream_cell(stream, (EngCellCoord){x, z});
                if (cell == NULL) {
                    continue;
                }
                for (uint32_t i = 0; i < cell->object_count; i++) {
                    const EngCellObject *o = &cell->objects[i];
                    if (o->kind == ENG_CELL_CAMP) {
                        continue;
                    }
                    const double ox = (double)x * ENG_CELL_SIZE + (double)o->pos[0];
                    const double oz = (double)z * ENG_CELL_SIZE + (double)o->pos[1];
                    const double d = fmax(fabs(ox - cx), fabs(oz - cz));
                    if (d > (double)ring || d <= (double)ring - 8.0) {
                        continue;
                    }
                    (void)eng_aiw_add_circle(&a->world, (float)(ox - origin->x),
                                             (float)(oz - origin->z), o->scale);
                }
            }
        }
    }
    const float mn[2] = {a->center[0] - a->half, a->center[1] - a->half};
    const float mx[2] = {a->center[0] + a->half, a->center[1] + a->half};
    a->valid = eng_nav_build(&a->nav, &a->world, mn, mx, 0.5F, a->agent_radius);
    a->rebuilds++;
    return a->valid;
}

void eng_ai_area_shift(EngAiArea *a, const float shift[3])
{
    if (a == NULL || shift == NULL) {
        return;
    }
    a->center[0] += shift[0];
    a->center[1] += shift[2];
    for (uint32_t i = 0; i < a->world.count; i++) {
        a->world.obs[i].c[0] += shift[0];
        a->world.obs[i].c[1] += shift[2];
    }
    a->nav.origin[0] += shift[0];
    a->nav.origin[1] += shift[2];
}
