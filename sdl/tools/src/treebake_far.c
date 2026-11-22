/* treebake : la vue lointaine. L'arbre entier (tronc et feuillage) photographié de côté sous trois
 * angles et de dessus, sur quatre rectangles qui se croisent le long du tronc. Voir treebake.h. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "treebake_interne.h"

#define SIDE_VIEWS 3U
#define PI_F 3.14159265F
#define TOP_HEIGHT                                                                                 \
    0.62F /* hauteur (part de l'arbre) du rectangle horizontal : dans le houppier                  \
           */

/* les triangles gardés (tronc et feuillage) et l'étendue de l'arbre */
static uint32_t *kept_triangles(const TbSource *s, uint32_t *count, float low[3], float high[3])
{
    uint32_t *list =
        malloc((size_t)(s->triangle_count > 0U ? s->triangle_count : 1U) * sizeof *list);
    *count = 0;
    if (list == NULL) {
        return NULL;
    }
    for (uint32_t t = 0; t < s->triangle_count; t++) {
        const uint32_t m = s->triangle_material[t];
        if (m >= s->material_count || s->materials[m].role == TB_DROPPED) {
            continue;
        }
        for (uint32_t k = 0; k < 3U; k++) {
            const float *p = s->vertices[s->indices[((size_t)t * 3U) + k]].position;
            for (int a = 0; a < 3; a++) {
                low[a] = *count == 0U && k == 0U ? p[a] : (p[a] < low[a] ? p[a] : low[a]);
                high[a] = *count == 0U && k == 0U ? p[a] : (p[a] > high[a] ? p[a] : high[a]);
            }
        }
        list[(*count)++] = t;
    }
    return list;
}

/* demi-largeur de l'arbre le long de `axis` (depuis l'axe du tronc, en x = z = 0) */
static float half_extent(const TbSource *s, const uint32_t *list, uint32_t count,
                         const float axis[3])
{
    float half = 0.0F;
    for (uint32_t i = 0; i < count; i++) {
        for (uint32_t k = 0; k < 3U; k++) {
            const float *p = s->vertices[s->indices[((size_t)list[i] * 3U) + k]].position;
            const float along = fabsf((p[0] * axis[0]) + (p[2] * axis[2]));
            half = along > half ? along : half;
        }
    }
    return half > 1e-3F ? half : 1e-3F;
}

static void make_views(const TbSource *s, const uint32_t *list, uint32_t count, const float low[3],
                       const float high[3], EngBakeView views[TB_FAR_VIEWS])
{
    const float middle = 0.5F * (low[1] + high[1]);
    const float half_height = 0.5F * (high[1] - low[1]);
    for (uint32_t k = 0; k < SIDE_VIEWS; k++) { /* de côté : tous les 60 degrés */
        const float angle = (float)k * PI_F / (float)SIDE_VIEWS;
        EngBakeView *v = &views[k];
        *v = (EngBakeView){.origin = {0.0F, middle, 0.0F},
                           .axis_u = {cosf(angle), 0.0F, -sinf(angle)},
                           .axis_v = {0.0F, 1.0F, 0.0F},
                           .axis_n = {sinf(angle), 0.0F, cosf(angle)},
                           .half_v = half_height > 1e-3F ? half_height : 1e-3F};
        v->half_u = half_extent(s, list, count, v->axis_u);
    }
    EngBakeView *top = &views[SIDE_VIEWS]; /* de dessus */
    *top = (EngBakeView){.origin = {0.0F, low[1] + (TOP_HEIGHT * (high[1] - low[1])), 0.0F},
                         .axis_u = {1.0F, 0.0F, 0.0F},
                         .axis_v = {0.0F, 0.0F, -1.0F},
                         .axis_n = {0.0F, 1.0F, 0.0F}};
    top->half_u = half_extent(s, list, count, top->axis_u);
    top->half_v = half_extent(s, list, count, top->axis_v);
}

bool tb_far_build(TbFar *out, const TbSource *s, const TbOptions *o)
{
    memset(out, 0, sizeof *out);
    uint32_t count = 0;
    float low[3] = {0.0F, 0.0F, 0.0F};
    float high[3] = {0.0F, 0.0F, 0.0F};
    uint32_t *list = kept_triangles(s, &count, low, high);
    EngBakeView views[TB_FAR_VIEWS];
    EngBakeRect rects[TB_FAR_VIEWS] = {{0U, 0U, 0U, 0U}};
    TbAtlas atlas = {0};
    bool ok = list != NULL && count > 0U;
    float density = 0.0F;
    if (ok) {
        make_views(s, list, count, low, high, views);
        density = tb_atlas_layout(views, TB_FAR_VIEWS, o->far_atlas, o->far_atlas, rects);
        ok = density > 0.0F && tb_atlas_init(&atlas, o->far_atlas, o->far_atlas) &&
             tb_mesh_alloc(&out->mesh, TB_FAR_VIEWS);
    }
    const EngBakeMesh mesh = tb_source_mesh(s);
    for (uint32_t k = 0; ok && k < TB_FAR_VIEWS; k++) {
        ok = tb_atlas_shoot(&atlas, &mesh, list, count, &views[k], &rects[k], o->snow, NULL);
        if (ok) {
            /* l'ombre du feuillage est déjà dans la photographie : pas de grille */
            tb_mesh_add_quad(&out->mesh, &views[k], &rects[k], o->far_atlas, o->far_atlas,
                             TB_FLIP_NONE, NULL);
        }
    }
    ok = ok && tb_atlas_finish(&atlas, rects, TB_FAR_VIEWS) &&
         tb_atlas_write(&atlas, o, "far", out->color_uri, out->normal_uri, out->arm_uri);
    if (ok) {
        (void)printf("vue lointaine : %u triangles sur %u rectangles, %.0f pixels par mètre\n",
                     (unsigned)count, (unsigned)TB_FAR_VIEWS, (double)density);
    } else {
        (void)fprintf(stderr, "treebake : vue lointaine impossible\n");
    }
    tb_atlas_free(&atlas);
    free(list);
    return ok;
}

void tb_far_free(TbFar *f)
{
    tb_mesh_free(&f->mesh);
    memset(f, 0, sizeof *f);
}
