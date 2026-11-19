#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/noise.h"
#include "engine/splatmap.h"
#include "engine/terrain_texture.h"
#include "utest.h"

/* Carte de mélange du terrain (M3-03) et textures de couche générées. Les champs de hauteurs sont
 * fabriqués à la main avec altitude_min = 0 et altitude_max = 65535 : l'altitude en mètres est la
 * valeur 16 bits, ce qui donne des pentes et des altitudes relatives exactes. */

static EngHeightmap make_map(const uint16_t *samples, uint32_t size, float spacing)
{
    return (EngHeightmap){.size = size,
                          .spacing = spacing,
                          .altitude_min = 0.0F,
                          .altitude_max = 65535.0F,
                          .samples = samples};
}

static float sum4(const float w[4])
{
    return w[0] + w[1] + w[2] + w[3];
}

UTEST(eng_splat, defaults_are_valid_and_every_invalid_parameter_is_refused)
{
    EngSplatParams p = eng_splat_default_params();
    EXPECT_TRUE(eng_splat_params_valid(&p));
    EXPECT_FALSE(eng_splat_params_valid(NULL));
    const EngSplatParams good = p;
    p.rock_slope_start_deg = NAN;
    EXPECT_FALSE(eng_splat_params_valid(&p));
    p = good;
    p.snow_full = INFINITY;
    EXPECT_FALSE(eng_splat_params_valid(&p));
    p = good;
    p.rock_slope_start_deg = -1.0F;
    EXPECT_FALSE(eng_splat_params_valid(&p));
    p = good;
    p.rock_slope_full_deg = p.rock_slope_start_deg; /* début == plein : refusé */
    EXPECT_FALSE(eng_splat_params_valid(&p));
    p = good;
    p.rock_slope_full_deg = 91.0F;
    EXPECT_FALSE(eng_splat_params_valid(&p));
    p.rock_slope_full_deg = 90.0F; /* 90 : accepté */
    EXPECT_TRUE(eng_splat_params_valid(&p));
    p = good;
    p.snow_start = -0.1F;
    EXPECT_FALSE(eng_splat_params_valid(&p));
    p = good;
    p.snow_start = 0.9F;
    p.snow_full = 0.8F;
    EXPECT_FALSE(eng_splat_params_valid(&p));
    p.snow_full = 0.9F; /* début == plein : accepté (marche) */
    EXPECT_TRUE(eng_splat_params_valid(&p));
    p = good;
    p.snow_full = 1.1F;
    EXPECT_FALSE(eng_splat_params_valid(&p));
    p = good;
    p.dirt_amount = -0.1F;
    EXPECT_FALSE(eng_splat_params_valid(&p));
    p.dirt_amount = 1.1F;
    EXPECT_FALSE(eng_splat_params_valid(&p));
    p.dirt_amount = 0.0F;
    EXPECT_TRUE(eng_splat_params_valid(&p));
    p.dirt_amount = 1.0F;
    EXPECT_TRUE(eng_splat_params_valid(&p));
    p = good;
    p.dirt_wavelength = 0.0F;
    EXPECT_FALSE(eng_splat_params_valid(&p));
}

UTEST(eng_splat, weights_follow_the_documented_rules)
{
    EngSplatParams p = eng_splat_default_params();
    float w[4];
    /* sol plat, bas, sans terre : herbe seule */
    p.dirt_amount = 0.0F;
    eng_splat_weights(&p, 0.0F, 0.2F, 0.5F, w);
    EXPECT_NEAR(w[ENG_SPLAT_GRASS], 1.0F, 1e-6F);
    EXPECT_NEAR(sum4(w), 1.0F, 1e-6F);
    /* pente : roche seule au-delà de 50°, rien en deçà de 30°, smoothstep au milieu (40° : 0,5) */
    eng_splat_weights(&p, 29.9F, 0.2F, 0.5F, w);
    EXPECT_NEAR(w[ENG_SPLAT_ROCK], 0.0F, 1e-6F);
    eng_splat_weights(&p, 40.0F, 0.2F, 0.5F, w);
    EXPECT_NEAR(w[ENG_SPLAT_ROCK], 0.5F, 1e-5F);
    EXPECT_NEAR(w[ENG_SPLAT_GRASS], 0.5F, 1e-5F);
    eng_splat_weights(&p, 50.0F, 0.2F, 0.5F, w);
    EXPECT_NEAR(w[ENG_SPLAT_ROCK], 1.0F, 1e-6F);
    eng_splat_weights(&p, 89.0F, 0.2F, 0.5F, w);
    EXPECT_NEAR(w[ENG_SPLAT_ROCK], 1.0F, 1e-6F);
    /* neige : 0 sous 70 %, 1 au-dessus de 85 %, et jamais sur la roche */
    eng_splat_weights(&p, 0.0F, 0.69F, 0.5F, w);
    EXPECT_NEAR(w[ENG_SPLAT_SNOW], 0.0F, 1e-6F);
    eng_splat_weights(&p, 0.0F, 0.775F, 0.5F, w);
    EXPECT_NEAR(w[ENG_SPLAT_SNOW], 0.5F, 1e-5F);
    eng_splat_weights(&p, 0.0F, 1.0F, 0.5F, w);
    EXPECT_NEAR(w[ENG_SPLAT_SNOW], 1.0F, 1e-6F);
    eng_splat_weights(&p, 90.0F, 1.0F, 0.5F, w); /* falaise enneigée : roche */
    EXPECT_NEAR(w[ENG_SPLAT_SNOW], 0.0F, 1e-6F);
    EXPECT_NEAR(w[ENG_SPLAT_ROCK], 1.0F, 1e-6F);
    /* neige et roche partagent : à mi-pente (40°) et en altitude, la neige vaut (1 - roche) */
    eng_splat_weights(&p, 40.0F, 1.0F, 0.5F, w);
    EXPECT_NEAR(w[ENG_SPLAT_SNOW], 0.5F, 1e-5F);
    EXPECT_NEAR(w[ENG_SPLAT_ROCK], 0.5F, 1e-5F);
    /* terre : 30 % de terre, seuil du bruit 0,7 ; bruit 0,5 : rien, bruit 0,9 : tout le reste */
    p = eng_splat_default_params();
    eng_splat_weights(&p, 0.0F, 0.2F, 0.5F, w);
    EXPECT_NEAR(w[ENG_SPLAT_DIRT], 0.0F, 1e-6F);
    eng_splat_weights(&p, 0.0F, 0.2F, 0.7F, w);
    EXPECT_NEAR(w[ENG_SPLAT_DIRT], 0.5F, 1e-5F);
    eng_splat_weights(&p, 0.0F, 0.2F, 0.95F, w);
    EXPECT_NEAR(w[ENG_SPLAT_DIRT], 1.0F, 1e-6F);
    EXPECT_NEAR(w[ENG_SPLAT_GRASS], 0.0F, 1e-6F);
    /* la terre cède la place à la roche et à la neige */
    eng_splat_weights(&p, 50.0F, 0.2F, 0.95F, w);
    EXPECT_NEAR(w[ENG_SPLAT_DIRT], 0.0F, 1e-6F);
    eng_splat_weights(&p, 0.0F, 1.0F, 0.95F, w);
    EXPECT_NEAR(w[ENG_SPLAT_DIRT], 0.0F, 1e-6F);
    /* les entrées hors plage sont bornées : altitude 5 = 1, bruit -3 = 0 */
    eng_splat_weights(&p, 0.0F, 5.0F, -3.0F, w);
    EXPECT_NEAR(w[ENG_SPLAT_SNOW], 1.0F, 1e-6F);
}

UTEST(eng_splat, weights_are_a_mixture_everywhere_and_the_rock_never_decreases_with_the_slope)
{
    const EngSplatParams p = eng_splat_default_params();
    float previous_rock = 0.0F;
    for (int s = 0; s <= 90; s++) {
        for (int a = 0; a <= 20; a++) {
            for (int n = 0; n <= 20; n++) {
                float w[4];
                eng_splat_weights(&p, (float)s, (float)a / 20.0F, (float)n / 20.0F, w);
                ASSERT_NEAR(sum4(w), 1.0F, 1e-5F);
                for (int c = 0; c < 4; c++) {
                    ASSERT_GE(w[c], -1e-6F);
                    ASSERT_LE(w[c], 1.0F + 1e-6F);
                }
            }
        }
        float w[4];
        eng_splat_weights(&p, (float)s, 0.2F, 0.5F, w);
        ASSERT_GE(w[ENG_SPLAT_ROCK], previous_rock);
        previous_rock = w[ENG_SPLAT_ROCK];
    }
}

UTEST(eng_splat, quantization_sums_to_exactly_255_for_any_mixture)
{
    uint8_t q[4];
    const float even[4] = {0.25F, 0.25F, 0.25F, 0.25F}; /* 4 x 64 = 256 : le reste est négatif */
    eng_splat_quantize(even, q);
    EXPECT_EQ(q[0] + q[1] + q[2] + q[3], 255);
    const float third[4] = {1.0F / 3.0F, 1.0F / 3.0F, 1.0F / 3.0F, 0.0F}; /* 3 x 85 = 255 */
    eng_splat_quantize(third, q);
    EXPECT_EQ(q[3], 0);
    EXPECT_EQ(q[0] + q[1] + q[2], 255);
    const float pure[4] = {0.0F, 1.0F, 0.0F, 0.0F};
    eng_splat_quantize(pure, q);
    EXPECT_EQ(q[1], 255);
    EXPECT_EQ(q[0] + q[2] + q[3], 0);
    /* balayage pseudo-aléatoire de mélanges : la somme vaut toujours 255, chaque octet est proche
     */
    uint32_t state = 12345U;
    for (int i = 0; i < 20000; i++) {
        float w[4];
        float total = 0.0F;
        for (int c = 0; c < 4; c++) {
            state = (state * 1664525U) + 1013904223U;
            w[c] = (float)(state >> 8) / 16777216.0F;
            total += w[c];
        }
        for (int c = 0; c < 4; c++) {
            w[c] /= total;
        }
        eng_splat_quantize(w, q);
        ASSERT_EQ(q[0] + q[1] + q[2] + q[3], 255);
        for (int c = 0; c < 4; c++) {
            ASSERT_LE(fabsf((float)q[c] - (w[c] * 255.0F)), 2.5F);
        }
    }
}

/* Plan incliné : h = 1000 + k * x (pente de k mètres par échantillon, espacement 1 m). */
UTEST(eng_splat, the_slope_of_a_plane_is_the_angle_of_its_gradient)
{
    static uint16_t samples[17 * 17];
    const float k_values[3] = {0.0F, 1.0F, 5.0F};
    for (int t = 0; t < 3; t++) {
        for (uint32_t z = 0; z < 17; z++) {
            for (uint32_t x = 0; x < 17; x++) {
                samples[(z * 17) + x] = (uint16_t)(1000.0F + (k_values[t] * (float)x));
            }
        }
        const EngHeightmap map = make_map(samples, 17, 1.0F);
        const float expected = atanf(k_values[t]) * 57.29577951F;
        for (uint32_t x = 0; x < 17; x++) { /* bords compris : différences unilatérales exactes */
            EXPECT_NEAR(eng_heightmap_slope_degrees(&map, x, 8), expected, 1e-3F);
        }
    }
    /* l'espacement divise la pente : 1 m par échantillon de 2 m = 26,57° pour k = 1 */
    for (uint32_t z = 0; z < 17; z++) {
        for (uint32_t x = 0; x < 17; x++) {
            samples[(z * 17) + x] = (uint16_t)(1000U + x);
        }
    }
    const EngHeightmap wide = make_map(samples, 17, 2.0F);
    EXPECT_NEAR(eng_heightmap_slope_degrees(&wide, 5, 5), atanf(0.5F) * 57.29577951F, 1e-3F);
    /* les deux axes comptent : h = x + z a un gradient de racine de 2 */
    for (uint32_t z = 0; z < 17; z++) {
        for (uint32_t x = 0; x < 17; x++) {
            samples[(z * 17) + x] = (uint16_t)(1000U + x + z);
        }
    }
    const EngHeightmap diag = make_map(samples, 17, 1.0F);
    EXPECT_NEAR(eng_heightmap_slope_degrees(&diag, 8, 8), atanf(sqrtf(2.0F)) * 57.29577951F, 1e-3F);
    EXPECT_NEAR(eng_heightmap_slope_degrees(&diag, 0, 0), atanf(sqrtf(2.0F)) * 57.29577951F, 1e-3F);
    EXPECT_NEAR(eng_heightmap_slope_degrees(&diag, 16, 16), atanf(sqrtf(2.0F)) * 57.29577951F,
                1e-3F);
}

typedef struct Scene {
    EngArena arena;
    uint16_t *samples;
    EngHeightmap map;
    EngSplatmap splat;
} Scene;

static bool open_scene(Scene *s, uint32_t size, const EngSplatParams *params,
                       uint16_t (*height)(uint32_t x, uint32_t z))
{
    *s = (Scene){0};
    if (!eng_arena_init(&s->arena, "test-splat", (size_t)16 << 20)) {
        return false;
    }
    s->samples = eng_arena_alloc(&s->arena, (size_t)size * size * sizeof(uint16_t), 16);
    if (s->samples == NULL) {
        return false;
    }
    for (uint32_t z = 0; z < size; z++) {
        for (uint32_t x = 0; x < size; x++) {
            s->samples[((size_t)z * size) + x] = height(x, z);
        }
    }
    s->map = make_map(s->samples, size, 1.0F);
    return eng_splatmap_generate(&s->arena, &s->map, params, &s->splat);
}

static const uint8_t *texel(const Scene *s, uint32_t x, uint32_t z)
{
    return &s->splat.pixels[(((size_t)z * s->splat.size) + x) * 4U];
}

/* Plaine basse à gauche, falaise au milieu (une marche de 30 m sur un échantillon), plateau haut à
 * droite (altitude relative > 0,85 : le champ monte à 65535). */
static uint16_t terraced(uint32_t x, uint32_t z)
{
    (void)z;
    if (x < 20U) {
        return 1000U;
    }
    if (x == 20U) {
        return 1030U;
    }
    return 60000U;
}

UTEST(eng_splat, the_map_puts_grass_on_the_plain_rock_on_the_cliff_and_snow_on_the_summit)
{
    EngSplatParams p = eng_splat_default_params();
    p.dirt_amount = 0.0F;
    Scene s;
    ASSERT_TRUE(open_scene(&s, 41, &p, terraced));
    EXPECT_EQ(s.splat.size, (uint32_t)41);
    /* plaine : herbe pure */
    EXPECT_EQ(texel(&s, 5, 10)[ENG_SPLAT_GRASS], 255);
    /* la marche (x = 19, 20, 21) : pentes de 15° (30/2) à 89°+ : la roche domine à partir de 20 */
    EXPECT_GT(texel(&s, 20, 10)[ENG_SPLAT_ROCK], 250);
    /* plateau haut : neige pure à 30 échantillons de la marche */
    EXPECT_EQ(texel(&s, 35, 10)[ENG_SPLAT_SNOW], 255);
    /* chaque texel est un mélange de somme 255 */
    for (uint32_t z = 0; z < 41; z++) {
        for (uint32_t x = 0; x < 41; x++) {
            const uint8_t *t = texel(&s, x, z);
            ASSERT_EQ(t[0] + t[1] + t[2] + t[3], 255);
        }
    }
    /* cohérence avec la fonction pure aux deux bouts (pas de décalage de ligne ou de colonne) */
    float w[4];
    uint8_t q[4];
    eng_splat_weights(&p, eng_heightmap_slope_degrees(&s.map, 20, 3), 1030.0F / 65535.0F, 0.5F, w);
    eng_splat_quantize(w, q);
    EXPECT_EQ(texel(&s, 20, 3)[ENG_SPLAT_ROCK], q[ENG_SPLAT_ROCK]);
    eng_arena_shutdown(&s.arena);
}

static uint16_t flat(uint32_t x, uint32_t z)
{
    (void)x;
    (void)z;
    return 2000U;
}

/* Ligne z croissante : la carte est bien ligne par ligne comme le champ (z est l'axe des lignes).
 */
static uint16_t ramp_z(uint32_t x, uint32_t z)
{
    (void)x;
    return z < 10U ? 1000U : 60000U;
}

UTEST(eng_splat, texels_are_in_the_same_row_and_column_order_as_the_heightmap)
{
    EngSplatParams p = eng_splat_default_params();
    p.dirt_amount = 0.0F;
    Scene s;
    ASSERT_TRUE(open_scene(&s, 41, &p, ramp_z));
    EXPECT_EQ(texel(&s, 7, 3)[ENG_SPLAT_GRASS], 255);  /* z petit : bas : herbe */
    EXPECT_EQ(texel(&s, 7, 35)[ENG_SPLAT_SNOW], 255);  /* z grand : haut : neige */
    EXPECT_EQ(texel(&s, 35, 3)[ENG_SPLAT_GRASS], 255); /* x grand, z petit : toujours l'herbe */
    eng_arena_shutdown(&s.arena);
}

UTEST(eng_splat, dirt_patches_follow_the_amount_the_seed_and_are_deterministic)
{
    Scene a;
    Scene b;
    Scene none;
    Scene all;
    EngSplatParams p = eng_splat_default_params(); /* 30 % */
    ASSERT_TRUE(open_scene(&a, 129, &p, flat));
    ASSERT_TRUE(open_scene(&b, 129, &p, flat));
    EXPECT_EQ(memcmp(a.splat.pixels, b.splat.pixels, (size_t)129 * 129 * 4), 0); /* déterministe */
    size_t dirty = 0;
    for (size_t i = 0; i < (size_t)129 * 129; i++) {
        dirty += a.splat.pixels[(i * 4) + ENG_SPLAT_DIRT] > 127U ? 1U : 0U;
    }
    const double fraction = (double)dirty / (129.0 * 129.0);
    EXPECT_GT(fraction, 0.05); /* la part de terre est grosso modo la part demandée (30 %) */
    EXPECT_LT(fraction, 0.60);
    /* la graine change les plaques */
    EngSplatParams other = p;
    other.seed = 8;
    Scene c;
    ASSERT_TRUE(open_scene(&c, 129, &other, flat));
    EXPECT_NE(memcmp(a.splat.pixels, c.splat.pixels, (size_t)129 * 129 * 4), 0);
    /* 0 % : jamais de terre ; 100 % : presque tout en terre */
    EngSplatParams zero = p;
    zero.dirt_amount = 0.0F;
    ASSERT_TRUE(open_scene(&none, 129, &zero, flat));
    EngSplatParams full = p;
    full.dirt_amount = 1.0F;
    ASSERT_TRUE(open_scene(&all, 129, &full, flat));
    size_t zero_dirt = 0;
    size_t mostly_dirt = 0;
    for (size_t i = 0; i < (size_t)129 * 129; i++) {
        zero_dirt += none.splat.pixels[(i * 4) + ENG_SPLAT_DIRT];
        mostly_dirt += all.splat.pixels[(i * 4) + ENG_SPLAT_DIRT] > 127U ? 1U : 0U;
    }
    EXPECT_EQ(zero_dirt, (size_t)0);
    EXPECT_GT(mostly_dirt, (size_t)129 * 129 * 8 / 10);
    eng_arena_shutdown(&a.arena);
    eng_arena_shutdown(&b.arena);
    eng_arena_shutdown(&c.arena);
    eng_arena_shutdown(&none.arena);
    eng_arena_shutdown(&all.arena);
}

UTEST(eng_splat, generation_refuses_bad_input_and_a_full_arena_is_left_untouched)
{
    static uint16_t samples[65 * 65];
    const EngHeightmap map = make_map(samples, 65, 1.0F);
    const EngSplatParams p = eng_splat_default_params();
    EngSplatmap out = {.size = 9};
    EngArena arena;
    /* 65 x 65 x 4 = 16900 octets de pixels, plus une ligne de 260 octets : trois capacités. */
    const size_t capacities[2] = {8192,
                                  17000}; /* trop petite ; les pixels tiennent, pas la ligne */
    for (int c = 0; c < 2; c++) {
        ASSERT_TRUE(eng_arena_init(&arena, "test-splat-small", capacities[c]));
        const EngArenaMark start = eng_arena_mark(&arena);
        EXPECT_FALSE(eng_splatmap_generate(&arena, &map, &p, &out));
        EXPECT_EQ(out.size, (uint32_t)0);
        EXPECT_TRUE(out.pixels == NULL);
        const EngArenaMark end = eng_arena_mark(&arena);
        EXPECT_EQ(memcmp(&start, &end, sizeof start), 0); /* restituée */
        eng_arena_shutdown(&arena);
    }
    ASSERT_TRUE(eng_arena_init(&arena, "test-splat-exact", 17400)); /* pixels + ligne : tient */
    EXPECT_TRUE(eng_splatmap_generate(&arena, &map, &p, &out));
    eng_arena_shutdown(&arena);
    /* entrées invalides */
    ASSERT_TRUE(eng_arena_init(&arena, "test-splat-ok", (size_t)1 << 20));
    EXPECT_FALSE(eng_splatmap_generate(&arena, NULL, &p, &out));
    EngHeightmap bad = map;
    bad.samples = NULL;
    EXPECT_FALSE(eng_splatmap_generate(&arena, &bad, &p, &out));
    bad = map;
    bad.altitude_max = bad.altitude_min;
    EXPECT_FALSE(eng_splatmap_generate(&arena, &bad, &p, &out));
    bad = map;
    bad.spacing = 0.0F;
    EXPECT_FALSE(eng_splatmap_generate(&arena, &bad, &p, &out));
    EngSplatParams bad_params = p;
    bad_params.dirt_wavelength = -1.0F;
    EXPECT_FALSE(eng_splatmap_generate(&arena, &map, &bad_params, &out));
    EXPECT_TRUE(eng_splatmap_generate(&arena, &map, &p, &out));
    EXPECT_EQ(out.size, (uint32_t)65);
    eng_arena_shutdown(&arena);
}

/* Bornes exactes des paramètres : début = 0 et plein = 1 sont valides, et une marche (début ==
 * plein) est une marche : 0 en dessous, 1 à partir du seuil (égalité comprise). */
UTEST(eng_splat, parameter_bounds_are_inclusive_where_documented_and_a_step_is_a_step)
{
    EngSplatParams p = eng_splat_default_params();
    p.rock_slope_start_deg = 0.0F;
    EXPECT_TRUE(eng_splat_params_valid(&p));
    p = eng_splat_default_params();
    p.snow_start = 0.0F;
    p.snow_full = 1.0F;
    EXPECT_TRUE(eng_splat_params_valid(&p));
    p = eng_splat_default_params();
    p.dirt_wavelength = 1e-3F;
    EXPECT_TRUE(eng_splat_params_valid(&p));
    p.snow_start = 0.8F;
    p.snow_full = 0.8F;
    p.dirt_amount = 0.0F;
    float w[4];
    eng_splat_weights(&p, 0.0F, 0.79F, 0.5F, w);
    EXPECT_NEAR(w[ENG_SPLAT_SNOW], 0.0F, 1e-6F);
    eng_splat_weights(&p, 0.0F, 0.8F, 0.5F, w); /* à la marche : déjà de la neige */
    EXPECT_NEAR(w[ENG_SPLAT_SNOW], 1.0F, 1e-6F);
    eng_splat_weights(&p, 0.0F, 0.81F, 0.5F, w);
    EXPECT_NEAR(w[ENG_SPLAT_SNOW], 1.0F, 1e-6F);
}

/* Le plus petit champ (2 x 2) est accepté ; une altitude_min non nulle compte dans l'altitude
 * relative ; l'espacement compte dans la pente selon les DEUX axes. */
UTEST(eng_splat, the_smallest_map_works_and_altitude_and_spacing_are_in_metres)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test-splat-mini", (size_t)1 << 16));
    uint16_t tiny[4] = {100, 100, 100, 100};
    EngHeightmap map = make_map(tiny, 2, 1.0F);
    EngSplatParams p = eng_splat_default_params();
    p.dirt_amount = 0.0F;
    EngSplatmap out;
    ASSERT_TRUE(eng_splatmap_generate(&arena, &map, &p, &out));
    EXPECT_EQ(out.size, (uint32_t)2);
    EXPECT_EQ(out.pixels[ENG_SPLAT_GRASS], 255);
    eng_arena_shutdown(&arena);

    /* altitude de -1000 à 1000 m : la valeur 60000 donne 830,7 m, soit 91,5 % de la plage : neige ;
     * 20000 donne -390,5 m, soit 30 % : herbe (champs plats : pas de roche) */
    const uint16_t levels[2] = {60000, 20000};
    for (int k = 0; k < 2; k++) {
        ASSERT_TRUE(eng_arena_init(&arena, "test-splat-alt", (size_t)1 << 16));
        uint16_t quad[4] = {levels[k], levels[k], levels[k], levels[k]};
        map = make_map(quad, 2, 100.0F);
        map.altitude_min = -1000.0F;
        map.altitude_max = 1000.0F;
        ASSERT_TRUE(eng_splatmap_generate(&arena, &map, &p, &out));
        EXPECT_EQ(out.pixels[k == 0 ? ENG_SPLAT_SNOW : ENG_SPLAT_GRASS], 255);
        eng_arena_shutdown(&arena);
    }

    /* pente selon z avec un espacement de 2 m : 1 m d'altitude par échantillon = 26,57° */
    uint16_t z_ramp[25];
    for (uint32_t z = 0; z < 5; z++) {
        for (uint32_t x = 0; x < 5; x++) {
            z_ramp[(z * 5) + x] = (uint16_t)(1000U + (z * 1U));
        }
    }
    map = make_map(z_ramp, 5, 2.0F);
    EXPECT_NEAR(eng_heightmap_slope_degrees(&map, 2, 2), atanf(0.5F) * 57.29577951F, 1e-3F);
    EXPECT_NEAR(eng_heightmap_slope_degrees(&map, 2, 0), atanf(0.5F) * 57.29577951F, 1e-3F);
}

/* Référence indépendante : la carte d'un champ à espacement 2 m et altitude_min non nulle,
 * recalculée texel par texel avec l'API publique (bruit, pente, poids, quantification) et les
 * formules de docs/terrain.md. */
static uint16_t hash_height(uint32_t x, uint32_t z)
{
    uint32_t h = (x * 73856093U) ^ (z * 19349663U);
    h ^= h >> 13;
    h *= 0x5BD1E995U;
    /* rampe en x et en z, ondulation et petit bruit : pentes de 0 à plus de 45°, altitudes de 10 %
     * à plus de 90 % de la plage */
    const float wave = 2000.0F * sinf(0.5F * (float)x) * sinf(0.4F * (float)z);
    return (uint16_t)(8000.0F + (900.0F * (float)x) + (700.0F * (float)z) + wave +
                      (float)((h >> 8) % 300U));
}

UTEST(eng_splat, the_map_equals_an_independent_recomputation_texel_by_texel)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test-splat-ref", (size_t)1 << 20));
    uint16_t samples[33 * 33];
    for (uint32_t z = 0; z < 33; z++) {
        for (uint32_t x = 0; x < 33; x++) {
            samples[(z * 33) + x] = hash_height(x, z);
        }
    }
    EngHeightmap map = make_map(samples, 33, 2.0F);
    map.altitude_min = -10.0F;
    map.altitude_max = 70.0F;
    EngSplatParams p = eng_splat_default_params();
    p.dirt_amount = 0.5F;
    p.dirt_wavelength = 37.0F;
    p.seed = 99;
    p.snow_start = 0.55F;
    p.snow_full = 0.75F;
    p.rock_slope_start_deg = 20.0F;
    p.rock_slope_full_deg = 60.0F;
    EngSplatmap out;
    ASSERT_TRUE(eng_splatmap_generate(&arena, &map, &p, &out));
    const EngNoiseParams noise = {.seed = 99,
                                  .frequency = 1.0F / 37.0F,
                                  .octaves = 3,
                                  .lacunarity = 2.0F,
                                  .gain = 0.5F,
                                  .fractal = ENG_NOISE_FBM};
    uint32_t checked = 0;
    uint32_t differing = 0;
    for (uint32_t z = 0; z < 33; z++) {
        float row[33];
        eng_noise_row_2d(&noise, 0.0F, 2.0F, (float)z * 2.0F, 33, row);
        for (uint32_t x = 0; x < 33; x++) {
            const float altitude = -10.0F + ((float)samples[(z * 33) + x] / 65535.0F) * 80.0F;
            const float relative = (altitude + 10.0F) / 80.0F;
            const float dx = (float)((int)hash_height(x < 32 ? x + 1 : x, z) -
                                     (int)hash_height(x > 0 ? x - 1 : x, z));
            const float dz = (float)((int)hash_height(x, z < 32 ? z + 1 : z) -
                                     (int)hash_height(x, z > 0 ? z - 1 : z));
            const float run_x = (x > 0 && x < 32) ? 4.0F : 2.0F;
            const float run_z = (z > 0 && z < 32) ? 4.0F : 2.0F;
            const float gx = (dx * 80.0F / 65535.0F) / run_x;
            const float gz = (dz * 80.0F / 65535.0F) / run_z;
            const float slope = atanf(sqrtf((gx * gx) + (gz * gz))) * 57.29577951F;
            float w[4];
            uint8_t q[4];
            eng_splat_weights(&p, slope, relative, (row[x] * 0.5F) + 0.5F, w);
            eng_splat_quantize(w, q);
            const uint8_t *got = &out.pixels[(((size_t)z * 33) + x) * 4];
            for (int c = 0; c < 4; c++) {
                if (abs((int)got[c] - (int)q[c]) > 1) {
                    differing++;
                }
            }
            checked++;
        }
    }
    EXPECT_EQ(checked, (uint32_t)(33 * 33));
    EXPECT_EQ(differing, (uint32_t)0);
    /* le terrain de test contient bien les quatre couches (le test n'est pas trivial) */
    uint32_t seen[4] = {0, 0, 0, 0};
    for (size_t i = 0; i < (size_t)33 * 33; i++) {
        for (int c = 0; c < 4; c++) {
            seen[c] += out.pixels[(i * 4) + (size_t)c] > 127U ? 1U : 0U;
        }
    }
    for (int c = 0; c < 4; c++) {
        EXPECT_GT(seen[c], (uint32_t)10);
    }
    eng_arena_shutdown(&arena);
}

/* Les plaques de terre sont lisses : deux texels voisins diffèrent peu quand la longueur d'onde est
 * grande, beaucoup quand elle est petite. */
UTEST(eng_splat, the_dirt_patch_size_follows_the_wavelength)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test-splat-wave", (size_t)1 << 21));
    static uint16_t flat_samples[129 * 129];
    for (size_t i = 0; i < (size_t)129 * 129; i++) {
        flat_samples[i] = 2000;
    }
    const EngHeightmap map = make_map(flat_samples, 129, 1.0F);
    const float waves[2] = {40.0F, 1.5F};
    double mean_step[2];
    for (int k = 0; k < 2; k++) {
        EngSplatParams p = eng_splat_default_params();
        p.dirt_amount = 0.5F;
        p.dirt_wavelength = waves[k];
        EngSplatmap out;
        ASSERT_TRUE(eng_splatmap_generate(&arena, &map, &p, &out));
        double sum = 0.0;
        for (uint32_t z = 0; z < 129; z++) {
            for (uint32_t x = 0; x < 128; x++) {
                sum += abs((int)out.pixels[((((size_t)z * 129) + x) * 4) + ENG_SPLAT_DIRT] -
                           (int)out.pixels[((((size_t)z * 129) + x + 1) * 4) + ENG_SPLAT_DIRT]);
            }
        }
        mean_step[k] = sum / (129.0 * 128.0);
        eng_arena_reset(&arena);
    }
    EXPECT_LT(mean_step[0], 20.0);
    EXPECT_GT(mean_step[1], 3.0 * mean_step[0]);
    eng_arena_shutdown(&arena);
}

/* Valeurs d'apparence : quelques pixels de chaque texture (64 x 64), à 2 près (les calculs en
 * flottant peuvent différer d'un compilateur à l'autre d'une unité d'arrondi). Changer l'aspect
 * d'une texture demande de changer ces valeurs : c'est voulu. */
UTEST(eng_terrain_texture, the_appearance_is_pinned_by_spot_values)
{
    static const unsigned xs[6] = {0, 13, 31, 32, 50, 63};
    static const unsigned ys[6] = {0, 5, 40, 32, 17, 63};
    static const int expected[4][6][3] = {
        {{68, 106, 33}, {68, 107, 33}, {71, 110, 35}, {67, 106, 33}, {69, 107, 33}, {68, 106, 33}},
        {{134, 127, 118},
         {123, 117, 108},
         {95, 89, 83},
         {81, 76, 70},
         {132, 125, 116},
         {133, 126, 117}},
        {{98, 69, 42}, {107, 76, 46}, {108, 78, 47}, {99, 70, 42}, {95, 67, 40}, {101, 72, 43}},
        {{231, 237, 244},
         {227, 234, 242},
         {235, 240, 246},
         {226, 233, 242},
         {230, 236, 244},
         {229, 235, 243}},
    };
    static uint8_t px[64 * 64 * 4];
    for (uint32_t l = 0; l < 4; l++) {
        eng_terrain_layer_pixels((EngSplatLayer)l, 64, px);
        for (int i = 0; i < 6; i++) {
            for (int c = 0; c < 3; c++) {
                const int got = px[(((size_t)ys[i] * 64) + xs[i]) * 4 + (size_t)c];
                ASSERT_LE(abs(got - expected[l][i][c]), 2);
            }
        }
    }
}

/* ── Textures de couche ── */

static int channel_mean(const uint8_t *rgba, uint32_t size, int channel)
{
    uint64_t sum = 0;
    for (size_t i = 0; i < (size_t)size * size; i++) {
        sum += rgba[(i * 4) + (size_t)channel];
    }
    return (int)(sum / ((uint64_t)size * size));
}

UTEST(eng_terrain_texture, layers_have_the_colour_of_their_material_and_are_opaque)
{
    static uint8_t px[4][64 * 64 * 4];
    for (uint32_t l = 0; l < 4; l++) {
        eng_terrain_layer_pixels((EngSplatLayer)l, 64, px[l]);
        for (size_t i = 0; i < (size_t)64 * 64; i++) {
            ASSERT_EQ(px[l][(i * 4) + 3], 255);
        }
    }
    /* herbe : le vert domine */
    EXPECT_GT(channel_mean(px[0], 64, 1), channel_mean(px[0], 64, 0) + 20);
    EXPECT_GT(channel_mean(px[0], 64, 1), channel_mean(px[0], 64, 2) + 40);
    /* roche : grise (écart faible entre canaux) et moyenne */
    EXPECT_LT(abs(channel_mean(px[1], 64, 0) - channel_mean(px[1], 64, 2)), 30);
    EXPECT_GT(channel_mean(px[1], 64, 1), 90);
    EXPECT_LT(channel_mean(px[1], 64, 1), 180);
    /* terre : brune, rouge > vert > bleu */
    EXPECT_GT(channel_mean(px[2], 64, 0), channel_mean(px[2], 64, 1));
    EXPECT_GT(channel_mean(px[2], 64, 1), channel_mean(px[2], 64, 2));
    /* neige : très claire */
    EXPECT_GT(channel_mean(px[3], 64, 0), 215);
    EXPECT_GT(channel_mean(px[3], 64, 2), 225);
    /* les quatre couches diffèrent, et chacune varie (ce n'est pas un aplat) */
    for (uint32_t a = 0; a < 4; a++) {
        for (uint32_t b = a + 1; b < 4; b++) {
            EXPECT_NE(memcmp(px[a], px[b], sizeof px[a]), 0);
        }
        int lo = 255;
        int hi = 0;
        for (size_t i = 0; i < (size_t)64 * 64; i++) {
            lo = px[a][i * 4] < lo ? px[a][i * 4] : lo;
            hi = px[a][i * 4] > hi ? px[a][i * 4] : hi;
        }
        EXPECT_GT(hi - lo, 8);
    }
}

UTEST(eng_terrain_texture, a_layer_texture_tiles_without_a_seam_and_does_not_depend_on_its_size)
{
    static uint8_t small[64 * 64 * 4];
    static uint8_t large[128 * 128 * 4];
    for (uint32_t l = 0; l < 4; l++) {
        eng_terrain_layer_pixels((EngSplatLayer)l, 64, small);
        eng_terrain_layer_pixels((EngSplatLayer)l, 128, large);
        /* aux positions paires, la grande texture est la petite : une même fonction de (x/n, y/n)
         */
        for (uint32_t y = 0; y < 64; y++) {
            for (uint32_t x = 0; x < 64; x++) {
                ASSERT_EQ(memcmp(&small[(((size_t)y * 64) + x) * 4],
                                 &large[(((size_t)y * 2 * 128) + ((size_t)x * 2)) * 4], 4),
                          0);
            }
        }
        /* raccord : le saut entre la dernière colonne et la première n'est pas plus grand que le
         * plus grand saut entre colonnes voisines à l'intérieur (la fonction est périodique) */
        int worst_inside = 0;
        int worst_seam = 0;
        for (uint32_t y = 0; y < 64; y++) {
            for (uint32_t x = 0; x < 64; x++) {
                const int here = small[((y * 64) + x) * 4 + 1];
                const int right = small[((y * 64) + ((x + 1) % 64)) * 4 + 1];
                const int down = small[((((y + 1) % 64) * 64) + x) * 4 + 1];
                const int dx = abs(right - here);
                const int dy = abs(down - here);
                if (x == 63) {
                    worst_seam = dx > worst_seam ? dx : worst_seam;
                } else {
                    worst_inside = dx > worst_inside ? dx : worst_inside;
                }
                if (y == 63) {
                    worst_seam = dy > worst_seam ? dy : worst_seam;
                } else {
                    worst_inside = dy > worst_inside ? dy : worst_inside;
                }
            }
        }
        EXPECT_LE(worst_seam, worst_inside + 1);
    }
}

UTEST(eng_terrain_texture, invalid_requests_write_nothing)
{
    uint8_t px[8 * 8 * 4];
    memset(px, 0x5A, sizeof px);
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    eng_terrain_layer_pixels((EngSplatLayer)7, 8, px);
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    eng_terrain_layer_pixels((EngSplatLayer)ENG_SPLAT_LAYERS, 8,
                             px); /* première valeur hors plage */
    eng_terrain_layer_pixels(ENG_SPLAT_GRASS, 3, px);
    for (size_t i = 0; i < sizeof px; i++) {
        ASSERT_EQ(px[i], 0x5A);
    }
    eng_terrain_layer_pixels(ENG_SPLAT_GRASS, 4, px); /* 4 : accepté */
    EXPECT_EQ(px[3], 255);
}
