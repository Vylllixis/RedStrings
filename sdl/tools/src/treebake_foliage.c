/* treebake : les triangles de feuillage (feuilles et rameaux) de la source, leurs grappes et
 * l'ombre du houppier sur lui-même. Voir treebake_interne.h. */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "treebake_interne.h"

#define GRID_RESOLUTION 40U
#define FLAT_MIN                                                                                   \
    0.15F /* |e3.y| au-dessus duquel une grappe est « à plat » : son dessus est en haut */

void tb_foliage_free(TbFoliage *f)
{
    free(f->triangles);
    free(f->centers);
    free(f->areas);
    free(f->clusters);
    free(f->start);
    free(f->sorted);
    memset(f, 0, sizeof *f);
}

static bool on_cards(const TbSource *s, uint32_t triangle)
{
    const uint32_t m = s->triangle_material[triangle];
    return m < s->material_count && s->materials[m].role == TB_FOLIAGE;
}

bool tb_foliage_gather(TbFoliage *f, const TbSource *s)
{
    memset(f, 0, sizeof *f);
    uint32_t count = 0;
    for (uint32_t t = 0; t < s->triangle_count; t++) {
        count += on_cards(s, t) ? 1U : 0U;
    }
    if (count == 0U) {
        return true;
    }
    f->triangles = malloc((size_t)count * sizeof *f->triangles);
    f->centers = malloc((size_t)count * 3U * sizeof(float));
    f->areas = malloc((size_t)count * sizeof(float));
    if (f->triangles == NULL || f->centers == NULL || f->areas == NULL) {
        tb_foliage_free(f);
        return false;
    }
    double sum[3] = {0.0, 0.0, 0.0};
    double total = 0.0;
    for (uint32_t t = 0; t < s->triangle_count; t++) {
        if (!on_cards(s, t)) {
            continue;
        }
        const float *a = s->vertices[s->indices[(size_t)t * 3U]].position;
        const float *b = s->vertices[s->indices[((size_t)t * 3U) + 1U]].position;
        const float *c = s->vertices[s->indices[((size_t)t * 3U) + 2U]].position;
        const float u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const float v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const float n[3] = {(u[1] * v[2]) - (u[2] * v[1]), (u[2] * v[0]) - (u[0] * v[2]),
                            (u[0] * v[1]) - (u[1] * v[0])};
        const float area = 0.5F * sqrtf((n[0] * n[0]) + (n[1] * n[1]) + (n[2] * n[2]));
        float *center = &f->centers[(size_t)f->count * 3U];
        for (int k = 0; k < 3; k++) {
            center[k] = (a[k] + b[k] + c[k]) / 3.0F;
            sum[k] += (double)center[k] * (double)area;
        }
        total += (double)area;
        f->areas[f->count] = area;
        f->triangles[f->count++] = t;
    }
    for (int k = 0; k < 3; k++) {
        f->crown[k] = total > 0.0 ? (float)(sum[k] / total) : 0.0F;
    }
    return true;
}

bool tb_foliage_occlusion(TbSource *s, const TbOptions *o)
{
    s->occlusion = malloc((size_t)(s->vertex_count > 0U ? s->vertex_count : 1U) * sizeof(float));
    TbFoliage f;
    if (s->occlusion == NULL || !tb_foliage_gather(&f, s)) {
        return false;
    }
    for (uint32_t v = 0; v < s->vertex_count; v++) {
        s->occlusion[v] = 1.0F;
    }
    if (f.count > 0U && o->absorption > 0.0F &&
        eng_bake_grid_build(&s->grid, f.centers, f.areas, f.count, GRID_RESOLUTION,
                            o->absorption)) {
        for (uint32_t v = 0; v < s->vertex_count; v++) {
            s->occlusion[v] = eng_bake_grid_sky(&s->grid, s->vertices[v].position);
        }
    }
    tb_foliage_free(&f);
    return true;
}

static float dot3(const float *a, const float *b)
{
    return (a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]);
}

/* Oriente le repère d'une grappe toujours de la même façon, pour que la photographie d'un bouquet
 * se pose correctement sur une autre grappe : le grand axe e1 pointe vers l'extérieur du houppier
 * (la pointe du rameau), l'axe mince e3 vers le haut (le dessus des feuilles), e2 = e3 x e1. */
static void orient(EngBakeFrame *frame, const float crown[3])
{
    const float outward[3] = {frame->center[0] - crown[0], frame->center[1] - crown[1],
                              frame->center[2] - crown[2]};
    float *e1 = frame->axis[0];
    float *e2 = frame->axis[1];
    float *e3 = frame->axis[2];
    if (dot3(e1, outward) < 0.0F) {
        for (int a = 0; a < 3; a++) {
            e1[a] = -e1[a];
        }
    }
    const bool down = fabsf(e3[1]) > FLAT_MIN ? e3[1] < 0.0F : dot3(e3, outward) < 0.0F;
    if (down) {
        for (int a = 0; a < 3; a++) {
            e3[a] = -e3[a];
        }
    }
    e2[0] = (e3[1] * e1[2]) - (e3[2] * e1[1]);
    e2[1] = (e3[2] * e1[0]) - (e3[0] * e1[2]);
    e2[2] = (e3[0] * e1[1]) - (e3[1] * e1[0]);
}

/* le repère de chaque grappe (ajusté sur les sommets de ses triangles) et l'aire de ses feuilles */
static bool make_frames(TbFoliage *f, const TbSource *s, const uint32_t *slot_of)
{
    uint32_t largest = 0;
    for (uint32_t k = 0; k < f->cluster_count; k++) {
        const uint32_t n = f->start[k + 1U] - f->start[k];
        largest = n > largest ? n : largest;
    }
    float *corners = malloc((size_t)(largest > 0U ? largest : 1U) * 9U * sizeof(float));
    if (corners == NULL) {
        return false;
    }
    bool ok = true;
    for (uint32_t k = 0; ok && k < f->cluster_count; k++) {
        const uint32_t n = f->start[k + 1U] - f->start[k];
        TbCluster *cluster = &f->clusters[k];
        for (uint32_t i = 0; i < n; i++) {
            const uint32_t t = f->sorted[f->start[k] + i];
            for (uint32_t corner = 0; corner < 3U; corner++) {
                memcpy(&corners[(((size_t)i * 3U) + corner) * 3U],
                       s->vertices[s->indices[((size_t)t * 3U) + corner]].position,
                       3U * sizeof(float));
            }
            cluster->leaf_area += f->areas[slot_of[f->start[k] + i]];
        }
        ok = n > 0U && eng_bake_fit(corners, 3U * sizeof(float), n * 3U, &cluster->frame);
        if (ok) {
            orient(&cluster->frame, f->crown);
        }
    }
    free(corners);
    return ok;
}

bool tb_foliage_cluster(TbFoliage *f, const TbSource *s, const TbOptions *o)
{
    const uint32_t wanted = o->clusters > 0U ? o->clusters : 1U;
    uint32_t *assignment = malloc((size_t)f->count * sizeof *assignment);
    float *centers = malloc((size_t)wanted * 3U * sizeof(float));
    uint32_t *cursor = malloc(((size_t)wanted + 1U) * sizeof *cursor);
    uint32_t *slot_of =
        malloc((size_t)f->count * sizeof *slot_of); /* rang trié -> rang d'origine */
    f->start = calloc((size_t)wanted + 2U, sizeof *f->start);
    f->sorted = malloc((size_t)f->count * sizeof *f->sorted);
    f->clusters = calloc(wanted, sizeof *f->clusters);
    bool ok = assignment != NULL && centers != NULL && cursor != NULL && slot_of != NULL &&
              f->start != NULL && f->sorted != NULL && f->clusters != NULL;
    if (ok) {
        f->cluster_count =
            eng_bake_cluster(f->centers, f->areas, f->count, wanted, o->seed, assignment, centers);
        ok = f->cluster_count > 0U;
    }
    if (ok) { /* triangles rangés par grappe (tri par comptage) */
        for (uint32_t i = 0; i < f->count; i++) {
            f->start[assignment[i] + 1U]++;
        }
        for (uint32_t k = 0; k < f->cluster_count; k++) {
            f->start[k + 1U] += f->start[k];
            cursor[k] = f->start[k];
        }
        for (uint32_t i = 0; i < f->count; i++) {
            const uint32_t at = cursor[assignment[i]]++;
            f->sorted[at] = f->triangles[i];
            slot_of[at] = i;
        }
        ok = make_frames(f, s, slot_of);
    }
    free(assignment);
    free(centers);
    free(cursor);
    free(slot_of);
    return ok;
}
