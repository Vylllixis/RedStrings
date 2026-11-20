#include "engine/biome.h"

#include <math.h>
#include <stddef.h>

static const float ALTITUDE_SCALE[ENG_BIOME_COUNT] = {0.45F, 1.8F, 0.35F, 1.2F};

EngBiomeMap eng_biome_map_default(void)
{
    return (EngBiomeMap){.enabled = true,
                         .start_x = 0.0F,
                         .band_m = 800.0F,
                         .blend_m = 160.0F,
                         .wobble_m = 70.0F,
                         .wobble_wavelength_m = 900.0F};
}

bool eng_biome_map_valid(const EngBiomeMap *map)
{
    return map != NULL && isfinite(map->start_x) && isfinite(map->band_m) &&
           isfinite(map->blend_m) && isfinite(map->wobble_m) &&
           isfinite(map->wobble_wavelength_m) && map->blend_m > 0.0F &&
           map->band_m > map->blend_m && map->wobble_m >= 0.0F && map->wobble_wavelength_m > 0.0F;
}

static float smooth(float edge0, float edge1, float x)
{
    const float t = (x - edge0) / (edge1 - edge0);
    const float c = t < 0.0F ? 0.0F : (t > 1.0F ? 1.0F : t);
    return c * c * (3.0F - (2.0F * c));
}

void eng_biome_weights(const EngBiomeMap *map, double x, double z, float out[ENG_BIOME_COUNT])
{
    out[ENG_BIOME_PLAIN] = 1.0F;
    out[ENG_BIOME_SNOW] = 0.0F;
    out[ENG_BIOME_DESERT] = 0.0F;
    out[ENG_BIOME_VOLCANO] = 0.0F;
    if (map == NULL || !map->enabled || !eng_biome_map_valid(map)) {
        return;
    }
    /* la frontière ondule : on déplace le point le long de X selon z */
    const float u =
        (float)(x - (double)map->start_x) +
        (map->wobble_m * sinf((float)(z * 6.283185307179586 / (double)map->wobble_wavelength_m)));
    const float half = 0.5F * map->blend_m;
    float t[ENG_BIOME_COUNT]; /* t[k] : part des biomes d'indice >= k */
    t[0] = 1.0F;
    for (int k = 1; k < ENG_BIOME_COUNT; k++) {
        const float edge = (float)k * map->band_m;
        t[k] = smooth(edge - half, edge + half, u);
    }
    for (int k = 0; k < ENG_BIOME_COUNT; k++) {
        const float next = k + 1 < ENG_BIOME_COUNT ? t[k + 1] : 0.0F;
        out[k] = t[k] - next;
    }
}

EngBiome eng_biome_at(const EngBiomeMap *map, double x, double z)
{
    float w[ENG_BIOME_COUNT];
    eng_biome_weights(map, x, z, w);
    int best = 0;
    for (int k = 1; k < ENG_BIOME_COUNT; k++) {
        if (w[k] > w[best]) {
            best = k;
        }
    }
    return (EngBiome)best;
}

const char *eng_biome_name(EngBiome biome)
{
    switch (biome) {
    case ENG_BIOME_PLAIN:
        return "plaine";
    case ENG_BIOME_SNOW:
        return "montagne enneigée";
    case ENG_BIOME_DESERT:
        return "désert";
    case ENG_BIOME_VOLCANO:
        return "volcan";
    default:
        return "?";
    }
}

float eng_biome_altitude_scale(const float weights[ENG_BIOME_COUNT])
{
    if (weights == NULL) {
        return 1.0F;
    }
    float scale = 0.0F;
    for (int k = 0; k < ENG_BIOME_COUNT; k++) {
        scale += weights[k] * ALTITUDE_SCALE[k];
    }
    return scale;
}

float eng_biome_shade(const float weights[ENG_BIOME_COUNT])
{
    /* plaine, montagne, désert : pleine lumière ; volcan : basalte, deux fois plus sombre */
    static const float shade[ENG_BIOME_COUNT] = {1.0F, 1.0F, 1.0F, 0.45F};
    if (weights == NULL) {
        return 1.0F;
    }
    float out = 0.0F;
    float total = 0.0F;
    for (int k = 0; k < ENG_BIOME_COUNT; k++) {
        const float w = isfinite(weights[k]) && weights[k] > 0.0F ? weights[k] : 0.0F;
        out += w * shade[k];
        total += w;
    }
    return total > 1e-6F ? out / total : 1.0F;
}

void eng_biome_cell_layers(const EngBiomeMap *map, double x_min, bool *sand, bool *lava)
{
    if (sand != NULL) {
        *sand = false;
    }
    if (lava != NULL) {
        *lava = false;
    }
    if (map == NULL || !map->enabled || !isfinite(x_min)) {
        return;
    }
    /* au-delà de la frontière + ondulation + demi-fondu, le biome de gauche ne pèse plus rien */
    const double reach = (double)map->wobble_m + (0.5 * (double)map->blend_m);
    if (sand != NULL) { /* plus de plaine : le canal de l'herbe devient du sable */
        *sand = x_min >= (double)map->start_x + (double)map->band_m + reach;
    }
    if (lava != NULL) { /* plus de montagne : le canal de la neige devient de la lave */
        *lava = x_min >= (double)map->start_x + (2.0 * (double)map->band_m) + reach;
    }
}

/* part de lave pour un écart `d` à la ligne médiane d'une coulée */
static float lava_band(double f)
{
    const float d = (float)fabs(f);
    if (d <= ENG_BIOME_LAVA_CORE) {
        return 1.0F;
    }
    return d >= ENG_BIOME_LAVA_EDGE
               ? 0.0F
               : (ENG_BIOME_LAVA_EDGE - d) / (ENG_BIOME_LAVA_EDGE - ENG_BIOME_LAVA_CORE);
}

float eng_biome_lava_river(double x, double z)
{
    if (!isfinite(x) || !isfinite(z)) {
        return 0.0F;
    }
    /* deux familles de lignes sinueuses (nord-sud tous les 116 m, est-ouest tous les 129 m). Chaque
     * coulée est là où UN sinus s'annule : sa pente y est maximale, donc la largeur constante
     * (pas de lac là où deux sinus s'annuleraient ensemble). */
    const float ns = lava_band(sin((x / 37.0) + (1.7 * sin(z / 53.0))));
    const float ew = lava_band(sin((z / 41.0) + (1.3 * sin(x / 67.0))));
    return ns > ew ? ns : ew;
}

void eng_biome_splat(const float weights[ENG_BIOME_COUNT], const float base[ENG_SPLAT_LAYERS],
                     float river, float out[ENG_SPLAT_LAYERS])
{
    const float grass = base[ENG_SPLAT_GRASS];
    const float rock = base[ENG_SPLAT_ROCK];
    const float dirt = base[ENG_SPLAT_DIRT];
    const float snow = base[ENG_SPLAT_SNOW];
    const float total = grass + rock + dirt + snow;
    const float rest = total - rock;
    /* volcan : la coulée de lave remplace le sol meuble (pas la roche des pentes) */
    const float lava =
        isfinite(river) ? (river < 0.0F ? 0.0F : (river > 1.0F ? 1.0F : river)) : 0.0F;
    /* une règle par biome : [herbe OU sable, roche, terre, neige OU lave], de somme `total`. Le
     * canal 0 est du sable et le canal 3 de la lave dans les cellules qu'indique
     * eng_biome_cell_layers. */
    const float rules[ENG_BIOME_COUNT][ENG_SPLAT_LAYERS] = {
        {grass + snow, rock, dirt, 0.0F},        /* plaine : jamais de neige */
        {0.0F, rock, 0.0F, grass + dirt + snow}, /* montagne : neige */
        {rest, rock, 0.0F, 0.0F},                /* désert : sable, roche sur les pentes */
        {0.0F, rock + (0.8F * rest * (1.0F - lava)), 0.2F * rest * (1.0F - lava),
         rest * lava}, /* volcan : basalte, cendre, lave */
    };
    for (uint32_t l = 0; l < ENG_SPLAT_LAYERS; l++) {
        out[l] = 0.0F;
        for (int k = 0; k < ENG_BIOME_COUNT; k++) {
            out[l] += weights[k] * rules[k][l];
        }
    }
}
