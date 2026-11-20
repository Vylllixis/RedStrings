#include "engine/world_cell.h"

#include <math.h>
#include <string.h>

EngWorldGen eng_world_gen_default(int32_t seed)
{
    EngWorldGen g;
    memset(&g, 0, sizeof g);
    g.seed = seed;
    g.altitude = 60.0F;
    g.noise = (EngNoiseParams){.seed = seed,
                               .frequency = 1.0F / 700.0F,
                               .octaves = 5,
                               .lacunarity = 2.0F,
                               .gain = 0.5F,
                               .fractal = ENG_NOISE_FBM};
    g.rocks_per_cell = 12;
    g.trees_per_cell = 24;
    g.camp_every = 4;
    return g;
}

EngCellCoord eng_cell_of(double x, double z)
{
    const EngCellCoord c = {(int32_t)floor(x / ENG_CELL_SIZE), (int32_t)floor(z / ENG_CELL_SIZE)};
    return c;
}

bool eng_cell_equal(EngCellCoord a, EngCellCoord b)
{
    return a.x == b.x && a.z == b.z;
}

/* hachage entier sans état (splitmix) : placement déterministe par cellule et par indice */
static uint64_t mix(uint64_t x)
{
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

static float unit(uint64_t h) /* [0 ; 1[ */
{
    return (float)(h >> 40) / (float)(1ULL << 24);
}

static uint64_t cell_key(const EngWorldGen *g, EngCellCoord c, uint32_t i)
{
    return mix(((uint64_t)(uint32_t)c.x << 32) ^ (uint64_t)(uint32_t)c.z ^
               mix((uint64_t)(uint32_t)g->seed + ((uint64_t)i << 20)));
}

float eng_world_gen_height(const EngWorldGen *gen, double x, double z)
{
    if (gen == NULL) {
        return 0.0F;
    }
    float h = 0.0F;
    eng_noise_row_2d(&gen->noise, (float)x, ENG_CELL_SPACING, (float)z, 1U, &h);
    h *= gen->altitude;
    if (gen->biomes.enabled) {
        float w[ENG_BIOME_COUNT];
        eng_biome_weights(&gen->biomes, x, z, w);
        h *= eng_biome_altitude_scale(w);
    }
    return h;
}

bool eng_cell_generate(const EngWorldGen *gen, EngCellCoord c, EngCellData *out)
{
    if (gen == NULL || out == NULL || !(gen->altitude >= 0.0F) || gen->noise.octaves == 0U ||
        gen->rocks_per_cell + gen->trees_per_cell + 1U > ENG_CELL_MAX_OBJECTS) {
        return false;
    }
    memset(out, 0, sizeof *out);
    out->coord = c;
    /* origine de la cellule en double, puis lignes de bruit en float : l'erreur reste < 1 mm à
     * 100 km (le bruit est évalué à la coordonnée monde, identique pour les deux voisines). */
    const double ox = (double)c.x * ENG_CELL_SIZE;
    const double oz = (double)c.z * ENG_CELL_SIZE;
    float row[ENG_CELL_RES];
    out->height_min = INFINITY;
    out->height_max = -INFINITY;
    for (uint32_t z = 0; z < ENG_CELL_RES; z++) {
        eng_noise_row_2d(&gen->noise, (float)ox, ENG_CELL_SPACING,
                         (float)(oz + (double)z * ENG_CELL_SPACING), ENG_CELL_RES, row);
        for (uint32_t x = 0; x < ENG_CELL_RES; x++) {
            float h = row[x] * gen->altitude;
            if (gen->biomes.enabled) { /* grande carte : relief à l'échelle du biome */
                float w[ENG_BIOME_COUNT];
                eng_biome_weights(&gen->biomes, ox + ((double)x * ENG_CELL_SPACING),
                                  oz + ((double)z * ENG_CELL_SPACING), w);
                h *= eng_biome_altitude_scale(w);
            }
            out->heights[(size_t)z * ENG_CELL_RES + x] = h;
            out->height_min = h < out->height_min ? h : out->height_min;
            out->height_max = h > out->height_max ? h : out->height_max;
        }
    }
    uint32_t n = 0;
    const uint32_t total = gen->rocks_per_cell + gen->trees_per_cell;
    for (uint32_t i = 0; i < total; i++) {
        const uint64_t h = cell_key(gen, c, i);
        EngCellObject *o = &out->objects[n++];
        o->kind = i < gen->rocks_per_cell ? (uint8_t)ENG_CELL_ROCK : (uint8_t)ENG_CELL_TREE;
        o->pos[0] = 4.0F + unit(h) * 248.0F;
        o->pos[1] = 4.0F + unit(mix(h)) * 248.0F;
        o->yaw = unit(mix(h ^ 0x51ULL)) * 6.2831853F;
        o->scale = o->kind == ENG_CELL_ROCK ? 0.6F + unit(mix(h ^ 0x77ULL)) * 1.4F : 0.4F;
        o->y = eng_cell_height(out, o->pos[0], o->pos[1]);
        if (gen->biomes.enabled && o->kind == ENG_CELL_TREE) {
            /* grande carte : des arbres en plaine seulement (ni neige, ni désert, ni volcan) */
            float w[ENG_BIOME_COUNT];
            eng_biome_weights(&gen->biomes, ox + (double)o->pos[0], oz + (double)o->pos[1], w);
            if (w[ENG_BIOME_PLAIN] < 0.5F) {
                n--;
            }
        }
    }
    if (gen->camp_every > 0U && (cell_key(gen, c, 999U) % gen->camp_every) == 0U) {
        EngCellObject *o = &out->objects[n++];
        o->kind = ENG_CELL_CAMP;
        o->pos[0] = 128.0F;
        o->pos[1] = 128.0F;
        o->y = eng_cell_height(out, 128.0F, 128.0F);
        o->scale = 12.0F; /* rayon du camp */
    }
    out->object_count = n;
    return true;
}

float eng_cell_height(const EngCellData *cell, float lx, float lz)
{
    if (cell == NULL || !isfinite(lx) || !isfinite(lz)) {
        return 0.0F;
    }
    const float maxc = (float)(ENG_CELL_RES - 1U);
    float fx = lx / ENG_CELL_SPACING;
    float fz = lz / ENG_CELL_SPACING;
    fx = fx < 0.0F ? 0.0F : (fx > maxc ? maxc : fx);
    fz = fz < 0.0F ? 0.0F : (fz > maxc ? maxc : fz);
    uint32_t x0 = (uint32_t)fx;
    uint32_t z0 = (uint32_t)fz;
    x0 = x0 >= ENG_CELL_RES - 1U ? ENG_CELL_RES - 2U : x0;
    z0 = z0 >= ENG_CELL_RES - 1U ? ENG_CELL_RES - 2U : z0;
    const float tx = fx - (float)x0;
    const float tz = fz - (float)z0;
    const float *hgt = cell->heights;
    const float a = hgt[(size_t)z0 * ENG_CELL_RES + x0];
    const float b = hgt[(size_t)z0 * ENG_CELL_RES + x0 + 1U];
    const float c = hgt[(size_t)(z0 + 1U) * ENG_CELL_RES + x0];
    const float d = hgt[(size_t)(z0 + 1U) * ENG_CELL_RES + x0 + 1U];
    return (a * (1.0F - tx) + b * tx) * (1.0F - tz) + (c * (1.0F - tx) + d * tx) * tz;
}

uint64_t eng_cell_hash(const EngCellData *cell)
{
    uint64_t h = 1469598103934665603ULL;
    if (cell == NULL) {
        return h;
    }
    const uint8_t *p = (const uint8_t *)cell->heights;
    for (size_t i = 0; i < sizeof cell->heights; i++) {
        h = (h ^ p[i]) * 1099511628211ULL;
    }
    for (uint32_t i = 0; i < cell->object_count; i++) {
        const EngCellObject *o = &cell->objects[i];
        const float v[5] = {o->pos[0], o->pos[1], o->y, o->yaw, o->scale};
        const uint8_t *q = (const uint8_t *)v;
        for (size_t k = 0; k < sizeof v; k++) {
            h = (h ^ q[k]) * 1099511628211ULL;
        }
        h = (h ^ o->kind) * 1099511628211ULL;
    }
    return h;
}

/* ── Format RSCL v1 ── */

#define RSCL_HEADER 24U
#define RSCL_OBJECT 21U

size_t eng_cell_encoded_size(const EngCellData *cell)
{
    if (cell == NULL || cell->object_count > ENG_CELL_MAX_OBJECTS) {
        return 0;
    }
    return RSCL_HEADER + (size_t)ENG_CELL_RES * ENG_CELL_RES * 4U +
           (size_t)cell->object_count * RSCL_OBJECT;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void putf(uint8_t *p, float f)
{
    uint32_t u = 0;
    memcpy(&u, &f, sizeof u);
    put32(p, u);
}

static float getf(const uint8_t *p)
{
    const uint32_t u = get32(p);
    float f = 0.0F;
    memcpy(&f, &u, sizeof f);
    return f;
}

size_t eng_cell_encode(const EngCellData *cell, void *out, size_t capacity)
{
    const size_t need = eng_cell_encoded_size(cell);
    if (need == 0U || out == NULL || capacity < need) {
        return 0;
    }
    uint8_t *p = out;
    p[0] = 'R';
    p[1] = 'S';
    p[2] = 'C';
    p[3] = 'L';
    put32(p + 4, 1U);
    put32(p + 8, (uint32_t)cell->coord.x);
    put32(p + 12, (uint32_t)cell->coord.z);
    put32(p + 16, ENG_CELL_RES);
    put32(p + 20, cell->object_count);
    uint8_t *q = p + RSCL_HEADER;
    for (size_t i = 0; i < (size_t)ENG_CELL_RES * ENG_CELL_RES; i++, q += 4) {
        putf(q, cell->heights[i]);
    }
    for (uint32_t i = 0; i < cell->object_count; i++, q += RSCL_OBJECT) {
        const EngCellObject *o = &cell->objects[i];
        q[0] = o->kind;
        putf(q + 1, o->pos[0]);
        putf(q + 5, o->pos[1]);
        putf(q + 9, o->y);
        putf(q + 13, o->yaw);
        putf(q + 17, o->scale);
    }
    return need;
}

bool eng_cell_decode(const void *data, size_t size, EngCellData *out)
{
    const uint8_t *p = data;
    if (p == NULL || out == NULL || size < RSCL_HEADER || memcmp(p, "RSCL", 4) != 0 ||
        get32(p + 4) != 1U || get32(p + 16) != ENG_CELL_RES) {
        return false;
    }
    const uint32_t n = get32(p + 20);
    if (n > ENG_CELL_MAX_OBJECTS ||
        size != RSCL_HEADER + (size_t)ENG_CELL_RES * ENG_CELL_RES * 4U + (size_t)n * RSCL_OBJECT) {
        return false;
    }
    memset(out, 0, sizeof *out);
    out->coord.x = (int32_t)get32(p + 8);
    out->coord.z = (int32_t)get32(p + 12);
    const uint8_t *q = p + RSCL_HEADER;
    out->height_min = INFINITY;
    out->height_max = -INFINITY;
    for (size_t i = 0; i < (size_t)ENG_CELL_RES * ENG_CELL_RES; i++, q += 4) {
        const float h = getf(q);
        if (!isfinite(h)) {
            return false;
        }
        out->heights[i] = h;
        out->height_min = h < out->height_min ? h : out->height_min;
        out->height_max = h > out->height_max ? h : out->height_max;
    }
    for (uint32_t i = 0; i < n; i++, q += RSCL_OBJECT) {
        EngCellObject *o = &out->objects[i];
        if (q[0] >= (uint8_t)ENG_CELL_OBJECT_KINDS) {
            return false;
        }
        o->kind = q[0];
        o->pos[0] = getf(q + 1);
        o->pos[1] = getf(q + 5);
        o->y = getf(q + 9);
        o->yaw = getf(q + 13);
        o->scale = getf(q + 17);
    }
    out->object_count = n;
    return true;
}
