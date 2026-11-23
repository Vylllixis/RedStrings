#include <math.h>
#include <stdint.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/placement.h"
#include "utest.h"

#define N 129U /* champ de 128 m à 1 m */

typedef struct Field {
    uint16_t samples[N * N];
    uint8_t pixels[N * N * 4U];
    EngHeightmap map;
    EngSplatmap splat;
} Field;

static Field g_field;

/* altitude relative (0..1) de l'échantillon (x, z) -> valeur 16 bits */
static uint16_t quant(float rel)
{
    return (uint16_t)(rel * 65535.0F);
}

/* Champ N x N de 128 m, altitudes 0 à 100 m. `ramp` : altitude relative par mètre (0 : plat à
 * 0). Splatmap : herbe 255 partout. */
static void make_field(float ramp_per_m)
{
    for (uint32_t z = 0; z < N; z++) {
        for (uint32_t x = 0; x < N; x++) {
            g_field.samples[((size_t)z * N) + x] = quant(ramp_per_m * (float)x);
            uint8_t *p = &g_field.pixels[(((size_t)z * N) + x) * 4U];
            p[0] = 255;
            p[1] = 0;
            p[2] = 0;
            p[3] = 0;
        }
    }
    g_field.map = (EngHeightmap){.size = N,
                                 .spacing = 1.0F,
                                 .altitude_min = 0.0F,
                                 .altitude_max = 100.0F,
                                 .samples = g_field.samples};
    g_field.splat = (EngSplatmap){.size = N, .pixels = g_field.pixels};
}

static void set_pixel(uint32_t x, uint32_t z, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    uint8_t *p = &g_field.pixels[(((size_t)z * N) + x) * 4U];
    p[0] = r;
    p[1] = g;
    p[2] = b;
    p[3] = a;
}

/* règle pleine densité : adéquation 1 partout où elle est permise */
static EngPlacementRule full_rule(void)
{
    EngPlacementRule r = eng_placement_vegetation_rule();
    r.slope_max_deg = 90.0F;
    r.slope_falloff_deg = 0.0F;
    r.rock_weight_max = 1.0F;
    r.layer_weight_min = 0.0F;
    return r;
}

static uint32_t generate(const EngPlacementRule *rule, float ox, float oz, EngArena *arena,
                         EngInstance **out)
{
    uint32_t count = 0;
    if (!eng_placement_generate(arena, &g_field.map, &g_field.splat, rule, ox, oz, out, &count)) {
        return UINT32_MAX;
    }
    return count;
}

UTEST(eng_placement, the_default_rules_are_valid)
{
    const EngPlacementRule v = eng_placement_vegetation_rule();
    const EngPlacementRule r = eng_placement_rock_rule();
    EXPECT_TRUE(eng_placement_rule_valid(&v));
    EXPECT_TRUE(eng_placement_rule_valid(&r));
    EXPECT_EQ(v.density_layer, (int32_t)ENG_SPLAT_GRASS);
    EXPECT_EQ(r.density_layer, (int32_t)ENG_SPLAT_ROCK);
}

UTEST(eng_placement, every_invalid_field_is_rejected)
{
    EXPECT_FALSE(eng_placement_rule_valid(NULL));
    EngPlacementRule r;
#define BAD(field, value)                                                                          \
    r = eng_placement_vegetation_rule();                                                           \
    r.field = (value);                                                                             \
    EXPECT_FALSE(eng_placement_rule_valid(&r))
    BAD(cell_meters, 0.0F);
    BAD(cell_meters, NAN);
    BAD(slope_max_deg, 0.0F);
    BAD(slope_max_deg, 90.5F);
    BAD(slope_falloff_deg, -1.0F);
    BAD(slope_falloff_deg, 29.0F);
    BAD(altitude_min, -0.1F);
    BAD(altitude_max, 1.1F);
    BAD(density_layer, -2);
    BAD(density_layer, 4);
    BAD(layer_weight_min, 1.5F);
    BAD(layer_weight_min, -0.5F);
    BAD(rock_weight_max, 1.5F);
    BAD(rock_weight_max, -0.1F);
    BAD(scale_min, 0.0F);
    BAD(scale_max, 0.1F);
    BAD(sink, 1.0F);
    BAD(sink, -0.1F);
    BAD(margin_meters, -1.0F);
    BAD(margin_meters, INFINITY);
#undef BAD
    r = eng_placement_vegetation_rule();
    r.altitude_min = 0.6F;
    r.altitude_max = 0.4F;
    EXPECT_FALSE(eng_placement_rule_valid(&r));
    r = eng_placement_vegetation_rule();
    r.density_layer = -1;
    EXPECT_TRUE(eng_placement_rule_valid(&r));
}

UTEST(eng_placement, same_seed_gives_identical_bytes_and_another_seed_another_scatter)
{
    make_field(0.0F);
    EngArena a;
    ASSERT_TRUE(eng_arena_init(&a, "pl", 4U << 20));
    EngPlacementRule rule = full_rule();
    rule.seed = 5;
    EngInstance *first = NULL;
    const uint32_t n1 = generate(&rule, 0.0F, 0.0F, &a, &first);
    EngInstance *second = NULL;
    const uint32_t n2 = generate(&rule, 0.0F, 0.0F, &a, &second);
    rule.seed = 6;
    EngInstance *third = NULL;
    const uint32_t n3 = generate(&rule, 0.0F, 0.0F, &a, &third);
    ASSERT_NE(n1, UINT32_MAX);
    ASSERT_GT(n1, 100U);
    ASSERT_EQ(n1, n2);
    EXPECT_EQ(memcmp(first, second, (size_t)n1 * sizeof(EngInstance)), 0);
    ASSERT_EQ(n3, n1); /* densité 1 : le même nombre, pas les mêmes positions */
    EXPECT_NE(memcmp(first, third, (size_t)n1 * sizeof(EngInstance)), 0);
    eng_arena_shutdown(&a);
}

UTEST(eng_placement, full_suitability_places_exactly_one_instance_per_cell_inside_the_margin)
{
    make_field(0.0F);
    EngPlacementRule rule = full_rule(); /* cellule 2 m, marge 2 m : cellules 1 à 62 -> 62 x 62 */
    EXPECT_EQ(eng_placement_count(&g_field.map, &g_field.splat, &rule), 62U * 62U);
    rule.cell_meters = 4.0F; /* 32 cellules, marge 2 m : la cellule 0 perd tout ce qui est < 2 m */
    const uint32_t n = eng_placement_count(&g_field.map, &g_field.splat, &rule);
    EXPECT_GE(n, 30U * 30U);
    EXPECT_LE(n, 32U * 32U);
}

UTEST(eng_placement, instances_are_valid_inside_the_margin_and_sit_on_the_ground)
{
    make_field(0.002F); /* 0,2 m par mètre : pente 11 degrés */
    EngArena a;
    ASSERT_TRUE(eng_arena_init(&a, "pl", 4U << 20));
    EngPlacementRule rule = full_rule();
    rule.margin_meters = 5.0F;
    rule.sink = 0.25F;
    EngInstance *list = NULL;
    const uint32_t n = generate(&rule, 1000.0F, -300.0F, &a, &list);
    ASSERT_NE(n, UINT32_MAX);
    ASSERT_GT(n, 100U);
    for (uint32_t i = 0; i < n; i++) {
        const EngInstance *in = &list[i];
        ASSERT_TRUE(eng_instance_valid(in));
        const float lx = in->position[0] - 1000.0F;
        const float lz = in->position[2] + 300.0F;
        ASSERT_GE(lx, 5.0F - 1e-3F);
        ASSERT_LE(lx, 123.0F + 1e-3F);
        ASSERT_GE(lz, 5.0F - 1e-3F);
        ASSERT_LE(lz, 123.0F + 1e-3F);
        ASSERT_GE(in->scale, rule.scale_min);
        ASSERT_LE(in->scale, rule.scale_max);
        const float ground = eng_heightmap_sample(&g_field.map, lx, lz);
        ASSERT_NEAR(in->position[1], ground - (0.25F * in->scale), 1e-3F);
        ASSERT_GE(in->tint[0], rule.tint_min);
        ASSERT_EQ(in->tint[3], 255U);
    }
    eng_arena_shutdown(&a);
}

UTEST(eng_placement, a_steep_zone_stays_empty_and_the_flat_zone_is_planted)
{
    /* moitié gauche plate (x < 64), moitié droite à 45 degrés (1 m par mètre) */
    make_field(0.0F);
    for (uint32_t z = 0; z < N; z++) {
        for (uint32_t x = 64; x < N; x++) {
            g_field.samples[((size_t)z * N) + x] = quant((float)(x - 64U) / 100.0F);
        }
    }
    EngArena a;
    ASSERT_TRUE(eng_arena_init(&a, "pl", 4U << 20));
    EngPlacementRule rule = full_rule();
    rule.slope_max_deg = 30.0F;
    rule.slope_falloff_deg = 0.0F;
    EngInstance *list = NULL;
    const uint32_t n = generate(&rule, 0.0F, 0.0F, &a, &list);
    ASSERT_NE(n, UINT32_MAX);
    uint32_t flat = 0;
    for (uint32_t i = 0; i < n; i++) {
        ASSERT_LT(list[i].position[0], 66.0F);
        if (list[i].position[0] < 60.0F) {
            flat++;
        }
    }
    EXPECT_GT(flat, 800U); /* 29 colonnes x 62 lignes de cellules */
    eng_arena_shutdown(&a);
}

UTEST(eng_placement, the_slope_falloff_thins_the_scatter_progressively)
{
    make_field(0.0F);
    EngPlacementRule rule = full_rule();
    rule.slope_max_deg = 40.0F;
    rule.slope_falloff_deg = 20.0F;
    /* pente de 30 degrés = milieu de la décroissance : adéquation 0,5 ; pente < 20 : 1 */
    const float tan30 = tanf(30.0F * 3.14159265F / 180.0F);
    for (uint32_t z = 0; z < N; z++) {
        for (uint32_t x = 0; x < N; x++) {
            g_field.samples[((size_t)z * N) + x] = quant(tan30 * (float)x / 100.0F);
        }
    }
    const float s30 = eng_placement_suitability(&g_field.map, &g_field.splat, &rule, 60.0F, 60.0F);
    EXPECT_NEAR(s30, 0.5F, 0.03F); /* smoothstep(0,5) = 0,5 */
    rule.slope_max_deg = 60.0F;
    const float s20 = eng_placement_suitability(&g_field.map, &g_field.splat, &rule, 60.0F, 60.0F);
    EXPECT_NEAR(s20, 1.0F, 1e-6F); /* 30 < 60 - 20 = 40 : pleine */
    rule.slope_max_deg = 30.0F;
    rule.slope_falloff_deg = 0.0F;
    EXPECT_EQ(eng_placement_suitability(&g_field.map, &g_field.splat, &rule, 60.0F, 60.0F), 0.0F);
}

UTEST(eng_placement, the_altitude_band_is_respected_and_inclusive)
{
    make_field(0.004F); /* altitude relative 0 à 0,51 sur le champ, pente 22 degrés */
    EngArena a;
    ASSERT_TRUE(eng_arena_init(&a, "pl", 4U << 20));
    EngPlacementRule rule = full_rule();
    rule.altitude_min = 0.1F;
    rule.altitude_max = 0.3F;
    EngInstance *list = NULL;
    const uint32_t n = generate(&rule, 0.0F, 0.0F, &a, &list);
    ASSERT_NE(n, UINT32_MAX);
    ASSERT_GT(n, 200U);
    for (uint32_t i = 0; i < n; i++) {
        const float rel =
            eng_heightmap_sample(&g_field.map, list[i].position[0], list[i].position[2]) / 100.0F;
        ASSERT_GE(rel, 0.1F - 1e-4F);
        ASSERT_LE(rel, 0.3F + 1e-4F);
    }
    /* bornes incluses : à l'altitude exacte de la borne, l'adéquation vaut 1 */
    rule.altitude_min = 0.2F;
    rule.altitude_max = 0.2F;
    const float x = 0.2F / 0.004F;
    EXPECT_GT(eng_placement_suitability(&g_field.map, &g_field.splat, &rule, x, 60.0F), 0.99F);
    rule.altitude_min = 0.2005F;
    rule.altitude_max = 0.3F;
    EXPECT_EQ(eng_placement_suitability(&g_field.map, &g_field.splat, &rule, x, 60.0F), 0.0F);
    eng_arena_shutdown(&a);
}

UTEST(eng_placement, density_follows_the_splatmap_weight)
{
    make_field(0.0F);
    /* moitié gauche : herbe 255 ; moitié droite : herbe 128 et terre 127 */
    for (uint32_t z = 0; z < N; z++) {
        for (uint32_t x = 64; x < N; x++) {
            set_pixel(x, z, 128, 0, 127, 0);
        }
    }
    EngArena a;
    ASSERT_TRUE(eng_arena_init(&a, "pl", 4U << 20));
    EngPlacementRule rule = full_rule();
    rule.layer_weight_min = 0.2F;
    EngInstance *list = NULL;
    const uint32_t n = generate(&rule, 0.0F, 0.0F, &a, &list);
    ASSERT_NE(n, UINT32_MAX);
    uint32_t left = 0;
    uint32_t right = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (list[i].position[0] < 62.0F) {
            left++;
        } else if (list[i].position[0] > 66.0F) {
            right++;
        }
    }
    ASSERT_GT(left, 1000U);
    const float ratio = (float)right / (float)left;
    EXPECT_GT(ratio, 0.45F);
    EXPECT_LT(ratio, 0.56F);
    /* le poids minimum coupe net : 0,6 > 0,50 -> plus rien à droite */
    rule.layer_weight_min = 0.6F;
    const uint32_t n2 = generate(&rule, 0.0F, 0.0F, &a, &list);
    for (uint32_t i = 0; i < n2; i++) {
        ASSERT_LT(list[i].position[0], 65.0F);
    }
    eng_arena_shutdown(&a);
}

UTEST(eng_placement, no_vegetation_on_rock_and_rocks_on_rock)
{
    make_field(0.0F);
    /* bande de roche 40 <= x <= 80 : herbe 100, roche 155 (0,61) */
    for (uint32_t z = 0; z < N; z++) {
        for (uint32_t x = 40; x <= 80; x++) {
            set_pixel(x, z, 100, 155, 0, 0);
        }
    }
    EngArena a;
    ASSERT_TRUE(eng_arena_init(&a, "pl", 4U << 20));
    EngPlacementRule veg = eng_placement_vegetation_rule();
    veg.slope_max_deg = 90.0F;
    veg.slope_falloff_deg = 0.0F;
    veg.layer_weight_min = 0.0F;
    EngInstance *list = NULL;
    uint32_t n = generate(&veg, 0.0F, 0.0F, &a, &list);
    ASSERT_NE(n, UINT32_MAX);
    ASSERT_GT(n, 500U);
    for (uint32_t i = 0; i < n; i++) {
        const float x = list[i].position[0];
        ASSERT_TRUE(x < 39.5F || x > 80.5F);
    }
    EngPlacementRule rocks = eng_placement_rock_rule();
    n = generate(&rocks, 0.0F, 0.0F, &a, &list);
    ASSERT_NE(n, UINT32_MAX);
    ASSERT_GT(n, 50U);
    for (uint32_t i = 0; i < n; i++) {
        const float x = list[i].position[0];
        ASSERT_TRUE(x >= 39.5F && x <= 80.5F);
    }
    eng_arena_shutdown(&a);
}

UTEST(eng_placement, suitability_is_zero_outside_the_field_and_the_margin)
{
    make_field(0.0F);
    const EngPlacementRule rule = full_rule();
    EXPECT_GT(eng_placement_suitability(&g_field.map, &g_field.splat, &rule, 60.0F, 60.0F), 0.99F);
    EXPECT_EQ(eng_placement_suitability(&g_field.map, &g_field.splat, &rule, 1.9F, 60.0F), 0.0F);
    EXPECT_EQ(eng_placement_suitability(&g_field.map, &g_field.splat, &rule, 60.0F, 1.9F), 0.0F);
    EXPECT_EQ(eng_placement_suitability(&g_field.map, &g_field.splat, &rule, 126.1F, 60.0F), 0.0F);
    EXPECT_EQ(eng_placement_suitability(&g_field.map, &g_field.splat, &rule, 60.0F, 126.1F), 0.0F);
    EXPECT_EQ(eng_placement_suitability(&g_field.map, &g_field.splat, &rule, -5.0F, 60.0F), 0.0F);
    EXPECT_EQ(eng_placement_suitability(&g_field.map, &g_field.splat, &rule, NAN, 60.0F), 0.0F);
    /* sans couche ni limite de roche, la splatmap peut manquer */
    EngPlacementRule bare = full_rule();
    bare.density_layer = -1;
    EXPECT_GT(eng_placement_suitability(&g_field.map, NULL, &bare, 60.0F, 60.0F), 0.99F);
}

UTEST(eng_placement, the_splatmap_weight_is_bilinear)
{
    make_field(0.0F);
    set_pixel(10, 10, 0, 255, 0, 0);
    set_pixel(11, 10, 0, 0, 0, 0);
    set_pixel(10, 11, 0, 0, 0, 0);
    set_pixel(11, 11, 0, 0, 0, 0);
    EXPECT_NEAR(eng_splatmap_weight_at(&g_field.map, &g_field.splat, ENG_SPLAT_ROCK, 10.0F, 10.0F),
                1.0F, 1e-6F);
    EXPECT_NEAR(eng_splatmap_weight_at(&g_field.map, &g_field.splat, ENG_SPLAT_ROCK, 10.5F, 10.0F),
                0.5F, 1e-6F);
    EXPECT_NEAR(eng_splatmap_weight_at(&g_field.map, &g_field.splat, ENG_SPLAT_ROCK, 10.5F, 10.5F),
                0.25F, 1e-6F);
    EXPECT_EQ(eng_splatmap_weight_at(&g_field.map, NULL, ENG_SPLAT_ROCK, 10.0F, 10.0F), 0.0F);
    EXPECT_EQ(eng_splatmap_weight_at(&g_field.map, &g_field.splat, 4, 60.0F, 60.0F), 0.0F);
    /* hors de la grille : ramené sur le bord */
    EXPECT_NEAR(
        eng_splatmap_weight_at(&g_field.map, &g_field.splat, ENG_SPLAT_GRASS, 500.0F, 500.0F), 1.0F,
        1e-6F);
}

UTEST(eng_placement, count_matches_generate_and_failures_restore_the_arena)
{
    make_field(0.0F);
    EngArena a;
    ASSERT_TRUE(eng_arena_init(&a, "pl", 4U << 20));
    const EngPlacementRule rule = full_rule();
    const size_t before = eng_arena_remaining(&a);
    EngInstance *list = (EngInstance *)1;
    uint32_t n = 99;
    ASSERT_TRUE(
        eng_placement_generate(&a, &g_field.map, &g_field.splat, &rule, 0.0F, 0.0F, &list, &n));
    EXPECT_EQ(n, eng_placement_count(&g_field.map, &g_field.splat, &rule));
    EXPECT_LT(eng_arena_remaining(&a), before);

    /* règle invalide */
    EngPlacementRule bad = rule;
    bad.cell_meters = -1.0F;
    const size_t mid = eng_arena_remaining(&a);
    EXPECT_FALSE(
        eng_placement_generate(&a, &g_field.map, &g_field.splat, &bad, 0.0F, 0.0F, &list, &n));
    EXPECT_EQ(list, NULL);
    EXPECT_EQ(n, 0U);
    EXPECT_EQ(eng_placement_count(&g_field.map, &g_field.splat, &bad), 0U);
    EXPECT_EQ(eng_arena_remaining(&a), mid);
    /* splatmap de mauvaise taille ou absente alors que la règle la demande */
    EngSplatmap wrong = {.size = N - 1U, .pixels = g_field.pixels};
    EXPECT_FALSE(eng_placement_generate(&a, &g_field.map, &wrong, &rule, 0.0F, 0.0F, &list, &n));
    EXPECT_FALSE(eng_placement_generate(&a, &g_field.map, NULL, &rule, 0.0F, 0.0F, &list, &n));
    /* origine non finie, champ nul */
    EXPECT_FALSE(
        eng_placement_generate(&a, &g_field.map, &g_field.splat, &rule, NAN, 0.0F, &list, &n));
    EXPECT_FALSE(eng_placement_generate(&a, NULL, &g_field.splat, &rule, 0.0F, 0.0F, &list, &n));
    EXPECT_EQ(eng_arena_remaining(&a), mid);
    eng_arena_shutdown(&a);

    /* arène trop petite : false, rien d'alloué */
    EngArena tiny;
    ASSERT_TRUE(eng_arena_init(&tiny, "tiny", 1024));
    const size_t tiny_before = eng_arena_remaining(&tiny);
    EXPECT_FALSE(
        eng_placement_generate(&tiny, &g_field.map, &g_field.splat, &rule, 0.0F, 0.0F, &list, &n));
    EXPECT_EQ(list, NULL);
    EXPECT_EQ(eng_arena_remaining(&tiny), tiny_before);
    eng_arena_shutdown(&tiny);
}

UTEST(eng_placement, a_rule_that_places_nothing_returns_true_with_no_array)
{
    make_field(0.0F);
    EngArena a;
    ASSERT_TRUE(eng_arena_init(&a, "pl", 1U << 20));
    EngPlacementRule rule = full_rule();
    rule.altitude_min = 0.5F; /* le champ est plat à 0 */
    EngInstance *list = (EngInstance *)1;
    uint32_t n = 99;
    const size_t before = eng_arena_remaining(&a);
    EXPECT_TRUE(
        eng_placement_generate(&a, &g_field.map, &g_field.splat, &rule, 0.0F, 0.0F, &list, &n));
    EXPECT_EQ(list, NULL);
    EXPECT_EQ(n, 0U);
    EXPECT_EQ(eng_arena_remaining(&a), before);
    eng_arena_shutdown(&a);
}

UTEST(eng_placement, on_a_generated_terrain_no_vegetation_stands_on_rocky_slopes)
{
    EngArena a;
    ASSERT_TRUE(eng_arena_init(&a, "pl", 64U << 20));
    EngHeightmapParams hp = eng_heightmap_default_params();
    hp.size = 257;
    hp.wavelength = 120.0F;
    hp.altitude_max = 90.0F;
    EngHeightmap map;
    ASSERT_TRUE(eng_heightmap_generate(&a, &hp, &map));
    EngSplatmap splat;
    const EngSplatParams sp = eng_splat_default_params();
    ASSERT_TRUE(eng_splatmap_generate(&a, &map, &sp, &splat));
    const EngPlacementRule rule = eng_placement_vegetation_rule();
    EngInstance *list = NULL;
    uint32_t n = 0;
    ASSERT_TRUE(eng_placement_generate(&a, &map, &splat, &rule, 0.0F, 0.0F, &list, &n));
    ASSERT_GT(n, 100U);
    for (uint32_t i = 0; i < n; i++) {
        const float x = list[i].position[0];
        const float z = list[i].position[2];
        const uint32_t vx = (uint32_t)(x / map.spacing + 0.5F);
        const uint32_t vz = (uint32_t)(z / map.spacing + 0.5F);
        ASSERT_LE(eng_heightmap_slope_degrees(&map, vx, vz), rule.slope_max_deg + 12.0F);
        ASSERT_LE(eng_splatmap_weight_at(&map, &splat, ENG_SPLAT_ROCK, x, z),
                  rule.rock_weight_max + 1e-4F);
        ASSERT_GE(eng_splatmap_weight_at(&map, &splat, ENG_SPLAT_GRASS, x, z),
                  rule.layer_weight_min - 1e-4F);
    }
    eng_arena_shutdown(&a);
}

UTEST(eng_placement, rule_limits_are_inclusive_where_documented_and_exclusive_elsewhere)
{
    EngPlacementRule r;
#define OK(field, value)                                                                           \
    r = eng_placement_vegetation_rule();                                                           \
    r.field = (value);                                                                             \
    EXPECT_TRUE(eng_placement_rule_valid(&r))
#define BAD(field, value)                                                                          \
    r = eng_placement_vegetation_rule();                                                           \
    r.field = (value);                                                                             \
    EXPECT_FALSE(eng_placement_rule_valid(&r))
    OK(cell_meters, 0.001F);
    OK(slope_max_deg, 90.0F);
    OK(slope_max_deg, 8.0F); /* == slope_falloff_deg */
    BAD(slope_max_deg, 7.9F);
    BAD(slope_max_deg, 0.0F);
    BAD(slope_max_deg, NAN);
    BAD(slope_max_deg, INFINITY);
    OK(slope_falloff_deg, 0.0F);
    OK(slope_falloff_deg, 28.0F); /* == slope_max_deg */
    BAD(slope_falloff_deg, 28.01F);
    BAD(slope_falloff_deg, NAN);
    OK(altitude_min, 0.0F);
    OK(altitude_max, 1.0F);
    BAD(altitude_min, NAN);
    BAD(altitude_max, NAN);
    OK(density_layer, -1);
    OK(density_layer, 3);
    OK(layer_weight_min, 0.0F);
    OK(layer_weight_min, 1.0F);
    BAD(layer_weight_min, NAN);
    OK(rock_weight_max, 0.0F);
    OK(rock_weight_max, 1.0F);
    BAD(rock_weight_max, NAN);
    BAD(scale_min, NAN);
    BAD(scale_min, -1.0F);
    BAD(scale_max, NAN);
    OK(scale_max, 0.6F); /* == scale_min */
    OK(sink, 0.0F);
    OK(sink, 0.99F);
    BAD(sink, NAN);
    OK(margin_meters, 0.0F);
    BAD(margin_meters, NAN);
    r = eng_placement_vegetation_rule();
    r.altitude_min = 0.5F;
    r.altitude_max = 0.5F; /* bande réduite à un point : valide */
    EXPECT_TRUE(eng_placement_rule_valid(&r));
#undef OK
#undef BAD
}

UTEST(eng_placement, scale_yaw_and_tint_are_drawn_over_their_whole_range)
{
    make_field(0.0F);
    EngArena a;
    ASSERT_TRUE(eng_arena_init(&a, "pl", 4U << 20));
    EngPlacementRule rule = full_rule();
    rule.scale_min = 1.0F;
    rule.scale_max = 3.0F;
    rule.tint_min = 100;
    EngInstance *list = NULL;
    const uint32_t n = generate(&rule, 0.0F, 0.0F, &a, &list);
    ASSERT_GT(n, 1000U);
    float scale_sum = 0.0F;
    float tint_sum = 0.0F;
    float scale_lo = 99.0F;
    float scale_hi = 0.0F;
    uint32_t yaw_quadrant[4] = {0, 0, 0, 0};
    for (uint32_t i = 0; i < n; i++) {
        scale_sum += list[i].scale;
        tint_sum += (float)list[i].tint[0];
        scale_lo = fminf(scale_lo, list[i].scale);
        scale_hi = fmaxf(scale_hi, list[i].scale);
        const float yaw = 2.0F * atan2f(list[i].rotation[1], list[i].rotation[3]);
        const float turn = yaw < 0.0F ? yaw + 6.2831853F : yaw;
        yaw_quadrant[(uint32_t)fminf(turn / 1.5707964F, 3.0F)]++;
        ASSERT_EQ(list[i].tint[0], list[i].tint[1]);
        ASSERT_EQ(list[i].tint[0], list[i].tint[2]);
    }
    EXPECT_NEAR(scale_sum / (float)n, 2.0F, 0.05F);
    EXPECT_LT(scale_lo, 1.05F);
    EXPECT_GT(scale_hi, 2.95F);
    EXPECT_NEAR(tint_sum / (float)n, 177.5F, 3.0F);
    for (int q = 0; q < 4; q++) {
        EXPECT_GT(yaw_quadrant[q], n / 5U);
    }
    eng_arena_shutdown(&a);
}

UTEST(eng_placement, margin_bounds_are_inclusive_and_spacing_scales_slope_and_splat_lookups)
{
    make_field(0.0F);
    const EngPlacementRule rule = full_rule();
    EXPECT_GT(eng_placement_suitability(&g_field.map, &g_field.splat, &rule, 2.0F, 2.0F), 0.99F);
    EXPECT_GT(eng_placement_suitability(&g_field.map, &g_field.splat, &rule, 126.0F, 126.0F),
              0.99F);

    /* espacement de 2 m : le champ fait 256 m ; la rampe monte de 1 m par échantillon = 0,5 m/m */
    g_field.map.spacing = 2.0F;
    for (uint32_t z = 0; z < N; z++) {
        for (uint32_t x = 0; x < N; x++) {
            g_field.samples[((size_t)z * N) + x] = quant(0.01F * (float)x);
        }
    }
    EngPlacementRule r = full_rule();
    r.slope_max_deg = 30.0F;
    r.slope_falloff_deg = 20.0F;
    const float slope = atanf(0.5F) * 180.0F / 3.14159265F;
    const float t = (slope - 10.0F) / 20.0F;
    const float expected = 1.0F - (t * t * (3.0F - (2.0F * t)));
    EXPECT_NEAR(eng_placement_suitability(&g_field.map, &g_field.splat, &r, 100.0F, 100.0F),
                expected, 0.01F);
    /* poids bilinéaire en mètres : le texel 10 est à x = 20 m */
    for (uint32_t z = 0; z < N; z++) {
        for (uint32_t x = 0; x < N; x++) {
            set_pixel(x, z, 0, 0, 0, 0);
        }
    }
    set_pixel(10, 10, 0, 200, 0, 0);
    EXPECT_NEAR(eng_splatmap_weight_at(&g_field.map, &g_field.splat, ENG_SPLAT_ROCK, 20.0F, 20.0F),
                200.0F / 255.0F, 1e-5F);
    EXPECT_NEAR(eng_splatmap_weight_at(&g_field.map, &g_field.splat, ENG_SPLAT_ROCK, 21.0F, 20.0F),
                100.0F / 255.0F, 1e-5F);
    EXPECT_NEAR(eng_splatmap_weight_at(&g_field.map, &g_field.splat, ENG_SPLAT_ROCK, 20.0F, 21.0F),
                100.0F / 255.0F, 1e-5F);
    EXPECT_EQ(eng_splatmap_weight_at(&g_field.map, &g_field.splat, ENG_SPLAT_ROCK, 24.0F, 20.0F),
              0.0F);
}

UTEST(eng_placement, the_splatmap_lookup_stays_inside_the_last_row_and_column)
{
    make_field(0.0F);
    for (uint32_t i = 0; i < N; i++) {
        set_pixel(N - 1U, i, 0, 255, 0, 0); /* dernière colonne */
        set_pixel(i, N - 1U, 0, 255, 0, 0); /* dernière ligne */
    }
    const float edge = 128.0F;
    EXPECT_NEAR(eng_splatmap_weight_at(&g_field.map, &g_field.splat, ENG_SPLAT_ROCK, edge, 60.0F),
                1.0F, 1e-6F);
    EXPECT_NEAR(eng_splatmap_weight_at(&g_field.map, &g_field.splat, ENG_SPLAT_ROCK, 60.0F, edge),
                1.0F, 1e-6F);
    EXPECT_NEAR(eng_splatmap_weight_at(&g_field.map, &g_field.splat, ENG_SPLAT_ROCK, 500.0F, 60.0F),
                1.0F, 1e-6F);
    EXPECT_NEAR(eng_splatmap_weight_at(&g_field.map, &g_field.splat, ENG_SPLAT_ROCK, 60.0F, 500.0F),
                1.0F, 1e-6F);
    EXPECT_NEAR(eng_splatmap_weight_at(&g_field.map, &g_field.splat, ENG_SPLAT_ROCK, 60.0F, 60.0F),
                0.0F, 1e-6F);
    EXPECT_NEAR(eng_splatmap_weight_at(&g_field.map, &g_field.splat, ENG_SPLAT_ROCK, -5.0F, 60.0F),
                0.0F, 1e-6F);
}

UTEST(eng_placement, slope_is_measured_along_z_and_altitude_is_relative_to_the_field_range)
{
    make_field(0.0F);
    /* rampe en z : 1 m par mètre = 45 degrés, altitudes de 100 à 200 m */
    g_field.map.altitude_min = 100.0F;
    g_field.map.altitude_max = 200.0F;
    for (uint32_t z = 0; z < N; z++) {
        for (uint32_t x = 0; x < N; x++) {
            g_field.samples[((size_t)z * N) + x] = quant((float)z / 100.0F);
        }
    }
    EngPlacementRule rule = full_rule();
    rule.slope_max_deg = 60.0F;
    rule.slope_falloff_deg = 30.0F; /* début à 30 degrés : à 45 degrés, milieu : 0,5 */
    EXPECT_NEAR(eng_placement_suitability(&g_field.map, &g_field.splat, &rule, 60.0F, 60.0F), 0.5F,
                0.02F);
    /* altitude relative = (160 - 100) / 100 = 0,6 en z = 60 */
    rule = full_rule();
    rule.altitude_min = 0.55F;
    rule.altitude_max = 0.65F;
    EXPECT_GT(eng_placement_suitability(&g_field.map, &g_field.splat, &rule, 60.0F, 60.0F), 0.99F);
    EXPECT_EQ(eng_placement_suitability(&g_field.map, &g_field.splat, &rule, 60.0F, 90.0F), 0.0F);
    EXPECT_EQ(eng_placement_suitability(&g_field.map, &g_field.splat, &rule, 60.0F, 30.0F), 0.0F);
}

UTEST(eng_placement, input_validation_accepts_the_smallest_field_and_rejects_degenerate_ones)
{
    make_field(0.0F);
    EngArena a;
    ASSERT_TRUE(eng_arena_init(&a, "pl", 1U << 20));
    EngInstance *list = NULL;
    uint32_t n = 0;
    EngPlacementRule rule = full_rule();
    rule.density_layer = -1; /* ni couche ni limite de roche : pas de splatmap nécessaire */
    EXPECT_TRUE(eng_placement_generate(&a, &g_field.map, NULL, &rule, 0.0F, 0.0F, &list, &n));
    EXPECT_GT(n, 100U);
    rule.rock_weight_max = 0.5F; /* limite de roche : la splatmap redevient nécessaire */
    EXPECT_FALSE(eng_placement_generate(&a, &g_field.map, NULL, &rule, 0.0F, 0.0F, &list, &n));
    rule.rock_weight_max = 1.0F;
    rule.density_layer = (int32_t)ENG_SPLAT_GRASS; /* couche 0 : splatmap nécessaire aussi */
    EXPECT_FALSE(eng_placement_generate(&a, &g_field.map, NULL, &rule, 0.0F, 0.0F, &list, &n));

    EngHeightmap bad = g_field.map;
    bad.spacing = 0.0F;
    EXPECT_FALSE(eng_placement_generate(&a, &bad, &g_field.splat, &rule, 0.0F, 0.0F, &list, &n));
    bad = g_field.map;
    bad.altitude_max = bad.altitude_min;
    EXPECT_FALSE(eng_placement_generate(&a, &bad, &g_field.splat, &rule, 0.0F, 0.0F, &list, &n));
    bad = g_field.map;
    bad.size = 1;
    EXPECT_FALSE(eng_placement_generate(&a, &bad, &g_field.splat, &rule, 0.0F, 0.0F, &list, &n));
    bad = g_field.map;
    bad.samples = NULL;
    EXPECT_FALSE(eng_placement_generate(&a, &bad, &g_field.splat, &rule, 0.0F, 0.0F, &list, &n));

    /* le plus petit champ admis : 2 x 2, valide, rien à placer (marge) */
    uint16_t tiny_samples[4] = {0, 0, 0, 0};
    uint8_t tiny_pixels[16] = {255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0};
    const EngHeightmap tiny = {.size = 2,
                               .spacing = 1.0F,
                               .altitude_min = 0.0F,
                               .altitude_max = 1.0F,
                               .samples = tiny_samples};
    const EngSplatmap tiny_splat = {.size = 2, .pixels = tiny_pixels};
    EXPECT_TRUE(eng_placement_generate(&a, &tiny, &tiny_splat, &rule, 0.0F, 0.0F, &list, &n));
    EXPECT_EQ(n, 0U);
    eng_arena_shutdown(&a);
}

UTEST(eng_placement, slope_along_z_scales_with_the_sample_spacing)
{
    make_field(0.0F);
    g_field.map.spacing = 2.0F; /* 1 m de dénivelé par échantillon de 2 m : pente 0,5 */
    for (uint32_t z = 0; z < N; z++) {
        for (uint32_t x = 0; x < N; x++) {
            g_field.samples[((size_t)z * N) + x] = quant((float)z / 100.0F);
        }
    }
    EngPlacementRule rule = full_rule();
    rule.slope_max_deg = 40.0F;
    rule.slope_falloff_deg = 30.0F;
    const float slope = atanf(0.5F) * 180.0F / 3.14159265F;
    const float t = (slope - 10.0F) / 30.0F;
    const float expected = 1.0F - (t * t * (3.0F - (2.0F * t)));
    EXPECT_NEAR(eng_placement_suitability(&g_field.map, &g_field.splat, &rule, 100.0F, 100.0F),
                expected, 0.01F);
}

UTEST(eng_placement,
      the_grass_rule_is_valid_denser_than_vegetation_and_avoids_rock_and_steep_ground)
{
    make_field(0.0F);
    const EngPlacementRule grass = eng_placement_grass_rule();
    const EngPlacementRule vegetation = eng_placement_vegetation_rule();
    EXPECT_TRUE(eng_placement_rule_valid(&grass));
    EXPECT_EQ(grass.density_layer, (int32_t)ENG_SPLAT_GRASS);
    EXPECT_LT(grass.cell_meters, vegetation.cell_meters);
    /* herbe 255 partout, sol plat : une instance par cellule de 1 m dans la marge (124 x 124) */
    const uint32_t n = eng_placement_count(&g_field.map, &g_field.splat, &grass);
    EXPECT_GT(n, 124U * 124U * 9U / 10U);
    EXPECT_LE(n, 124U * 124U);
    /* de la roche (poids 0,2 > 0,1) : plus d'herbe dans la bande */
    for (uint32_t z = 0; z < N; z++) {
        for (uint32_t x = 40; x <= 80; x++) {
            set_pixel(x, z, 200, 55, 0, 0);
        }
    }
    EngArena a;
    ASSERT_TRUE(eng_arena_init(&a, "pl", 4U << 20));
    EngInstance *list = NULL;
    const uint32_t m = generate(&grass, 0.0F, 0.0F, &a, &list);
    ASSERT_NE(m, UINT32_MAX);
    for (uint32_t i = 0; i < m; i++) {
        ASSERT_TRUE(list[i].position[0] < 40.5F || list[i].position[0] > 80.5F);
    }
    eng_arena_shutdown(&a);
}
