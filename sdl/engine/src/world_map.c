/* Carte du monde : lecture de la trame (relief, régions) et aperçu vu de dessus. */
#include <math.h>
#include <string.h>

#include "engine/world_map.h"

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* position monde -> coordonnées de trame (bornées au bord) et fractions */
static bool locate(const EngWorldMap *m, double x, double z, uint32_t *i, uint32_t *j, float *fx,
                   float *fz)
{
    if (m == NULL || m->heights == NULL || m->size < 2U || !isfinite(x) || !isfinite(z)) {
        return false;
    }
    const double half = 0.5 * (double)m->side;
    const double last = (double)(m->size - 1U);
    double u = (x + half) / (double)m->spacing;
    double v = (z + half) / (double)m->spacing;
    u = u < 0.0 ? 0.0 : (u > last ? last : u);
    v = v < 0.0 ? 0.0 : (v > last ? last : v);
    const double iu = floor(u) >= last ? last - 1.0 : floor(u);
    const double iv = floor(v) >= last ? last - 1.0 : floor(v);
    *i = (uint32_t)iu;
    *j = (uint32_t)iv;
    *fx = (float)(u - iu);
    *fz = (float)(v - iv);
    return true;
}

float eng_worldmap_height(const EngWorldMap *map, double x, double z)
{
    uint32_t i = 0;
    uint32_t j = 0;
    float fx = 0.0F;
    float fz = 0.0F;
    if (!locate(map, x, z, &i, &j, &fx, &fz)) {
        return 0.0F;
    }
    const float *h = &map->heights[((size_t)j * map->size) + i];
    const float top = h[0] + ((h[1] - h[0]) * fx);
    const float bottom = h[map->size] + ((h[map->size + 1U] - h[map->size]) * fx);
    return top + ((bottom - top) * fz);
}

void eng_worldmap_weights(const EngWorldMap *map, double x, double z, float out[ENG_BIOME_COUNT],
                          float *path)
{
    out[ENG_BIOME_PLAIN] = 1.0F;
    out[ENG_BIOME_SNOW] = 0.0F;
    out[ENG_BIOME_DESERT] = 0.0F;
    out[ENG_BIOME_VOLCANO] = 0.0F;
    if (path != NULL) {
        *path = 0.0F;
    }
    uint32_t i = 0;
    uint32_t j = 0;
    float fx = 0.0F;
    float fz = 0.0F;
    if (out == NULL || !locate(map, x, z, &i, &j, &fx, &fz) || map->weights == NULL) {
        return;
    }
    const size_t row = (size_t)map->size * 4U;
    const uint8_t *p = &map->weights[(((size_t)j * map->size) + i) * 4U];
    float ch[4];
    for (int c = 0; c < 4; c++) {
        const float top = (float)p[c] + (((float)p[4 + c] - (float)p[c]) * fx);
        const float bottom = (float)p[row + (size_t)c] +
                             (((float)p[row + 4U + (size_t)c] - (float)p[row + (size_t)c]) * fx);
        ch[c] = (top + ((bottom - top) * fz)) / 255.0F;
    }
    const float others = ch[0] + ch[1] + ch[2];
    const float scale = others > 1.0F ? 1.0F / others : 1.0F;
    out[ENG_BIOME_SNOW] = ch[0] * scale;
    out[ENG_BIOME_DESERT] = ch[1] * scale;
    out[ENG_BIOME_VOLCANO] = ch[2] * scale;
    out[ENG_BIOME_PLAIN] = clampf(1.0F - (others * scale), 0.0F, 1.0F);
    if (path != NULL) {
        *path = clampf(ch[3], 0.0F, 1.0F);
    }
}

float eng_worldmap_lava(const EngWorldMap *map, double x, double z)
{
    uint32_t i = 0;
    uint32_t j = 0;
    float fx = 0.0F;
    float fz = 0.0F;
    if (!locate(map, x, z, &i, &j, &fx, &fz) || map->lava == NULL) {
        return 0.0F;
    }
    const uint8_t *p = &map->lava[((size_t)j * map->size) + i];
    const float top = (float)p[0] + (((float)p[1] - (float)p[0]) * fx);
    const float bottom =
        (float)p[map->size] + (((float)p[map->size + 1U] - (float)p[map->size]) * fx);
    return (top + ((bottom - top) * fz)) / 255.0F;
}

/* ── Aperçu ── */

static void mix(float c[3], const float to[3], float k)
{
    for (int a = 0; a < 3; a++) {
        c[a] += (to[a] - c[a]) * k;
    }
}

static void pixel_color(const EngWorldMap *m, double x, double z, double step, float c[3])
{
    static const float biome_color[ENG_BIOME_COUNT][3] = {
        {0.36F, 0.52F, 0.22F}, {0.92F, 0.94F, 0.97F}, {0.86F, 0.73F, 0.49F}, {0.21F, 0.18F, 0.17F}};
    static const float rock[3] = {0.5F, 0.47F, 0.43F};
    static const float path_color[3] = {0.7F, 0.58F, 0.4F};
    static const float lava_color[3] = {1.0F, 0.42F, 0.07F};
    static const float water_color[3] = {0.16F, 0.36F, 0.58F};
    const float h = eng_worldmap_height(m, x, z);
    const float dx = (eng_worldmap_height(m, x + step, z) - eng_worldmap_height(m, x - step, z)) /
                     (float)(2.0 * step);
    const float dz = (eng_worldmap_height(m, x, z + step) - eng_worldmap_height(m, x, z - step)) /
                     (float)(2.0 * step);
    float w[ENG_BIOME_COUNT];
    float path = 0.0F;
    eng_worldmap_weights(m, x, z, w, &path);
    c[0] = c[1] = c[2] = 0.0F;
    for (int k = 0; k < ENG_BIOME_COUNT; k++) {
        for (int a = 0; a < 3; a++) {
            c[a] += w[k] * biome_color[k][a];
        }
    }
    const float slope = sqrtf((dx * dx) + (dz * dz));
    mix(c, rock, clampf((slope - 0.55F) / 0.6F, 0.0F, 1.0F) * (1.0F - (0.6F * w[ENG_BIOME_SNOW])));
    mix(c, path_color, 0.85F * path);
    const float lava = eng_worldmap_lava(m, x, z);
    /* relief ombré : lumière du nord-ouest */
    const float n[3] = {-dx, 1.0F, -dz};
    const float len = sqrtf((n[0] * n[0]) + 1.0F + (n[2] * n[2]));
    const float lit = ((n[0] * -0.5F) + (n[1] * 0.7F) + (n[2] * -0.5F)) / (len * 0.9899F);
    const float shade = 0.45F + (0.7F * clampf(lit, 0.0F, 1.0F));
    for (int a = 0; a < 3; a++) {
        c[a] *= shade;
    }
    mix(c, lava_color, lava); /* la lave brille : pas d'ombre */
    if (h < m->water) {       /* eau : plus sombre en profondeur */
        const float depth = clampf((m->water - h) / 6.0F, 0.0F, 1.0F);
        float water[3] = {water_color[0] + 0.15F, water_color[1] + 0.15F, water_color[2] + 0.12F};
        mix(water, water_color, depth);
        memcpy(c, water, sizeof water);
    }
}

static void disc(uint8_t *rgb, uint32_t size, float cx, float cy, float r, const uint8_t col[3])
{
    for (int y = (int)(cy - r - 2.0F); y <= (int)(cy + r + 2.0F); y++) {
        for (int x = (int)(cx - r - 2.0F); x <= (int)(cx + r + 2.0F); x++) {
            if (x < 0 || y < 0 || x >= (int)size || y >= (int)size) {
                continue;
            }
            const float d = hypotf((float)x - cx, (float)y - cy);
            uint8_t *p = &rgb[(((size_t)y * size) + (size_t)x) * 3U];
            if (d <= r) {
                memcpy(p, col, 3);
            } else if (d <= r + 1.5F) { /* liseré noir */
                p[0] = p[1] = p[2] = 0U;
            }
        }
    }
}

bool eng_worldmap_render(const EngWorldMap *map, uint32_t size, uint8_t *rgb)
{
    if (map == NULL || map->heights == NULL || rgb == NULL || size < 16U) {
        return false;
    }
    const double half = 0.5 * (double)map->side;
    const double px = (double)map->side / (double)size;
    const double step = px > (double)map->spacing ? px : (double)map->spacing;
    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            float c[3];
            pixel_color(map, -half + (((double)x + 0.5) * px), -half + (((double)y + 0.5) * px),
                        step, c);
            uint8_t *p = &rgb[(((size_t)y * size) + x) * 3U];
            for (int a = 0; a < 3; a++) {
                p[a] = (uint8_t)lroundf(clampf(c[a], 0.0F, 1.0F) * 255.0F);
            }
        }
    }
    static EngMapTree trees[16384]; /* aperçu : un point sombre par arbre, par bandes de 512 m */
    static const uint8_t tree_color[ENG_TREE_KINDS][3] = {
        {28, 70, 22}, {20, 60, 45}, {95, 110, 40}};
    const int blocks = (int)ceil(map->side / 512.0);
    for (int jz = 0; jz < blocks; jz++) {
        for (int jx = 0; jx < blocks; jx++) {
            const double bx = -half + (512.0 * jx);
            const double bz = -half + (512.0 * jz);
            const uint32_t n = eng_worldmap_trees(map, bx, bz, 512.0, trees, 16384U);
            for (uint32_t t = 0; t < n; t++) {
                const int px_x = (int)(((double)trees[t].x + half) / px);
                const int px_y = (int)(((double)trees[t].z + half) / px);
                if (px_x >= 0 && px_y >= 0 && px_x < (int)size && px_y < (int)size) {
                    uint8_t *p = &rgb[(((size_t)px_y * size) + (size_t)px_x) * 3U];
                    p[0] = tree_color[trees[t].kind][0];
                    p[1] = tree_color[trees[t].kind][1];
                    p[2] = tree_color[trees[t].kind][2];
                }
            }
        }
    }
    static const uint8_t kind_color[ENG_MAPZONE_KINDS][3] = {
        {60, 200, 70}, {245, 150, 30}, {220, 30, 30}};
    const float r = fmaxf(3.0F, (float)size / 220.0F);
    for (uint32_t k = 0; k < ENG_MAPZONE_COUNT; k++) {
        const EngMapZoneSpot *s = &map->zones[k];
        if (s->used) {
            disc(rgb, size, (float)((s->x + half) / px), (float)((s->z + half) / px),
                 s->kind == (uint8_t)ENG_MAPZONE_BOSS ? 1.4F * r : r, kind_color[s->kind]);
        }
    }
    static const uint8_t white[3] = {255, 255, 255};
    disc(rgb, size, (float)((map->spawn[0] + half) / px), (float)((map->spawn[1] + half) / px),
         1.6F * r, white);
    return true;
}
