/* Carte du monde : trame du relief et des régions (une fois, au chargement). Ordre :
 *   1. régions (graines, frontières déformées), relief de base des biomes, formes en relief
 *      (collines, montagnes, crêtes, volcans, plateaux, dunes), bordure ;
 *   2. aires aplanies (vers leur hauteur, ou vers le sol à leur centre) ;
 *   3. chemins (pente linéaire entre leurs sommets, pris sur le relief de l'étape 2) ;
 *   4. lacs et rivières (creusés sous le niveau de l'eau, berges en pente douce) ;
 *   5. coulées de lave (sillon de 1,5 m), effacées sur les chemins.
 * Le seul bruit est un détail de surface à graine fixe : même description, même carte. */
#include <math.h>
#include <string.h>

#include "engine/noise.h"
#include "engine/world_map.h"

#define DETAIL_SEED 20261121
#define REGION_SHARPNESS 28.0F /* frontières : environ 10 % de la portée */
#define WARP_M 160.0F          /* déformation des frontières */
#define PATH_MARGIN 7.0F       /* raccord d'un chemin avec le relief voisin */
#define BANK_M 22.0F           /* berges des lacs et rivières */
#define PATH_MAX_SLOPE 0.3F    /* un chemin ne monte jamais plus de 30 % (17 degrés) */

/* relief de base par biome : hauteur, amplitude du détail (la montagne en crêtes) */
static const float BASE_HEIGHT[ENG_BIOME_COUNT] = {10.0F, 42.0F, 8.0F, 24.0F};
static const float DETAIL_AMP[ENG_BIOME_COUNT] = {4.0F, 26.0F, 1.5F, 7.0F};

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static float smooth(float e0, float e1, float v)
{
    const float t = clampf((v - e0) / (e1 - e0), 0.0F, 1.0F);
    return t * t * (3.0F - (2.0F * t));
}

size_t eng_worldmap_build_bytes(const EngWorldMap *map)
{
    if (map == NULL || !(map->spacing > 0.0F) || !(map->side > 0.0F)) {
        return 0U;
    }
    const size_t n = (size_t)lroundf(map->side / map->spacing) + 1U;
    return (n * n * (sizeof(float) + 5U)) + 64U;
}

/* distance du point (x, z) à la ligne de la forme `f` ; `seg`, `t` : segment et position */
static float polyline_distance(const EngWorldMap *m, const EngMapFeature *f, float x, float z,
                               uint32_t *seg, float *t)
{
    float best = INFINITY;
    for (uint32_t k = 0; k + 1U < f->point_count; k++) {
        const float *a = m->points[f->first_point + k];
        const float *b = m->points[f->first_point + k + 1U];
        const float dx = b[0] - a[0];
        const float dz = b[1] - a[1];
        const float len2 = (dx * dx) + (dz * dz);
        const float u =
            len2 > 1e-6F ? clampf((((x - a[0]) * dx) + ((z - a[1]) * dz)) / len2, 0, 1) : 0.0F;
        const float px = a[0] + (u * dx) - x;
        const float pz = a[1] + (u * dz) - z;
        const float d = sqrtf((px * px) + (pz * pz));
        if (d < best) {
            best = d;
            *seg = k;
            *t = u;
        }
    }
    return best;
}

/* boîte englobante de la forme, élargie de `margin` */
static void bounds(const EngWorldMap *m, const EngMapFeature *f, float margin, float out[4])
{
    if (f->point_count == 0U) {
        const float r = f->radius + margin;
        out[0] = f->x - r;
        out[1] = f->z - r;
        out[2] = f->x + r;
        out[3] = f->z + r;
        return;
    }
    out[0] = out[1] = INFINITY;
    out[2] = out[3] = -INFINITY;
    for (uint32_t k = 0; k < f->point_count; k++) {
        const float *p = m->points[f->first_point + k];
        out[0] = fminf(out[0], p[0]);
        out[1] = fminf(out[1], p[1]);
        out[2] = fmaxf(out[2], p[0]);
        out[3] = fmaxf(out[3], p[1]);
    }
    const float r = (0.5F * f->radius) + margin;
    out[0] -= r;
    out[1] -= r;
    out[2] += r;
    out[3] += r;
}

/* hauteur ajoutée par une forme en relief au point (x, z) ; `ridge` : bruit de crêtes [0, 1] */
static float relief(const EngWorldMap *m, const EngMapFeature *f, float x, float z, float ridge)
{
    const float r = hypotf(x - f->x, z - f->z);
    switch ((EngMapFeatureKind)f->kind) {
    case ENG_MAPF_HILL: {
        if (r >= f->radius) {
            return 0.0F;
        }
        const float u = r <= f->a ? 0.0F : (r - f->a) / (f->radius - f->a);
        return f->height * 0.5F * (1.0F + cosf(3.1415927F * u));
    }
    case ENG_MAPF_MOUNTAIN: {
        const float s = smooth(0.0F, 1.0F, 1.0F - (r / f->radius));
        return f->height * powf(s, 1.35F) * (0.82F + (0.36F * ridge));
    }
    case ENG_MAPF_VOLCANO: {
        if (r >= f->radius) {
            return 0.0F;
        }
        const float rim = f->height * powf(1.0F - (f->a / f->radius), 1.35F);
        if (r < f->a) { /* cratère à fond plat */
            return rim - (f->b * (1.0F - smooth(0.7F * f->a, f->a, r)));
        }
        const float t = 1.0F - (r / f->radius);
        return (f->height * powf(t, 1.35F)) * (0.94F + (0.12F * ridge));
    }
    case ENG_MAPF_MESA:
        return f->height * (1.0F - smooth(0.82F * f->radius, f->radius, r));
    case ENG_MAPF_DUNES: {
        const float fall = 1.0F - smooth(0.6F * f->radius, f->radius, r);
        if (fall <= 0.0F) {
            return 0.0F;
        }
        const float u = (x * cosf(f->c)) + (z * sinf(f->c));
        const float v = (-x * sinf(f->c)) + (z * cosf(f->c));
        const float phase = (6.2831853F * u / f->b) + (1.7F * sinf(6.2831853F * v / (2.7F * f->b)));
        return f->a * fall * powf(0.5F + (0.5F * sinf(phase)), 1.6F);
    }
    case ENG_MAPF_RIDGE: {
        uint32_t seg = 0;
        float t = 0.0F;
        const float d = polyline_distance(m, f, x, z, &seg, &t);
        const float half = 0.5F * f->radius;
        return d < half ? f->height * 0.5F * (1.0F + cosf(3.1415927F * d / half)) *
                              (0.78F + (0.44F * ridge))
                        : 0.0F;
    }
    default:
        return 0.0F;
    }
}

static bool is_relief(uint8_t kind)
{
    return kind <= (uint8_t)ENG_MAPF_DUNES;
}

/* poids des biomes au point (x, z) déformé : graines de régions, frontières adoucies */
static void region_weights(const EngWorldMap *m, float x, float z, float out[ENG_BIOME_COUNT])
{
    float score[ENG_WORLDMAP_MAX_REGIONS];
    float best = INFINITY;
    for (uint32_t i = 0; i < m->region_count; i++) {
        score[i] = hypotf(x - m->regions[i].x, z - m->regions[i].z) / m->regions[i].reach;
        best = fminf(best, score[i]);
    }
    for (int k = 0; k < ENG_BIOME_COUNT; k++) {
        out[k] = 0.0F;
    }
    float total = 0.0F;
    for (uint32_t i = 0; i < m->region_count; i++) {
        const float w = expf(-REGION_SHARPNESS * (score[i] - best));
        out[m->regions[i].biome] += w;
        total += w;
    }
    for (int k = 0; k < ENG_BIOME_COUNT; k++) {
        out[k] /= total;
    }
}

/* étape 1, une ligne z de la trame */
static void build_row(EngWorldMap *m, uint32_t j, const float *fbm, const float *ridged,
                      const float *warp_x, const float *warp_z, const uint16_t *list,
                      uint32_t list_count)
{
    const float half = 0.5F * m->side;
    const float z = -half + ((float)j * m->spacing);
    for (uint32_t i = 0; i < m->size; i++) {
        const float x = -half + ((float)i * m->spacing);
        float w[ENG_BIOME_COUNT];
        region_weights(m, x + (WARP_M * warp_x[i]), z + (WARP_M * warp_z[i]), w);
        const float ridge = clampf(0.5F * (ridged[i] + 1.0F), 0.0F, 1.0F);
        float h = 0.0F;
        for (int k = 0; k < ENG_BIOME_COUNT; k++) {
            const float detail = k == ENG_BIOME_SNOW ? ridge : fbm[i];
            h += w[k] * (BASE_HEIGHT[k] + (DETAIL_AMP[k] * detail));
        }
        for (uint32_t n = 0; n < list_count; n++) {
            h += relief(m, &m->features[list[n]], x, z, ridge);
        }
        if (m->border_start > 0.0F) { /* falaises du bord : norme « carré arrondi » */
            const float q = powf(powf(fabsf(x), 6.0F) + powf(fabsf(z), 6.0F), 1.0F / 6.0F);
            const float angle = atan2f(z, x);
            const float wobble =
                (55.0F * sinf(7.0F * angle)) + (30.0F * sinf((13.0F * angle) + 1.0F));
            const float f = smooth(m->border_start, half, q + wobble);
            h += (m->border_height * (0.8F + (0.4F * ridge)) - h) * f;
        }
        const size_t at = ((size_t)j * m->size) + i;
        m->heights[at] = h;
        uint8_t *px = &m->weights[at * 4U];
        px[0] = (uint8_t)lroundf(w[ENG_BIOME_SNOW] * 255.0F);
        px[1] = (uint8_t)lroundf(w[ENG_BIOME_DESERT] * 255.0F);
        px[2] = (uint8_t)lroundf(w[ENG_BIOME_VOLCANO] * 255.0F);
        px[3] = 0U;
    }
}

static void build_relief(EngWorldMap *m, float *rows)
{
    float *fbm = rows;
    float *ridged = rows + (size_t)m->size;
    float *warp_x = rows + (2U * (size_t)m->size);
    float *warp_z = rows + (3U * (size_t)m->size);
    const EngNoiseParams p_fbm = {DETAIL_SEED, 1.0F / 160.0F, 4U, 2.0F, 0.5F, ENG_NOISE_FBM};
    const EngNoiseParams p_ridge = {DETAIL_SEED + 1, 1.0F / 340.0F, 4U, 2.1F, 0.5F,
                                    ENG_NOISE_RIDGED};
    const EngNoiseParams p_wx = {DETAIL_SEED + 2, 1.0F / 700.0F, 2U, 2.0F, 0.5F, ENG_NOISE_FBM};
    const EngNoiseParams p_wz = {DETAIL_SEED + 3, 1.0F / 700.0F, 2U, 2.0F, 0.5F, ENG_NOISE_FBM};
    const float half = 0.5F * m->side;
    static uint16_t list[ENG_WORLDMAP_MAX_FEATURES];
    for (uint32_t j = 0; j < m->size; j++) {
        const float z = -half + ((float)j * m->spacing);
        uint32_t count = 0;
        for (uint32_t n = 0; n < m->feature_count; n++) { /* formes qui touchent cette ligne */
            float b[4];
            bounds(m, &m->features[n], 0.0F, b);
            if (is_relief(m->features[n].kind) && z >= b[1] && z <= b[3]) {
                list[count++] = (uint16_t)n;
            }
        }
        eng_noise_row_2d(&p_fbm, -half, m->spacing, z, m->size, fbm);
        eng_noise_row_2d(&p_ridge, -half, m->spacing, z, m->size, ridged);
        eng_noise_row_2d(&p_wx, -half, m->spacing, z, m->size, warp_x);
        eng_noise_row_2d(&p_wz, -half, m->spacing, z, m->size, warp_z);
        build_row(m, j, fbm, ridged, warp_x, warp_z, list, count);
    }
}

/* parcourt les échantillons de la boîte `b` ; `fn` reçoit l'indice et la position */
typedef void (*SampleFn)(EngWorldMap *m, const EngMapFeature *f, size_t at, float x, float z,
                         const float *extra);

static void for_box(EngWorldMap *m, const EngMapFeature *f, const float b[4], SampleFn fn,
                    const float *extra)
{
    const float half = 0.5F * m->side;
    const int32_t last = (int32_t)m->size - 1;
    const int32_t i0 = (int32_t)clampf(floorf((b[0] + half) / m->spacing), 0.0F, (float)last);
    const int32_t i1 = (int32_t)clampf(ceilf((b[2] + half) / m->spacing), 0.0F, (float)last);
    const int32_t j0 = (int32_t)clampf(floorf((b[1] + half) / m->spacing), 0.0F, (float)last);
    const int32_t j1 = (int32_t)clampf(ceilf((b[3] + half) / m->spacing), 0.0F, (float)last);
    for (int32_t j = j0; j <= j1; j++) {
        for (int32_t i = i0; i <= i1; i++) {
            fn(m, f, ((size_t)j * m->size) + (size_t)i, -half + ((float)i * m->spacing),
               -half + ((float)j * m->spacing), extra);
        }
    }
}

static void flatten_at(EngWorldMap *m, const EngMapFeature *f, size_t at, float x, float z,
                       const float *target)
{
    const float k = 1.0F - smooth(0.55F * f->radius, f->radius, hypotf(x - f->x, z - f->z));
    m->heights[at] += (*target - m->heights[at]) * k;
}

/* `heights` : hauteur du chemin à chacun de ses sommets */
static void path_at(EngWorldMap *m, const EngMapFeature *f, size_t at, float x, float z,
                    const float *heights)
{
    uint32_t seg = 0;
    float t = 0.0F;
    if (f->point_count < 2U) {
        return;
    }
    const float d = polyline_distance(m, f, x, z, &seg, &t);
    const float half = 0.5F * f->radius;
    const float k = 1.0F - smooth(half, half + PATH_MARGIN, d);
    if (k <= 0.0F || seg + 1U >= f->point_count) {
        return;
    }
    const float h = heights[seg] + ((heights[seg + 1U] - heights[seg]) * t);
    m->heights[at] += (h - m->heights[at]) * k;
    const uint8_t mark = (uint8_t)lroundf(255.0F * (1.0F - smooth(0.6F * half, half + 1.0F, d)));
    m->weights[(at * 4U) + 3U] =
        mark > m->weights[(at * 4U) + 3U] ? mark : m->weights[(at * 4U) + 3U];
}

/* lacs et rivières : le relief ne dépasse jamais le profil du lit et des berges */
static void carve_at(EngWorldMap *m, const EngMapFeature *f, size_t at, float x, float z,
                     const float *unused)
{
    (void)unused;
    float d = 0.0F;
    float half = f->radius;
    if (f->kind == (uint8_t)ENG_MAPF_RIVER) {
        uint32_t seg = 0;
        float t = 0.0F;
        d = polyline_distance(m, f, x, z, &seg, &t);
        half = 0.5F * f->radius;
    } else {
        d = hypotf(x - f->x, z - f->z);
    }
    float target = 0.0F;
    if (d < half) {
        const float u = d / half;
        target = m->water - (f->height * (1.0F - (u * u)));
    } else {
        target = m->water + (0.45F * (d - half) * (d - half) / BANK_M);
        if (d > half + (3.0F * BANK_M)) {
            return;
        }
    }
    m->heights[at] = fminf(m->heights[at], target);
}

static void lava_at(EngWorldMap *m, const EngMapFeature *f, size_t at, float x, float z,
                    const float *unused)
{
    (void)unused;
    uint32_t seg = 0;
    float t = 0.0F;
    const float half = 0.5F * f->radius;
    const float d = polyline_distance(m, f, x, z, &seg, &t);
    const float k = 1.0F - smooth(0.5F * half, half + 2.0F, d);
    if (k <= 0.0F) {
        return;
    }
    const float free = 1.0F - ((float)m->weights[(at * 4U) + 3U] / 255.0F); /* pas sur un chemin */
    const uint8_t v = (uint8_t)lroundf(255.0F * k * free);
    m->lava[at] = v > m->lava[at] ? v : m->lava[at];
    m->heights[at] -= 1.5F * k * free;
}

/* hauteurs des sommets d'un chemin : celles du sol, puis pente bornée à PATH_MAX_SLOPE, les deux
 * extrémités restant au sol. Si c'est impossible (extrémités trop éloignées en altitude pour la
 * longueur du chemin), la pente est répartie uniformément d'une extrémité à l'autre. */
static void limit_path_slope(const EngWorldMap *m, const EngMapFeature *f, float *h)
{
    const uint32_t n = f->point_count;
    float d[ENG_WORLDMAP_MAX_POINTS] = {0};
    float length = 0.0F;
    if (n < 2U || n > ENG_WORLDMAP_MAX_POINTS) {
        return;
    }
    for (uint32_t p = 0; p + 1U < n; p++) {
        const float *a = m->points[f->first_point + p];
        const float *b = m->points[f->first_point + p + 1U];
        d[p] = hypotf(b[0] - a[0], b[1] - a[1]);
        length += d[p];
    }
    if (fabsf(h[n - 1U] - h[0]) > PATH_MAX_SLOPE * length) {
        float along = 0.0F;
        for (uint32_t p = 1; p + 1U < n; p++) {
            along += d[p - 1U];
            h[p] = h[0] + ((h[n - 1U] - h[0]) * (along / length));
        }
        return;
    }
    for (int iter = 0; iter < 64; iter++) {
        for (uint32_t p = 1; p + 1U < n; p++) { /* vers l'avant, puis vers l'arrière */
            const float r = PATH_MAX_SLOPE * d[p - 1U];
            h[p] = clampf(h[p], h[p - 1U] - r, h[p - 1U] + r);
        }
        for (uint32_t p = n - 2U; p >= 1U && p + 1U < n; p--) {
            const float r = PATH_MAX_SLOPE * d[p];
            h[p] = clampf(h[p], h[p + 1U] - r, h[p + 1U] + r);
        }
    }
}

bool eng_worldmap_build(EngWorldMap *map, EngArena *arena)
{
    const size_t bytes = eng_worldmap_build_bytes(map);
    if (bytes == 0U || arena == NULL || map->region_count == 0U) {
        return false;
    }
    map->size = (uint32_t)lroundf(map->side / map->spacing) + 1U;
    const size_t n = (size_t)map->size * map->size;
    map->heights = eng_arena_alloc_array(arena, sizeof(float), n, sizeof(float));
    map->weights = eng_arena_alloc_array(arena, 4U, n, 4U);
    map->lava = eng_arena_alloc_array(arena, 1U, n, 1U);
    const EngArenaMark mark = eng_arena_mark(arena);
    float *rows =
        eng_arena_alloc_array(arena, sizeof(float), 4U * (size_t)map->size, sizeof(float));
    if (map->heights == NULL || map->weights == NULL || map->lava == NULL || rows == NULL) {
        map->heights = NULL;
        map->weights = NULL;
        map->lava = NULL;
        return false;
    }
    memset(map->lava, 0, n);
    build_relief(map, rows);
    eng_arena_reset_to(arena, mark);
    for (uint32_t k = 0; k < map->feature_count; k++) { /* 2. aires aplanies */
        const EngMapFeature *f = &map->features[k];
        if (f->kind == (uint8_t)ENG_MAPF_FLATTEN) {
            const float target =
                isfinite(f->height) ? f->height : eng_worldmap_height(map, f->x, f->z);
            float b[4];
            bounds(map, f, 0.0F, b);
            for_box(map, f, b, flatten_at, &target);
        }
    }
    for (uint32_t k = 0; k < map->feature_count; k++) { /* 3. chemins */
        const EngMapFeature *f = &map->features[k];
        if (f->kind == (uint8_t)ENG_MAPF_PATH) {
            float heights[ENG_WORLDMAP_MAX_POINTS] = {0};
            for (uint32_t p = 0; p < f->point_count; p++) {
                const float *pt = map->points[f->first_point + p];
                heights[p] = eng_worldmap_height(map, pt[0], pt[1]);
            }
            limit_path_slope(map, f, heights);
            float b[4];
            bounds(map, f, PATH_MARGIN, b);
            for_box(map, f, b, path_at, heights);
        }
    }
    for (uint32_t k = 0; k < map->feature_count; k++) { /* 4. lacs et rivières */
        const EngMapFeature *f = &map->features[k];
        if (f->kind == (uint8_t)ENG_MAPF_LAKE || f->kind == (uint8_t)ENG_MAPF_RIVER) {
            float b[4];
            bounds(map, f, 3.0F * BANK_M, b);
            for_box(map, f, b, carve_at, NULL);
        }
    }
    for (uint32_t k = 0; k < map->feature_count; k++) { /* 5. lave */
        const EngMapFeature *f = &map->features[k];
        if (f->kind == (uint8_t)ENG_MAPF_LAVA) {
            float b[4];
            bounds(map, f, 2.0F, b);
            for_box(map, f, b, lava_at, NULL);
        }
    }
    map->height_min = INFINITY;
    map->height_max = -INFINITY;
    for (size_t i = 0; i < n; i++) {
        map->height_min = fminf(map->height_min, map->heights[i]);
        map->height_max = fmaxf(map->height_max, map->heights[i]);
    }
    return true;
}
