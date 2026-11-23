#include "engine/ibl.h"

#include <math.h>
#include <string.h>

#define PI_F 3.14159265358979323846F
#define FACES 6U
#ifndef ENG_IBL_LOD_BIAS
#define ENG_IBL_LOD_BIAS 0.0F
#endif

static float dot3(const float a[3], const float b[3])
{
    return (a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]);
}

static void normalize3(float v[3])
{
    const float len2 = dot3(v, v);
    if (len2 > 1e-30F) {
        const float inv = 1.0F / sqrtf(len2);
        v[0] *= inv;
        v[1] *= inv;
        v[2] *= inv;
    }
}

static uint32_t level_size(uint32_t size, uint32_t level)
{
    const uint32_t s = level < 31U ? size >> level : 0U;
    return s > 0U ? s : 1U;
}

static size_t layer_floats(uint32_t size, uint32_t levels)
{
    size_t total = 0;
    for (uint32_t l = 0; l < levels; l++) {
        const size_t s = level_size(size, l);
        total += s * s * 3U;
    }
    return total;
}

size_t eng_ibl_cube_floats(uint32_t size, uint32_t levels)
{
    return layer_floats(size, levels) * FACES;
}

uint32_t eng_ibl_full_levels(uint32_t size)
{
    uint32_t levels = 1;
    while ((size >> levels) > 0U) {
        levels++;
    }
    return levels;
}

size_t eng_ibl_cube_offset(uint32_t size, uint32_t levels, uint32_t face, uint32_t level)
{
    size_t offset = layer_floats(size, levels) * face;
    for (uint32_t l = 0; l < level; l++) {
        const size_t s = level_size(size, l);
        offset += s * s * 3U;
    }
    return offset;
}

/* Convention de la spécification OpenGL (tableau 8.19) : (sc, tc, ma) par face. */
void eng_ibl_cube_direction(uint32_t face, float s, float t, float out[3])
{
    const float sc = (2.0F * s) - 1.0F;
    const float tc = (2.0F * t) - 1.0F;
    switch (face) {
    case 0: /* +X */
        out[0] = 1.0F;
        out[1] = -tc;
        out[2] = -sc;
        break;
    case 1: /* -X */
        out[0] = -1.0F;
        out[1] = -tc;
        out[2] = sc;
        break;
    case 2: /* +Y */
        out[0] = sc;
        out[1] = 1.0F;
        out[2] = tc;
        break;
    case 3: /* -Y */
        out[0] = sc;
        out[1] = -1.0F;
        out[2] = -tc;
        break;
    case 4: /* +Z */
        out[0] = sc;
        out[1] = -tc;
        out[2] = 1.0F;
        break;
    default: /* -Z */
        out[0] = -sc;
        out[1] = -tc;
        out[2] = -1.0F;
        break;
    }
}

uint32_t eng_ibl_cube_face_st(const float dir[3], float *s, float *t)
{
    const float ax = fabsf(dir[0]);
    const float ay = fabsf(dir[1]);
    const float az = fabsf(dir[2]);
    uint32_t face;
    float sc;
    float tc;
    float ma;
    if (ax >= ay && ax >= az) {
        ma = ax;
        face = dir[0] >= 0.0F ? 0U : 1U;
        sc = dir[0] >= 0.0F ? -dir[2] : dir[2];
        tc = -dir[1];
    } else if (ay >= az) {
        ma = ay;
        face = dir[1] >= 0.0F ? 2U : 3U;
        sc = dir[0];
        tc = dir[1] >= 0.0F ? dir[2] : -dir[2];
    } else {
        ma = az;
        face = dir[2] >= 0.0F ? 4U : 5U;
        sc = dir[2] >= 0.0F ? dir[0] : -dir[0];
        tc = -dir[1];
    }
    *s = ((sc / ma) + 1.0F) * 0.5F;
    *t = ((tc / ma) + 1.0F) * 0.5F;
    return face;
}

static void sample_level(const float *cube, uint32_t size, uint32_t levels, uint32_t face,
                         uint32_t level, float s, float t, float out[3])
{
    const uint32_t n = level_size(size, level);
    const float *base = cube + eng_ibl_cube_offset(size, levels, face, level);
    const float x = (s * (float)n) - 0.5F;
    const float y = (t * (float)n) - 0.5F;
    const float fx = floorf(x);
    const float fy = floorf(y);
    const float wx = x - fx;
    const float wy = y - fy;
    int32_t x0 = (int32_t)fx;
    int32_t y0 = (int32_t)fy;
    int32_t x1 = x0 + 1;
    int32_t y1 = y0 + 1;
    const int32_t last = (int32_t)n - 1;
    x0 = x0 < 0 ? 0 : (x0 > last ? last : x0);
    x1 = x1 < 0 ? 0 : (x1 > last ? last : x1);
    y0 = y0 < 0 ? 0 : (y0 > last ? last : y0);
    y1 = y1 < 0 ? 0 : (y1 > last ? last : y1);
    for (int c = 0; c < 3; c++) {
        const float a = base[((((size_t)y0 * n) + (size_t)x0) * 3U) + (size_t)c];
        const float b = base[((((size_t)y0 * n) + (size_t)x1) * 3U) + (size_t)c];
        const float d = base[((((size_t)y1 * n) + (size_t)x0) * 3U) + (size_t)c];
        const float e = base[((((size_t)y1 * n) + (size_t)x1) * 3U) + (size_t)c];
        out[c] = ((a * (1.0F - wx)) + (b * wx)) * (1.0F - wy) + ((d * (1.0F - wx)) + (e * wx)) * wy;
    }
}

void eng_ibl_cube_sample(const float *cube, uint32_t size, uint32_t levels, const float dir[3],
                         float lod, float out[3])
{
    float s;
    float t;
    const uint32_t face = eng_ibl_cube_face_st(dir, &s, &t);
    const float max_lod = (float)(levels - 1U);
    lod = lod < 0.0F ? 0.0F : (lod > max_lod ? max_lod : lod);
    const uint32_t l0 = (uint32_t)lod;
    const uint32_t l1 = l0 + 1U < levels ? l0 + 1U : l0;
    const float w = lod - (float)l0;
    float a[3];
    sample_level(cube, size, levels, face, l0, s, t, a);
    if (w <= 0.0F || l1 == l0) {
        memcpy(out, a, sizeof a);
        return;
    }
    float b[3];
    sample_level(cube, size, levels, face, l1, s, t, b);
    for (int c = 0; c < 3; c++) {
        out[c] = (a[c] * (1.0F - w)) + (b[c] * w);
    }
}

void eng_ibl_sky_default(EngSkyDesc *sky)
{
    const float zenith[3] = {0.12F, 0.30F, 0.75F};
    const float horizon[3] = {0.65F, 0.75F, 0.90F};
    const float ground[3] = {0.18F, 0.16F, 0.14F};
    memcpy(sky->zenith, zenith, sizeof zenith);
    memcpy(sky->horizon, horizon, sizeof horizon);
    memcpy(sky->ground, ground, sizeof ground);
    sky->sun_direction[0] = 0.4F;
    sky->sun_direction[1] = 0.7F;
    sky->sun_direction[2] = 0.59F;
    normalize3(sky->sun_direction);
    sky->sun_radiance[0] = 40.0F;
    sky->sun_radiance[1] = 36.0F;
    sky->sun_radiance[2] = 30.0F;
    sky->sun_angular_radius_deg = 3.0F;
}

void eng_ibl_make_sky(float *cube, uint32_t size, uint32_t levels, const EngSkyDesc *sky)
{
    const float cos_sun = cosf(sky->sun_angular_radius_deg * PI_F / 180.0F);
    for (uint32_t face = 0; face < FACES; face++) {
        float *base = cube + eng_ibl_cube_offset(size, levels, face, 0);
        for (uint32_t y = 0; y < size; y++) {
            for (uint32_t x = 0; x < size; x++) {
                float d[3];
                eng_ibl_cube_direction(face, ((float)x + 0.5F) / (float)size,
                                       ((float)y + 0.5F) / (float)size, d);
                normalize3(d);
                float *px = base + ((((size_t)y * size) + x) * 3U);
                const float up = d[1];
                const float k = sqrtf(up > 0.0F ? up : 0.0F);
                for (int c = 0; c < 3; c++) {
                    px[c] = up > 0.0F ? sky->horizon[c] + ((sky->zenith[c] - sky->horizon[c]) * k)
                                      : sky->ground[c];
                    if (dot3(d, sky->sun_direction) >= cos_sun) {
                        px[c] = sky->sun_radiance[c];
                    }
                }
            }
        }
    }
}

/* Échantillon bilinéaire de l'image équirectangulaire dans la direction `d` (unitaire). */
static void equirect_sample(const float *rgb, uint32_t width, uint32_t height, const float d[3],
                            float out[3])
{
    const float u = (atan2f(d[2], d[0]) / (2.0F * PI_F)) + 0.5F;
    const float v = (asinf(fmaxf(-1.0F, fminf(1.0F, d[1]))) / PI_F) + 0.5F;
    const float fx = (u * (float)width) - 0.5F;
    const float fy = ((1.0F - v) * (float)height) - 0.5F;
    const float flx = floorf(fx);
    const float fly = floorf(fy);
    const float wx = fx - flx;
    const float wy = fy - fly;
    int32_t xs[2];
    int32_t ys[2];
    xs[0] = (int32_t)flx;
    xs[1] = xs[0] + 1;
    ys[0] = (int32_t)fly;
    ys[1] = ys[0] + 1;
    for (int i = 0; i < 2; i++) {
        xs[i] = ((xs[i] % (int32_t)width) + (int32_t)width) % (int32_t)width;
        ys[i] = ys[i] < 0 ? 0 : (ys[i] >= (int32_t)height ? (int32_t)height - 1 : ys[i]);
    }
    for (int c = 0; c < 3; c++) {
        const float a = rgb[((((size_t)ys[0] * width) + (size_t)xs[0]) * 3U) + (size_t)c];
        const float b = rgb[((((size_t)ys[0] * width) + (size_t)xs[1]) * 3U) + (size_t)c];
        const float e = rgb[((((size_t)ys[1] * width) + (size_t)xs[0]) * 3U) + (size_t)c];
        const float f = rgb[((((size_t)ys[1] * width) + (size_t)xs[1]) * 3U) + (size_t)c];
        out[c] = ((a * (1.0F - wx)) + (b * wx)) * (1.0F - wy) + ((e * (1.0F - wx)) + (f * wx)) * wy;
    }
}

void eng_ibl_from_equirect(float *cube, uint32_t size, uint32_t levels, const float *rgb,
                           uint32_t width, uint32_t height)
{
    /* Sur-échantillonnage : sub x sub points par texel du cube, moyennés. Un seul point par texel
     * sous-échantillonne un petit soleil très intense (quelques texels du cube) et fait varier son
     * énergie de plusieurs dizaines de % ; avec sub >= 2 x (pixels de l'image par texel) l'énergie
     * est conservée à quelques % (mesuré : tools/reference/ibl_brute.py). */
    uint32_t sub = 2U * (((width / 4U) + size - 1U) / size);
    sub = sub < 2U ? 2U : (sub > 8U ? 8U : sub);
    const float inv_count = 1.0F / (float)(sub * sub);
    for (uint32_t face = 0; face < FACES; face++) {
        float *base = cube + eng_ibl_cube_offset(size, levels, face, 0);
        for (uint32_t y = 0; y < size; y++) {
            for (uint32_t x = 0; x < size; x++) {
                float sum[3] = {0.0F, 0.0F, 0.0F};
                for (uint32_t sy = 0; sy < sub; sy++) {
                    for (uint32_t sx = 0; sx < sub; sx++) {
                        float d[3];
                        eng_ibl_cube_direction(
                            face, ((float)x + (((float)sx + 0.5F) / (float)sub)) / (float)size,
                            ((float)y + (((float)sy + 0.5F) / (float)sub)) / (float)size, d);
                        normalize3(d);
                        float c3[3];
                        equirect_sample(rgb, width, height, d, c3);
                        sum[0] += c3[0];
                        sum[1] += c3[1];
                        sum[2] += c3[2];
                    }
                }
                float *px = base + ((((size_t)y * size) + x) * 3U);
                px[0] = sum[0] * inv_count;
                px[1] = sum[1] * inv_count;
                px[2] = sum[2] * inv_count;
            }
        }
    }
}

void eng_ibl_build_mips(float *cube, uint32_t size, uint32_t levels)
{
    for (uint32_t face = 0; face < FACES; face++) {
        for (uint32_t l = 1; l < levels; l++) {
            const uint32_t n = level_size(size, l);
            const uint32_t pn = level_size(size, l - 1U);
            const float *src = cube + eng_ibl_cube_offset(size, levels, face, l - 1U);
            float *dst = cube + eng_ibl_cube_offset(size, levels, face, l);
            for (uint32_t y = 0; y < n; y++) {
                for (uint32_t x = 0; x < n; x++) {
                    for (int c = 0; c < 3; c++) {
                        float sum = 0.0F;
                        for (uint32_t dy = 0; dy < 2U; dy++) {
                            for (uint32_t dx = 0; dx < 2U; dx++) {
                                uint32_t sx = (2U * x) + dx;
                                uint32_t sy = (2U * y) + dy;
                                sx = sx < pn ? sx : pn - 1U;
                                sy = sy < pn ? sy : pn - 1U;
                                sum += src[((((size_t)sy * pn) + sx) * 3U) + (size_t)c];
                            }
                        }
                        dst[((((size_t)y * n) + x) * 3U) + (size_t)c] = sum * 0.25F;
                    }
                }
            }
        }
    }
}

/* Plus grand niveau de `source` de côté <= max_size. */
static uint32_t level_at_most(uint32_t size, uint32_t levels, uint32_t max_size)
{
    uint32_t l = 0;
    while (l + 1U < levels && level_size(size, l) > max_size) {
        l++;
    }
    return l;
}

/* irradiance/pi dans la direction `normal` (unitaire), par quadrature sur le niveau `level` de
 * `source` (de côté n). */
static void irradiance_at(const float *source, uint32_t source_size, uint32_t source_levels,
                          uint32_t level, const float normal[3], float out[3])
{
    const uint32_t n = level_size(source_size, level);
    double sum[3] = {0.0, 0.0, 0.0};
    for (uint32_t sf = 0; sf < FACES; sf++) {
        const float *base = source + eng_ibl_cube_offset(source_size, source_levels, sf, level);
        for (uint32_t sy = 0; sy < n; sy++) {
            for (uint32_t sx = 0; sx < n; sx++) {
                const float s = ((float)sx + 0.5F) / (float)n;
                const float t = ((float)sy + 0.5F) / (float)n;
                float w[3];
                eng_ibl_cube_direction(sf, s, t, w);
                const float a = (2.0F * s) - 1.0F;
                const float b = (2.0F * t) - 1.0F;
                const float len2 = (a * a) + (b * b) + 1.0F;
                /* angle solide du texel : aire (2/n)² / (1 + a² + b²)^(3/2) */
                const float solid = (4.0F / ((float)n * (float)n)) / (len2 * sqrtf(len2));
                normalize3(w);
                const float cosine = dot3(normal, w);
                if (cosine > 0.0F) {
                    const float *px = base + ((((size_t)sy * n) + sx) * 3U);
                    for (int c = 0; c < 3; c++) {
                        sum[c] += (double)px[c] * (double)cosine * (double)solid;
                    }
                }
            }
        }
    }
    for (int c = 0; c < 3; c++) {
        out[c] = (float)(sum[c] / (double)PI_F);
    }
}

void eng_ibl_irradiance(const float *source, uint32_t source_size, uint32_t source_levels,
                        float *out, uint32_t out_size)
{
    const uint32_t level = level_at_most(source_size, source_levels, 32U);
    for (uint32_t face = 0; face < FACES; face++) {
        float *base = out + eng_ibl_cube_offset(out_size, 1U, face, 0);
        for (uint32_t y = 0; y < out_size; y++) {
            for (uint32_t x = 0; x < out_size; x++) {
                float normal[3];
                eng_ibl_cube_direction(face, ((float)x + 0.5F) / (float)out_size,
                                       ((float)y + 0.5F) / (float)out_size, normal);
                normalize3(normal);
                irradiance_at(source, source_size, source_levels, level, normal,
                              base + ((((size_t)y * out_size) + x) * 3U));
            }
        }
    }
}

static float radical_inverse(uint32_t bits)
{
    bits = (bits << 16U) | (bits >> 16U);
    bits = ((bits & 0x55555555U) << 1U) | ((bits & 0xAAAAAAAAU) >> 1U);
    bits = ((bits & 0x33333333U) << 2U) | ((bits & 0xCCCCCCCCU) >> 2U);
    bits = ((bits & 0x0F0F0F0FU) << 4U) | ((bits & 0xF0F0F0F0U) >> 4U);
    bits = ((bits & 0x00FF00FFU) << 8U) | ((bits & 0xFF00FF00U) >> 8U);
    return (float)bits * 2.3283064365386963e-10F;
}

/* Demi-vecteur GGX échantillonné autour de +Z. */
static void ggx_half(uint32_t i, uint32_t count, float alpha, float h[3])
{
    const float u1 = ((float)i + 0.5F) / (float)count;
    const float u2 = radical_inverse(i);
    const float phi = 2.0F * PI_F * u2;
    const float cos_theta = sqrtf((1.0F - u1) / (1.0F + (((alpha * alpha) - 1.0F) * u1)));
    const float sin_theta = sqrtf(fmaxf(0.0F, 1.0F - (cos_theta * cos_theta)));
    h[0] = sin_theta * cosf(phi);
    h[1] = sin_theta * sinf(phi);
    h[2] = cos_theta;
}

void eng_ibl_prefilter(const float *source, uint32_t source_size, uint32_t source_levels,
                       float *out, uint32_t out_size, uint32_t levels, uint32_t samples)
{
    const float texel_solid = 4.0F * PI_F / (6.0F * (float)source_size * (float)source_size);
    for (uint32_t level = 0; level < levels; level++) {
        const float roughness = levels > 1U ? (float)level / (float)(levels - 1U) : 0.0F;
        const float alpha = roughness * roughness;
        const uint32_t n = level_size(out_size, level);
        for (uint32_t face = 0; face < FACES; face++) {
            float *base = out + eng_ibl_cube_offset(out_size, levels, face, level);
            for (uint32_t y = 0; y < n; y++) {
                for (uint32_t x = 0; x < n; x++) {
                    float nrm[3];
                    eng_ibl_cube_direction(face, ((float)x + 0.5F) / (float)n,
                                           ((float)y + 0.5F) / (float)n, nrm);
                    normalize3(nrm);
                    float *px = base + ((((size_t)y * n) + x) * 3U);
                    if (roughness <= 0.0F) {
                        eng_ibl_cube_sample(source, source_size, source_levels, nrm, 0.0F, px);
                        continue;
                    }
                    if (roughness >= 1.0F) {
                        /* alpha = 1 : le lobe GGX (N = V) est exactement le lobe en cosinus de
                         * l'irradiance, que la quadrature donne bien plus juste qu'un tirage de
                         * quelques centaines d'échantillons (soleil très intense : fuites). */
                        irradiance_at(source, source_size, source_levels,
                                      level_at_most(source_size, source_levels, 32U), nrm, px);
                        continue;
                    }
                    /* Base orthonormée (N = V = R). */
                    const float up[3] = {fabsf(nrm[2]) < 0.999F ? 0.0F : 1.0F, 0.0F,
                                         fabsf(nrm[2]) < 0.999F ? 1.0F : 0.0F};
                    float tx[3] = {(up[1] * nrm[2]) - (up[2] * nrm[1]),
                                   (up[2] * nrm[0]) - (up[0] * nrm[2]),
                                   (up[0] * nrm[1]) - (up[1] * nrm[0])};
                    normalize3(tx);
                    const float ty[3] = {(nrm[1] * tx[2]) - (nrm[2] * tx[1]),
                                         (nrm[2] * tx[0]) - (nrm[0] * tx[2]),
                                         (nrm[0] * tx[1]) - (nrm[1] * tx[0])};
                    double sum[3] = {0.0, 0.0, 0.0};
                    double weight = 0.0;
                    for (uint32_t i = 0; i < samples; i++) {
                        float h[3];
                        ggx_half(i, samples, alpha, h);
                        float hw[3];
                        float lw[3];
                        for (int c = 0; c < 3; c++) {
                            hw[c] = (tx[c] * h[0]) + (ty[c] * h[1]) + (nrm[c] * h[2]);
                        }
                        const float v_dot_h = dot3(nrm, hw);
                        for (int c = 0; c < 3; c++) {
                            lw[c] = (2.0F * v_dot_h * hw[c]) - nrm[c];
                        }
                        const float n_dot_l = dot3(nrm, lw);
                        if (n_dot_l <= 0.0F) {
                            continue;
                        }
                        /* pdf(L) = D × N·H / (4 V·H) avec N = V : D / 4. */
                        const float a2 = alpha * alpha;
                        const float d_den = (h[2] * h[2] * (a2 - 1.0F)) + 1.0F;
                        const float ndf = a2 / (PI_F * d_den * d_den);
                        const float pdf = ndf * h[2] / (4.0F * v_dot_h);
                        const float sample_solid = 1.0F / ((float)samples * pdf);
                        const float lod = fmaxf(0.0F, (0.5F * log2f(sample_solid / texel_solid)) +
                                                          ENG_IBL_LOD_BIAS);
                        float c3[3];
                        eng_ibl_cube_sample(source, source_size, source_levels, lw, lod, c3);
                        for (int c = 0; c < 3; c++) {
                            sum[c] += (double)c3[c] * (double)n_dot_l;
                        }
                        weight += (double)n_dot_l;
                    }
                    for (int c = 0; c < 3; c++) {
                        px[c] = weight > 0.0 ? (float)(sum[c] / weight) : 0.0F;
                    }
                }
            }
        }
    }
}

void eng_ibl_brdf_lut(float *out, uint32_t size, uint32_t samples)
{
    for (uint32_t j = 0; j < size; j++) {
        const float roughness = ((float)j + 0.5F) / (float)size;
        const float alpha = roughness * roughness;
        for (uint32_t i = 0; i < size; i++) {
            const float n_dot_v = ((float)i + 0.5F) / (float)size;
            const float v[3] = {sqrtf(1.0F - (n_dot_v * n_dot_v)), 0.0F, n_dot_v};
            double a_sum = 0.0;
            double b_sum = 0.0;
            for (uint32_t s = 0; s < samples; s++) {
                float h[3];
                ggx_half(s, samples, alpha, h);
                const float v_dot_h = dot3(v, h);
                const float l[3] = {(2.0F * v_dot_h * h[0]) - v[0], (2.0F * v_dot_h * h[1]) - v[1],
                                    (2.0F * v_dot_h * h[2]) - v[2]};
                const float n_dot_l = l[2];
                if (n_dot_l <= 0.0F || v_dot_h <= 0.0F) {
                    continue;
                }
                const float vis = eng_pbr_visibility_smith_ggx(n_dot_l, n_dot_v, alpha);
                /* Estimateur : f × N·L / pdf avec f = D Vis, pdf = D N·H / (4 V·H). */
                const float g_vis = 4.0F * vis * n_dot_l * v_dot_h / h[2];
                const float fc = powf(1.0F - v_dot_h, 5.0F);
                a_sum += (double)((1.0F - fc) * g_vis);
                b_sum += (double)(fc * g_vis);
            }
            float *px = out + ((((size_t)j * size) + i) * 2U);
            px[0] = (float)(a_sum / (double)samples);
            px[1] = (float)(b_sum / (double)samples);
        }
    }
}

void eng_ibl_brdf_lut_sample(const float *lut, uint32_t size, float n_dot_v, float roughness,
                             float *a, float *b)
{
    const float x = (fminf(fmaxf(n_dot_v, 0.0F), 1.0F) * (float)size) - 0.5F;
    const float y = (fminf(fmaxf(roughness, 0.0F), 1.0F) * (float)size) - 0.5F;
    const float fx = floorf(x);
    const float fy = floorf(y);
    const float wx = x - fx;
    const float wy = y - fy;
    const int32_t last = (int32_t)size - 1;
    int32_t x0 = (int32_t)fx;
    int32_t y0 = (int32_t)fy;
    int32_t x1 = x0 + 1;
    int32_t y1 = y0 + 1;
    x0 = x0 < 0 ? 0 : (x0 > last ? last : x0);
    x1 = x1 < 0 ? 0 : (x1 > last ? last : x1);
    y0 = y0 < 0 ? 0 : (y0 > last ? last : y0);
    y1 = y1 < 0 ? 0 : (y1 > last ? last : y1);
    float r[2];
    for (size_t c = 0; c < 2U; c++) {
        const float p00 = lut[((((size_t)y0 * size) + (size_t)x0) * 2U) + c];
        const float p10 = lut[((((size_t)y0 * size) + (size_t)x1) * 2U) + c];
        const float p01 = lut[((((size_t)y1 * size) + (size_t)x0) * 2U) + c];
        const float p11 = lut[((((size_t)y1 * size) + (size_t)x1) * 2U) + c];
        r[c] = ((p00 * (1.0F - wx)) + (p10 * wx)) * (1.0F - wy) +
               ((p01 * (1.0F - wx)) + (p11 * wx)) * wy;
    }
    *a = r[0];
    *b = r[1];
}

uint16_t eng_float_to_half(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    const uint32_t sign = (bits >> 16U) & 0x8000U;
    const uint32_t abs_bits = bits & 0x7FFFFFFFU;
    if (abs_bits >= 0x7F800000U) { /* inf, NaN */
        return (uint16_t)(sign | (abs_bits > 0x7F800000U ? 0x7E00U : 0x7C00U));
    }
    if (abs_bits >= 0x477FF000U) { /* >= 65520 : arrondit à l'infini */
        return (uint16_t)(sign | 0x7C00U);
    }
    if (abs_bits < 0x33000001U) { /* <= 2^-25 : arrondit à 0 */
        return (uint16_t)sign;
    }
    uint32_t exponent = abs_bits >> 23U;
    uint32_t mantissa = abs_bits & 0x7FFFFFU;
    if (exponent < 113U) { /* sous-normal en demi */
        mantissa |= 0x800000U;
        const uint32_t shift = 126U - exponent; /* 14..24 */
        const uint32_t half_m = mantissa >> shift;
        const uint32_t rem = mantissa & ((1U << shift) - 1U);
        const uint32_t halfway = 1U << (shift - 1U);
        uint32_t r = half_m;
        if (rem > halfway || (rem == halfway && (half_m & 1U) != 0U)) {
            r++;
        }
        return (uint16_t)(sign | r);
    }
    uint32_t r = ((exponent - 112U) << 10U) | (mantissa >> 13U);
    const uint32_t rem = mantissa & 0x1FFFU;
    if (rem > 0x1000U || (rem == 0x1000U && (r & 1U) != 0U)) {
        r++; /* la retenue peut monter l'exposant, voire atteindre l'infini : correct */
    }
    return (uint16_t)(sign | r);
}

float eng_half_to_float(uint16_t half)
{
    const uint32_t sign = ((uint32_t)half & 0x8000U) << 16U;
    const uint32_t exponent = ((uint32_t)half >> 10U) & 0x1FU;
    const uint32_t mantissa = (uint32_t)half & 0x3FFU;
    uint32_t bits;
    if (exponent == 0U) {
        if (mantissa == 0U) {
            bits = sign;
        } else {
            float f = (float)mantissa * (1.0F / 16777216.0F); /* mantissa × 2^-24 */
            memcpy(&bits, &f, sizeof bits);
            bits |= sign;
        }
    } else if (exponent == 31U) {
        bits = sign | 0x7F800000U | (mantissa << 13U);
    } else {
        bits = sign | ((exponent + 112U) << 23U) | (mantissa << 13U);
    }
    float out;
    memcpy(&out, &bits, sizeof out);
    return out;
}

void eng_ibl_pack_rgb_to_rgba16f(const float *rgb, size_t texels, uint16_t *out)
{
    for (size_t i = 0; i < texels; i++) {
        out[(i * 4U) + 0U] = eng_float_to_half(rgb[(i * 3U) + 0U]);
        out[(i * 4U) + 1U] = eng_float_to_half(rgb[(i * 3U) + 1U]);
        out[(i * 4U) + 2U] = eng_float_to_half(rgb[(i * 3U) + 2U]);
        out[(i * 4U) + 3U] = eng_float_to_half(1.0F);
    }
}

void eng_ibl_pack_rg_to_rg16f(const float *rg, size_t texels, uint16_t *out)
{
    for (size_t i = 0; i < texels * 2U; i++) {
        out[i] = eng_float_to_half(rg[i]);
    }
}

void eng_pbr_shade_ibl(const EngLighting *lighting, const EngIbl *ibl, const EngPbrSurface *surface,
                       const float normal[3], const float position[3], float out[3])
{
    EngLighting direct = *lighting;
    direct.ambient_intensity = 0.0F;
    eng_pbr_shade(&direct, surface, normal, position, out);

    float n[3] = {normal[0], normal[1], normal[2]};
    normalize3(n);
    float v[3] = {lighting->camera_position[0] - position[0],
                  lighting->camera_position[1] - position[1],
                  lighting->camera_position[2] - position[2]};
    normalize3(v);
    const float n_dot_v = fmaxf(dot3(n, v), 1e-4F);
    const float roughness = fmaxf(surface->roughness, ENG_ROUGHNESS_MIN);
    float r[3];
    for (int c = 0; c < 3; c++) {
        r[c] = (2.0F * dot3(n, v) * n[c]) - v[c];
    }
    float irr[3];
    eng_ibl_cube_sample(ibl->irradiance, ibl->irradiance_size, 1U, n, 0.0F, irr);
    float spec[3];
    eng_ibl_cube_sample(ibl->prefiltered, ibl->prefiltered_size, ibl->prefiltered_levels, r,
                        roughness * (float)(ibl->prefiltered_levels - 1U), spec);
    float a_term;
    float b_term;
    eng_ibl_brdf_lut_sample(ibl->brdf_lut, ENG_IBL_BRDF_LUT_SIZE, n_dot_v, roughness, &a_term,
                            &b_term);
    for (int c = 0; c < 3; c++) {
        const float f0 = 0.04F + ((surface->base_color[c] - 0.04F) * surface->metallic);
        const float c_diff = surface->base_color[c] * (1.0F - surface->metallic);
        out[c] += lighting->ambient_color[c] * lighting->ambient_intensity * surface->occlusion *
                  ((c_diff * irr[c]) + (spec[c] * ((f0 * a_term) + b_term)));
    }
}
