/* Arbre scanné -> arbre de jeu : le traceur qui « photographie » des triangles à plat sur une
 * carte (couleur, découpe, relief, occlusion). Voir engine/tree_bake.h. */
#include <math.h>
#include <string.h>

#include "engine/tree_bake.h"

#define SAMPLE_FLOATS 16U /* par échantillon : profondeur, couleur, normale, ARM, couvert */
#define SNOW_FROM 0.35F   /* normale.y à partir de laquelle la neige tient */
#define SNOW_FULL 0.8F

enum { S_DEPTH = 0, S_COLOR = 1, S_NORMAL = 4, S_ARM = 7, S_COVERED = 10 };

typedef struct Corner {
    float sx; /* position dans la grille d'échantillons */
    float sy;
    float depth; /* le long de axis_n : le plus grand est le plus proche de l'observateur */
    const EngBakeVertex *vertex;
} Corner;

static float dot3(const float *a, const float *b)
{
    return (a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]);
}

static float clamp01(float x)
{
    return x < 0.0F ? 0.0F : (x > 1.0F ? 1.0F : x);
}

static float smooth_between(float a, float b, float x)
{
    const float t = clamp01((x - a) / (b - a));
    return t * t * (3.0F - (2.0F * t));
}

/* lecture bilinéaire, image répétée ; `out` : trois composantes de 0 à 255 */
static void sample_image(const EngBakeImage *image, float u, float v, const float fallback[3],
                         float out[3])
{
    if (image->rgba == NULL || image->width == 0U || image->height == 0U) {
        memcpy(out, fallback, 3U * sizeof(float));
        return;
    }
    const float fw = (float)image->width;
    const float fh = (float)image->height;
    const float x = ((u - floorf(u)) * fw) - 0.5F;
    const float y = ((v - floorf(v)) * fh) - 0.5F;
    const float fx0 = floorf(x);
    const float fy0 = floorf(y);
    const float tx = x - fx0;
    const float ty = y - fy0;
    const int32_t w = (int32_t)image->width;
    const int32_t h = (int32_t)image->height;
    const int32_t x0 = (((int32_t)fx0 % w) + w) % w;
    const int32_t y0 = (((int32_t)fy0 % h) + h) % h;
    const int32_t x1 = (x0 + 1) % w;
    const int32_t y1 = (y0 + 1) % h;
    const uint8_t *p00 = &image->rgba[(((size_t)y0 * image->width) + (size_t)x0) * 4U];
    const uint8_t *p10 = &image->rgba[(((size_t)y0 * image->width) + (size_t)x1) * 4U];
    const uint8_t *p01 = &image->rgba[(((size_t)y1 * image->width) + (size_t)x0) * 4U];
    const uint8_t *p11 = &image->rgba[(((size_t)y1 * image->width) + (size_t)x1) * 4U];
    for (int c = 0; c < 3; c++) {
        const float top = (float)p00[c] + (((float)p10[c] - (float)p00[c]) * tx);
        const float bottom = (float)p01[c] + (((float)p11[c] - (float)p01[c]) * tx);
        out[c] = top + ((bottom - top) * ty);
    }
}

/* couleur, normale et ARM d'un point du triangle (poids barycentriques b0, b1, b2) */
static void shade(const EngBakeMesh *mesh, const EngBakeView *view, const Corner *c,
                  uint32_t material, float b0, float b1, float b2,
                  const EngBakeRasterOptions *options, float *sample)
{
    static const float white[3] = {255.0F, 255.0F, 255.0F};
    static const float plain_arm[3] = {255.0F, 255.0F, 0.0F}; /* occlusion 1, rugosité 1, métal 0 */
    const EngBakeVertex *v0 = c[0].vertex;
    const EngBakeVertex *v1 = c[1].vertex;
    const EngBakeVertex *v2 = c[2].vertex;
    const float u = (v0->uv[0] * b0) + (v1->uv[0] * b1) + (v2->uv[0] * b2);
    const float v = (v0->uv[1] * b0) + (v1->uv[1] * b1) + (v2->uv[1] * b2);
    const EngBakeMaterial none = {{NULL, 0U, 0U}, {NULL, 0U, 0U}};
    const EngBakeMaterial *m = material < mesh->material_count ? &mesh->materials[material] : &none;
    sample_image(&m->color, u, v, white, &sample[S_COLOR]);
    sample_image(&m->arm, u, v, plain_arm, &sample[S_ARM]);

    float n[3];
    for (int a = 0; a < 3; a++) {
        n[a] = (v0->normal[a] * b0) + (v1->normal[a] * b1) + (v2->normal[a] * b2);
    }
    const float length = sqrtf(dot3(n, n));
    if (length > 1e-12F) {
        for (int a = 0; a < 3; a++) {
            n[a] /= length;
        }
    } else {
        memcpy(n, view->axis_n, sizeof n);
    }
    if (dot3(n, view->axis_n) < 0.0F) { /* l'autre face du triangle : on la voit aussi */
        for (int a = 0; a < 3; a++) {
            n[a] = -n[a];
        }
    }
    const float snow =
        clamp01(options->snow) * smooth_between(SNOW_FROM, SNOW_FULL, n[1]); /* vers le ciel */
    if (snow > 0.0F) {
        static const float snow_color[3] = {236.0F, 241.0F, 248.0F};
        for (int a = 0; a < 3; a++) {
            sample[S_COLOR + a] += (snow_color[a] - sample[S_COLOR + a]) * snow;
        }
    }
    float sky = 1.0F; /* part du ciel vue par ce point du feuillage */
    if (mesh->occlusion != NULL) {
        sky = (mesh->occlusion[v0 - mesh->vertices] * b0) +
              (mesh->occlusion[v1 - mesh->vertices] * b1) +
              (mesh->occlusion[v2 - mesh->vertices] * b2);
    }
    sample[S_ARM] *= options->occlusion * clamp01(sky);
    sample[S_NORMAL] = dot3(n, view->axis_u);
    sample[S_NORMAL + 1] = dot3(n, view->axis_v);
    sample[S_NORMAL + 2] = dot3(n, view->axis_n);
    sample[S_COVERED] = 1.0F;
}

/* trace un triangle dans la grille d'échantillons de gw x gh */
static void draw_triangle(const EngBakeMesh *mesh, const EngBakeView *view, const Corner *c,
                          uint32_t material, const EngBakeRasterOptions *options, float *grid,
                          uint32_t gw, uint32_t gh)
{
    const float area =
        ((c[1].sx - c[0].sx) * (c[2].sy - c[0].sy)) - ((c[2].sx - c[0].sx) * (c[1].sy - c[0].sy));
    if (!isfinite(area) || fabsf(area) < 1e-12F) {
        return; /* triangle vu par la tranche, ou dégénéré */
    }
    float min_x = fminf(c[0].sx, fminf(c[1].sx, c[2].sx));
    float max_x = fmaxf(c[0].sx, fmaxf(c[1].sx, c[2].sx));
    float min_y = fminf(c[0].sy, fminf(c[1].sy, c[2].sy));
    float max_y = fmaxf(c[0].sy, fmaxf(c[1].sy, c[2].sy));
    if (max_x < 0.0F || max_y < 0.0F || min_x >= (float)gw || min_y >= (float)gh) {
        return; /* hors du rectangle */
    }
    min_x = fmaxf(min_x, 0.0F);
    min_y = fmaxf(min_y, 0.0F);
    max_x = fminf(max_x, (float)gw - 1.0F);
    max_y = fminf(max_y, (float)gh - 1.0F);
    const uint32_t x0 = (uint32_t)min_x;
    const uint32_t y0 = (uint32_t)min_y;
    const uint32_t x1 = (uint32_t)max_x;
    const uint32_t y1 = (uint32_t)max_y;
    const float inverse = 1.0F / area;
    for (uint32_t y = y0; y <= y1; y++) {
        const float py = (float)y + 0.5F;
        for (uint32_t x = x0; x <= x1; x++) {
            const float px = (float)x + 0.5F;
            /* poids barycentriques : tous positifs à l'intérieur, quel que soit le sens du
             * triangle (on divise par l'aire signée) */
            const float b0 =
                (((c[1].sx - px) * (c[2].sy - py)) - ((c[2].sx - px) * (c[1].sy - py))) * inverse;
            const float b1 =
                (((c[2].sx - px) * (c[0].sy - py)) - ((c[0].sx - px) * (c[2].sy - py))) * inverse;
            const float b2 = 1.0F - b0 - b1;
            if (b0 < 0.0F || b1 < 0.0F || b2 < 0.0F) {
                continue;
            }
            const float depth = (c[0].depth * b0) + (c[1].depth * b1) + (c[2].depth * b2);
            float *sample = &grid[(((size_t)y * gw) + x) * SAMPLE_FLOATS];
            if (sample[S_COVERED] > 0.5F && depth <= sample[S_DEPTH]) {
                continue; /* caché par une surface plus proche */
            }
            sample[S_DEPTH] = depth;
            shade(mesh, view, c, material, b0, b1, b2, options, sample);
        }
    }
}

static uint8_t to_byte(float value)
{
    const float rounded = value + 0.5F;
    return rounded <= 0.0F ? 0U : (rounded >= 255.0F ? 255U : (uint8_t)rounded);
}

/* moyenne des échantillons couverts d'un pixel, écrite dans les trois images */
static float resolve_pixel(const float *grid, uint32_t gw, uint32_t s, uint32_t px, uint32_t py,
                           uint8_t *color, uint8_t *normal, uint8_t *arm)
{
    float sum[9] = {0.0F};
    uint32_t covered = 0;
    for (uint32_t j = 0; j < s; j++) {
        for (uint32_t i = 0; i < s; i++) {
            const float *sample =
                &grid[((((size_t)py * s) + j) * gw + ((size_t)px * s) + i) * SAMPLE_FLOATS];
            if (sample[S_COVERED] > 0.5F) {
                for (int c = 0; c < 9; c++) {
                    sum[c] += sample[S_COLOR + c];
                }
                covered++;
            }
        }
    }
    const float coverage = (float)covered / (float)(s * s);
    const uint8_t alpha = to_byte(coverage * 255.0F);
    if (covered == 0U) { /* vide : noir, normale de face, occlusion 1, rugosité 1 */
        memcpy(color, (const uint8_t[4]){0U, 0U, 0U, 0U}, 4U);
        memcpy(normal, (const uint8_t[4]){128U, 128U, 255U, 0U}, 4U);
        memcpy(arm, (const uint8_t[4]){255U, 255U, 0U, 0U}, 4U);
        return 0.0F;
    }
    const float inverse = 1.0F / (float)covered;
    float n[3] = {sum[3], sum[4], sum[5]};
    const float length = sqrtf(dot3(n, n));
    if (length > 1e-12F) {
        for (int a = 0; a < 3; a++) {
            n[a] /= length;
        }
    } else {
        n[0] = 0.0F;
        n[1] = 0.0F;
        n[2] = 1.0F;
    }
    for (int c = 0; c < 3; c++) {
        color[c] = to_byte(sum[c] * inverse);
        normal[c] = to_byte(((n[c] * 0.5F) + 0.5F) * 255.0F);
        arm[c] = to_byte(sum[6 + c] * inverse);
    }
    color[3] = alpha;
    normal[3] = alpha;
    arm[3] = alpha;
    return coverage;
}

static bool view_valid(const EngBakeView *view)
{
    for (int a = 0; a < 3; a++) {
        if (!isfinite(view->origin[a]) || !isfinite(view->axis_u[a]) ||
            !isfinite(view->axis_v[a]) || !isfinite(view->axis_n[a])) {
            return false;
        }
    }
    return isfinite(view->half_u) && isfinite(view->half_v) && view->half_u > 0.0F &&
           view->half_v > 0.0F;
}

bool eng_bake_raster(const EngBakeMesh *mesh, const uint32_t *triangles, uint32_t count,
                     const EngBakeView *view, const EngBakeTarget *target, uint32_t x, uint32_t y,
                     uint32_t w, uint32_t h, const EngBakeRasterOptions *options, float *scratch,
                     float *covered)
{
    if (mesh == NULL || view == NULL || target == NULL || scratch == NULL ||
        mesh->vertices == NULL || mesh->indices == NULL || target->color == NULL ||
        target->normal == NULL || target->arm == NULL || w == 0U || h == 0U ||
        (uint64_t)x + w > target->width || (uint64_t)y + h > target->height || !view_valid(view)) {
        return false;
    }
    EngBakeRasterOptions opt = {.supersample = 3U, .occlusion = 1.0F, .snow = 0.0F};
    if (options != NULL) {
        opt.supersample = options->supersample == 0U ? 3U : options->supersample;
        opt.occlusion = options->occlusion > 0.0F ? options->occlusion : 1.0F;
        opt.snow = isfinite(options->snow) ? options->snow : 0.0F;
    }
    const uint32_t s = opt.supersample > 4U ? 4U : opt.supersample;
    const uint32_t gw = w * s;
    const uint32_t gh = h * s;
    memset(scratch, 0, (size_t)gw * gh * SAMPLE_FLOATS * sizeof(float));
    const uint32_t total = triangles != NULL ? count : mesh->triangle_count;
    for (uint32_t k = 0; k < total; k++) {
        const uint32_t t = triangles != NULL ? triangles[k] : k;
        if (t >= mesh->triangle_count) {
            continue;
        }
        Corner corner[3];
        bool valid = true;
        for (int c = 0; c < 3; c++) {
            const uint32_t index = mesh->indices[((size_t)t * 3U) + (size_t)c];
            if (index >= mesh->vertex_count) {
                valid = false;
                break;
            }
            const EngBakeVertex *vertex = &mesh->vertices[index];
            const float rel[3] = {vertex->position[0] - view->origin[0],
                                  vertex->position[1] - view->origin[1],
                                  vertex->position[2] - view->origin[2]};
            corner[c].sx = (((dot3(rel, view->axis_u) / view->half_u) * 0.5F) + 0.5F) * (float)gw;
            corner[c].sy = (0.5F - ((dot3(rel, view->axis_v) / view->half_v) * 0.5F)) * (float)gh;
            corner[c].depth = dot3(rel, view->axis_n);
            corner[c].vertex = vertex;
        }
        if (valid) {
            const uint32_t material =
                mesh->triangle_material != NULL ? mesh->triangle_material[t] : 0U;
            draw_triangle(mesh, view, corner, material, &opt, scratch, gw, gh);
        }
    }
    double sum = 0.0;
    for (uint32_t py = 0; py < h; py++) {
        for (uint32_t px = 0; px < w; px++) {
            const size_t at = ((((size_t)y + py) * target->width) + x + px) * 4U;
            sum += (double)resolve_pixel(scratch, gw, s, px, py, &target->color[at],
                                         &target->normal[at], &target->arm[at]);
        }
    }
    if (covered != NULL) {
        *covered = (float)(sum / ((double)w * (double)h));
    }
    return true;
}
