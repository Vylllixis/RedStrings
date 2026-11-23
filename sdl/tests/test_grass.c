#include <math.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/grass.h"
#include "utest.h"

static void instance_at(EngInstance *in, float x, float y, float z, float scale, float yaw)
{
    const float position[3] = {x, y, z};
    eng_instance_make_yaw(in, position, scale, yaw, NULL);
}

UTEST(eng_grass, the_default_style_is_valid_and_each_bad_field_is_rejected)
{
    EngGrassStyle s = eng_grass_style_default();
    EXPECT_TRUE(eng_grass_style_valid(&s));
    EXPECT_FALSE(eng_grass_style_valid(NULL));
    EngGrassStyle b;
#define BAD(field, value)                                                                          \
    b = s;                                                                                         \
    b.field = (value);                                                                             \
    EXPECT_FALSE(eng_grass_style_valid(&b))
    BAD(wind_direction[0], NAN);
    BAD(wind_direction[1], INFINITY);
    BAD(wind_strength, -0.1F);
    BAD(wind_strength, NAN);
    BAD(wind_frequency, -1.0F);
    BAD(wave_scale, -1.0F);
    BAD(fade_start, -1.0F);
    BAD(fade_end, s.fade_start); /* strictement supérieur */
    BAD(fade_end, NAN);
#undef BAD
    b = s;
    b.wind_direction[0] = 0.0F;
    b.wind_direction[1] = 0.0F;
    EXPECT_FALSE(eng_grass_style_valid(&b));
    b = s;
    b.wind_strength = 0.0F; /* pas de vent : valide */
    b.wind_frequency = 0.0F;
    b.wave_scale = 0.0F;
    b.fade_start = 0.0F;
    EXPECT_TRUE(eng_grass_style_valid(&b));
}

UTEST(eng_grass, pack_fills_the_documented_layout_and_normalises_the_direction)
{
    EngGrassStyle s = eng_grass_style_default();
    s.wind_direction[0] = 3.0F;
    s.wind_direction[1] = 4.0F;
    s.wind_strength = 0.4F;
    s.wave_scale = 0.7F;
    s.fade_start = 10.0F;
    s.fade_end = 25.0F;
    s.wind_frequency = 0.5F;
    float out[ENG_GRASS_UNIFORM_FLOATS];
    const float camera[3] = {1.0F, 2.0F, 3.0F};
    ASSERT_TRUE(eng_grass_pack(&s, camera, 0.5, out));
    EXPECT_EQ(out[0], 1.0F);
    EXPECT_EQ(out[1], 2.0F);
    EXPECT_EQ(out[2], 3.0F);
    EXPECT_EQ(out[3], 10.0F);
    EXPECT_NEAR(out[4], 0.6F, 1e-6F);
    EXPECT_NEAR(out[5], 0.8F, 1e-6F);
    EXPECT_EQ(out[6], 0.4F);
    EXPECT_EQ(out[7], 0.7F);
    EXPECT_NEAR(out[8], 3.14159265F * 0.5F, 1e-5F); /* 0,5 Hz x 0,5 s = un quart de tour */
    EXPECT_EQ(out[9], 25.0F);
    EXPECT_EQ(out[10], 0.0F);
    EXPECT_EQ(out[11], 0.0F);
    /* la phase est reportée modulo un tour : t et t + 1/f donnent la même */
    float again[ENG_GRASS_UNIFORM_FLOATS];
    ASSERT_TRUE(eng_grass_pack(&s, camera, 0.5 + 2.0 * 1000.0, again));
    EXPECT_NEAR(again[8], out[8], 1e-4F);
    /* style invalide, temps non fini */
    EngGrassStyle bad = s;
    bad.fade_end = 1.0F;
    EXPECT_FALSE(eng_grass_pack(&bad, camera, 0.0, out));
    EXPECT_FALSE(eng_grass_pack(&s, camera, (double)NAN, out));
}

UTEST(eng_grass, fade_is_one_then_smoothly_zero_between_start_and_end)
{
    EngGrassStyle s = eng_grass_style_default();
    s.fade_start = 20.0F;
    s.fade_end = 60.0F;
    EXPECT_EQ(eng_grass_fade(&s, 0.0F), 1.0F);
    EXPECT_EQ(eng_grass_fade(&s, 20.0F), 1.0F);
    EXPECT_NEAR(eng_grass_fade(&s, 40.0F), 0.5F, 1e-6F);
    EXPECT_EQ(eng_grass_fade(&s, 60.0F), 0.0F);
    EXPECT_EQ(eng_grass_fade(&s, 1000.0F), 0.0F);
    float previous = 1.0F;
    for (int k = 20; k <= 60; k++) {
        const float d = (float)k;
        const float f = eng_grass_fade(&s, d);
        EXPECT_LE(f, previous + 1e-6F);
        previous = f;
    }
}

UTEST(eng_grass, the_blade_is_a_closed_two_sided_ribbon_one_metre_tall)
{
    EngArena a;
    ASSERT_TRUE(eng_arena_init(&a, "grass", 1U << 20));
    for (uint32_t segments = 1; segments <= 8; segments++) {
        EngModel m;
        ASSERT_TRUE(eng_grass_blade_build(&a, segments, 0.1F, NULL, &m));
        ASSERT_EQ(m.vertex_count, 2U * ((2U * segments) + 1U));
        ASSERT_EQ(m.index_count, 6U * ((2U * segments) - 1U));
        EXPECT_EQ(m.vertex_count, eng_grass_blade_vertex_count(segments));
        EXPECT_EQ(m.index_count / 3U, eng_grass_blade_triangle_count(segments));
        EXPECT_EQ(m.part_count, 1U);
        EXPECT_EQ(m.material_count, 2U);
        EXPECT_EQ(m.parts[0].index_count, m.index_count);
        EXPECT_EQ(m.parts[0].material, 1U);
        float top = 0.0F;
        for (uint32_t i = 0; i < m.vertex_count; i++) {
            const EngModelVertex *v = &m.vertices[i];
            top = fmaxf(top, v->position[1]);
            ASSERT_GE(v->position[1], 0.0F);
            ASSERT_LE(v->position[1], 1.0F);
            ASSERT_LE(fabsf(v->position[0]), 0.05F + 1e-6F);
            /* le ruban se rétrécit : demi-largeur = 0,05 x (1 - y) */
            ASSERT_NEAR(fabsf(v->position[0]), 0.05F * (1.0F - v->position[1]), 1e-6F);
            ASSERT_NEAR(fabsf(v->normal[2]), 1.0F, 1e-6F);
        }
        EXPECT_EQ(top, 1.0F);
        /* boîte englobante et uv : la largeur est celle de la base, v monte de 0 à 1 */
        EXPECT_NEAR(m.bounds_min[0], -0.05F, 1e-6F);
        EXPECT_NEAR(m.bounds_max[0], 0.05F, 1e-6F);
        for (uint32_t i = 0; i < m.vertex_count; i++) {
            ASSERT_NEAR(m.vertices[i].uv[1], 1.0F - m.vertices[i].position[1], 1e-6F);
            ASSERT_GE(m.vertices[i].uv[1], 0.0F);
            ASSERT_LE(m.vertices[i].uv[1], 1.0F);
        }
        for (uint32_t i = 0; i < m.index_count; i++) {
            ASSERT_LT(m.indices[i], m.vertex_count);
        }
        /* chaque triangle est anti-horaire vu du côté de sa normale : (b-a) x (c-a) . n > 0 */
        for (uint32_t t = 0; t < m.index_count; t += 3) {
            const EngModelVertex *p = &m.vertices[m.indices[t]];
            const EngModelVertex *q = &m.vertices[m.indices[t + 1]];
            const EngModelVertex *r = &m.vertices[m.indices[t + 2]];
            const float cz =
                ((q->position[0] - p->position[0]) * (r->position[1] - p->position[1])) -
                ((q->position[1] - p->position[1]) * (r->position[0] - p->position[0]));
            ASSERT_GT(cz * p->normal[2], 1e-9F);
            ASSERT_EQ(p->normal[2], q->normal[2]);
            ASSERT_EQ(p->normal[2], r->normal[2]);
        }
        /* autant de triangles de chaque face */
        uint32_t front = 0;
        for (uint32_t t = 0; t < m.index_count; t += 3) {
            front += m.vertices[m.indices[t]].normal[2] > 0.0F ? 1U : 0U;
        }
        EXPECT_EQ(front * 2U, m.index_count / 3U);
    }
    eng_arena_shutdown(&a);
}

UTEST(eng_grass, the_blade_rejects_bad_parameters_and_restores_the_arena)
{
    EngArena a;
    ASSERT_TRUE(eng_arena_init(&a, "grass", 1U << 16));
    const size_t before = eng_arena_remaining(&a);
    EngModel m;
    EXPECT_FALSE(eng_grass_blade_build(&a, 0, 0.1F, NULL, &m));
    EXPECT_FALSE(eng_grass_blade_build(&a, 9, 0.1F, NULL, &m));
    EXPECT_FALSE(eng_grass_blade_build(&a, 3, 0.0F, NULL, &m));
    EXPECT_FALSE(eng_grass_blade_build(&a, 3, NAN, NULL, &m));
    EXPECT_EQ(m.vertex_count, 0U);
    EXPECT_EQ(eng_arena_remaining(&a), before);
    eng_arena_shutdown(&a);
    EngArena tiny;
    ASSERT_TRUE(eng_arena_init(&tiny, "tiny", 256));
    const size_t tiny_before = eng_arena_remaining(&tiny);
    EXPECT_FALSE(eng_grass_blade_build(&tiny, 8, 0.1F, NULL, &m));
    EXPECT_EQ(eng_arena_remaining(&tiny), tiny_before);
    eng_arena_shutdown(&tiny);
}

UTEST(eng_grass, a_custom_material_replaces_the_default_green)
{
    EngArena a;
    ASSERT_TRUE(eng_arena_init(&a, "grass", 1U << 16));
    EngModelMaterial red = eng_grass_default_material();
    red.base_color[0] = 1.0F;
    EngModel m;
    ASSERT_TRUE(eng_grass_blade_build(&a, 3, 0.1F, &red, &m));
    EXPECT_EQ(m.materials[1].base_color[0], 1.0F);
    EXPECT_NE(m.materials[0].base_color[0], 1.0F);
    eng_arena_shutdown(&a);
}

UTEST(eng_grass, the_base_never_moves_and_the_tip_leans_downwind_by_at_most_the_strength)
{
    EngGrassStyle s = eng_grass_style_default();
    s.wind_direction[0] = 0.0F;
    s.wind_direction[1] = 1.0F; /* vent vers +z */
    s.wind_strength = 0.5F;
    s.wind_frequency = 0.0F; /* phase 0 */
    s.wave_scale = 0.0F;     /* onde = 0,5 + 0,5 sin(0) = 0,5 */
    EngInstance in;
    instance_at(&in, 10.0F, 2.0F, 5.0F, 2.0F, 0.0F);
    const float camera[3] = {10.0F, 2.0F, 5.0F}; /* sur l'instance : pas de fondu */
    float out[3];
    const float base[3] = {0.0F, 0.0F, 0.0F};
    eng_grass_vertex_world(&s, camera, 0.0, &in, base, out);
    EXPECT_EQ(out[0], 10.0F);
    EXPECT_EQ(out[1], 2.0F);
    EXPECT_EQ(out[2], 5.0F);
    const float tip[3] = {0.0F, 1.0F, 0.0F};
    eng_grass_vertex_world(&s, camera, 0.0, &in, tip, out);
    EXPECT_NEAR(out[0], 10.0F, 1e-5F);
    EXPECT_NEAR(out[1], 2.0F + 2.0F, 1e-5F);                 /* hauteur = échelle */
    EXPECT_NEAR(out[2], 5.0F + (0.5F * 2.0F * 0.5F), 1e-5F); /* force x échelle x y² x onde 0,5 */
    /* à mi-hauteur : y² = 0,25 */
    const float mid[3] = {0.0F, 0.5F, 0.0F};
    eng_grass_vertex_world(&s, camera, 0.0, &in, mid, out);
    EXPECT_NEAR(out[2], 5.0F + (0.5F * 2.0F * 0.25F * 0.5F), 1e-5F);
    /* onde = 1 (sin = 1) : le décalage atteint force x échelle */
    s.wind_frequency = 1.0F;
    eng_grass_vertex_world(&s, camera, 0.25, &in, tip, out);
    EXPECT_NEAR(out[2], 5.0F + (0.5F * 2.0F), 1e-5F);
    /* onde = 0 : pas de décalage */
    eng_grass_vertex_world(&s, camera, 0.75, &in, tip, out);
    EXPECT_NEAR(out[2], 5.0F, 1e-5F);
}

UTEST(eng_grass, the_wave_travels_along_the_wind_and_repeats_every_period)
{
    EngGrassStyle s = eng_grass_style_default();
    s.wind_direction[0] = 1.0F;
    s.wind_direction[1] = 0.0F;
    s.wind_strength = 1.0F;
    s.wind_frequency = 0.5F;
    s.wave_scale = 0.5F;
    EngInstance a;
    EngInstance b;
    instance_at(&a, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F);
    instance_at(&b, 3.0F, 0.0F, 0.0F, 1.0F, 0.0F);
    const float camera[3] = {0.0F, 0.0F, 0.0F};
    const float tip[3] = {0.0F, 1.0F, 0.0F};
    float oa[3];
    float ob[3];
    eng_grass_vertex_world(&s, camera, 0.3, &a, tip, oa);
    eng_grass_vertex_world(&s, camera, 0.3, &b, tip, ob);
    /* deux brins à des positions différentes le long du vent n'ont pas le même décalage */
    EXPECT_GT(fabsf((oa[0] - 0.0F) - (ob[0] - 3.0F)), 0.01F);
    /* même instant décalé d'une période (2 s) : mêmes positions */
    float oa2[3];
    eng_grass_vertex_world(&s, camera, 2.3, &a, tip, oa2);
    EXPECT_NEAR(oa2[0], oa[0], 1e-4F);
    /* déplacement uniquement dans la direction du vent (x), jamais en z */
    EXPECT_EQ(oa[2], 0.0F);
    /* l'onde se propage : brin b à t équivaut à brin a à un autre instant (phase k x vs phase 2 pi
     * f t) */
    float shifted[3];
    const double dt = (double)(0.5F * 3.0F) / (6.2831853 * 0.5); /* k x / (2 pi f) */
    eng_grass_vertex_world(&s, camera, 0.3 + dt, &a, tip, shifted);
    EXPECT_NEAR(shifted[0] - 0.0F, ob[0] - 3.0F, 1e-4F);
}

UTEST(eng_grass, a_far_blade_collapses_to_its_base_and_the_rotation_orients_the_ribbon)
{
    EngGrassStyle s = eng_grass_style_default();
    s.fade_start = 10.0F;
    s.fade_end = 20.0F;
    EngInstance in;
    instance_at(&in, 0.0F, 1.0F, 0.0F, 1.5F, 0.0F);
    const float tip[3] = {0.0F, 1.0F, 0.0F};
    float out[3];
    const float far_camera[3] = {0.0F, 1.0F, 20.0F};
    eng_grass_vertex_world(&s, far_camera, 0.1, &in, tip, out);
    EXPECT_EQ(out[0], 0.0F);
    EXPECT_EQ(out[1], 1.0F);
    EXPECT_EQ(out[2], 0.0F);
    const float mid_camera[3] = {0.0F, 1.0F, 15.0F}; /* fondu 0,5 */
    eng_grass_vertex_world(&s, mid_camera, 0.1, &in, tip, out);
    EXPECT_NEAR(out[1], 1.0F + (1.5F * 0.5F), 1e-4F);
    /* cap de 90 degrés : le bord droit du brin (+x) part vers -z */
    instance_at(&in, 0.0F, 0.0F, 0.0F, 1.0F, 3.14159265F * 0.5F);
    s.wind_strength = 0.0F;
    const float edge[3] = {1.0F, 0.0F, 0.0F};
    const float near_camera[3] = {0.0F, 0.0F, 0.0F};
    eng_grass_vertex_world(&s, near_camera, 0.0, &in, edge, out);
    EXPECT_NEAR(out[0], 0.0F, 1e-5F);
    EXPECT_NEAR(out[2], -1.0F, 1e-5F);
}

UTEST(eng_grass, the_fade_uses_the_true_three_dimensional_distance_to_an_offset_blade)
{
    EngGrassStyle s = eng_grass_style_default();
    s.wind_strength = 0.0F;
    s.fade_start = 5.0F;
    s.fade_end = 15.0F;
    EngInstance in;
    instance_at(&in, 10.0F, 2.0F, -5.0F, 2.0F, 0.0F);
    const float camera[3] = {3.0F, 6.0F, 4.0F};
    const float tip[3] = {0.5F, 1.0F, 0.0F};
    float out[3];
    eng_grass_vertex_world(&s, camera, 0.0, &in, tip, out);
    /* distance exacte : sqrt(7^2 + 4^2 + 9^2) */
    const float fade = eng_grass_fade(&s, sqrtf(49.0F + 16.0F + 81.0F));
    ASSERT_GT(fade, 0.0F);
    ASSERT_LT(fade, 1.0F);
    EXPECT_NEAR(out[0], 10.0F + (0.5F * 2.0F * fade), 1e-4F);
    EXPECT_NEAR(out[1], 2.0F + (2.0F * fade), 1e-4F);
    EXPECT_NEAR(out[2], -5.0F, 1e-4F);
    /* la même lame vue de 100 m de haut, à la verticale : entièrement fondue */
    const float above[3] = {10.0F, 102.0F, -5.0F};
    eng_grass_vertex_world(&s, above, 0.0, &in, tip, out);
    EXPECT_NEAR(out[1], 2.0F, 1e-5F);
}

UTEST(eng_grass, the_tip_of_an_offset_blade_only_ever_leans_downwind)
{
    EngGrassStyle s = eng_grass_style_default();
    s.wind_direction[0] = 0.6F;
    s.wind_direction[1] = 0.8F;
    s.wind_strength = 0.5F;
    s.fade_start = 100.0F;
    s.fade_end = 200.0F;
    EngInstance in;
    instance_at(&in, 10.0F, 2.0F, -5.0F, 1.0F, 0.0F);
    const float camera[3] = {10.0F, 2.0F, -5.0F};
    const float tip[3] = {0.0F, 1.0F, 0.0F};
    float best = 0.0F;
    for (int step = 0; step < 40; step++) {
        float out[3];
        eng_grass_vertex_world(&s, camera, 0.1 * (double)step, &in, tip, out);
        const float along = ((out[0] - 10.0F) * 0.6F) + ((out[2] + 5.0F) * 0.8F);
        ASSERT_GE(out[0] - 10.0F, -1e-5F);
        ASSERT_GE(out[2] + 5.0F, -1e-5F);
        ASSERT_GE(along, -1e-5F);
        ASSERT_LE(along, 0.5F + 1e-5F);
        best = fmaxf(best, along);
    }
    EXPECT_GT(best, 0.1F);
}

/* ── Touffe (2026-11-22) ── */

static const float ROOT[3] = {0.04F, 0.1F, 0.02F};
static const float TIP[3] = {0.2F, 0.4F, 0.08F};

UTEST(eng_grass, the_tuft_is_a_fan_of_two_sided_blades_one_metre_tall_lit_from_above)
{
    EngArena a;
    ASSERT_TRUE(eng_arena_init(&a, "grass", 1U << 20));
    for (uint32_t blades = 3; blades <= 12; blades++) {
        EngModel m;
        ASSERT_TRUE(eng_grass_tuft_build(&a, blades, 7U, ROOT, TIP, &m));
        ASSERT_EQ(m.vertex_count, blades * 10U);
        ASSERT_EQ(m.index_count, blades * 18U);
        EXPECT_EQ(m.vertex_count, eng_grass_tuft_vertex_count(blades));
        EXPECT_EQ(m.index_count / 3U, eng_grass_tuft_triangle_count(blades));
        EXPECT_EQ(m.part_count, 1U);
        EXPECT_EQ(m.parts[0].index_count, m.index_count);
        EXPECT_EQ(m.parts[0].material, 1U);
        EXPECT_EQ(m.bounds_min[1], 0.0F); /* le pied au sol */
        EXPECT_EQ(m.bounds_max[1], 1.0F); /* le plus grand brin fait 1 m */
        EXPECT_LT(m.bounds_max[0], 0.8F); /* un éventail, pas un tapis */
        EXPECT_GT(m.bounds_max[0] - m.bounds_min[0], 0.25F);
        for (uint32_t i = 0; i < m.vertex_count; i++) {
            const EngModelVertex *v = &m.vertices[i];
            const float len = sqrtf((v->normal[0] * v->normal[0]) + (v->normal[1] * v->normal[1]) +
                                    (v->normal[2] * v->normal[2]));
            ASSERT_NEAR(len, 1.0F, 1e-5F);
            ASSERT_GT(v->normal[1], 0.9F); /* tournée vers le haut : pas de face noire */
            ASSERT_GE(v->uv[1], 0.0F);     /* v = 1 au pied, 0 à la pointe du brin */
            ASSERT_LE(v->uv[1], 1.0F);
            if (v->position[1] == 0.0F) {
                ASSERT_EQ(v->uv[1], 1.0F);
                /* le pied reste près du centre : le vent (y²) ne le déplace pas */
                ASSERT_LT(hypotf(v->position[0], v->position[2]), 0.3F);
            }
        }
        /* chaque brin : deux faces de trois triangles, de normales géométriques opposées */
        for (uint32_t b = 0; b < blades; b++) {
            float n[2][3];
            for (uint32_t face = 0; face < 2U; face++) {
                const uint32_t *t = &m.indices[(size_t)((b * 2U) + face) * 9U];
                ASSERT_LT(t[0], m.vertex_count);
                const float *p0 = m.vertices[t[0]].position;
                const float *p1 = m.vertices[t[1]].position;
                const float *p2 = m.vertices[t[2]].position;
                const float u[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
                const float w[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};
                n[face][0] = (u[1] * w[2]) - (u[2] * w[1]);
                n[face][1] = (u[2] * w[0]) - (u[0] * w[2]);
                n[face][2] = (u[0] * w[1]) - (u[1] * w[0]);
            }
            const float dot = (n[0][0] * n[1][0]) + (n[0][1] * n[1][1]) + (n[0][2] * n[1][2]);
            ASSERT_LT(dot, 0.0F);
        }
        eng_model_free(&m);
    }
    eng_arena_shutdown(&a);
}

UTEST(eng_grass, the_tuft_colour_goes_from_the_root_to_the_tip_and_the_seed_fixes_the_shape)
{
    EngArena a;
    ASSERT_TRUE(eng_arena_init(&a, "grass", 1U << 20));
    EngModel m;
    ASSERT_TRUE(eng_grass_tuft_build(&a, 6U, 7U, ROOT, TIP, &m));
    ASSERT_EQ(m.image_count, 1U);
    ASSERT_EQ(m.materials[1].image, 0);
    ASSERT_EQ(m.images[0].width, 1U);
    ASSERT_EQ(m.images[0].height, 16U);
    const uint8_t *top = m.images[0].pixels;                       /* v = 0 : la pointe */
    const uint8_t *bottom = &m.images[0].pixels[(size_t)15U * 4U]; /* v = 1 : le pied */
    EXPECT_GT(top[1], bottom[1] + 60); /* la pointe est bien plus claire */
    EXPECT_EQ(top[3], 255);
    /* sRGB de 0,4 et de 0,1 (linéaire) : 170 et 89 */
    EXPECT_NEAR((float)top[1], 170.0F, 1.5F);
    EXPECT_NEAR((float)bottom[1], 89.0F, 1.5F);
    for (uint32_t row = 1; row < 16U; row++) { /* de plus en plus sombre vers le pied */
        EXPECT_LE(m.images[0].pixels[(row * 4U) + 1U], m.images[0].pixels[((row - 1U) * 4U) + 1U]);
    }
    EngModel same;
    EngModel other;
    ASSERT_TRUE(eng_grass_tuft_build(&a, 6U, 7U, ROOT, TIP, &same));
    ASSERT_TRUE(eng_grass_tuft_build(&a, 6U, 8U, ROOT, TIP, &other));
    uint32_t differ = 0;
    for (uint32_t i = 0; i < m.vertex_count; i++) {
        ASSERT_EQ(m.vertices[i].position[0], same.vertices[i].position[0]);
        ASSERT_EQ(m.vertices[i].position[2], same.vertices[i].position[2]);
        differ += m.vertices[i].position[0] != other.vertices[i].position[0] ? 1U : 0U;
    }
    EXPECT_GT(differ, m.vertex_count / 2U);
    eng_model_free(&m);
    eng_model_free(&same);
    eng_model_free(&other);
    /* paramètres invalides : modèle à zéro, arène restituée */
    const EngArenaMark mark = eng_arena_mark(&a);
    EXPECT_FALSE(eng_grass_tuft_build(&a, 2U, 7U, ROOT, TIP, &m));
    EXPECT_EQ(m.vertex_count, 0U);
    EXPECT_FALSE(eng_grass_tuft_build(&a, 13U, 7U, ROOT, TIP, &m));
    EXPECT_FALSE(eng_grass_tuft_build(&a, 6U, 7U, NULL, TIP, &m));
    EXPECT_FALSE(eng_grass_tuft_build(&a, 6U, 7U, ROOT, NULL, &m));
    EXPECT_FALSE(eng_grass_tuft_build(NULL, 6U, 7U, ROOT, TIP, &m));
    EXPECT_FALSE(eng_grass_tuft_build(&a, 6U, 7U, ROOT, TIP, NULL));
    EXPECT_EQ(eng_arena_mark(&a).used, mark.used);
    eng_arena_shutdown(&a);
    EngArena tiny;
    ASSERT_TRUE(eng_arena_init(&tiny, "petite", 256U));
    EXPECT_FALSE(eng_grass_tuft_build(&tiny, 6U, 7U, ROOT, TIP, &m)); /* arène trop petite */
    EXPECT_EQ(m.vertex_count, 0U);
    eng_arena_shutdown(&tiny);
}
