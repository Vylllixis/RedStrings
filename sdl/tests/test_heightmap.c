#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>
#include <math.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/asset_format.h"
#include "engine/files.h"
#include "engine/heightmap.h"
#include "utest.h"

/* Champ de hauteurs (M3-01). */

/* PNG produits par Pillow (compression zlib réelle, filtres PNG), pas par notre encodeur :
 * 5 x 5 en 16 bits, valeur = 2600 * (z * 5 + x) ; 3 x 3 en 8 bits, valeur = 30 * (z * 3 + x). */
static const uint8_t PNG16_5X5[104] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44,
    0x52, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05, 0x10, 0x00, 0x00, 0x00, 0x00, 0xF8,
    0x94, 0xA5, 0x7A, 0x00, 0x00, 0x00, 0x2F, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0x64,
    0x60, 0xE0, 0xD2, 0x80, 0x40, 0x16, 0xA3, 0x13, 0x5C, 0x1A, 0xDC, 0x1A, 0x5C, 0x1F, 0xB8,
    0x3E, 0xB0, 0x18, 0x9F, 0xE0, 0xFA, 0xC0, 0x75, 0x82, 0x5B, 0x03, 0xC6, 0xFC, 0xC0, 0x75,
    0x82, 0x4B, 0x03, 0xC6, 0xFC, 0xC0, 0xF5, 0x01, 0x00, 0x6D, 0x71, 0x10, 0xF7, 0x6C, 0xEF,
    0x7C, 0x47, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
};

static const uint8_t PNG8_3X3[77] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x08, 0x00, 0x00, 0x00, 0x00, 0x73, 0x43, 0xEA,
    0x63, 0x00, 0x00, 0x00, 0x14, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0x64, 0x90, 0x93, 0x63,
    0x8C, 0x92, 0x93, 0x63, 0xDC, 0x22, 0x27, 0x07, 0x00, 0x08, 0x94, 0x01, 0xC6, 0x0E, 0x38, 0x8C,
    0xBD, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
};

static bool make_arena(EngArena *arena, size_t bytes)
{
    return eng_arena_init(arena, "test-heightmap", bytes);
}

static EngHeightmapParams small_params(uint32_t size)
{
    EngHeightmapParams p = eng_heightmap_default_params();
    p.size = size;
    p.wavelength = 24.0F;
    return p;
}

UTEST(eng_heightmap, the_default_parameters_are_the_documented_ones_and_valid)
{
    const EngHeightmapParams p = eng_heightmap_default_params();
    EXPECT_EQ(p.size, (uint32_t)1025);
    EXPECT_EQ(p.octaves, (uint32_t)6);
    EXPECT_EQ(p.seed, 1337);
    EXPECT_TRUE(eng_heightmap_params_valid(&p));
    EXPECT_FALSE(eng_heightmap_params_valid(NULL));
}

UTEST(eng_heightmap, every_invalid_parameter_is_refused)
{
    EngHeightmapParams base = eng_heightmap_default_params();
    EngHeightmapParams p = base;
    p.size = 1;
    EXPECT_FALSE(eng_heightmap_params_valid(&p));
    p = base;
    p.size = 2;
    EXPECT_TRUE(eng_heightmap_params_valid(&p));
    p.size = ENG_HEIGHTMAP_MAX_SIZE;
    EXPECT_TRUE(eng_heightmap_params_valid(&p));
    p.size = ENG_HEIGHTMAP_MAX_SIZE + 1U;
    EXPECT_FALSE(eng_heightmap_params_valid(&p));
    p = base;
    p.spacing = 0.0F;
    EXPECT_FALSE(eng_heightmap_params_valid(&p));
    p.spacing = -1.0F;
    EXPECT_FALSE(eng_heightmap_params_valid(&p));
    p.spacing = NAN;
    EXPECT_FALSE(eng_heightmap_params_valid(&p));
    p = base;
    p.altitude_max = p.altitude_min;
    EXPECT_FALSE(eng_heightmap_params_valid(&p));
    p.altitude_max = p.altitude_min - 1.0F;
    EXPECT_FALSE(eng_heightmap_params_valid(&p));
    p = base;
    p.altitude_max = INFINITY;
    EXPECT_FALSE(eng_heightmap_params_valid(&p));
    p = base;
    p.wavelength = 0.0F;
    EXPECT_FALSE(eng_heightmap_params_valid(&p));
    p.wavelength = NAN;
    EXPECT_FALSE(eng_heightmap_params_valid(&p));
    p = base;
    p.octaves = 0;
    EXPECT_FALSE(eng_heightmap_params_valid(&p));
    p.octaves = 12;
    EXPECT_TRUE(eng_heightmap_params_valid(&p));
    p.octaves = 13;
    EXPECT_FALSE(eng_heightmap_params_valid(&p));
    p.octaves = 1;
    EXPECT_TRUE(eng_heightmap_params_valid(&p));
    p = base;
    p.lacunarity = 0.0F;
    EXPECT_FALSE(eng_heightmap_params_valid(&p));
    p.lacunarity = 8.0F;
    EXPECT_TRUE(eng_heightmap_params_valid(&p));
    p.lacunarity = 8.5F;
    EXPECT_FALSE(eng_heightmap_params_valid(&p));
    p = base;
    p.gain = 0.0F;
    EXPECT_FALSE(eng_heightmap_params_valid(&p));
    p.gain = 8.0F;
    EXPECT_TRUE(eng_heightmap_params_valid(&p));
    p.gain = 8.5F;
    EXPECT_FALSE(eng_heightmap_params_valid(&p));
    p = base;
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    p.fractal = (EngNoiseFractal)7;
    EXPECT_FALSE(eng_heightmap_params_valid(&p));
}

UTEST(eng_heightmap, generation_is_deterministic_and_the_seed_changes_the_terrain)
{
    EngArena arena;
    ASSERT_TRUE(make_arena(&arena, 1U << 20));
    const EngHeightmapParams p = small_params(65);
    EngHeightmap a;
    EngHeightmap b;
    ASSERT_TRUE(eng_heightmap_generate(&arena, &p, &a));
    ASSERT_TRUE(eng_heightmap_generate(&arena, &p, &b));
    EXPECT_EQ(memcmp(a.samples, b.samples, (size_t)65 * 65 * sizeof(uint16_t)), 0);

    EngHeightmapParams q = p;
    q.seed = p.seed + 1;
    EngHeightmap c;
    ASSERT_TRUE(eng_heightmap_generate(&arena, &q, &c));
    EXPECT_NE(memcmp(a.samples, c.samples, (size_t)65 * 65 * sizeof(uint16_t)), 0);

    q = p;
    q.fractal = ENG_NOISE_RIDGED;
    ASSERT_TRUE(eng_heightmap_generate(&arena, &q, &c));
    EXPECT_NE(memcmp(a.samples, c.samples, (size_t)65 * 65 * sizeof(uint16_t)), 0);
    eng_arena_shutdown(&arena);
}

UTEST(eng_heightmap, the_terrain_spans_the_whole_16_bit_range_whatever_the_seed)
{
    EngArena arena;
    ASSERT_TRUE(make_arena(&arena, 1U << 20));
    for (int32_t seed = 0; seed < 5; seed++) {
        EngHeightmapParams p = small_params(49);
        p.seed = seed * 7919;
        EngHeightmap map;
        ASSERT_TRUE(eng_heightmap_generate(&arena, &p, &map));
        uint16_t low = 65535;
        uint16_t high = 0;
        for (size_t i = 0; i < (size_t)49 * 49; i++) {
            low = map.samples[i] < low ? map.samples[i] : low;
            high = map.samples[i] > high ? map.samples[i] : high;
        }
        EXPECT_EQ(low, (uint16_t)0);
        EXPECT_EQ(high, (uint16_t)65535);
        eng_arena_reset(&arena);
    }
    eng_arena_shutdown(&arena);
}

UTEST(eng_heightmap, neighbouring_samples_differ_little_so_the_terrain_is_continuous)
{
    EngArena arena;
    ASSERT_TRUE(make_arena(&arena, 1U << 20));
    EngHeightmapParams p = small_params(129);
    p.wavelength = 64.0F;
    EngHeightmap map;
    ASSERT_TRUE(eng_heightmap_generate(&arena, &p, &map));
    int32_t worst = 0;
    for (uint32_t z = 0; z < 129; z++) {
        for (uint32_t x = 0; x + 1 < 129; x++) {
            const int32_t a = map.samples[(z * 129U) + x];
            const int32_t b = map.samples[(z * 129U) + x + 1U];
            worst = SDL_abs(a - b) > worst ? SDL_abs(a - b) : worst;
        }
    }
    /* Un bruit sans continuité (valeurs indépendantes) donnerait des écarts de l'ordre de 65535. */
    EXPECT_LT(worst, 8000);
    EXPECT_GT(worst, 0);
    eng_arena_shutdown(&arena);
}

UTEST(eng_heightmap, the_spacing_scales_the_terrain_in_metres_not_in_samples)
{
    EngArena arena;
    ASSERT_TRUE(make_arena(&arena, 1U << 20));
    EngHeightmapParams p = small_params(33);
    p.wavelength = 32.0F;
    p.spacing = 1.0F;
    EngHeightmap fine;
    ASSERT_TRUE(eng_heightmap_generate(&arena, &p, &fine));
    p.size = 17;
    p.spacing = 2.0F;
    EngHeightmap coarse;
    ASSERT_TRUE(eng_heightmap_generate(&arena, &p, &coarse));
    /* Même monde sur 32 m : un échantillon sur deux, dans les DEUX axes, de la grille fine. Les
     * deux champs sont normalisés séparément (extrema différents) : on compare donc la corrélation,
     * pas les valeurs. Une erreur d'échelle sur z (ou sur x) la ferait chuter. */
    double mean_f = 0.0;
    double mean_c = 0.0;
    for (uint32_t z = 0; z < 17; z++) {
        for (uint32_t x = 0; x < 17; x++) {
            mean_f += fine.samples[(z * 2U * 33U) + (x * 2U)];
            mean_c += coarse.samples[(z * 17U) + x];
        }
    }
    mean_f /= 289.0;
    mean_c /= 289.0;
    double cov = 0.0;
    double var_f = 0.0;
    double var_c = 0.0;
    for (uint32_t z = 0; z < 17; z++) {
        for (uint32_t x = 0; x < 17; x++) {
            const double df = fine.samples[(z * 2U * 33U) + (x * 2U)] - mean_f;
            const double dc = coarse.samples[(z * 17U) + x] - mean_c;
            cov += df * dc;
            var_f += df * df;
            var_c += dc * dc;
        }
    }
    EXPECT_GT(cov / sqrt(var_f * var_c), 0.97);
    eng_arena_shutdown(&arena);
}

/* Spécification de la génération réécrite à part : bruit sur toute la grille, étendue, puis
 * v = (n - min) / (max - min) arrondi au plus proche sur 16 bits. Doit coïncider EXACTEMENT. */
UTEST(eng_heightmap, generation_matches_an_independent_reference_for_many_seeds)
{
    EngArena arena;
    ASSERT_TRUE(make_arena(&arena, 1U << 20));
    enum { N = 9 };
    for (int32_t seed = 1; seed <= 30; seed++) {
        EngHeightmapParams p = small_params(N);
        p.seed = seed;
        p.spacing = 1.5F;
        p.wavelength = 5.0F;
        p.octaves = 3;
        p.fractal = (seed % 2) != 0 ? ENG_NOISE_FBM : ENG_NOISE_RIDGED;
        EngHeightmap map;
        ASSERT_TRUE(eng_heightmap_generate(&arena, &p, &map));

        const EngNoiseParams noise = {.seed = p.seed,
                                      .frequency = 1.0F / p.wavelength,
                                      .octaves = p.octaves,
                                      .lacunarity = p.lacunarity,
                                      .gain = p.gain,
                                      .fractal = p.fractal};
        float values[N * N];
        for (uint32_t z = 0; z < N; z++) {
            eng_noise_row_2d(&noise, 0.0F, p.spacing, (float)z * p.spacing, N,
                             values + ((size_t)z * N));
        }
        float low = values[0];
        float high = values[0];
        for (uint32_t i = 1; i < N * N; i++) {
            low = values[i] < low ? values[i] : low;
            high = values[i] > high ? values[i] : high;
        }
        for (uint32_t i = 0; i < N * N; i++) {
            const float v = (values[i] - low) / (high - low);
            const uint16_t expected = (uint16_t)((v * 65535.0F) + 0.5F);
            ASSERT_EQ(map.samples[i], expected);
        }
        eng_arena_reset(&arena);
    }
    eng_arena_shutdown(&arena);
}

UTEST(eng_heightmap, a_full_arena_fails_cleanly_and_leaves_it_untouched)
{
    EngArena arena;
    ASSERT_TRUE(make_arena(&arena, 1024));
    const EngHeightmapParams p = small_params(65);
    EngHeightmap map = {.size = 9};
    const size_t used = arena.used;
    EXPECT_FALSE(eng_heightmap_generate(&arena, &p, &map));
    EXPECT_EQ(map.size, (uint32_t)0);
    EXPECT_EQ(map.samples, (const uint16_t *)NULL);
    EXPECT_EQ(arena.used, used);
    eng_arena_shutdown(&arena);
}

UTEST(eng_heightmap, generation_takes_the_samples_and_one_working_row_from_the_arena)
{
    EngArena arena;
    ASSERT_TRUE(make_arena(&arena, 1U << 20));
    const EngHeightmapParams p = small_params(65);
    EngHeightmap map;
    ASSERT_TRUE(eng_heightmap_generate(&arena, &p, &map));
    EXPECT_LE(arena.used, ((size_t)65 * 65 * 2) + (65U * sizeof(float)) + ENG_ASSET_ALIGN);
    EXPECT_GE(arena.used, (size_t)65 * 65 * 2);
    EXPECT_EQ((const uint8_t *)map.samples + ((size_t)65 * 65 * 2),
              arena.base + arena.used); /* terminent l'allocation */
    eng_arena_shutdown(&arena);
}

/* Un champ 3 x 3 à la main : z = ligne. valeurs 0, 100, 200 / 300, 400, 500 / 600, 700, 65535. */
static const uint16_t HAND[9] = {0, 100, 200, 300, 400, 500, 600, 700, 65535};

static EngHeightmap hand_map(void)
{
    return (EngHeightmap){
        .size = 3, .spacing = 2.0F, .altitude_min = 10.0F, .altitude_max = 110.0F, .samples = HAND};
}

static float alt(float raw)
{
    return 10.0F + (raw / 65535.0F * 100.0F);
}

UTEST(eng_heightmap, altitude_and_extent_follow_the_documented_formula)
{
    const EngHeightmap map = hand_map();
    EXPECT_NEAR(eng_heightmap_altitude(&map, 0, 0), 10.0F, 1e-5F);
    EXPECT_NEAR(eng_heightmap_altitude(&map, 2, 2), 110.0F, 1e-4F);
    EXPECT_NEAR(eng_heightmap_altitude(&map, 1, 0), alt(100.0F), 1e-4F);
    EXPECT_NEAR(eng_heightmap_altitude(&map, 0, 1), alt(300.0F),
                1e-4F); /* x = colonne, z = ligne */
    EXPECT_NEAR(eng_heightmap_altitude(&map, 2, 1), alt(500.0F), 1e-4F);
    EXPECT_NEAR(eng_heightmap_altitude(&map, 3, 0), 10.0F, 1e-5F); /* hors grille : altitude_min */
    EXPECT_NEAR(eng_heightmap_altitude(&map, 0, 3), 10.0F, 1e-5F);
    EXPECT_NEAR(eng_heightmap_extent(&map), 4.0F, 1e-6F);
}

UTEST(eng_heightmap, sampling_is_exact_on_nodes_and_bilinear_between_them)
{
    const EngHeightmap map = hand_map();
    /* Nœuds : (x_m, z_m) = (2 * x, 2 * z). */
    EXPECT_NEAR(eng_heightmap_sample(&map, 2.0F, 0.0F), alt(100.0F), 1e-4F);
    EXPECT_NEAR(eng_heightmap_sample(&map, 0.0F, 2.0F), alt(300.0F), 1e-4F);
    EXPECT_NEAR(eng_heightmap_sample(&map, 4.0F, 4.0F), 110.0F, 1e-4F);
    /* Milieu de la cellule (0,0) : moyenne de 0, 100, 300, 400. */
    EXPECT_NEAR(eng_heightmap_sample(&map, 1.0F, 1.0F), alt(200.0F), 1e-4F);
    /* Le long d'une arête (z = 0) : interpolation linéaire entre 0 et 100, au quart. */
    EXPECT_NEAR(eng_heightmap_sample(&map, 0.5F, 0.0F), alt(25.0F), 1e-4F);
    /* Le long de x = 0 : entre 0 et 300 au quart (z_m = 0.5). */
    EXPECT_NEAR(eng_heightmap_sample(&map, 0.0F, 0.5F), alt(75.0F), 1e-4F);
    /* Point quelconque de la dernière cellule : (3, 3) m, entre 400, 500, 700, 65535 au milieu. */
    EXPECT_NEAR(eng_heightmap_sample(&map, 3.0F, 3.0F),
                alt((400.0F + 500.0F + 700.0F + 65535.0F) / 4.0F), 1e-3F);
    /* Dissymétrie x / z : (3, 2) m = milieu de l'arête z = 1 entre 400 et 500. */
    EXPECT_NEAR(eng_heightmap_sample(&map, 3.0F, 2.0F), alt(450.0F), 1e-4F);
    EXPECT_NEAR(eng_heightmap_sample(&map, 2.0F, 3.0F), alt(550.0F), 1e-4F);
}

UTEST(eng_heightmap, sampling_outside_the_grid_clamps_to_the_border)
{
    const EngHeightmap map = hand_map();
    EXPECT_NEAR(eng_heightmap_sample(&map, -50.0F, -50.0F), 10.0F, 1e-5F);
    EXPECT_NEAR(eng_heightmap_sample(&map, 50.0F, 50.0F), 110.0F, 1e-4F);
    EXPECT_NEAR(eng_heightmap_sample(&map, 50.0F, 0.0F), alt(200.0F), 1e-4F);
    EXPECT_NEAR(eng_heightmap_sample(&map, 0.0F, 50.0F), alt(600.0F), 1e-4F);
    EXPECT_NEAR(eng_heightmap_sample(&map, NAN, NAN), 10.0F, 1e-5F);
    EXPECT_TRUE(isfinite(eng_heightmap_sample(&map, INFINITY, -INFINITY)));
}

UTEST(eng_heightmap, the_smallest_grid_of_two_by_two_samples_works)
{
    static const uint16_t four[4] = {0, 65535, 65535, 0};
    const EngHeightmap map = {
        .size = 2, .spacing = 1.0F, .altitude_min = 0.0F, .altitude_max = 1.0F, .samples = four};
    EXPECT_NEAR(eng_heightmap_sample(&map, 0.5F, 0.5F), 0.5F, 1e-5F);
    EXPECT_NEAR(eng_heightmap_sample(&map, 1.0F, 0.0F), 1.0F, 1e-5F);
    EXPECT_NEAR(eng_heightmap_sample(&map, 1.0F, 1.0F), 0.0F, 1e-5F);
}

/* ── fichier .hmap ── */

UTEST(eng_heightmap, a_hmap_goes_through_memory_unchanged_and_deterministically)
{
    EngArena arena;
    ASSERT_TRUE(make_arena(&arena, 1U << 20));
    EngHeightmap map;
    const EngHeightmapParams params = small_params(33);
    ASSERT_TRUE(eng_heightmap_generate(&arena, &params, &map));
    const size_t bytes = eng_heightmap_file_size(&map);
    EXPECT_GT(bytes, (size_t)33 * 33 * 2);
    uint8_t *a = eng_arena_alloc(&arena, bytes, 16);
    uint8_t *b = eng_arena_alloc(&arena, bytes, 16);
    ASSERT_TRUE(a != NULL && b != NULL);
    size_t written = 0;
    ASSERT_TRUE(eng_heightmap_encode(&map, a, bytes, &written));
    EXPECT_EQ(written, bytes);
    ASSERT_TRUE(eng_heightmap_encode(&map, b, bytes, NULL));
    EXPECT_EQ(memcmp(a, b, bytes), 0);

    EngHeightmap back;
    ASSERT_TRUE(eng_heightmap_open(a, bytes, "test", &back));
    EXPECT_EQ(back.size, map.size);
    EXPECT_EQ(back.spacing, map.spacing);
    EXPECT_EQ(back.altitude_min, map.altitude_min);
    EXPECT_EQ(back.altitude_max, map.altitude_max);
    EXPECT_EQ(memcmp(back.samples, map.samples, (size_t)33 * 33 * sizeof(uint16_t)), 0);
    EXPECT_TRUE((const uint8_t *)back.samples >= a && (const uint8_t *)back.samples < a + bytes);
    eng_arena_shutdown(&arena);
}

UTEST(eng_heightmap, encoding_into_too_small_a_buffer_or_an_invalid_map_is_refused)
{
    const EngHeightmap map = hand_map();
    const size_t bytes = eng_heightmap_file_size(&map);
    uint8_t big[512];
    ASSERT_LE(bytes, sizeof big);
    EXPECT_FALSE(eng_heightmap_encode(&map, big, bytes - 1U, NULL));
    EXPECT_TRUE(eng_heightmap_encode(&map, big, bytes, NULL));
    EngHeightmap bad = map;
    bad.samples = NULL;
    EXPECT_EQ(eng_heightmap_file_size(&bad), (size_t)0);
    EXPECT_FALSE(eng_heightmap_encode(&bad, big, sizeof big, NULL));
    bad = map;
    bad.size = 1;
    EXPECT_EQ(eng_heightmap_file_size(&bad), (size_t)0);
    bad = map;
    bad.altitude_max = bad.altitude_min;
    EXPECT_EQ(eng_heightmap_file_size(&bad), (size_t)0);
    bad = map;
    bad.spacing = 0.0F;
    EXPECT_EQ(eng_heightmap_file_size(&bad), (size_t)0);
    EXPECT_EQ(eng_heightmap_file_size(NULL), (size_t)0);
}

/* Fabrique un .hmap avec des sections choisies (pour tester les refus du lecteur). */
static size_t build_custom(uint8_t *out, size_t capacity, uint32_t size, float spacing, float lo,
                           float hi, uint32_t info_bytes, uint32_t data_bytes, uint32_t data_count)
{
    uint8_t info[32] = {0};
    memcpy(info, &size, 4);
    memcpy(info + 4, &spacing, 4);
    memcpy(info + 8, &lo, 4);
    memcpy(info + 12, &hi, 4);
    static uint16_t data[4096];
    const EngAssetSectionDesc sections[2] = {
        {.tag = ENG_ASSET_TAG_INFO, .data = info, .size = info_bytes, .count = 1},
        {.tag = ENG_ASSET_TAG_DATA, .data = data, .size = data_bytes, .count = data_count}};
    size_t written = 0;
    return eng_asset_build(ENG_ASSET_MAGIC_HMAP, ENG_ASSET_VERSION_HMAP, sections, 2, out, capacity,
                           &written)
               ? written
               : 0;
}

UTEST(eng_heightmap, the_reader_refuses_every_inconsistent_file)
{
    static _Alignas(16) uint8_t file[16384];
    EngHeightmap out = {.size = 5};
    /* Référence valide : 4 x 4. */
    size_t n = build_custom(file, sizeof file, 4, 1.0F, 0.0F, 10.0F, 16, 32, 16);
    ASSERT_GT(n, (size_t)0);
    EXPECT_TRUE(eng_heightmap_open(file, n, "ok", &out));
    EXPECT_EQ(out.size, (uint32_t)4);

    const struct {
        const char *what;
        uint32_t size;
        float spacing;
        float lo;
        float hi;
        uint32_t info_bytes;
        uint32_t data_bytes;
        uint32_t data_count;
    } bad[] = {
        {"taille 1", 1, 1.0F, 0.0F, 10.0F, 16, 2, 1},
        {"taille 0", 0, 1.0F, 0.0F, 10.0F, 16, 0, 0},
        {"taille > max", 8194, 1.0F, 0.0F, 10.0F, 16, 32, 16},
        {"espacement nul", 4, 0.0F, 0.0F, 10.0F, 16, 32, 16},
        {"espacement NaN", 4, NAN, 0.0F, 10.0F, 16, 32, 16},
        {"altitudes égales", 4, 1.0F, 5.0F, 5.0F, 16, 32, 16},
        {"altitudes inversées", 4, 1.0F, 10.0F, 0.0F, 16, 32, 16},
        {"altitude infinie", 4, 1.0F, 0.0F, INFINITY, 16, 32, 16},
        {"INFO trop courte", 4, 1.0F, 0.0F, 10.0F, 12, 32, 16},
        {"INFO trop longue", 4, 1.0F, 0.0F, 10.0F, 20, 32, 16},
        {"DATA trop courte", 4, 1.0F, 0.0F, 10.0F, 16, 30, 15},
        {"DATA trop longue", 4, 1.0F, 0.0F, 10.0F, 16, 34, 17},
        {"compte faux", 4, 1.0F, 0.0F, 10.0F, 16, 32, 15},
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        n = build_custom(file, sizeof file, bad[i].size, bad[i].spacing, bad[i].lo, bad[i].hi,
                         bad[i].info_bytes, bad[i].data_bytes, bad[i].data_count);
        ASSERT_GT(n, (size_t)0);
        out = (EngHeightmap){.size = 5};
        EXPECT_FALSE(eng_heightmap_open(file, n, bad[i].what, &out));
        EXPECT_EQ(out.size, (uint32_t)0);
        EXPECT_EQ(out.samples, (const uint16_t *)NULL);
    }

    /* Conteneur : tronqué, mauvais magic, version, sections absentes. */
    n = build_custom(file, sizeof file, 4, 1.0F, 0.0F, 10.0F, 16, 32, 16);
    EXPECT_FALSE(eng_heightmap_open(file, n - 1U, "tronqué", &out));
    EXPECT_FALSE(eng_heightmap_open(file, 10, "minuscule", &out));
    EXPECT_FALSE(eng_heightmap_open(NULL, 0, "nul", &out));
    file[0] ^= 1U;
    EXPECT_FALSE(eng_heightmap_open(file, n, "magic", &out));
    file[0] ^= 1U;
    EXPECT_TRUE(eng_heightmap_open(file, n, "rétabli", &out));
    const EngAssetSectionDesc only_info[1] = {
        {.tag = ENG_ASSET_TAG_INFO, .data = file, .size = 16, .count = 1}};
    static _Alignas(16) uint8_t other[256];
    size_t written = 0;
    ASSERT_TRUE(eng_asset_build(ENG_ASSET_MAGIC_HMAP, ENG_ASSET_VERSION_HMAP, only_info, 1, other,
                                sizeof other, &written));
    EXPECT_FALSE(eng_heightmap_open(other, written, "sans DATA", &out));
    ASSERT_TRUE(eng_asset_build(ENG_ASSET_MAGIC_TEX, ENG_ASSET_VERSION_TEX, only_info, 1, other,
                                sizeof other, &written));
    EXPECT_FALSE(eng_heightmap_open(other, written, "magic de .tex", &out));
    ASSERT_TRUE(eng_asset_build(ENG_ASSET_MAGIC_HMAP, ENG_ASSET_VERSION_HMAP + 1U, only_info, 1,
                                other, sizeof other, &written));
    EXPECT_FALSE(eng_heightmap_open(other, written, "version future", &out));
}

#define HMAP_FILE "heightmap_test_file.hmap"

UTEST(eng_heightmap, a_hmap_is_saved_to_and_loaded_from_a_file)
{
    EngArena arena;
    ASSERT_TRUE(make_arena(&arena, 4U << 20));
    EngHeightmap map;
    const EngHeightmapParams params = small_params(129);
    ASSERT_TRUE(eng_heightmap_generate(&arena, &params, &map));
    ASSERT_TRUE(eng_heightmap_save(&arena, &map, HMAP_FILE));
    EngHeightmap back;
    ASSERT_TRUE(eng_heightmap_load(&arena, HMAP_FILE, &back));
    EXPECT_EQ(back.size, (uint32_t)129);
    EXPECT_EQ(memcmp(back.samples, map.samples, (size_t)129 * 129 * sizeof(uint16_t)), 0);
    EXPECT_FALSE(eng_heightmap_load(&arena, "n_existe_pas.hmap", &back));
    EXPECT_FALSE(eng_heightmap_save(&arena, &map, "dossier_inexistant/x.hmap"));
    EngHeightmap bad = map;
    bad.samples = NULL;
    EXPECT_FALSE(eng_heightmap_save(&arena, &bad, HMAP_FILE));
    char resolved[512];
    ASSERT_TRUE(eng_path_resolve(resolved, sizeof resolved, HMAP_FILE));
    (void)SDL_RemovePath(resolved);
    eng_arena_shutdown(&arena);
}

/* ── PNG ── */

UTEST(eng_heightmap, a_16_bit_png_from_an_image_editor_is_imported_exactly)
{
    EngArena arena;
    ASSERT_TRUE(make_arena(&arena, 1U << 20));
    EngHeightmap map;
    ASSERT_TRUE(
        eng_heightmap_import_image(&arena, PNG16_5X5, sizeof PNG16_5X5, 0.5F, -20.0F, 80.0F, &map));
    EXPECT_EQ(map.size, (uint32_t)5);
    EXPECT_EQ(map.spacing, 0.5F);
    EXPECT_EQ(map.altitude_min, -20.0F);
    EXPECT_EQ(map.altitude_max, 80.0F);
    for (uint32_t z = 0; z < 5; z++) {
        for (uint32_t x = 0; x < 5; x++) {
            EXPECT_EQ(map.samples[(z * 5U) + x], (uint16_t)(2600U * ((z * 5U) + x)));
        }
    }
    eng_arena_shutdown(&arena);
}

UTEST(eng_heightmap, an_8_bit_png_is_widened_to_16_bits)
{
    EngArena arena;
    ASSERT_TRUE(make_arena(&arena, 1U << 20));
    EngHeightmap map;
    ASSERT_TRUE(
        eng_heightmap_import_image(&arena, PNG8_3X3, sizeof PNG8_3X3, 1.0F, 0.0F, 10.0F, &map));
    EXPECT_EQ(map.size, (uint32_t)3);
    for (uint32_t i = 0; i < 9; i++) {
        EXPECT_EQ(map.samples[i], (uint16_t)(30U * i * 257U));
    }
    eng_arena_shutdown(&arena);
}

UTEST(eng_heightmap, import_refuses_bad_images_and_bad_ranges)
{
    EngArena arena;
    ASSERT_TRUE(make_arena(&arena, 1U << 20));
    EngHeightmap map = {.size = 3};
    EXPECT_FALSE(eng_heightmap_import_image(&arena, NULL, 0, 1.0F, 0.0F, 1.0F, &map));
    EXPECT_EQ(map.size, (uint32_t)0);
    const uint8_t garbage[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    EXPECT_FALSE(
        eng_heightmap_import_image(&arena, garbage, sizeof garbage, 1.0F, 0.0F, 1.0F, &map));
    EXPECT_FALSE(eng_heightmap_import_image(&arena, PNG16_5X5, sizeof PNG16_5X5 - 10U, 1.0F, 0.0F,
                                            1.0F, &map));
    EXPECT_FALSE(
        eng_heightmap_import_image(&arena, PNG16_5X5, sizeof PNG16_5X5, 0.0F, 0.0F, 1.0F, &map));
    EXPECT_FALSE(
        eng_heightmap_import_image(&arena, PNG16_5X5, sizeof PNG16_5X5, 1.0F, 1.0F, 1.0F, &map));
    EXPECT_FALSE(
        eng_heightmap_import_image(&arena, PNG16_5X5, sizeof PNG16_5X5, 1.0F, 0.0F, NAN, &map));
    /* Arène trop petite pour les échantillons. */
    EngArena tiny;
    ASSERT_TRUE(make_arena(&tiny, 8));
    EXPECT_FALSE(
        eng_heightmap_import_image(&tiny, PNG16_5X5, sizeof PNG16_5X5, 1.0F, 0.0F, 1.0F, &map));
    eng_arena_shutdown(&tiny);
    eng_arena_shutdown(&arena);
}

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint32_t crc32_ref(const uint8_t *data, size_t n)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < n; i++) {
        crc ^= data[i];
        for (int k = 0; k < 8; k++) {
            crc = (crc & 1U) != 0U ? (crc >> 1) ^ 0xEDB88320U : crc >> 1;
        }
    }
    return ~crc;
}

UTEST(eng_heightmap, the_exported_png_is_a_well_formed_16_bit_greyscale_png)
{
    EngArena arena;
    ASSERT_TRUE(make_arena(&arena, 4U << 20));
    EngHeightmap map;
    const EngHeightmapParams params = small_params(129);
    ASSERT_TRUE(eng_heightmap_generate(&arena, &params, &map));
    const void *png = NULL;
    size_t size = 0;
    ASSERT_TRUE(eng_heightmap_encode_png(&arena, &map, &png, &size));
    const uint8_t *p = png;
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    ASSERT_GT(size, (size_t)8);
    EXPECT_EQ(memcmp(p, sig, 8), 0);
    size_t at = 8;
    int chunks = 0;
    bool ended = false;
    while (at + 12U <= size && !ended) {
        const uint32_t length = be32(p + at);
        ASSERT_LE(at + 12U + length, size);
        EXPECT_EQ(be32(p + at + 8U + length), crc32_ref(p + at + 4U, 4U + length));
        if (chunks == 0) {
            EXPECT_EQ(memcmp(p + at + 4U, "IHDR", 4), 0);
            EXPECT_EQ(length, (uint32_t)13);
            EXPECT_EQ(be32(p + at + 8U), (uint32_t)129);
            EXPECT_EQ(be32(p + at + 12U), (uint32_t)129);
            EXPECT_EQ(p[at + 16U], (uint8_t)16); /* profondeur */
            EXPECT_EQ(p[at + 17U], (uint8_t)0);  /* gris */
        }
        ended = memcmp(p + at + 4U, "IEND", 4) == 0;
        at += 12U + length;
        chunks++;
    }
    EXPECT_TRUE(ended);
    EXPECT_EQ(at, size);
    EXPECT_EQ(chunks, 3);
    eng_arena_shutdown(&arena);
}

static uint32_t adler32_ref(const uint8_t *data, size_t n)
{
    uint32_t a = 1U;
    uint32_t b = 0U;
    for (size_t i = 0; i < n; i++) {
        a = (a + data[i]) % 65521U;
        b = (b + a) % 65521U;
    }
    return (b << 16) | a;
}

/* Parcourt le flux zlib de l'IDAT : en-tête valide, blocs non compressés bien formés (LEN et NLEN
 * complémentaires, BFINAL seulement sur le dernier), Adler-32 du flux brut correct, flux brut =
 * lignes de (filtre 0 + size échantillons grand-boutistes). stb_image ne vérifie pas l'Adler-32 :
 * c'est ce test qui le fait. */
UTEST(eng_heightmap, the_png_zlib_stream_has_well_formed_stored_blocks_and_a_correct_adler32)
{
    EngArena arena;
    ASSERT_TRUE(make_arena(&arena, 8U << 20));
    const uint32_t sizes[] = {2, 33, 257}; /* 257 : trois blocs */
    for (size_t k = 0; k < sizeof sizes / sizeof sizes[0]; k++) {
        EngHeightmapParams params = small_params(sizes[k]);
        params.wavelength = 40.0F;
        EngHeightmap map;
        ASSERT_TRUE(eng_heightmap_generate(&arena, &params, &map));
        const void *png = NULL;
        size_t png_size = 0;
        ASSERT_TRUE(eng_heightmap_encode_png(&arena, &map, &png, &png_size));
        const uint8_t *p = png;
        const size_t idat = 8U + 12U + 13U; /* signature + IHDR (12 + 13) */
        const uint32_t zlib_bytes = be32(p + idat);
        ASSERT_EQ(memcmp(p + idat + 4U, "IDAT", 4), 0);
        const uint8_t *z = p + idat + 8U;
        ASSERT_EQ(z[0], (uint8_t)0x78);
        EXPECT_EQ((((uint32_t)z[0] << 8) | z[1]) % 31U, (uint32_t)0);
        size_t at = 2;
        size_t raw_total = 0;
        const size_t raw_expected = (size_t)sizes[k] * (1U + (sizes[k] * 2U));
        uint8_t *raw = eng_arena_alloc(&arena, raw_expected, 1);
        ASSERT_TRUE(raw != NULL);
        bool last = false;
        while (!last) {
            ASSERT_LE(at + 5U, (size_t)zlib_bytes);
            last = (z[at] & 1U) != 0U;
            EXPECT_EQ((uint32_t)(z[at] & 0xFEU), (uint32_t)0); /* type 00 : non compressé */
            const uint32_t len = (uint32_t)z[at + 1U] | ((uint32_t)z[at + 2U] << 8);
            const uint32_t nlen = (uint32_t)z[at + 3U] | ((uint32_t)z[at + 4U] << 8);
            EXPECT_EQ(len ^ nlen, (uint32_t)0xFFFF);
            EXPECT_LE(len, (uint32_t)65535);
            ASSERT_LE(raw_total + len, raw_expected);
            ASSERT_LE(at + 5U + len, (size_t)zlib_bytes);
            memcpy(raw + raw_total, z + at + 5U, len);
            raw_total += len;
            at += 5U + len;
        }
        EXPECT_EQ(raw_total, raw_expected);
        EXPECT_EQ(at + 4U, (size_t)zlib_bytes);
        EXPECT_EQ(be32(z + at), adler32_ref(raw, raw_total));
        /* Contenu : filtre 0 puis échantillons grand-boutistes. */
        for (uint32_t row = 0; row < sizes[k]; row++) {
            const uint8_t *line = raw + ((size_t)row * (1U + (sizes[k] * 2U)));
            ASSERT_EQ(line[0], (uint8_t)0);
            for (uint32_t col = 0; col < sizes[k]; col++) {
                const uint16_t v =
                    (uint16_t)(((uint16_t)line[1U + (col * 2U)] << 8) | line[2U + (col * 2U)]);
                ASSERT_EQ(v, map.samples[((size_t)row * sizes[k]) + col]);
            }
        }
        eng_arena_reset(&arena);
    }
    eng_arena_shutdown(&arena);
}

UTEST(eng_heightmap, a_png_export_imports_back_to_the_same_samples_for_several_sizes)
{
    EngArena arena;
    ASSERT_TRUE(make_arena(&arena, 8U << 20));
    /* 257 : le flux brut dépasse 65 535 octets (plusieurs blocs non compressés). */
    const uint32_t sizes[] = {2, 3, 33, 257};
    for (size_t k = 0; k < sizeof sizes / sizeof sizes[0]; k++) {
        EngHeightmapParams p = small_params(sizes[k]);
        p.wavelength = 40.0F;
        EngHeightmap map;
        ASSERT_TRUE(eng_heightmap_generate(&arena, &p, &map));
        const void *png = NULL;
        size_t png_size = 0;
        ASSERT_TRUE(eng_heightmap_encode_png(&arena, &map, &png, &png_size));
        EngHeightmap back;
        ASSERT_TRUE(eng_heightmap_import_image(&arena, png, png_size, p.spacing, p.altitude_min,
                                               p.altitude_max, &back));
        EXPECT_EQ(back.size, sizes[k]);
        EXPECT_EQ(memcmp(back.samples, map.samples, (size_t)sizes[k] * sizes[k] * 2U), 0);
        eng_arena_reset(&arena);
    }
    eng_arena_shutdown(&arena);
}

UTEST(eng_heightmap, png_export_refuses_an_invalid_map_and_a_full_arena)
{
    EngArena arena;
    ASSERT_TRUE(make_arena(&arena, 64));
    const EngHeightmap map = hand_map();
    const void *png = (const void *)1;
    size_t size = 9;
    EXPECT_FALSE(eng_heightmap_encode_png(&arena, &map, &png, &size));
    EXPECT_EQ(png, (const void *)NULL);
    EXPECT_EQ(size, (size_t)0);
    EngHeightmap bad = map;
    bad.samples = NULL;
    EXPECT_FALSE(eng_heightmap_encode_png(&arena, &bad, &png, &size));
    eng_arena_shutdown(&arena);
}

/* ── critère d'acceptation : 1025 x 1025 générée et sauvegardée ── */

UTEST(eng_heightmap, a_1025_by_1025_heightmap_is_generated_saved_and_read_back)
{
    EngArena arena;
    ASSERT_TRUE(make_arena(&arena, 16U << 20));
    const EngHeightmapParams p = eng_heightmap_default_params();
    EngHeightmap map;
    ASSERT_TRUE(eng_heightmap_generate(&arena, &p, &map));
    EXPECT_EQ(map.size, (uint32_t)1025);
    EXPECT_NEAR(eng_heightmap_extent(&map), 1024.0F, 1e-3F); /* 1 km, soit 16 chunks de 64 m */
    uint16_t low = 65535;
    uint16_t high = 0;
    for (size_t i = 0; i < (size_t)1025 * 1025; i++) {
        low = map.samples[i] < low ? map.samples[i] : low;
        high = map.samples[i] > high ? map.samples[i] : high;
    }
    EXPECT_EQ(low, (uint16_t)0);
    EXPECT_EQ(high, (uint16_t)65535);

    ASSERT_TRUE(eng_heightmap_save(&arena, &map, HMAP_FILE));
    EngHeightmap back;
    ASSERT_TRUE(eng_heightmap_load(&arena, HMAP_FILE, &back));
    EXPECT_EQ(back.size, (uint32_t)1025);
    EXPECT_EQ(memcmp(back.samples, map.samples, (size_t)1025 * 1025 * sizeof(uint16_t)), 0);
    EXPECT_NEAR(eng_heightmap_sample(&back, 512.0F, 512.0F),
                eng_heightmap_sample(&map, 512.0F, 512.0F), 0.0F);
    char resolved[512];
    ASSERT_TRUE(eng_path_resolve(resolved, sizeof resolved, HMAP_FILE));
    (void)SDL_RemovePath(resolved);
    eng_arena_shutdown(&arena);
}
