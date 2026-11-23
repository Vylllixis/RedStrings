/* treebake : l'atlas, l'image commune où sont rangées les photographies (cartes de feuillage, vues
 * lointaines). Voir treebake_interne.h. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "treebake_interne.h"

#define GAP 4U      /* pixels entre deux photographies */
#define TILE_MIN 6U /* côté minimal d'une photographie */
#define SUPERSAMPLE 4U
#define DILATE_PASSES 12U
#define SEARCH_STEPS 28
#define JPEG_QUALITY 92 /* au-dessus de 90 : pas de sous-échantillonnage des couleurs */
#define SKY_REACH 0.7F  /* part du chemin vers le coin d'une carte où l'on mesure le ciel */

bool tb_atlas_init(TbAtlas *a, uint32_t width, uint32_t height)
{
    memset(a, 0, sizeof *a);
    const size_t bytes = (size_t)width * height * 4U;
    a->target.color = calloc(bytes, 1);
    a->target.normal = calloc(bytes, 1);
    a->target.arm = calloc(bytes, 1);
    a->target.width = width;
    a->target.height = height;
    if (a->target.color == NULL || a->target.normal == NULL || a->target.arm == NULL) {
        tb_atlas_free(a);
        return false;
    }
    return true;
}

void tb_atlas_free(TbAtlas *a)
{
    free(a->target.color);
    free(a->target.normal);
    free(a->target.arm);
    free(a->scratch);
    memset(a, 0, sizeof *a);
}

static uint32_t tile_side(float half, float density, uint32_t limit)
{
    const float side = ceilf(2.0F * half * density);
    const uint32_t pixels = side < (float)TILE_MIN ? TILE_MIN : (uint32_t)side;
    return pixels > limit ? limit : pixels;
}

static bool try_layout(const EngBakeView *views, uint32_t count, uint32_t width, uint32_t height,
                       float density, EngBakeRect *rects, uint32_t *order)
{
    for (uint32_t i = 0; i < count; i++) {
        rects[i].w = tile_side(views[i].half_u, density, width - (2U * GAP));
        rects[i].h = tile_side(views[i].half_v, density, height - (2U * GAP));
    }
    return eng_bake_pack(rects, count, width, height, GAP, order);
}

float tb_atlas_layout(const EngBakeView *views, uint32_t count, uint32_t width, uint32_t height,
                      EngBakeRect *rects)
{
    uint32_t *order = malloc((size_t)count * sizeof *order);
    if (order == NULL || count == 0U || width <= 4U * GAP || height <= 4U * GAP) {
        free(order);
        return 0.0F;
    }
    /* la plus grande densité (pixels par mètre) pour laquelle tout tient : recherche par moitiés */
    float low = 0.0F;
    float high = 8192.0F;
    for (int step = 0; step < SEARCH_STEPS; step++) {
        const float middle = 0.5F * (low + high);
        if (try_layout(views, count, width, height, middle, rects, order)) {
            low = middle;
        } else {
            high = middle;
        }
    }
    const bool fits = low > 0.0F && try_layout(views, count, width, height, low, rects, order);
    free(order);
    return fits ? low : 0.0F;
}

bool tb_atlas_shoot(TbAtlas *a, const EngBakeMesh *mesh, const uint32_t *triangles, uint32_t count,
                    const EngBakeView *view, const EngBakeRect *rect, float snow, float *covered)
{
    const size_t needed =
        (size_t)rect->w * SUPERSAMPLE * rect->h * SUPERSAMPLE * 16U; /* voir eng_bake_raster */
    if (needed > a->scratch_floats) {
        free(a->scratch);
        a->scratch = malloc(needed * sizeof(float));
        a->scratch_floats = a->scratch != NULL ? needed : 0U;
        if (a->scratch == NULL) {
            return false;
        }
    }
    const EngBakeRasterOptions options = {
        .supersample = SUPERSAMPLE, .occlusion = 1.0F, .snow = snow};
    return eng_bake_raster(mesh, triangles, count, view, &a->target, rect->x, rect->y, rect->w,
                           rect->h, &options, a->scratch, covered);
}

/* Les pixels vides reçoivent la couleur moyenne du feuillage (au lieu du noir) : les réductions de
 * l'image (mips) ne s'assombrissent pas au bord des feuilles. */
static void fill_empty(TbAtlas *a)
{
    const size_t pixels = (size_t)a->target.width * a->target.height;
    double sum[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double weight = 0.0;
    for (size_t i = 0; i < pixels; i++) {
        const double alpha = (double)a->target.color[(i * 4U) + 3U];
        for (int c = 0; c < 3; c++) {
            sum[c] += alpha * (double)a->target.color[(i * 4U) + (size_t)c];
            sum[3 + c] += alpha * (double)a->target.arm[(i * 4U) + (size_t)c];
        }
        weight += alpha;
    }
    if (weight <= 0.0) {
        return;
    }
    for (size_t i = 0; i < pixels; i++) {
        if (a->target.color[(i * 4U) + 3U] != 0U) {
            continue;
        }
        for (int c = 0; c < 3; c++) {
            a->target.color[(i * 4U) + (size_t)c] = (uint8_t)((sum[c] / weight) + 0.5);
            a->target.arm[(i * 4U) + (size_t)c] = (uint8_t)((sum[3 + c] / weight) + 0.5);
        }
        memcpy(&a->target.normal[i * 4U], (const uint8_t[4]){128U, 128U, 255U, 0U}, 4U);
    }
}

bool tb_atlas_finish(TbAtlas *a, const EngBakeRect *rects, uint32_t count)
{
    const size_t pixels = (size_t)a->target.width * a->target.height;
    if (pixels == 0U) {
        return false;
    }
    fill_empty(a);
    uint8_t *work = malloc(pixels);
    if (work == NULL) {
        return false;
    }
    const uint32_t margin = GAP / 2U; /* le prolongement déborde dans l'espace entre deux images */
    for (uint32_t i = 0; i < count; i++) {
        const EngBakeRect *r = &rects[i];
        if (r->w == 0U || r->h == 0U) {
            continue;
        }
        const uint32_t x = r->x >= margin ? r->x - margin : 0U;
        const uint32_t y = r->y >= margin ? r->y - margin : 0U;
        const uint32_t right =
            r->x + r->w + margin < a->target.width ? r->x + r->w + margin : a->target.width;
        const uint32_t bottom =
            r->y + r->h + margin < a->target.height ? r->y + r->h + margin : a->target.height;
        eng_bake_dilate(&a->target, x, y, right - x, bottom - y, DILATE_PASSES, work);
    }
    free(work);
    return true;
}

bool tb_atlas_write(const TbAtlas *a, const TbOptions *o, const char *suffix, char *color_uri,
                    char *normal_uri, char *arm_uri)
{
    char path[TB_PATH];
    (void)snprintf(color_uri, TB_NAME, "%s_%s.png", o->name, suffix);
    (void)snprintf(normal_uri, TB_NAME, "%s_%s_n.jpg", o->name, suffix);
    (void)snprintf(arm_uri, TB_NAME, "%s_%s_arm.jpg", o->name, suffix);
    (void)snprintf(path, sizeof path, "%s/%s", o->out_dir, color_uri);
    bool ok = tb_write_png(path, a->target.color, a->target.width, a->target.height);
    (void)snprintf(path, sizeof path, "%s/%s", o->out_dir, normal_uri);
    ok =
        ok && tb_write_jpg(path, a->target.normal, a->target.width, a->target.height, JPEG_QUALITY);
    (void)snprintf(path, sizeof path, "%s/%s", o->out_dir, arm_uri);
    ok = ok && tb_write_jpg(path, a->target.arm, a->target.width, a->target.height, JPEG_QUALITY);
    if (!ok) {
        (void)fprintf(stderr, "treebake : écriture des images « %s » impossible dans %s\n", suffix,
                      o->out_dir);
    }
    return ok;
}

void tb_mesh_add_quad(TbMeshOut *m, const EngBakeView *view, const EngBakeRect *rect,
                      uint32_t atlas_w, uint32_t atlas_h, TbFlip flip, const EngBakeGrid *grid)
{
    /* haut-gauche, bas-gauche, bas-droite, haut-droite : sens direct vu depuis +axis_n */
    static const float corner[4][2] = {{-1.0F, 1.0F}, {-1.0F, -1.0F}, {1.0F, -1.0F}, {1.0F, 1.0F}};
    /* une image retournée : la tangente suit les u croissants, la bitangente change de signe */
    const float along_u = flip == TB_FLIP_U ? -1.0F : 1.0F;
    const float handedness = flip == TB_FLIP_NONE ? 1.0F : -1.0F;
    const uint32_t base = m->vertex_count;
    for (uint32_t k = 0; k < 4U; k++) {
        EngBakeVertex *v = &m->vertices[base + k];
        float *tangent = &m->tangents[((size_t)base + k) * 4U];
        float probe[3]; /* où l'on mesure le ciel : vers le coin, mais dans le feuillage */
        for (int a = 0; a < 3; a++) {
            const float offset = (corner[k][0] * view->half_u * view->axis_u[a]) +
                                 (corner[k][1] * view->half_v * view->axis_v[a]);
            v->position[a] = view->origin[a] + offset;
            probe[a] = view->origin[a] + (SKY_REACH * offset);
            v->normal[a] = view->axis_n[a];
            tangent[a] = along_u * view->axis_u[a];
        }
        float sky = grid != NULL && grid->sky != NULL ? eng_bake_grid_sky(grid, probe) : 1.0F;
        sky = sky < TB_SKY_MIN ? TB_SKY_MIN : (sky > 1.0F ? 1.0F : sky);
        tangent[3] = handedness * sky;
        const bool right = (corner[k][0] > 0.0F) != (flip == TB_FLIP_U);
        const bool bottom = (corner[k][1] < 0.0F) != (flip == TB_FLIP_V);
        v->uv[0] = ((float)rect->x + (right ? (float)rect->w : 0.0F)) / (float)atlas_w;
        v->uv[1] = ((float)rect->y + (bottom ? (float)rect->h : 0.0F)) / (float)atlas_h;
    }
    static const uint32_t order[6] = {0U, 1U, 2U, 0U, 2U, 3U};
    for (uint32_t i = 0; i < 6U; i++) {
        m->indices[((size_t)m->triangle_count * 3U) + i] = base + order[i];
    }
    m->vertex_count += 4U;
    m->triangle_count += 2U;
}

bool tb_mesh_alloc(TbMeshOut *m, uint32_t quads)
{
    memset(m, 0, sizeof *m);
    const size_t n = quads > 0U ? quads : 1U;
    m->vertices = calloc(n * 4U, sizeof *m->vertices);
    m->tangents = calloc(n * 16U, sizeof(float));
    m->indices = calloc(n * 6U, sizeof *m->indices);
    if (m->vertices == NULL || m->tangents == NULL || m->indices == NULL) {
        tb_mesh_free(m);
        return false;
    }
    return true;
}

void tb_mesh_free(TbMeshOut *m)
{
    free(m->vertices);
    free(m->tangents);
    free(m->indices);
    memset(m, 0, sizeof *m);
}
