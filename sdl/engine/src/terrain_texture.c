#include "engine/terrain_texture.h"

#include <math.h>

#define OCTAVES 5U

/* Hachage d'entiers (lowbias32) : valeur dans [0, 1[ d'un point du réseau. */
static float lattice(uint32_t x, uint32_t y, uint32_t seed)
{
    uint32_t h = (x * 0x9E3779B1U) ^ (y * 0x85EBCA77U) ^ (seed * 0xC2B2AE3DU);
    h ^= h >> 16;
    h *= 0x7FEB352DU;
    h ^= h >> 15;
    h *= 0x846CA68BU;
    h ^= h >> 16;
    return (float)(h >> 8) / 16777216.0F;
}

static float fade(float t)
{
    return t * t * (3.0F - (2.0F * t));
}

/* Bruit de valeurs de période `period` cellules en x et en y, à (u, v) dans [0, 1[ : le point (u,
 * v) et (u + 1, v) donnent la même valeur. */
static float value_noise(float u, float v, uint32_t period, uint32_t seed)
{
    const float fx = u * (float)period;
    const float fy = v * (float)period;
    const float x0 = floorf(fx);
    const float y0 = floorf(fy);
    const float tx = fade(fx - x0);
    const float ty = fade(fy - y0);
    const uint32_t ix = (uint32_t)x0 % period;
    const uint32_t iy = (uint32_t)y0 % period;
    const uint32_t jx = (ix + 1U) % period;
    const uint32_t jy = (iy + 1U) % period;
    const float a = lattice(ix, iy, seed);
    const float b = lattice(jx, iy, seed);
    const float c = lattice(ix, jy, seed);
    const float d = lattice(jx, jy, seed);
    return (a + ((b - a) * tx)) + ((((c + ((d - c) * tx))) - (a + ((b - a) * tx))) * ty);
}

/* Somme d'octaves normalisée dans [0, 1] : période de départ `base_period`, doublée à chaque
 * octave, amplitude `gain` fois plus petite. */
static float fbm(float u, float v, uint32_t base_period, float gain, uint32_t seed)
{
    float sum = 0.0F;
    float amplitude = 1.0F;
    float total = 0.0F;
    uint32_t period = base_period;
    for (uint32_t o = 0; o < OCTAVES; o++) {
        sum += amplitude * value_noise(u, v, period, seed + (o * 101U));
        total += amplitude;
        amplitude *= gain;
        period *= 2U;
    }
    return sum / total;
}

typedef struct LayerLook {
    float dark[3]; /* sRGB, [0, 1] */
    float light[3];
    uint32_t base_period; /* cellules de la première octave */
    float gain;
    float contrast; /* >1 : plus de contraste dans le bruit */
    uint32_t seed;
} LayerLook;

static const LayerLook LOOKS[ENG_SPLAT_LAYERS] = {
    /* herbe : vert, détails fins */
    {{0.13F, 0.27F, 0.07F}, {0.36F, 0.52F, 0.17F}, 8U, 0.62F, 1.2F, 11U},
    /* roche : gris chaud, grandes fissures */
    {{0.30F, 0.28F, 0.26F}, {0.60F, 0.57F, 0.53F}, 4U, 0.55F, 1.5F, 23U},
    /* terre : brun */
    {{0.27F, 0.18F, 0.10F}, {0.47F, 0.34F, 0.21F}, 6U, 0.55F, 1.3F, 37U},
    /* neige : presque blanche, très peu de contraste */
    {{0.82F, 0.86F, 0.91F}, {0.98F, 0.99F, 1.00F}, 4U, 0.45F, 1.0F, 53U},
};

void eng_terrain_layer_pixels(EngSplatLayer layer, uint32_t size, uint8_t *rgba)
{
    if ((uint32_t)layer >= ENG_SPLAT_LAYERS || size < 4U) {
        return;
    }
    const LayerLook *look = &LOOKS[layer];
    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            const float n = fbm((float)x / (float)size, (float)y / (float)size, look->base_period,
                                look->gain, look->seed);
            float t = 0.5F + ((n - 0.5F) * look->contrast);
            t = t < 0.0F ? 0.0F : (t > 1.0F ? 1.0F : t);
            uint8_t *p = &rgba[((size_t)y * size + x) * 4U];
            for (int c = 0; c < 3; c++) {
                const float v = look->dark[c] + ((look->light[c] - look->dark[c]) * t);
                p[c] = (uint8_t)((v * 255.0F) + 0.5F);
            }
            p[3] = 255U;
        }
    }
}
