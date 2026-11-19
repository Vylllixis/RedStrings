#include <math.h>
#include <stdint.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/water.h"
#include "utest.h"

#define N 17U

static uint16_t g_samples[N * N];
static EngHeightmap g_map;

/* champ 17 x 17 de 16 m, altitudes de -10 à 10 m (valeur 0 : -10, 65535 : 10) ; `fn(x, z)` donne
 * l'altitude voulue */
static uint16_t quant(float altitude)
{
    const float rel = fminf(fmaxf((altitude + 10.0F) / 20.0F, 0.0F), 1.0F); /* bornée au champ */
    return (uint16_t)(rel * 65535.0F + 0.5F);
}

static void make_map(float (*fn)(float, float))
{
    for (uint32_t z = 0; z < N; z++) {
        for (uint32_t x = 0; x < N; x++) {
            g_samples[((size_t)z * N) + x] = quant(fn((float)x, (float)z));
        }
    }
    g_map = (EngHeightmap){.size = N,
                           .spacing = 1.0F,
                           .altitude_min = -10.0F,
                           .altitude_max = 10.0F,
                           .samples = g_samples};
}

static bool same3(const float a[3], const float b[3])
{
    return fabsf(a[0] - b[0]) <= 0.0F && fabsf(a[1] - b[1]) <= 0.0F && fabsf(a[2] - b[2]) <= 0.0F;
}

static float bowl(float x, float z)
{
    const float dx = x - 8.0F;
    const float dz = z - 8.0F;
    return -3.0F + (0.12F * ((dx * dx) + (dz * dz))); /* creux de -3 m au centre */
}

static float dry(float x, float z)
{
    (void)x;
    (void)z;
    return 5.0F;
}

static float wet(float x, float z)
{
    (void)x;
    (void)z;
    return -4.0F;
}

UTEST(eng_water, the_default_is_valid_and_each_bad_field_is_rejected)
{
    EngWaterParams p = eng_water_default(2.0F);
    EXPECT_TRUE(eng_water_valid(&p));
    EXPECT_EQ(p.level, 2.0F);
    EXPECT_FALSE(eng_water_valid(NULL));
    EngWaterParams b;
#define BAD(field, value)                                                                          \
    b = p;                                                                                         \
    b.field = (value);                                                                             \
    EXPECT_FALSE(eng_water_valid(&b))
#define OK(field, value)                                                                           \
    b = p;                                                                                         \
    b.field = (value);                                                                             \
    EXPECT_TRUE(eng_water_valid(&b))
    BAD(level, NAN);
    BAD(shallow_color[0], -0.1F);
    BAD(shallow_color[2], NAN);
    BAD(deep_color[1], -1.0F);
    BAD(deep_color[0], INFINITY);
    BAD(absorption, 0.0F);
    BAD(absorption, NAN);
    BAD(f0, -0.1F);
    BAD(f0, 1.1F);
    BAD(ripple_amplitude, -0.1F);
    BAD(ripple_amplitude, 0.6F);
    BAD(ripple_scale, 0.0F);
    BAD(ripple_speed, -1.0F);
    OK(f0, 0.0F);
    OK(f0, 1.0F);
    OK(ripple_amplitude, 0.0F);
    OK(ripple_amplitude, 0.5F);
    OK(ripple_speed, 0.0F);
    OK(level, -50.0F);
#undef BAD
#undef OK
}

UTEST(eng_water, body_colour_goes_from_shallow_to_deep_with_the_absorption_law)
{
    EngWaterParams p = eng_water_default(0.0F);
    p.absorption = 0.5F;
    float c[3];
    eng_water_body_color(&p, 0.0F, c);
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(c[i], p.shallow_color[i]);
    }
    eng_water_body_color(&p, -3.0F, c); /* profondeur négative : comme 0 */
    EXPECT_EQ(c[1], p.shallow_color[1]);
    eng_water_body_color(&p, 2.0F, c); /* t = 1 - e^-1 */
    const float t = 1.0F - expf(-1.0F);
    for (int i = 0; i < 3; i++) {
        EXPECT_NEAR(c[i], p.shallow_color[i] + ((p.deep_color[i] - p.shallow_color[i]) * t), 1e-6F);
    }
    eng_water_body_color(&p, 100.0F, c);
    for (int i = 0; i < 3; i++) {
        EXPECT_NEAR(c[i], p.deep_color[i], 1e-6F);
    }
    float prev = p.shallow_color[1];
    for (int k = 1; k < 40; k++) {
        const float d = 0.5F * (float)k;
        eng_water_body_color(&p, d, c);
        EXPECT_LE(c[1], prev + 1e-7F); /* le vert baisse (peu profond 0,32 -> profond 0,06) */
        prev = c[1];
    }
}

UTEST(eng_water, fresnel_is_f0_at_normal_incidence_one_at_grazing_and_monotone)
{
    EXPECT_NEAR(eng_water_fresnel(0.02F, 1.0F), 0.02F, 1e-7F);
    EXPECT_NEAR(eng_water_fresnel(0.02F, 0.0F), 1.0F, 1e-7F);
    EXPECT_NEAR(eng_water_fresnel(0.02F, 0.5F), 0.02F + (0.98F * powf(0.5F, 5.0F)), 1e-6F);
    EXPECT_NEAR(eng_water_fresnel(0.02F, 2.0F), 0.02F, 1e-7F); /* borné */
    EXPECT_NEAR(eng_water_fresnel(0.02F, -1.0F), 1.0F, 1e-7F);
    float prev = 2.0F;
    for (int k = 0; k <= 20; k++) {
        const float f = eng_water_fresnel(0.04F, 0.05F * (float)k);
        EXPECT_LE(f, prev);
        prev = f;
    }
}

UTEST(eng_water, the_normal_is_flat_without_ripples_unit_and_bounded_with_them)
{
    EngWaterParams p = eng_water_default(0.0F);
    p.ripple_amplitude = 0.0F;
    float n[3];
    eng_water_normal(&p, 3.0F, 4.0F, 1.0, n);
    EXPECT_EQ(n[0], 0.0F);
    EXPECT_EQ(n[1], 1.0F);
    EXPECT_EQ(n[2], 0.0F);
    p.ripple_amplitude = 0.2F;
    const float max_gradient = 1.5F * 0.2F; /* première ride + moitié de la seconde */
    const float min_y = 1.0F / sqrtf(1.0F + (max_gradient * max_gradient));
    bool varies = false;
    float first[3] = {0.0F, 0.0F, 0.0F};
    for (int i = 0; i < 200; i++) {
        eng_water_normal(&p, 0.37F * (float)i, 0.91F * (float)i, 0.1 * (double)i, n);
        ASSERT_NEAR((n[0] * n[0]) + (n[1] * n[1]) + (n[2] * n[2]), 1.0F, 1e-5F);
        ASSERT_GE(n[1], min_y - 1e-6F);
        if (i == 0) {
            memcpy(first, n, sizeof first);
        } else if (fabsf(n[0] - first[0]) > 0.01F) {
            varies = true;
        }
    }
    EXPECT_TRUE(varies);
    /* même point, même instant : même normale ; le temps la fait bouger */
    float a[3];
    float b[3];
    float c[3];
    eng_water_normal(&p, 5.0F, 7.0F, 2.0, a);
    eng_water_normal(&p, 5.0F, 7.0F, 2.0, b);
    eng_water_normal(&p, 5.0F, 7.0F, 2.7, c);
    EXPECT_TRUE(same3(a, b));
    EXPECT_FALSE(same3(a, c));
    /* sans vitesse, le temps ne change rien */
    p.ripple_speed = 0.0F;
    eng_water_normal(&p, 5.0F, 7.0F, 2.0, a);
    eng_water_normal(&p, 5.0F, 7.0F, 99.0, c);
    EXPECT_TRUE(same3(a, c));
    /* la première ride pousse la normale à l'opposé de sa pente : le long de (0,8, 0,6) */
    p.ripple_amplitude = 0.3F;
    p.ripple_speed = 0.0F;
    p.ripple_scale = 1.0F;
    eng_water_normal(&p, 0.0F, 0.0F, 0.0, a); /* w1 = 1, w2 = 1 */
    const float gx = 0.3F * ((0.8F) + (0.5F * -0.6F));
    const float gz = 0.3F * ((0.6F) + (0.5F * 0.8F));
    const float len = sqrtf((gx * gx) + 1.0F + (gz * gz));
    EXPECT_NEAR(a[0], -gx / len, 1e-6F);
    EXPECT_NEAR(a[1], 1.0F / len, 1e-6F);
    EXPECT_NEAR(a[2], -gz / len, 1e-6F);
}

UTEST(eng_water, shade_composes_body_fresnel_sky_and_sun_as_documented)
{
    EngWaterParams p = eng_water_default(0.0F);
    p.ripple_amplitude = 0.0F; /* normale (0, 1, 0) */
    const EngWaterSky sky = {.horizon = {0.8F, 0.7F, 0.6F}, .zenith = {0.2F, 0.3F, 0.9F}};
    EngWaterLight light = {.sun_direction = {0.0F, 1.0F, 0.0F},
                           .sun_radiance = {3.0F, 2.0F, 1.0F},
                           .ambient = {0.1F, 0.2F, 0.3F}};
    const float world[3] = {0.0F, 0.0F, 0.0F};
    /* caméra à la verticale : nv = 1, F = f0 ; r = (0, 1, 0) : ciel = zénith, soleil au zénith :
     * rl = 1, nl = 1 */
    const float above[3] = {0.0F, 10.0F, 0.0F};
    float out[3];
    eng_water_shade(&p, &sky, &light, above, world, 2.0F, 0.0, out);
    float body[3];
    eng_water_body_color(&p, 2.0F, body);
    for (int c = 0; c < 3; c++) {
        const float lit = body[c] * (light.ambient[c] + (light.sun_radiance[c] / 3.14159265F));
        const float expected =
            lit + ((sky.zenith[c] - lit) * p.f0) + (light.sun_radiance[c] * p.f0);
        EXPECT_NEAR(out[c], expected, 1e-5F);
    }
    /* soleil à l'horizon, caméra à la verticale : pas d'éclairage direct, pas de reflet du soleil
     */
    light.sun_direction[0] = 1.0F;
    light.sun_direction[1] = 0.0F;
    eng_water_shade(&p, &sky, &light, above, world, 2.0F, 0.0, out);
    for (int c = 0; c < 3; c++) {
        const float lit = body[c] * light.ambient[c];
        EXPECT_NEAR(out[c], lit + ((sky.zenith[c] - lit) * p.f0), 1e-5F);
    }
    /* regard rasant : F tend vers 1, la couleur tend vers le ciel à l'horizon (r_y = 0) */
    const float grazing[3] = {1000.0F, 0.0001F, 0.0F};
    eng_water_shade(&p, &sky, &light, grazing, world, 2.0F, 0.0, out);
    for (int c = 0; c < 3; c++) {
        EXPECT_NEAR(out[c], sky.horizon[c], 0.02F);
    }
    /* reflet du soleil : le miroir de la direction du soleil donne un éclat net, ailleurs rien */
    light.sun_direction[0] = 0.6F;
    light.sun_direction[1] = 0.8F;
    light.sun_direction[2] = 0.0F;
    const float mirror[3] = {-6.0F, 8.0F,
                             0.0F}; /* v = (-0,6, 0,8, 0) : r = (0,6, 0,8, 0) = soleil */
    float with_spec[3];
    eng_water_shade(&p, &sky, &light, mirror, world, 2.0F, 0.0, with_spec);
    const float off[3] = {6.0F, 8.0F, 0.0F};
    float without[3];
    eng_water_shade(&p, &sky, &light, off, world, 2.0F, 0.0, without);
    EXPECT_GT(with_spec[0], without[0] + 0.5F * light.sun_radiance[0] * 0.02F);
    const float nv = 0.8F;
    const float fresnel = eng_water_fresnel(p.f0, nv);
    float expected_spec = light.sun_radiance[0] * fresnel; /* rl = 1 */
    EXPECT_GT(with_spec[0] - without[0], expected_spec * 0.5F);
    /* caméra à la position du point : vecteur nul -> repli vertical, pas de NaN */
    eng_water_shade(&p, &sky, &light, world, world, 2.0F, 0.0, out);
    for (int c = 0; c < 3; c++) {
        EXPECT_TRUE(isfinite(out[c]));
    }
}

UTEST(eng_water, pack_fills_the_documented_layout)
{
    EngWaterParams p = eng_water_default(0.0F);
    p.ripple_speed = 1.0F;
    p.ripple_scale = 2.0F;
    const EngWaterSky sky = {.horizon = {0.1F, 0.2F, 0.3F}, .zenith = {0.4F, 0.5F, 0.6F}};
    float out[ENG_WATER_UNIFORM_FLOATS];
    ASSERT_TRUE(eng_water_pack(&p, &sky, 1.0, out));
    EXPECT_EQ(out[0], 0.1F);
    EXPECT_EQ(out[2], 0.3F);
    EXPECT_EQ(out[3], p.f0);
    EXPECT_EQ(out[4], 0.4F);
    EXPECT_EQ(out[6], 0.6F);
    EXPECT_EQ(out[7], p.absorption);
    EXPECT_EQ(out[8], p.shallow_color[0]);
    EXPECT_EQ(out[10], p.shallow_color[2]);
    EXPECT_EQ(out[11], p.ripple_amplitude);
    EXPECT_EQ(out[12], p.deep_color[0]);
    EXPECT_EQ(out[14], p.deep_color[2]);
    EXPECT_NEAR(out[15], 1.0F, 1e-6F); /* 1 rad/s x 1 s */
    EXPECT_EQ(out[16], 2.0F);
    EXPECT_NEAR(out[17], 1.3F, 1e-6F);
    EXPECT_EQ(out[18], 0.0F);
    EXPECT_EQ(out[19], 0.0F);
    /* phases ramenées dans [0, 2 pi[ */
    ASSERT_TRUE(eng_water_pack(&p, &sky, 1000.0, out));
    EXPECT_GE(out[15], 0.0F);
    EXPECT_LT(out[15], 6.2831854F);
    EXPECT_GE(out[17], 0.0F);
    EXPECT_LT(out[17], 6.2831854F);
    EXPECT_NEAR(out[15], fmodf(1000.0F, 6.2831853F), 2e-3F);
    EngWaterParams bad = p;
    bad.absorption = 0.0F;
    EXPECT_FALSE(eng_water_pack(&bad, &sky, 0.0, out));
    EXPECT_FALSE(eng_water_pack(&p, &sky, (double)NAN, out));
    EngWaterSky bad_sky = sky;
    bad_sky.zenith[0] = NAN;
    EXPECT_FALSE(eng_water_pack(&p, &bad_sky, 0.0, out));
    bad_sky = sky;
    bad_sky.horizon[1] = -1.0F;
    EXPECT_FALSE(eng_water_pack(&p, &bad_sky, 0.0, out));
}

UTEST(eng_water, build_covers_exactly_the_wet_cells_with_depths_at_the_vertices)
{
    make_map(bowl);
    EngWaterParams p = eng_water_default(0.0F);
    EngArena a;
    ASSERT_TRUE(eng_arena_init(&a, "water", 4U << 20));
    EngModel m;
    ASSERT_TRUE(eng_water_build(&a, &g_map, &p, 100.0F, -50.0F, &m));
    const uint32_t wet_cells = eng_water_wet_cells(&g_map, &p);
    ASSERT_GT(wet_cells, 20U);
    ASSERT_LT(wet_cells, (N - 1U) * (N - 1U));
    EXPECT_EQ(m.index_count, wet_cells * 6U);
    EXPECT_EQ(m.part_count, 1U);
    EXPECT_EQ(m.parts[0].index_count, m.index_count);
    EXPECT_EQ(m.material_count, 1U);
    /* toutes les cellules mouillées : au moins un coin sous le niveau */
    uint32_t expected = 0;
    for (uint32_t z = 0; z + 1U < N; z++) {
        for (uint32_t x = 0; x + 1U < N; x++) {
            bool any = false;
            for (uint32_t k = 0; k < 4; k++) {
                any = any || eng_heightmap_altitude(&g_map, x + (k & 1U), z + (k >> 1U)) < 0.0F;
            }
            expected += any ? 1U : 0U;
        }
    }
    EXPECT_EQ(wet_cells, expected);
    for (uint32_t i = 0; i < m.vertex_count; i++) {
        const EngModelVertex *v = &m.vertices[i];
        ASSERT_EQ(v->position[1], 0.0F);
        ASSERT_EQ(v->normal[1], 1.0F);
        const float sx = v->position[0] - 100.0F;
        const float sz = v->position[2] + 50.0F;
        const float altitude = eng_heightmap_altitude(&g_map, (uint32_t)sx, (uint32_t)sz);
        ASSERT_NEAR(v->uv[0], altitude < 0.0F ? -altitude : 0.0F, 1e-3F);
        ASSERT_EQ(v->uv[1], 0.0F);
    }
    for (uint32_t t = 0; t < m.index_count; t += 3) {
        const EngModelVertex *p0 = &m.vertices[m.indices[t]];
        const EngModelVertex *p1 = &m.vertices[m.indices[t + 1]];
        const EngModelVertex *p2 = &m.vertices[m.indices[t + 2]];
        ASSERT_LT(m.indices[t], m.vertex_count);
        ASSERT_LT(m.indices[t + 1], m.vertex_count);
        ASSERT_LT(m.indices[t + 2], m.vertex_count);
        /* anti-horaire vu du dessus : la normale (b - a) x (c - a) pointe vers +Y */
        const float e1x = p1->position[0] - p0->position[0];
        const float e1z = p1->position[2] - p0->position[2];
        const float e2x = p2->position[0] - p0->position[0];
        const float e2z = p2->position[2] - p0->position[2];
        ASSERT_GT((e1z * e2x) - (e1x * e2z), 0.0F);
    }
    /* le creux est au centre : la profondeur y vaut 3 m */
    float deepest = 0.0F;
    for (uint32_t i = 0; i < m.vertex_count; i++) {
        deepest = fmaxf(deepest, m.vertices[i].uv[0]);
    }
    EXPECT_NEAR(deepest, 3.0F, 1e-3F);
    EXPECT_LE(m.bounds_min[0], 100.0F + 8.0F);
    EXPECT_GE(m.bounds_max[0], 100.0F + 8.0F);
    EXPECT_EQ(m.bounds_min[1], 0.0F);
    EXPECT_EQ(m.bounds_max[1], 0.0F);
    eng_arena_shutdown(&a);
}

UTEST(eng_water, a_dry_field_gives_an_empty_model_and_a_fully_wet_one_covers_everything)
{
    EngArena a;
    ASSERT_TRUE(eng_arena_init(&a, "water", 4U << 20));
    EngWaterParams p = eng_water_default(0.0F);
    EngModel m;
    make_map(dry);
    const size_t before = eng_arena_remaining(&a);
    EXPECT_TRUE(eng_water_build(&a, &g_map, &p, 0.0F, 0.0F, &m));
    EXPECT_EQ(m.vertex_count, 0U);
    EXPECT_EQ(m.index_count, 0U);
    EXPECT_EQ(eng_water_wet_cells(&g_map, &p), 0U);
    EXPECT_EQ(eng_arena_remaining(&a), before);
    make_map(wet);
    ASSERT_TRUE(eng_water_build(&a, &g_map, &p, 0.0F, 0.0F, &m));
    EXPECT_EQ(m.vertex_count, N * N);
    EXPECT_EQ(m.index_count, (N - 1U) * (N - 1U) * 6U);
    EXPECT_EQ(eng_water_wet_cells(&g_map, &p), (N - 1U) * (N - 1U));
    /* niveau juste à l'altitude du terrain : sec (strictement sous le niveau) */
    EngWaterParams at = p;
    at.level = -4.0F;
    EXPECT_EQ(eng_water_wet_cells(&g_map, &at), 0U);
    eng_arena_shutdown(&a);
}

UTEST(eng_water, build_failures_restore_the_arena_and_zero_the_model)
{
    make_map(wet);
    EngArena a;
    ASSERT_TRUE(eng_arena_init(&a, "water", 1U << 20));
    EngWaterParams p = eng_water_default(0.0F);
    EngModel m;
    const size_t before = eng_arena_remaining(&a);
    EngWaterParams bad = p;
    bad.absorption = -1.0F;
    EXPECT_FALSE(eng_water_build(&a, &g_map, &bad, 0.0F, 0.0F, &m));
    EXPECT_EQ(m.vertex_count, 0U);
    EXPECT_EQ(eng_water_wet_cells(&g_map, &bad), 0U);
    EXPECT_FALSE(eng_water_build(&a, NULL, &p, 0.0F, 0.0F, &m));
    EXPECT_EQ(eng_water_wet_cells(NULL, &p), 0U);
    EXPECT_FALSE(eng_water_build(&a, &g_map, &p, NAN, 0.0F, &m));
    EXPECT_FALSE(eng_water_build(&a, &g_map, &p, 0.0F, INFINITY, &m));
    EngHeightmap broken = g_map;
    broken.samples = NULL;
    EXPECT_FALSE(eng_water_build(&a, &broken, &p, 0.0F, 0.0F, &m));
    broken = g_map;
    broken.spacing = 0.0F;
    EXPECT_FALSE(eng_water_build(&a, &broken, &p, 0.0F, 0.0F, &m));
    broken = g_map;
    broken.altitude_max = broken.altitude_min;
    EXPECT_FALSE(eng_water_build(&a, &broken, &p, 0.0F, 0.0F, &m));
    broken = g_map;
    broken.size = 1;
    EXPECT_FALSE(eng_water_build(&a, &broken, &p, 0.0F, 0.0F, &m));
    EXPECT_EQ(eng_arena_remaining(&a), before);
    eng_arena_shutdown(&a);
    EngArena tiny;
    ASSERT_TRUE(eng_arena_init(&tiny, "tiny", 512));
    const size_t tiny_before = eng_arena_remaining(&tiny);
    EXPECT_FALSE(eng_water_build(&tiny, &g_map, &p, 0.0F, 0.0F, &m));
    EXPECT_EQ(eng_arena_remaining(&tiny), tiny_before);
    eng_arena_shutdown(&tiny);
}

UTEST(eng_water, every_channel_and_scalar_is_checked_for_validity)
{
    const EngWaterParams p = eng_water_default(1.0F);
    for (int c = 0; c < 3; c++) {
        EngWaterParams b = p;
        b.shallow_color[c] = NAN;
        EXPECT_FALSE(eng_water_valid(&b));
        b = p;
        b.shallow_color[c] = -0.01F;
        EXPECT_FALSE(eng_water_valid(&b));
        b = p;
        b.shallow_color[c] = INFINITY;
        EXPECT_FALSE(eng_water_valid(&b));
        b = p;
        b.deep_color[c] = NAN;
        EXPECT_FALSE(eng_water_valid(&b));
        b = p;
        b.deep_color[c] = -0.01F;
        EXPECT_FALSE(eng_water_valid(&b));
        b = p;
        b.deep_color[c] = INFINITY;
        EXPECT_FALSE(eng_water_valid(&b));
    }
    EngWaterParams b = p;
    b.level = INFINITY;
    EXPECT_FALSE(eng_water_valid(&b));
    b = p;
    b.absorption = INFINITY;
    EXPECT_FALSE(eng_water_valid(&b));
    b = p;
    b.absorption = -1.0F;
    EXPECT_FALSE(eng_water_valid(&b));
    b = p;
    b.f0 = NAN;
    EXPECT_FALSE(eng_water_valid(&b));
    b = p;
    b.ripple_amplitude = NAN;
    EXPECT_FALSE(eng_water_valid(&b));
    b = p;
    b.ripple_scale = NAN;
    EXPECT_FALSE(eng_water_valid(&b));
    b = p;
    b.ripple_scale = -1.0F;
    EXPECT_FALSE(eng_water_valid(&b));
    b = p;
    b.ripple_speed = NAN;
    EXPECT_FALSE(eng_water_valid(&b));
    b = p;
    b.ripple_speed = INFINITY;
    EXPECT_FALSE(eng_water_valid(&b));
    /* le ciel de eng_water_pack : chaque canal fini et positif */
    const EngWaterSky sky = {.horizon = {0.5F, 0.6F, 0.7F}, .zenith = {0.1F, 0.2F, 0.3F}};
    float out[ENG_WATER_UNIFORM_FLOATS];
    ASSERT_TRUE(eng_water_pack(&p, &sky, 0.0, out));
    for (int c = 0; c < 3; c++) {
        EngWaterSky s = sky;
        s.horizon[c] = NAN;
        EXPECT_FALSE(eng_water_pack(&p, &s, 0.0, out));
        s = sky;
        s.horizon[c] = -0.5F;
        EXPECT_FALSE(eng_water_pack(&p, &s, 0.0, out));
        s = sky;
        s.zenith[c] = NAN;
        EXPECT_FALSE(eng_water_pack(&p, &s, 0.0, out));
        s = sky;
        s.zenith[c] = -0.5F;
        EXPECT_FALSE(eng_water_pack(&p, &s, 0.0, out));
        s = sky;
        s.zenith[c] = INFINITY;
        EXPECT_FALSE(eng_water_pack(&p, &s, 0.0, out));
    }
}

UTEST(eng_water, the_mesh_is_exact_for_a_two_metre_spacing_and_an_offset_origin)
{
    make_map(bowl);
    EngHeightmap map = g_map;
    map.spacing = 2.0F;
    EngWaterParams p = eng_water_default(0.0F);
    EngArena a;
    ASSERT_TRUE(eng_arena_init(&a, "water", 4U << 20));
    EngModel m;
    ASSERT_TRUE(eng_water_build(&a, &map, &p, 100.0F, -50.0F, &m));
    const uint32_t wet = eng_water_wet_cells(&map, &p);
    ASSERT_GT(wet, 20U);
    float min_x = 1e9F;
    float max_x = -1e9F;
    float min_z = 1e9F;
    float max_z = -1e9F;
    for (uint32_t i = 0; i < m.vertex_count; i++) {
        const EngModelVertex *v = &m.vertices[i];
        const float fx = (v->position[0] - 100.0F) / 2.0F;
        const float fz = (v->position[2] + 50.0F) / 2.0F;
        ASSERT_NEAR(fx, floorf(fx + 0.5F), 1e-5F);
        ASSERT_NEAR(fz, floorf(fz + 0.5F), 1e-5F);
        const float altitude =
            eng_heightmap_altitude(&map, (uint32_t)(fx + 0.5F), (uint32_t)(fz + 0.5F));
        ASSERT_NEAR(v->uv[0], fmaxf(0.0F - altitude, 0.0F), 1e-3F);
        min_x = fminf(min_x, v->position[0]);
        max_x = fmaxf(max_x, v->position[0]);
        min_z = fminf(min_z, v->position[2]);
        max_z = fmaxf(max_z, v->position[2]);
    }
    EXPECT_EQ(m.bounds_min[0], min_x);
    EXPECT_EQ(m.bounds_max[0], max_x);
    EXPECT_EQ(m.bounds_min[2], min_z);
    EXPECT_EQ(m.bounds_max[2], max_z);
    EXPECT_EQ(m.bounds_min[1], 0.0F);
    EXPECT_EQ(m.bounds_max[1], 0.0F);
    /* chaque triangle est une moitié de cellule : trois sommets d'une même cellule de 2 m, aire 2
     * m2 */
    for (uint32_t t = 0; t < m.index_count; t += 3) {
        const float *p0 = m.vertices[m.indices[t]].position;
        const float *p1 = m.vertices[m.indices[t + 1]].position;
        const float *p2 = m.vertices[m.indices[t + 2]].position;
        const float xs[3] = {p0[0], p1[0], p2[0]};
        const float zs[3] = {p0[2], p1[2], p2[2]};
        const float dx = fmaxf(xs[0], fmaxf(xs[1], xs[2])) - fminf(xs[0], fminf(xs[1], xs[2]));
        const float dz = fmaxf(zs[0], fmaxf(zs[1], zs[2])) - fminf(zs[0], fminf(zs[1], zs[2]));
        ASSERT_NEAR(dx, 2.0F, 1e-4F);
        ASSERT_NEAR(dz, 2.0F, 1e-4F);
        const float area =
            0.5F * (((p1[2] - p0[2]) * (p2[0] - p0[0])) - ((p1[0] - p0[0]) * (p2[2] - p0[2])));
        ASSERT_NEAR(area, 2.0F, 1e-4F);
    }
    eng_arena_shutdown(&a);
}

UTEST(eng_water, the_smallest_field_makes_one_cell_and_the_shade_ignores_the_level_offset)
{
    for (uint32_t i = 0; i < 4U; i++) {
        g_samples[i] = 0; /* altitude minimale : -10 m */
    }
    g_map = (EngHeightmap){.size = ENG_HEIGHTMAP_MIN_SIZE,
                           .spacing = 1.0F,
                           .altitude_min = -10.0F,
                           .altitude_max = 10.0F,
                           .samples = g_samples};
    EngWaterParams p = eng_water_default(0.0F);
    EngArena a;
    ASSERT_TRUE(eng_arena_init(&a, "water", 1U << 20));
    EngModel m;
    ASSERT_TRUE(eng_water_build(&a, &g_map, &p, 0.0F, 0.0F, &m));
    EXPECT_EQ(m.vertex_count, 4U);
    EXPECT_EQ(m.index_count, 6U);
    EXPECT_EQ(eng_water_wet_cells(&g_map, &p), 1U);
    /* niveau exactement à l'altitude du terrain : sec (comparaison stricte) */
    EngWaterParams flush = p;
    flush.level = eng_heightmap_altitude(&g_map, 0, 0);
    EXPECT_EQ(eng_water_wet_cells(&g_map, &flush), 0U);
    eng_arena_shutdown(&a);
    /* la couleur ne dépend que de la position relative à l'eau : translation verticale */
    const EngWaterSky sky = {.horizon = {0.8F, 0.7F, 0.6F}, .zenith = {0.2F, 0.3F, 0.9F}};
    const EngWaterLight light = {.sun_direction = {0.3F, 0.8F, 0.5F},
                                 .sun_radiance = {3.0F, 2.0F, 1.0F},
                                 .ambient = {0.1F, 0.2F, 0.3F}};
    EngWaterParams q = eng_water_default(2.0F);
    q.ripple_amplitude = 0.0F;
    float high[3];
    float low[3];
    const float world_high[3] = {1.0F, 2.0F, -3.0F};
    const float camera_high[3] = {7.0F, 10.0F, 5.0F};
    const float world_low[3] = {1.0F, 0.0F, -3.0F};
    const float camera_low[3] = {7.0F, 8.0F, 5.0F};
    eng_water_shade(&q, &sky, &light, camera_high, world_high, 1.5F, 0.0, high);
    eng_water_shade(&q, &sky, &light, camera_low, world_low, 1.5F, 0.0, low);
    for (int c = 0; c < 3; c++) {
        EXPECT_NEAR(high[c], low[c], 1e-6F);
    }
}
