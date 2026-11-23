/* treebake : le feuillage en cartes. Les triangles de feuilles et de rameaux sont regroupés en
 * grappes voisines (treebake_foliage.c) ; chaque grappe devient trois cartes perpendiculaires.
 *
 * Photographier chaque grappe donnerait des centaines de petites images floues. Comme dans les
 * jeux, seules quelques grappes, les BOUQUETS, sont photographiées en haute résolution ; chaque
 * autre grappe reprend les photographies du bouquet qui lui ressemble le plus (même forme), posées
 * à sa place, dans son orientation, parfois en miroir. Voir treebake.h. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "treebake_interne.h"

#define COVER_MIN 0.012F /* une photographie moins couverte que cela est abandonnée */
#define THIN_MIN                                                                                   \
    0.004F /* demi-hauteur (m) sous laquelle une carte n'est qu'un trait : abandonnée */
#define LOD_SCALE                                                                                  \
    1.12F               /* à mi-distance : deux cartes par grappe, agrandies pour couvrir autant  \
                         */
#define SCALE_MIN 0.75F /* une grappe reprend son bouquet à cette échelle au moins */
#define SCALE_MAX 1.3F
#define NONE UINT32_MAX

/* Les trois plans d'une grappe de repère (e1 vers l'extérieur, e2, e3 vers le haut) :
 *   0 : (e1, e2), vu de dessus ; 1 : (e1, e3), vu de côté ; 2 : (e2, e3), vu depuis la pointe. */
static void plane_view(const EngBakeFrame *f, uint32_t plane, const float half[3], float scale,
                       EngBakeView *out)
{
    static const int axes[TB_PLANES][2] = {{0, 1}, {0, 2}, {1, 2}};
    const float *u = f->axis[axes[plane][0]];
    const float *v = f->axis[axes[plane][1]];
    for (int a = 0; a < 3; a++) {
        out->origin[a] = f->center[a];
        out->axis_u[a] = u[a];
        out->axis_v[a] = v[a];
    }
    out->axis_n[0] = (u[1] * v[2]) - (u[2] * v[1]);
    out->axis_n[1] = (u[2] * v[0]) - (u[0] * v[2]);
    out->axis_n[2] = (u[0] * v[1]) - (u[1] * v[0]);
    out->half_u = half[axes[plane][0]] * scale;
    out->half_v = half[axes[plane][1]] * scale;
}

static int compare_floats(const void *a, const void *b)
{
    const float x = *(const float *)a;
    const float y = *(const float *)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

/* Range les grappes en `o->bouquets` familles de même forme (demi-étendues voisines) et choisit le
 * bouquet de chacune : la grappe la plus feuillue parmi la moitié la plus proche de la forme
 * moyenne. Écrit cluster->bouquet ; renvoie le nombre de bouquets et leurs grappes. */
static uint32_t choose_bouquets(TbFoliage *f, const TbOptions *o, uint32_t *bouquet_cluster)
{
    const uint32_t wanted = o->bouquets < f->cluster_count ? o->bouquets : f->cluster_count;
    float *shapes = malloc((size_t)f->cluster_count * 3U * sizeof(float));
    float *centers = malloc((size_t)wanted * 3U * sizeof(float));
    float *distance = malloc((size_t)f->cluster_count * sizeof(float));
    float *sorted = malloc((size_t)f->cluster_count * sizeof(float));
    uint32_t *family = malloc((size_t)f->cluster_count * sizeof *family);
    uint32_t families = 0;
    if (shapes != NULL && centers != NULL && distance != NULL && sorted != NULL && family != NULL) {
        for (uint32_t k = 0; k < f->cluster_count; k++) {
            memcpy(&shapes[(size_t)k * 3U], f->clusters[k].frame.half, 3U * sizeof(float));
        }
        families =
            eng_bake_cluster(shapes, NULL, f->cluster_count, wanted, o->seed + 1U, family, centers);
    }
    for (uint32_t g = 0; g < families; g++) {
        uint32_t members = 0;
        for (uint32_t k = 0; k < f->cluster_count; k++) {
            if (family[k] != g) {
                continue;
            }
            const float *half = f->clusters[k].frame.half;
            const float *c = &centers[(size_t)g * 3U];
            distance[k] = ((half[0] - c[0]) * (half[0] - c[0])) +
                          ((half[1] - c[1]) * (half[1] - c[1])) +
                          ((half[2] - c[2]) * (half[2] - c[2]));
            sorted[members++] = distance[k];
        }
        qsort(sorted, members, sizeof(float), compare_floats);
        const float limit = members > 0U ? sorted[(members - 1U) / 2U] : 0.0F;
        uint32_t best = NONE;
        for (uint32_t k = 0; k < f->cluster_count; k++) {
            if (family[k] == g && distance[k] <= limit &&
                (best == NONE || f->clusters[k].leaf_area > f->clusters[best].leaf_area)) {
                best = k;
            }
        }
        bouquet_cluster[g] = best;
        for (uint32_t k = 0; k < f->cluster_count; k++) {
            if (family[k] == g) {
                f->clusters[k].bouquet = g;
            }
        }
    }
    free(shapes);
    free(centers);
    free(distance);
    free(sorted);
    free(family);
    return families;
}

typedef struct Photos { /* les photographies des bouquets : TB_PLANES par bouquet */
    EngBakeView *views;
    EngBakeRect *rects;
    uint32_t *slot; /* par bouquet et par plan : rang dans views / rects, NONE si abandonnée */
    uint32_t count;
} Photos;

static void photos_free(Photos *p)
{
    free(p->views);
    free(p->rects);
    free(p->slot);
    memset(p, 0, sizeof *p);
}

static bool plan_photos(const TbFoliage *f, const uint32_t *bouquet_cluster, uint32_t bouquets,
                        Photos *p)
{
    memset(p, 0, sizeof *p);
    const size_t capacity = (size_t)bouquets * TB_PLANES;
    p->views = malloc(capacity * sizeof *p->views);
    p->rects = calloc(capacity, sizeof *p->rects);
    p->slot = malloc(capacity * sizeof *p->slot);
    if (p->views == NULL || p->rects == NULL || p->slot == NULL) {
        photos_free(p);
        return false;
    }
    for (uint32_t g = 0; g < bouquets; g++) {
        const EngBakeFrame *frame = &f->clusters[bouquet_cluster[g]].frame;
        for (uint32_t plane = 0; plane < TB_PLANES; plane++) {
            EngBakeView view;
            plane_view(frame, plane, frame->half, 1.0F, &view);
            p->slot[((size_t)g * TB_PLANES) + plane] = NONE;
            if (view.half_v >= THIN_MIN && view.half_u >= THIN_MIN) {
                p->slot[((size_t)g * TB_PLANES) + plane] = p->count;
                p->views[p->count++] = view;
            }
        }
    }
    return p->count > 0U;
}

/* photographie chaque bouquet ; une photographie presque vide est abandonnée */
static bool shoot_photos(TbAtlas *atlas, const TbSource *s, const TbFoliage *f, const TbOptions *o,
                         const uint32_t *bouquet_cluster, uint32_t bouquets, Photos *p)
{
    EngBakeMesh mesh = tb_source_mesh(s);
    mesh.occlusion = NULL; /* l'ombre du houppier dépend de l'endroit : elle va dans les sommets */
    for (uint32_t g = 0; g < bouquets; g++) {
        const uint32_t k = bouquet_cluster[g];
        for (uint32_t plane = 0; plane < TB_PLANES; plane++) {
            uint32_t *slot = &p->slot[((size_t)g * TB_PLANES) + plane];
            if (*slot == NONE) {
                continue;
            }
            float covered = 0.0F;
            if (!tb_atlas_shoot(atlas, &mesh, &f->sorted[f->start[k]],
                                f->start[k + 1U] - f->start[k], &p->views[*slot], &p->rects[*slot],
                                o->snow, &covered)) {
                return false;
            }
            if (covered < COVER_MIN) {
                *slot = NONE;
            }
        }
    }
    return true;
}

/* Les cartes de chaque grappe : les photographies de son bouquet, à sa place et à son échelle. Une
 * grappe sur deux les porte en miroir (le long de son axe e2) : les répétitions se voient moins. */
static bool make_meshes(TbCards *out, const TbSource *s, const TbFoliage *f, const TbOptions *o,
                        const uint32_t *bouquet_cluster, const Photos *p)
{
    static const TbFlip mirror[TB_PLANES] = {TB_FLIP_V, TB_FLIP_NONE, TB_FLIP_U};
    const uint32_t quads = f->cluster_count * TB_PLANES;
    if (!tb_mesh_alloc(&out->lod0, quads) || !tb_mesh_alloc(&out->lod1, quads)) {
        return false;
    }
    for (uint32_t k = 0; k < f->cluster_count; k++) {
        const TbCluster *cluster = &f->clusters[k];
        const float *own = cluster->frame.half;
        const float *model = f->clusters[bouquet_cluster[cluster->bouquet]].frame.half;
        float scale = cbrtf((own[0] * own[1] * own[2]) / (model[0] * model[1] * model[2]));
        scale = scale < SCALE_MIN ? SCALE_MIN : (scale > SCALE_MAX ? SCALE_MAX : scale);
        const bool mirrored = ((k * 2654435761U) >> 16U) % 2U == 1U;
        for (uint32_t plane = 0; plane < TB_PLANES; plane++) {
            const uint32_t slot = p->slot[((size_t)cluster->bouquet * TB_PLANES) + plane];
            if (slot == NONE) {
                continue;
            }
            const TbFlip flip = mirrored ? mirror[plane] : TB_FLIP_NONE;
            EngBakeView view;
            plane_view(&cluster->frame, plane, model, scale, &view);
            tb_mesh_add_quad(&out->lod0, &view, &p->rects[slot], o->atlas, o->atlas, flip,
                             &s->grid);
            if (plane < 2U) {
                plane_view(&cluster->frame, plane, model, scale * LOD_SCALE, &view);
                tb_mesh_add_quad(&out->lod1, &view, &p->rects[slot], o->atlas, o->atlas, flip,
                                 &s->grid);
            }
        }
    }
    return out->lod0.triangle_count > 0U;
}

bool tb_cards_build(TbCards *out, const TbSource *s, const TbOptions *o)
{
    memset(out, 0, sizeof *out);
    TbFoliage f;
    if (!tb_foliage_gather(&f, s)) {
        return false;
    }
    if (f.count == 0U) {
        tb_foliage_free(&f);
        return true; /* un arbre nu : pas de cartes */
    }
    memcpy(out->crown_center, f.crown, sizeof f.crown);
    Photos photos = {0};
    TbAtlas atlas = {0};
    uint32_t *bouquet_cluster = NULL;
    uint32_t bouquets = 0;
    bool ok = tb_foliage_cluster(&f, s, o);
    if (ok) {
        bouquet_cluster = malloc((size_t)f.cluster_count * sizeof *bouquet_cluster);
        bouquets = bouquet_cluster != NULL ? choose_bouquets(&f, o, bouquet_cluster) : 0U;
        ok = bouquets > 0U && plan_photos(&f, bouquet_cluster, bouquets, &photos);
    }
    if (ok) {
        out->clusters = f.cluster_count;
        out->bouquets = bouquets;
        out->density =
            tb_atlas_layout(photos.views, photos.count, o->atlas, o->atlas, photos.rects);
        ok = out->density > 0.0F && tb_atlas_init(&atlas, o->atlas, o->atlas) &&
             shoot_photos(&atlas, s, &f, o, bouquet_cluster, bouquets, &photos) &&
             tb_atlas_finish(&atlas, photos.rects, photos.count) &&
             tb_atlas_write(&atlas, o, "leaves", out->color_uri, out->normal_uri, out->arm_uri) &&
             make_meshes(out, s, &f, o, bouquet_cluster, &photos);
    }
    if (ok) {
        (void)printf("feuillage : %u triangles -> %u grappes, %u bouquets photographiés (%u "
                     "images, %.0f pixels par mètre), %u cartes de près, %u à mi-distance\n",
                     (unsigned)f.count, (unsigned)f.cluster_count, (unsigned)bouquets,
                     (unsigned)photos.count, (double)out->density,
                     (unsigned)(out->lod0.triangle_count / 2U),
                     (unsigned)(out->lod1.triangle_count / 2U));
    } else {
        (void)fprintf(stderr, "treebake : cartes du feuillage impossibles (mémoire, atlas trop "
                              "petit ou écriture)\n");
    }
    tb_atlas_free(&atlas);
    photos_free(&photos);
    free(bouquet_cluster);
    tb_foliage_free(&f);
    return ok;
}

void tb_cards_free(TbCards *c)
{
    tb_mesh_free(&c->lod0);
    tb_mesh_free(&c->lod1);
    memset(c, 0, sizeof *c);
}
