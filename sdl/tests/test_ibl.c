#include <SDL3/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/environment.h"
#include "engine/ibl.h"
#include "utest.h"

/* Éclairage basé image (M1-16), côté CPU. Références indépendantes du code testé : (1) formes
 * fermées (environnement uniforme : irradiance et pré-filtrée valent L ; L = 1 + y : irradiance
 * = 1 + 2/3 n_y) ; (2) un script numpy qui intègre par quadrature en (theta, phi) — une autre
 * méthode que l'importance sampling du code — la LUT BRDF et le coefficient du pré-filtrage de
 * L = 1 + y (tools/reference/ibl_ref.py) ; (3) la convention des faces d'un cube. */

#define PI_F 3.14159265358979323846F

static float *new_cube(uint32_t size, uint32_t levels)
{
    float *c = calloc(eng_ibl_cube_floats(size, levels), sizeof(float));
    if (c == NULL) {
        abort();
    }
    return c;
}

/* Remplit le niveau 0 avec f(direction unitaire) = 1 + y (gradient) ou une constante. */
static void fill_gradient(float *cube, uint32_t size, uint32_t levels, float constant, float slope)
{
    for (uint32_t face = 0; face < 6; face++) {
        float *base = cube + eng_ibl_cube_offset(size, levels, face, 0);
        for (uint32_t y = 0; y < size; y++) {
            for (uint32_t x = 0; x < size; x++) {
                float d[3];
                eng_ibl_cube_direction(face, ((float)x + 0.5F) / (float)size,
                                       ((float)y + 0.5F) / (float)size, d);
                const float len = sqrtf((d[0] * d[0]) + (d[1] * d[1]) + (d[2] * d[2]));
                for (int c = 0; c < 3; c++) {
                    base[((((size_t)y * size) + x) * 3U) + (size_t)c] =
                        constant + (slope * d[1] / len);
                }
            }
        }
    }
}

UTEST(ibl_half, roundtrip_all_finite_halves_is_exact)
{
    for (uint32_t h = 0; h < 65536U; h++) {
        if (((h >> 10U) & 0x1FU) == 0x1FU) {
            continue; /* inf, NaN */
        }
        const float f = eng_half_to_float((uint16_t)h);
        EXPECT_EQ((uint32_t)eng_float_to_half(f), h);
    }
}

UTEST(ibl_half, known_values)
{
    EXPECT_EQ((uint32_t)eng_float_to_half(1.0F), 0x3C00U);
    EXPECT_EQ((uint32_t)eng_float_to_half(-2.0F), 0xC000U);
    EXPECT_EQ((uint32_t)eng_float_to_half(65504.0F), 0x7BFFU);
    EXPECT_EQ((uint32_t)eng_float_to_half(65520.0F), 0x7C00U); /* arrondit à +inf */
    EXPECT_EQ((uint32_t)eng_float_to_half(1e-10F), 0x0000U);
    EXPECT_EQ((uint32_t)eng_float_to_half(5.9604645e-8F), 0x0001U); /* plus petit sous-normal */
    EXPECT_EQ((uint32_t)eng_float_to_half(1.0F + (1.0F / 2048.0F)), 0x3C00U); /* égalité -> pair */
    EXPECT_EQ((uint32_t)eng_float_to_half(1.0F + (3.0F / 2048.0F)), 0x3C02U);
    EXPECT_NEAR(eng_half_to_float(0x3555U), 0.33325195F, 1e-7F);
}

UTEST(ibl_half, ties_round_to_even_in_the_subnormal_range_and_nan_stays_nan)
{
    const float unit = 5.9604645e-8F; /* 2^-24 : un pas de sous-normal */
    EXPECT_EQ((uint32_t)eng_float_to_half(0.5F * unit), 0x0000U); /* égalité 0 | 1 -> pair (0) */
    EXPECT_EQ((uint32_t)eng_float_to_half(1.5F * unit), 0x0002U); /* 1 | 2 -> 2 */
    EXPECT_EQ((uint32_t)eng_float_to_half(2.5F * unit), 0x0002U); /* 2 | 3 -> 2 (pas 3) */
    EXPECT_EQ((uint32_t)eng_float_to_half(2.6F * unit),
              0x0003U); /* au-dessus : arrondi vers le haut */
    EXPECT_EQ((uint32_t)eng_float_to_half(-2.5F * unit), 0x8002U);
    const uint16_t nan = eng_float_to_half(NAN);
    EXPECT_EQ((uint32_t)(nan & 0x7C00U), 0x7C00U);
    EXPECT_TRUE((nan & 0x03FFU) != 0U); /* NaN, pas l'infini */
    EXPECT_EQ((uint32_t)eng_float_to_half(INFINITY), 0x7C00U);
}

UTEST(ibl_cube, face_directions_follow_opengl_convention)
{
    float d[3];
    /* centre de chaque face = son axe */
    const float axes[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (uint32_t f = 0; f < 6; f++) {
        eng_ibl_cube_direction(f, 0.5F, 0.5F, d);
        for (int c = 0; c < 3; c++) {
            EXPECT_NEAR(d[c], axes[f][c], 1e-6F);
        }
    }
    /* coin haut-gauche (s = t = 0) : +Z -> (-1, +1, 1), +X -> (1, 1, 1), +Y -> (-1, 1, -1) */
    eng_ibl_cube_direction(4, 0.0F, 0.0F, d);
    EXPECT_NEAR(d[0], -1.0F, 1e-6F);
    EXPECT_NEAR(d[1], 1.0F, 1e-6F);
    eng_ibl_cube_direction(0, 0.0F, 0.0F, d);
    EXPECT_NEAR(d[2], 1.0F, 1e-6F); /* +X, gauche = +Z (vu de l'intérieur) */
    EXPECT_NEAR(d[1], 1.0F, 1e-6F);
    eng_ibl_cube_direction(2, 0.0F, 0.0F, d);
    EXPECT_NEAR(d[0], -1.0F, 1e-6F);
    EXPECT_NEAR(d[2], -1.0F, 1e-6F); /* +Y, haut de l'image = -Z */
}

UTEST(ibl_cube, direction_and_face_st_are_inverse)
{
    for (uint32_t f = 0; f < 6; f++) {
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 5; j++) {
                const float s = 0.1F + (0.2F * (float)i);
                const float t = 0.1F + (0.2F * (float)j);
                float d[3];
                eng_ibl_cube_direction(f, s, t, d);
                float s2;
                float t2;
                EXPECT_EQ(eng_ibl_cube_face_st(d, &s2, &t2), f);
                EXPECT_NEAR(s2, s, 1e-5F);
                EXPECT_NEAR(t2, t, 1e-5F);
            }
        }
    }
}

UTEST(ibl_cube, layout_sizes)
{
    EXPECT_EQ(eng_ibl_full_levels(64U), 7U);
    EXPECT_EQ(eng_ibl_cube_floats(4U, 3U), 6U * (16U + 4U + 1U) * 3U);
    EXPECT_EQ(eng_ibl_cube_offset(4U, 3U, 0U, 1U), 16U * 3U);
    EXPECT_EQ(eng_ibl_cube_offset(4U, 3U, 1U, 0U), (16U + 4U + 1U) * 3U);
}

UTEST(ibl_irradiance, uniform_environment_gives_its_radiance)
{
    float *src = new_cube(32U, 6U);
    fill_gradient(src, 32U, 6U, 0.7F, 0.0F);
    eng_ibl_build_mips(src, 32U, 6U);
    float *irr = new_cube(8U, 1U);
    eng_ibl_irradiance(src, 32U, 6U, irr, 8U);
    for (size_t i = 0; i < eng_ibl_cube_floats(8U, 1U); i++) {
        EXPECT_NEAR(irr[i], 0.7F, 0.01F);
    }
    free(src);
    free(irr);
}

UTEST(ibl_irradiance, linear_gradient_matches_closed_form)
{
    /* L = 1 + y : E(n)/pi = 1 + (2/3) n_y. */
    float *src = new_cube(32U, 6U);
    fill_gradient(src, 32U, 6U, 1.0F, 1.0F);
    eng_ibl_build_mips(src, 32U, 6U);
    float *irr = new_cube(8U, 1U);
    eng_ibl_irradiance(src, 32U, 6U, irr, 8U);
    for (uint32_t face = 0; face < 6; face++) {
        for (uint32_t y = 0; y < 8U; y++) {
            for (uint32_t x = 0; x < 8U; x++) {
                float d[3];
                eng_ibl_cube_direction(face, ((float)x + 0.5F) / 8.0F, ((float)y + 0.5F) / 8.0F, d);
                const float len = sqrtf((d[0] * d[0]) + (d[1] * d[1]) + (d[2] * d[2]));
                const float expected = 1.0F + (2.0F / 3.0F * d[1] / len);
                EXPECT_NEAR(
                    irr[eng_ibl_cube_offset(8U, 1U, face, 0U) + ((((size_t)y * 8U) + x) * 3U)],
                    expected, 0.02F);
            }
        }
    }
    free(src);
    free(irr);
}

UTEST(ibl_prefilter, uniform_environment_stays_uniform_at_every_roughness)
{
    float *src = new_cube(32U, 6U);
    fill_gradient(src, 32U, 6U, 2.0F, 0.0F);
    eng_ibl_build_mips(src, 32U, 6U);
    float *pre = new_cube(16U, 5U);
    eng_ibl_prefilter(src, 32U, 6U, pre, 16U, 5U, 64U);
    for (size_t i = 0; i < eng_ibl_cube_floats(16U, 5U); i++) {
        EXPECT_NEAR(pre[i], 2.0F, 0.02F);
    }
    free(src);
    free(pre);
}

UTEST(ibl_prefilter, gradient_matches_numpy_quadrature_at_every_texel_center)
{
    /* Env 1 + y : par symétrie de révolution du lobe autour de N, la valeur filtrée vaut exactement
     * 1 + k(r) N_y pour TOUT N. k(r) vient de tools/reference/ibl_ref.py (quadrature en theta,
     * phi). */
    float *src = new_cube(64U, 7U);
    fill_gradient(src, 64U, 7U, 1.0F, 1.0F);
    eng_ibl_build_mips(src, 64U, 7U);
    float *pre = new_cube(16U, 5U);
    eng_ibl_prefilter(src, 64U, 7U, pre, 16U, 5U, 256U);
    const float k[5] = {1.0F, 0.97609F, 0.86740F, 0.74513F, 0.66667F};
    for (uint32_t level = 0; level < 5U; level++) {
        const uint32_t n = 16U >> level;
        for (uint32_t face = 0; face < 6U; face++) {
            for (uint32_t y = 0; y < n; y++) {
                for (uint32_t x = 0; x < n; x++) {
                    float d[3];
                    eng_ibl_cube_direction(face, ((float)x + 0.5F) / (float)n,
                                           ((float)y + 0.5F) / (float)n, d);
                    const float len = sqrtf((d[0] * d[0]) + (d[1] * d[1]) + (d[2] * d[2]));
                    const float got = pre[eng_ibl_cube_offset(16U, 5U, face, level) +
                                          ((((size_t)y * n) + x) * 3U)];
                    EXPECT_NEAR(got, 1.0F + (k[level] * d[1] / len), 0.03F);
                }
            }
        }
    }
    free(src);
    free(pre);
}

/* Source de test : ciel de radiance 1 et une tache de radiance 200 de 6 x 6 texels au centre de +X.
 */
static void fill_blob(float *cube, uint32_t size, uint32_t levels)
{
    fill_gradient(cube, size, levels, 1.0F, 0.0F);
    float *base = cube + eng_ibl_cube_offset(size, levels, 0, 0);
    for (uint32_t y = (size / 2U) - 3U; y < (size / 2U) + 3U; y++) {
        for (uint32_t x = (size / 2U) - 3U; x < (size / 2U) + 3U; x++) {
            for (int c = 0; c < 3; c++) {
                base[((((size_t)y * size) + x) * 3U) + (size_t)c] = 200.0F;
            }
        }
    }
}

/* Référence en force brute : somme sur CHAQUE texel de la source (angle solide exact) du lobe GGX
 * D(h) × (n . l), N = V. Aucun échantillonnage aléatoire ni aucun mip : indépendante du moteur. */
static double ggx_brute(const float *cube, uint32_t size, uint32_t levels, const float n[3],
                        float roughness)
{
    const double alpha = (double)roughness * (double)roughness;
    const double a2 = alpha * alpha;
    double num = 0.0;
    double den = 0.0;
    for (uint32_t face = 0; face < 6; face++) {
        const float *base = cube + eng_ibl_cube_offset(size, levels, face, 0);
        for (uint32_t y = 0; y < size; y++) {
            for (uint32_t x = 0; x < size; x++) {
                float l[3];
                eng_ibl_cube_direction(face, ((float)x + 0.5F) / (float)size,
                                       ((float)y + 0.5F) / (float)size, l);
                const double len =
                    sqrt(((double)l[0] * l[0]) + ((double)l[1] * l[1]) + ((double)l[2] * l[2]));
                const double a = (2.0 * (((double)x + 0.5) / (double)size)) - 1.0;
                const double b = (2.0 * (((double)y + 0.5) / (double)size)) - 1.0;
                const double len2 = (a * a) + (b * b) + 1.0;
                const double solid = (4.0 / ((double)size * (double)size)) / (len2 * sqrt(len2));
                double ld[3] = {l[0] / len, l[1] / len, l[2] / len};
                const double nl = (ld[0] * n[0]) + (ld[1] * n[1]) + (ld[2] * n[2]);
                if (nl <= 0.0) {
                    continue;
                }
                double h[3] = {ld[0] + n[0], ld[1] + n[1], ld[2] + n[2]};
                const double hl = sqrt((h[0] * h[0]) + (h[1] * h[1]) + (h[2] * h[2]));
                const double nh = ((h[0] * n[0]) + (h[1] * n[1]) + (h[2] * n[2])) / hl;
                const double t = (nh * nh * (a2 - 1.0)) + 1.0;
                const double weight = solid * (a2 / (PI_F * t * t)) * nl;
                num += weight * (double)base[((((size_t)y * size) + x) * 3U)];
                den += weight;
            }
        }
    }
    return num / den;
}

UTEST(ibl_prefilter, a_bright_spot_matches_a_brute_force_integral)
{
    /* Contrairement au gradient 1 + y (que tout filtrage cohérent reproduit, même avec un mauvais
     * choix de niveau de mip ou une mauvaise densité d'échantillonnage), une tache brillante rend
     * visible la densité (pdf) et le niveau de mip utilisés par l'importance sampling. */
    float *src = new_cube(32U, 6U);
    fill_blob(src, 32U, 6U);
    eng_ibl_build_mips(src, 32U, 6U);
    float *pre = new_cube(32U, 4U); /* r = 0, 1/3, 2/3, 1 */
    eng_ibl_prefilter(src, 32U, 6U, pre, 32U, 4U, 512U);
    const float dirs[4][3] = {
        {1, 0, 0}, {0.8F, 0.6F, 0.0F}, {0.6F, 0.0F, 0.8F}, {0.0F, 1.0F, 0.0F}};
    const float roughness[2] = {1.0F / 3.0F, 2.0F / 3.0F};
    for (int d = 0; d < 4; d++) {
        for (int k = 0; k < 2; k++) {
            float got[3];
            eng_ibl_cube_sample(pre, 32U, 4U, dirs[d], (float)(k + 1), got);
            const double want = ggx_brute(src, 32U, 6U, dirs[d], roughness[k]);
            EXPECT_NEAR(got[0] / want, 1.0, 0.12);
        }
    }
    /* r = 0 : copie exacte de la source (la chaîne a la même taille) */
    for (uint32_t face = 0; face < 6U; face++) {
        const float *a = pre + eng_ibl_cube_offset(32U, 4U, face, 0U);
        const float *b = src + eng_ibl_cube_offset(32U, 6U, face, 0U);
        for (size_t i = 0; i < (size_t)32U * 32U * 3U; i++) {
            EXPECT_NEAR(a[i], b[i], 1e-6F);
        }
    }
    free(src);
    free(pre);
}

UTEST(ibl_brdf_lut, matches_numpy_quadrature_at_texel_centers)
{
    /* Texel (i, j) : colonne i = N.V = (i + 1/2)/64, ligne j = rugosité (j + 1/2)/64. Valeurs de
     * tools/reference/ibl_ref.py (quadrature en theta_h, phi_h) EXACTEMENT à ces centres : une
     * lecture interpolée entre deux texels s'écarterait de 0,01 (la fonction n'est pas linéaire),
     * ce qui empêcherait de repérer un décalage d'un demi-texel. */
    float lut[ENG_IBL_BRDF_LUT_SIZE * ENG_IBL_BRDF_LUT_SIZE * 2U];
    eng_ibl_brdf_lut(lut, ENG_IBL_BRDF_LUT_SIZE, 1024U);
    const struct {
        uint32_t i;
        uint32_t j;
        float a;
        float b;
        float tolerance;
    } cases[5] = {{31, 31, 0.83775F, 0.02409F, 0.004F},
                  {57, 12, 0.99824F, 0.00003F, 0.004F},
                  {12, 50, 0.72691F, 0.03035F, 0.004F},
                  {48, 63, 0.36911F, 0.00062F, 0.004F},
                  /* N.V = 0,1 : fonction très pentue (dB/d(N.V) ~ 5), le Monte-Carlo du moteur et
                   * la grille de numpy s'écartent de 0,011 sur A ; un décalage d'un demi-texel en
                   * donnerait 0,036 sur B */
                  {6, 19, 0.56998F, 0.31392F, 0.015F}};
    /* Les cas extrêmes (r < 0,01 ou N.V < 0,01) sont exclus : la grille de numpy n'y résout plus le
     * lobe (elle donne A = 0,21 pour un miroir), le moteur y vaut ~ 1. */
    for (int c = 0; c < 5; c++) {
        const size_t at = (((size_t)cases[c].j * ENG_IBL_BRDF_LUT_SIZE) + cases[c].i) * 2U;
        EXPECT_NEAR(lut[at], cases[c].a, cases[c].tolerance);
        EXPECT_NEAR(lut[at + 1U], cases[c].b, cases[c].tolerance);
        float a;
        float b;
        eng_ibl_brdf_lut_sample(lut, ENG_IBL_BRDF_LUT_SIZE, ((float)cases[c].i + 0.5F) / 64.0F,
                                ((float)cases[c].j + 0.5F) / 64.0F, &a, &b);
        EXPECT_NEAR(a, lut[at], 1e-6F);
        EXPECT_NEAR(b, lut[at + 1U], 1e-6F);
    }
}

UTEST(ibl_shade, uniform_environment_matches_the_brdf_integral)
{
    /* Environnement uniforme de radiance 1, regard de face : couleur = c_diff + F0 × A + B avec
     * (A, B) intégrés par numpy (ibl_ref.py : A = 0,9906 à r = 0,3 ; 0,8245 à r = 0,6 ; B ~ 0).
     * NB : l'approximation de Karis de M1-15 (G de Schlick-Smith) s'écarte de cette intégrale
     * jusqu'à 15 % à r = 0,6 : le test ne s'y compare donc plus. Soleil éteint. */
    float *src = new_cube(16U, 5U);
    fill_gradient(src, 16U, 5U, 1.0F, 0.0F);
    eng_ibl_build_mips(src, 16U, 5U);
    float *irr = new_cube(8U, 1U);
    eng_ibl_irradiance(src, 16U, 5U, irr, 8U);
    float *pre = new_cube(16U, 5U);
    fill_gradient(pre, 16U, 5U, 1.0F, 0.0F);
    eng_ibl_build_mips(pre, 16U, 5U);
    float lut[ENG_IBL_BRDF_LUT_SIZE * ENG_IBL_BRDF_LUT_SIZE * 2U];
    eng_ibl_brdf_lut(lut, ENG_IBL_BRDF_LUT_SIZE, 512U);
    const EngIbl ibl = {irr, 8U, pre, 16U, 5U, lut};

    EngLighting light;
    eng_lighting_default(&light);
    light.sun_intensity = 0.0F;
    light.ambient_intensity = 1.0F;
    light.camera_position[2] = 5.0F;
    const float normal[3] = {0.0F, 0.0F, 1.0F};
    const float position[3] = {0.0F, 0.0F, 0.0F};
    for (int variant = 0; variant < 3; variant++) {
        EngPbrSurface s;
        eng_pbr_surface_default(&s);
        s.base_color[0] = 0.8F;
        s.base_color[1] = 0.5F;
        s.base_color[2] = 0.2F;
        s.metallic = variant == 0 ? 0.0F : (variant == 1 ? 1.0F : 0.5F);
        s.roughness = variant == 1 ? 0.3F : 0.6F;
        float got[3];
        eng_pbr_shade_ibl(&light, &ibl, &s, normal, position, got);
        const float spec_a = variant == 1 ? 0.9906F : 0.8245F;
        for (int c = 0; c < 3; c++) {
            const float f0 = 0.04F + ((s.base_color[c] - 0.04F) * s.metallic);
            const float c_diff = s.base_color[c] * (1.0F - s.metallic);
            EXPECT_NEAR(got[c], c_diff + (f0 * spec_a), 0.02F);
        }
    }
    free(src);
    free(irr);
    free(pre);
}

UTEST(ibl_shade, sky_orientation_a_mirror_sphere_reflects_up_from_above)
{
    /* Ciel bleu en haut, sol sombre : un métal lisse regardé d'en haut (normale +Y) reflète le
     * ciel, regardé d'en bas (normale -Y) reflète le sol. */
    EngSkyDesc sky;
    eng_ibl_sky_default(&sky);
    sky.sun_radiance[0] = sky.sun_radiance[1] = sky.sun_radiance[2] = 0.0F;
    float *src = new_cube(32U, 6U);
    eng_ibl_make_sky(src, 32U, 6U, &sky);
    eng_ibl_build_mips(src, 32U, 6U);
    float *irr = new_cube(8U, 1U);
    eng_ibl_irradiance(src, 32U, 6U, irr, 8U);
    float *pre = new_cube(16U, 5U);
    eng_ibl_prefilter(src, 32U, 6U, pre, 16U, 5U, 64U);
    float lut[ENG_IBL_BRDF_LUT_SIZE * ENG_IBL_BRDF_LUT_SIZE * 2U];
    eng_ibl_brdf_lut(lut, ENG_IBL_BRDF_LUT_SIZE, 256U);
    const EngIbl ibl = {irr, 8U, pre, 16U, 5U, lut};
    EngLighting light;
    eng_lighting_default(&light);
    light.sun_intensity = 0.0F;
    light.ambient_intensity = 1.0F;
    EngPbrSurface s;
    eng_pbr_surface_default(&s);
    s.metallic = 1.0F;
    s.roughness = 0.05F;
    const float position[3] = {0.0F, 0.0F, 0.0F};
    float up[3];
    float down[3];
    light.camera_position[0] = 0.0F;
    light.camera_position[1] = 5.0F;
    light.camera_position[2] = 0.0F;
    const float n_up[3] = {0.0F, 1.0F, 0.0F};
    eng_pbr_shade_ibl(&light, &ibl, &s, n_up, position, up);
    light.camera_position[1] = -5.0F;
    const float n_down[3] = {0.0F, -1.0F, 0.0F};
    eng_pbr_shade_ibl(&light, &ibl, &s, n_down, position, down);
    EXPECT_TRUE(up[2] > up[0]);          /* ciel : bleu dominant */
    EXPECT_TRUE(up[2] > 3.0F * down[2]); /* le ciel est nettement plus clair que le sol */
    EXPECT_NEAR(down[0], sky.ground[0], 0.05F);
    free(src);
    free(irr);
    free(pre);
}

/* Énergie d'un cube : somme des radiances pondérées par l'angle solide de chaque texel du niveau 0
 * (4 / n² / (1 + a² + b²)^(3/2) pour un texel de coordonnées centrées (a, b)). */
static double cube_energy(const float *cube, uint32_t size, uint32_t levels, int channel)
{
    double total = 0.0;
    for (uint32_t face = 0; face < 6; face++) {
        const float *base = cube + eng_ibl_cube_offset(size, levels, face, 0);
        for (uint32_t y = 0; y < size; y++) {
            for (uint32_t x = 0; x < size; x++) {
                const double a = (2.0 * (((double)x + 0.5) / (double)size)) - 1.0;
                const double b = (2.0 * (((double)y + 0.5) / (double)size)) - 1.0;
                const double len2 = (a * a) + (b * b) + 1.0;
                total += (double)base[((((size_t)y * size) + x) * 3U) + (size_t)channel] *
                         (4.0 / ((double)size * (double)size)) / (len2 * sqrt(len2));
            }
        }
    }
    return total;
}

/* Énergie d'une image équirectangulaire : somme L cos(latitude) dtheta dphi. */
static double equirect_energy(const float *rgb, uint32_t width, uint32_t height, int channel)
{
    double total = 0.0;
    for (uint32_t y = 0; y < height; y++) {
        const double latitude = (((double)y + 0.5) / (double)height - 0.5) * PI_F;
        for (uint32_t x = 0; x < width; x++) {
            total += (double)rgb[((((size_t)y * width) + x) * 3U) + (size_t)channel] *
                     cos(latitude) * (PI_F / (double)height) * (2.0 * PI_F / (double)width);
        }
    }
    return total;
}

UTEST(ibl_equirect, a_small_very_bright_source_keeps_its_energy)
{
    /* Un « soleil » de 5 x 5 pixels de radiance 5000 sur un ciel de 1, à 40° de latitude, puis
     * près du pôle (pixels plus étroits que les texels du cube) : avec UN point par texel du cube,
     * l'énergie variait de plusieurs dizaines de % (mesuré sur une image HDR réelle, où cela
     * donnait un éclairage de 33 % trop fort). */
    const uint32_t width = 512U;
    const uint32_t height = 256U;
    float *rgb = malloc((size_t)width * height * 3U * sizeof(float));
    ASSERT_TRUE(rgb != NULL);
    const uint32_t rows[2] = {(uint32_t)((float)height * (0.5F - (40.0F / 180.0F))), 10U};
    for (int variant = 0; variant < 2; variant++) {
        for (size_t i = 0; i < (size_t)width * height * 3U; i++) {
            rgb[i] = 1.0F;
        }
        for (uint32_t y = rows[variant]; y < rows[variant] + 5U; y++) {
            for (uint32_t x = 100; x < 105U; x++) {
                for (int c = 0; c < 3; c++) {
                    rgb[((((size_t)y * width) + x) * 3U) + (size_t)c] = 5000.0F;
                }
            }
        }
        float *cube = new_cube(64U, 7U);
        eng_ibl_from_equirect(cube, 64U, 7U, rgb, width, height);
        const double expected = equirect_energy(rgb, width, height, 0);
        const double got = cube_energy(cube, 64U, 7U, 0);
        EXPECT_NEAR(got / expected, 1.0, 0.05);
        free(cube);
    }
    free(rgb);
}

UTEST(ibl_equirect, orientation_matches_the_threejs_convention)
{
    /* u = atan2(z, x) / 2pi + 0,5 ; v = asin(y) / pi + 0,5 ; première ligne = haut (v = 1).
     * Un pixel brillant à (u, v) = (0,75 ; 0,5) est dans la direction +Z ; à (0,5 ; 0,5) : +X ;
     * à v près de 1 (première ligne) : +Y. UNE marque par image : avec plusieurs, un échange +X /
     * +Z (atan2(x, z) au lieu de atan2(z, x)) passerait inaperçu. Seule la direction marquée est
     * claire ; les cinq autres axes restent noirs. */
    const uint32_t width = 128U;
    const uint32_t height = 64U;
    struct {
        uint32_t x;
        uint32_t y;
    } marks[3] = {
        {(uint32_t)(0.75F * (float)width), height / 2U}, {width / 2U, height / 2U}, {10U, 0U}};
    const float axes[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    const int expected_axis[3] = {4, 0, 2}; /* +Z, +X, +Y */
    for (int m = 0; m < 3; m++) {
        float *rgb = calloc((size_t)width * height * 3U, sizeof(float));
        ASSERT_TRUE(rgb != NULL);
        for (uint32_t dy = 0; dy < 2U; dy++) {
            for (uint32_t dx = 0; dx < 2U; dx++) {
                const uint32_t y = marks[m].y > 0U && dy == 0U ? marks[m].y - 1U : marks[m].y + dy;
                rgb[((((size_t)y * width) + marks[m].x + dx) * 3U) + 0U] = 100.0F;
            }
        }
        float *cube = new_cube(32U, 6U);
        eng_ibl_from_equirect(cube, 32U, 6U, rgb, width, height);
        eng_ibl_build_mips(cube, 32U, 6U);
        for (int axis = 0; axis < 6; axis++) {
            float v[3];
            eng_ibl_cube_sample(cube, 32U, 6U, axes[axis], 1.0F, v);
            if (axis == expected_axis[m]) {
                EXPECT_GT(v[0], 1.0F);
            } else {
                EXPECT_NEAR(v[0], 0.0F, 1e-3F);
            }
        }
        free(cube);
        free(rgb);
    }
}

UTEST(ibl_equirect, the_seam_wraps_around_horizontally)
{
    /* Une colonne brillante tout à droite de l'image (x = width - 1, valeur 100). La couture est la
     * direction -X ; la colonne est centrée à u = 127,5/128, soit 0,0245 rad AVANT la couture (côté
     * +Z). Le bilinéaire, de part et d'autre, doit continuer à travers la couture :
     *  - côté +Z, à 0,02 rad : fx = 127,09, poids de la colonne 0,91 ;
     *  - côté -Z, à 0,02 rad : fx = -0,09, le pixel voisin est x = width - 1 (bouclage), poids
     * 0,09. Avec un clamp au lieu du bouclage, le second vaut 0 (et le premier 1) ; cube de 256 :
     * les texels sont assez fins pour distinguer ces deux positions. */
    const uint32_t width = 128U;
    const uint32_t height = 64U;
    float *rgb = calloc((size_t)width * height * 3U, sizeof(float));
    ASSERT_TRUE(rgb != NULL);
    for (uint32_t y = 0; y < height; y++) {
        rgb[((((size_t)y * width) + (width - 1U)) * 3U) + 0U] = 100.0F;
    }
    float *cube = new_cube(256U, 1U);
    eng_ibl_from_equirect(cube, 256U, 1U, rgb, width, height);
    float before[3];
    float after[3];
    const float plus_z[3] = {-1.0F, 0.0F, 0.02F};
    const float minus_z[3] = {-1.0F, 0.0F, -0.02F};
    eng_ibl_cube_sample(cube, 256U, 1U, plus_z, 0.0F, before);
    eng_ibl_cube_sample(cube, 256U, 1U, minus_z, 0.0F, after);
    EXPECT_NEAR(before[0], 91.0F, 4.0F);
    EXPECT_NEAR(after[0], 9.0F, 4.0F);
    free(cube);
    free(rgb);
}

UTEST(ibl_prefilter, the_roughest_level_is_the_irradiance)
{
    /* alpha = 1 : le lobe GGX (N = V) est le lobe en cosinus. Sur L = 1 + y, 1 + 2/3 n_y. */
    float *src = new_cube(32U, 6U);
    fill_gradient(src, 32U, 6U, 1.0F, 1.0F);
    eng_ibl_build_mips(src, 32U, 6U);
    float *pre = new_cube(16U, 2U);
    eng_ibl_prefilter(src, 32U, 6U, pre, 16U, 2U,
                      4U); /* 4 échantillons : seule la quadrature suffit */
    const float up[3] = {0.0F, 1.0F, 0.0F};
    float out[3];
    eng_ibl_cube_sample(pre, 16U, 2U, up, 1.0F, out);
    EXPECT_NEAR(out[0], 1.0F + (2.0F / 3.0F), 0.03F);
    free(src);
    free(pre);
}

UTEST(ibl_brdf_lut, is_bounded_and_monotone_in_roughness_for_a_white_metal)
{
    float lut[ENG_IBL_BRDF_LUT_SIZE * ENG_IBL_BRDF_LUT_SIZE * 2U];
    eng_ibl_brdf_lut(lut, ENG_IBL_BRDF_LUT_SIZE, 256U);
    for (uint32_t i = 0; i < ENG_IBL_BRDF_LUT_SIZE; i++) {
        float previous = 2.0F;
        for (uint32_t j = 0; j < ENG_IBL_BRDF_LUT_SIZE; j++) {
            const float a = lut[((((size_t)j * ENG_IBL_BRDF_LUT_SIZE) + i) * 2U) + 0U];
            const float b = lut[((((size_t)j * ENG_IBL_BRDF_LUT_SIZE) + i) * 2U) + 1U];
            EXPECT_TRUE(a >= 0.0F && b >= 0.0F && (a + b) <= 1.05F);
            /* A + B (réflectance d'un métal blanc) diminue quand la rugosité augmente */
            EXPECT_TRUE((a + b) <= previous + 0.03F);
            previous = a + b;
        }
    }
}

UTEST(ibl_brdf_lut, sample_is_exact_at_texel_centers_and_clamps_outside)
{
    float lut[4 * 4 * 2];
    for (int i = 0; i < 32; i++) {
        lut[i] = (float)i;
    }
    float a;
    float b;
    eng_ibl_brdf_lut_sample(lut, 4U, 0.125F, 0.375F, &a, &b); /* colonne 0, ligne 1 */
    EXPECT_NEAR(a, 8.0F, 1e-4F);
    EXPECT_NEAR(b, 9.0F, 1e-4F);
    eng_ibl_brdf_lut_sample(lut, 4U, 0.875F, 0.875F, &a, &b); /* colonne 3, ligne 3 */
    EXPECT_NEAR(a, 30.0F, 1e-4F);
    eng_ibl_brdf_lut_sample(lut, 4U, 5.0F, -1.0F, &a, &b); /* bornes : colonne 3, ligne 0 */
    EXPECT_NEAR(a, 6.0F, 1e-4F);
    eng_ibl_brdf_lut_sample(lut, 4U, 0.25F, 0.125F, &a, &b); /* entre les colonnes 0 et 1 */
    EXPECT_NEAR(a, 1.0F, 1e-4F);
}

UTEST(ibl_cube, sample_interpolates_between_levels_and_clamps_lod)
{
    float *cube = new_cube(4U, 3U);
    for (uint32_t face = 0; face < 6; face++) {
        for (uint32_t level = 0; level < 3U; level++) {
            const uint32_t n = 4U >> level;
            float *base = cube + eng_ibl_cube_offset(4U, 3U, face, level);
            for (uint32_t i = 0; i < n * n * 3U; i++) {
                base[i] = (float)level * 10.0F; /* niveau 0 : 0, niveau 1 : 10, niveau 2 : 20 */
            }
        }
    }
    const float d[3] = {0.3F, 0.5F, 0.8F};
    float out[3];
    eng_ibl_cube_sample(cube, 4U, 3U, d, 0.5F, out);
    EXPECT_NEAR(out[0], 5.0F, 1e-4F);
    eng_ibl_cube_sample(cube, 4U, 3U, d, 1.75F, out);
    EXPECT_NEAR(out[1], 17.5F, 1e-4F);
    eng_ibl_cube_sample(cube, 4U, 3U, d, 99.0F, out); /* borné au dernier niveau */
    EXPECT_NEAR(out[2], 20.0F, 1e-4F);
    eng_ibl_cube_sample(cube, 4U, 3U, d, -3.0F, out); /* borné au niveau 0 */
    EXPECT_NEAR(out[0], 0.0F, 1e-4F);
    free(cube);
}

UTEST(ibl_pack, rgb_gets_alpha_one_and_rg_is_converted)
{
    const float rgb[6] = {0.5F, 2.0F, -1.0F, 1.0F, 0.0F, 65504.0F};
    uint16_t out[8];
    eng_ibl_pack_rgb_to_rgba16f(rgb, 2U, out);
    EXPECT_EQ((uint32_t)out[0], 0x3800U);
    EXPECT_EQ((uint32_t)out[1], 0x4000U);
    EXPECT_EQ((uint32_t)out[2], 0xBC00U);
    EXPECT_EQ((uint32_t)out[3], 0x3C00U); /* alpha = 1 */
    EXPECT_EQ((uint32_t)out[7], 0x3C00U);
    EXPECT_EQ((uint32_t)out[6], 0x7BFFU);
    const float rg[4] = {0.25F, 4.0F, 8.0F, 0.0F};
    uint16_t two[4];
    eng_ibl_pack_rg_to_rg16f(rg, 2U, two);
    EXPECT_EQ((uint32_t)two[0], 0x3400U);
    EXPECT_EQ((uint32_t)two[1], 0x4400U);
    EXPECT_EQ((uint32_t)two[2], 0x4800U);
    EXPECT_EQ((uint32_t)two[3], 0x0000U);
}

UTEST(ibl_sky, has_a_sun_disk_a_gradient_and_a_ground)
{
    EngSkyDesc sky = {.zenith = {0, 0, 1},
                      .horizon = {0.5F, 0.5F, 0.5F},
                      .ground = {0.2F, 0.1F, 0},
                      .sun_direction = {1, 0, 0},
                      .sun_radiance = {50, 40, 30},
                      .sun_angular_radius_deg = 10.0F};
    float *cube = new_cube(32U, 6U);
    eng_ibl_make_sky(cube, 32U, 6U, &sky);
    const float sun[3] = {1, 0, 0};
    const float up[3] = {0.1F, 1, 0.1F};
    const float down[3] = {0, -1, 0};
    const float far_from_sun[3] = {-1, 0.3F, 0};
    float v[3];
    eng_ibl_cube_sample(cube, 32U, 6U, sun, 0.0F, v);
    EXPECT_NEAR(v[0], 50.0F, 1.0F); /* le disque */
    eng_ibl_cube_sample(cube, 32U, 6U, up, 0.0F, v);
    EXPECT_GT(v[2], 0.8F); /* zénith bleu */
    EXPECT_NEAR(v[0], 0.0F, 0.25F);
    /* à 14,5° d'élévation (y = 0,25) : horizon + (zénith - horizon) × sqrt(0,25) = mi-chemin */
    const float mid[3] = {-0.9682F, 0.25F, 0.0F};
    eng_ibl_cube_sample(cube, 32U, 6U, mid, 0.0F, v);
    EXPECT_NEAR(v[2], 0.75F, 0.03F);
    EXPECT_NEAR(v[0], 0.25F, 0.03F);
    eng_ibl_cube_sample(cube, 32U, 6U, down, 0.0F, v);
    EXPECT_NEAR(v[0], 0.2F, 1e-3F); /* sol uni */
    EXPECT_NEAR(v[1], 0.1F, 1e-3F);
    eng_ibl_cube_sample(cube, 32U, 6U, far_from_sun, 0.0F, v);
    EXPECT_TRUE(v[0] < 1.0F); /* pas de soleil ailleurs */
    free(cube);
}

/* Hors validation : sans OW_ENV (un .hdr équirectangulaire), ne fait rien. Sinon écrit sur la
 * sortie standard la radiance filtrée par le moteur dans quelques directions, pour la comparer à
 * une intégration par force brute (tools/reference/ibl_brute.py) sur la VRAIE image. */
UTEST(ibl_dump, filtered_values_for_a_real_environment)
{
    const char *path = getenv("OW_ENV");
    if (path == NULL) {
        return;
    }
    float *hdr = NULL;
    uint32_t hw;
    uint32_t hh;
    ASSERT_TRUE(eng_environment_load_hdr(path, &hdr, &hw, &hh));
    const char *sky_text =
        getenv("OW_SKY"); /* taille du cube source, 256 par défaut (comme le moteur) */
    const uint32_t size = sky_text != NULL ? (uint32_t)atoi(sky_text) : 256U;
    const uint32_t levels = eng_ibl_full_levels(size);
    const uint64_t started = SDL_GetTicks();
    float *src = new_cube(size, levels);
    eng_ibl_from_equirect(src, size, levels, hdr, hw, hh);
    eng_ibl_build_mips(src, size, levels);
    float *irr = new_cube(32U, 1U);
    eng_ibl_irradiance(src, size, levels, irr, 32U);
    float *pre = new_cube(128U, 4U);
    const char *samples_text = getenv("OW_SAMPLES");
    eng_ibl_prefilter(src, size, levels, pre, 128U, 4U,
                      samples_text != NULL ? (uint32_t)atoi(samples_text) : 512U);
    printf("TIME_MS %llu\n", (unsigned long long)(SDL_GetTicks() - started));
    const char *list = getenv("OW_DIRS"); /* « x,y,z;x,y,z;... » */
    ASSERT_TRUE(list != NULL);
    const char *cursor = list;
    while (*cursor != '\0') {
        float d[3];
        int used = 0;
        if (sscanf(cursor, "%f,%f,%f%n", &d[0], &d[1], &d[2], &used) != 3) {
            break;
        }
        cursor += used;
        if (*cursor == ';') {
            cursor++;
        }
        float v[3];
        printf("DIR %g %g %g\n", (double)d[0], (double)d[1], (double)d[2]);
        eng_ibl_cube_sample(src, size, levels, d, 0.0F, v);
        printf("  src %g %g %g\n", (double)v[0], (double)v[1], (double)v[2]);
        eng_ibl_cube_sample(irr, 32U, 1U, d, 0.0F, v);
        printf("  irr %g %g %g\n", (double)v[0], (double)v[1], (double)v[2]);
        for (uint32_t l = 0; l < 4U; l++) {
            eng_ibl_cube_sample(pre, 128U, 4U, d, (float)l, v);
            printf("  pre%u %g %g %g\n", l, (double)v[0], (double)v[1], (double)v[2]);
        }
    }
    free(src);
    free(irr);
    free(pre);
    eng_environment_free_hdr(hdr);
}
