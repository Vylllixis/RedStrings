/* Effets visuels de combat (M5-20) : particules et traînée d'arme. */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/vfx.h"
#include "utest.h"

static EngParticleSpec spec_of(EngVfxPreset p)
{
    EngParticleSpec s;
    memset(&s, 0, sizeof s);
    if (!eng_particle_preset(p, &s)) {
        abort();
    }
    return s;
}

UTEST(vfx, a_burst_creates_exactly_count_particles_that_all_die_by_their_max_life)
{
    EngParticles p;
    eng_particles_init(&p, 42);
    const EngParticleSpec s = spec_of(ENG_VFX_SPARK_LIGHT);
    const float o[3] = {1.0F, 2.0F, 3.0F};
    EXPECT_EQ(eng_particles_burst(&p, &s, o), 12U);
    EXPECT_EQ(p.count, 12U);
    EXPECT_EQ(p.spawned, 12U);
    uint32_t alive_at_min = 0;
    for (uint32_t t = 1; t <= s.life_max; t++) {
        eng_particles_update(&p);
        if (t == s.life_min - 1U) {
            alive_at_min = p.count;
        }
    }
    EXPECT_EQ(alive_at_min, 12U); /* personne ne meurt avant life_min */
    EXPECT_EQ(p.count, 0U);       /* tous morts à life_max */
}

UTEST(vfx, particles_start_at_the_origin_with_speeds_in_range_and_fall_with_gravity)
{
    EngParticles p;
    eng_particles_init(&p, 7);
    EngParticleSpec s = spec_of(ENG_VFX_SPARK_LIGHT);
    s.count = 64;
    const float o[3] = {0.0F, 10.0F, 0.0F};
    ASSERT_EQ(eng_particles_burst(&p, &s, o), 64U);
    for (uint32_t i = 0; i < p.count; i++) {
        const float v =
            sqrtf(p.items[i].vel[0] * p.items[i].vel[0] + p.items[i].vel[1] * p.items[i].vel[1] +
                  p.items[i].vel[2] * p.items[i].vel[2]);
        EXPECT_GE(v, s.speed_min - 1e-3F);
        EXPECT_LE(v, s.speed_max + 1e-3F);
        EXPECT_NEAR(p.items[i].pos[1], 10.0F, 1e-6F);
        EXPECT_GE(p.items[i].life, s.life_min);
        EXPECT_LE(p.items[i].life, s.life_max);
    }
    /* sans gravité, la vitesse verticale est conservée ; avec, elle diminue de g/60 par tick */
    const float vy0 = p.items[0].vel[1];
    eng_particles_update(&p);
    EXPECT_NEAR(p.items[0].vel[1], vy0 - 9.8F / 60.0F, 1e-5F);
    EngParticles q;
    eng_particles_init(&q, 7);
    const EngParticleSpec flash = spec_of(ENG_VFX_FLASH_PARRY);
    ASSERT_GT(eng_particles_burst(&q, &flash, o), 0U);
    const float vq = q.items[0].vel[1];
    eng_particles_update(&q);
    EXPECT_NEAR(q.items[0].vel[1], vq, 1e-6F);
    /* sphère complète : des particules montent ET descendent */
    uint32_t up = 0;
    uint32_t down = 0;
    for (uint32_t i = 0; i < 64U && i < p.count; i++) {
        if (p.items[i].vel[1] > 0.0F) {
            up++;
        } else {
            down++;
        }
    }
    EXPECT_GT(up, 10U);
    EXPECT_GT(down, 10U);
}

UTEST(vfx, the_same_seed_gives_the_same_burst_and_another_seed_a_different_one)
{
    EngParticles a;
    EngParticles b;
    EngParticles c;
    eng_particles_init(&a, 5);
    eng_particles_init(&b, 5);
    eng_particles_init(&c, 6);
    const EngParticleSpec s = spec_of(ENG_VFX_SPARK_HEAVY);
    const float o[3] = {0, 1, 0};
    (void)eng_particles_burst(&a, &s, o);
    (void)eng_particles_burst(&b, &s, o);
    (void)eng_particles_burst(&c, &s, o);
    EXPECT_EQ(memcmp(a.items, b.items, sizeof(EngParticle) * a.count), 0);
    EXPECT_NE(memcmp(a.items, c.items, sizeof(EngParticle) * a.count), 0);
    eng_particles_init(&a, 0); /* graine nulle remplacée : le générateur ne se fige pas */
    (void)eng_particles_burst(&a, &s, o);
    EXPECT_NE(a.items[0].vel[0], a.items[1].vel[0]);
}

UTEST(vfx, a_full_pool_drops_the_excess_and_keeps_the_stream_reproducible)
{
    EngParticles p;
    eng_particles_init(&p, 3);
    EngParticleSpec s = spec_of(ENG_VFX_SPARK_HEAVY);
    s.count = 64;
    s.life_min = 500;
    s.life_max = 500;
    const float o[3] = {0, 0, 0};
    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(eng_particles_burst(&p, &s, o), 64U);
    }
    EXPECT_EQ(p.count, ENG_PARTICLES_MAX);
    EXPECT_EQ(eng_particles_burst(&p, &s, o), 0U);
    EXPECT_EQ(p.dropped, 64U);
    EXPECT_EQ(p.spawned, ENG_PARTICLES_MAX);
    /* la même séquence de tirages, pool plein ou non : l'état du générateur est identique */
    EngParticles q;
    eng_particles_init(&q, 3);
    for (int i = 0; i < 9; i++) {
        (void)eng_particles_burst(&q, &s, o);
    }
    EXPECT_EQ(p.rng, q.rng);
}

UTEST(vfx, color_and_alpha_fade_over_the_particle_life)
{
    EngParticle q;
    memset(&q, 0, sizeof q);
    q.life = 10;
    q.start = (EngDebugColor){255, 0, 100, 255};
    q.end = (EngDebugColor){0, 200, 100, 255};
    EngDebugColor c = eng_particle_color(&q);
    EXPECT_EQ(c.r, 255U);
    EXPECT_EQ(c.g, 0U);
    EXPECT_NEAR(eng_particle_alpha(&q), 1.0F, 1e-6F);
    q.age = 5;
    c = eng_particle_color(&q);
    EXPECT_NEAR((float)c.r, 127.5F, 1.0F);
    EXPECT_NEAR((float)c.g, 100.0F, 1.0F);
    EXPECT_EQ(c.b, 100U);
    EXPECT_NEAR(eng_particle_alpha(&q), 0.5F, 1e-6F);
    q.age = 10;
    EXPECT_NEAR(eng_particle_alpha(&q), 0.0F, 1e-9F);
    EXPECT_NEAR(eng_particle_alpha(NULL), 0.0F, 1e-9F);
    q.life = 0;
    EXPECT_NEAR(eng_particle_alpha(&q), 0.0F, 1e-9F);
    EXPECT_EQ(eng_particle_color(NULL).r, 255U);
}

UTEST(vfx, events_pick_their_preset_and_ignored_hits_make_nothing)
{
    EngParticles p;
    eng_particles_init(&p, 1);
    EngSimEvent e;
    memset(&e, 0, sizeof e);
    e.kind = ENG_HITKIND_DAMAGE;
    e.hitstop = 3;
    EXPECT_EQ(eng_vfx_on_event(&p, &e), 12U); /* légère */
    e.hitstop = 6;
    EXPECT_EQ(eng_vfx_on_event(&p, &e), 28U); /* lourde */
    e.kind = ENG_HITKIND_BLOCKED;
    EXPECT_EQ(eng_vfx_on_event(&p, &e), 10U);
    e.kind = ENG_HITKIND_PARRIED;
    EXPECT_EQ(eng_vfx_on_event(&p, &e), 16U);
    e.kind = ENG_HITKIND_IGNORED;
    EXPECT_EQ(eng_vfx_on_event(&p, &e), 0U);
    EXPECT_EQ(eng_vfx_on_event(NULL, &e), 0U);
    EXPECT_EQ(eng_vfx_on_event(&p, NULL), 0U);
}

UTEST(vfx, specs_and_arguments_are_validated)
{
    EngParticleSpec s = spec_of(ENG_VFX_SPARK_LIGHT);
    EXPECT_TRUE(eng_particle_spec_valid(&s));
    EXPECT_FALSE(eng_particle_spec_valid(NULL));
    EngParticleSpec b = s;
    b.count = 0;
    EXPECT_FALSE(eng_particle_spec_valid(&b));
    b = s;
    b.count = 65;
    EXPECT_FALSE(eng_particle_spec_valid(&b));
    b = s;
    b.speed_min = 6.0F; /* > speed_max */
    EXPECT_FALSE(eng_particle_spec_valid(&b));
    b = s;
    b.speed_min = -1.0F;
    EXPECT_FALSE(eng_particle_spec_valid(&b));
    b = s;
    b.life_min = 0;
    EXPECT_FALSE(eng_particle_spec_valid(&b));
    b = s;
    b.life_max = b.life_min - 1U;
    EXPECT_FALSE(eng_particle_spec_valid(&b));
    b = s;
    b.life_max = 601;
    EXPECT_FALSE(eng_particle_spec_valid(&b));
    b = s;
    b.size = 0.0F;
    EXPECT_FALSE(eng_particle_spec_valid(&b));
    b = s;
    b.gravity = NAN;
    EXPECT_FALSE(eng_particle_spec_valid(&b));
    b = s;
    b.count = 64;
    b.life_max = 600;
    EXPECT_TRUE(eng_particle_spec_valid(&b)); /* bornes incluses */
    EngParticles p;
    eng_particles_init(&p, 1);
    const float nan_o[3] = {NAN, 0, 0};
    const float o[3] = {0, 0, 0};
    EXPECT_EQ(eng_particles_burst(&p, &s, nan_o), 0U);
    EXPECT_EQ(eng_particles_burst(&p, &b, NULL), 0U);
    EXPECT_EQ(eng_particles_burst(NULL, &s, o), 0U);
    EXPECT_EQ(eng_particles_burst(&p, NULL, o), 0U);
    EXPECT_FALSE(eng_particle_preset(ENG_VFX_PRESET_COUNT, &s));
    EXPECT_FALSE(eng_particle_preset(ENG_VFX_SPARK_LIGHT, NULL));
    for (uint32_t i = 0; i < (uint32_t)ENG_VFX_PRESET_COUNT; i++) {
        EXPECT_TRUE(eng_particle_preset((EngVfxPreset)i, &s));
        EXPECT_TRUE(eng_particle_spec_valid(&s));
    }
    eng_particles_update(NULL);
    eng_particles_init(NULL, 1);
}

UTEST(vfx, the_weapon_trail_fades_and_expires_after_its_life)
{
    EngWeaponTrail t;
    ASSERT_TRUE(eng_trail_init(&t, 4));
    const float a[3] = {0, 0, 0};
    const float b[3] = {0, 1, 0};
    ASSERT_TRUE(eng_trail_push(&t, a, b));
    EXPECT_NEAR(eng_trail_alpha(&t, 0), 1.0F, 1e-6F);
    eng_trail_update(&t);
    EXPECT_NEAR(eng_trail_alpha(&t, 0), 0.75F, 1e-6F);
    ASSERT_TRUE(eng_trail_push(&t, a, b));
    EXPECT_EQ(t.count, 2U);
    EXPECT_GT(eng_trail_alpha(&t, 1), eng_trail_alpha(&t, 0)); /* le récent est plus opaque */
    eng_trail_update(&t);
    eng_trail_update(&t);
    EXPECT_EQ(t.count, 2U); /* le premier a 3 ticks : il vit encore */
    eng_trail_update(&t);
    EXPECT_EQ(t.count, 1U); /* il atteint 4 ticks : expiré */
    eng_trail_update(&t);
    EXPECT_EQ(t.count, 0U);
    EXPECT_NEAR(eng_trail_alpha(&t, 0), 0.0F, 1e-9F);
}

UTEST(vfx, the_trail_keeps_only_the_newest_samples_and_builds_a_strip)
{
    EngWeaponTrail t;
    ASSERT_TRUE(eng_trail_init(&t, ENG_TRAIL_MAX));
    for (uint32_t i = 0; i < ENG_TRAIL_MAX + 3U; i++) {
        const float a[3] = {(float)i, 0, 0};
        const float b[3] = {(float)i, 1, 0};
        ASSERT_TRUE(eng_trail_push(&t, a, b));
    }
    EXPECT_EQ(t.count, ENG_TRAIL_MAX);
    EXPECT_NEAR(t.samples[0].a[0], 3.0F, 1e-6F); /* 0, 1, 2 perdus */
    EXPECT_NEAR(t.samples[ENG_TRAIL_MAX - 1U].a[0], (float)(ENG_TRAIL_MAX + 2U), 1e-6F);
    float pos[2U * ENG_TRAIL_MAX][3];
    float al[2U * ENG_TRAIL_MAX];
    EXPECT_EQ(eng_trail_build_strip(&t, pos, al, 2U * ENG_TRAIL_MAX), 2U * ENG_TRAIL_MAX);
    EXPECT_NEAR(pos[0][0], 3.0F, 1e-6F);
    EXPECT_NEAR(pos[0][1], 0.0F, 1e-6F); /* base */
    EXPECT_NEAR(pos[1][1], 1.0F, 1e-6F); /* pointe */
    EXPECT_NEAR(al[0], al[1], 1e-9F);
    EXPECT_EQ(eng_trail_build_strip(&t, pos, al, 5), 4U); /* limité à des paires entières */
    EXPECT_EQ(eng_trail_build_strip(&t, pos, al, 1), 0U);
    EXPECT_EQ(eng_trail_build_strip(NULL, pos, al, 4), 0U);
    EXPECT_EQ(eng_trail_build_strip(&t, NULL, al, 4), 0U);
    EXPECT_EQ(eng_trail_build_strip(&t, pos, NULL, 4), 0U);
}

UTEST(vfx, trail_arguments_are_validated)
{
    EngWeaponTrail t;
    EXPECT_FALSE(eng_trail_init(NULL, 4));
    EXPECT_FALSE(eng_trail_init(&t, 0));
    EXPECT_FALSE(eng_trail_init(&t, ENG_TRAIL_MAX + 1U));
    EXPECT_TRUE(eng_trail_init(&t, 1));
    EXPECT_TRUE(eng_trail_init(&t, ENG_TRAIL_MAX));
    const float ok[3] = {0, 0, 0};
    const float bad[3] = {0, NAN, 0};
    EXPECT_FALSE(eng_trail_push(&t, ok, bad));
    EXPECT_FALSE(eng_trail_push(&t, bad, ok));
    EXPECT_FALSE(eng_trail_push(&t, NULL, ok));
    EXPECT_FALSE(eng_trail_push(&t, ok, NULL));
    EXPECT_FALSE(eng_trail_push(NULL, ok, ok));
    EngWeaponTrail unset;
    memset(&unset, 0, sizeof unset);
    EXPECT_FALSE(eng_trail_push(&unset, ok, ok)); /* durée de vie nulle : jamais initialisée */
    EXPECT_NEAR(eng_trail_alpha(NULL, 0), 0.0F, 1e-9F);
    eng_trail_update(NULL);
}

UTEST(vfx, debug_drawing_counts_one_line_per_particle_and_per_trail_segment)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "vfx", (size_t)1 << 20));
    EngDebugLines lines;
    ASSERT_TRUE(eng_debug_lines_init(&lines, &arena, 2000));
    EngParticles p;
    eng_particles_init(&p, 9);
    const EngParticleSpec s = spec_of(ENG_VFX_SPARK_LIGHT);
    const float o[3] = {0, 1, 0};
    (void)eng_particles_burst(&p, &s, o);
    EXPECT_EQ(eng_particles_draw_debug(&p, &lines), 12U);
    EXPECT_EQ(eng_debug_lines_count(&lines), 12U);
    eng_debug_lines_clear(&lines);
    EngWeaponTrail t;
    ASSERT_TRUE(eng_trail_init(&t, 10));
    for (int i = 0; i < 4; i++) {
        const float a[3] = {(float)i, 0, 0};
        const float b[3] = {(float)i, 1, 0};
        ASSERT_TRUE(eng_trail_push(&t, a, b));
    }
    EXPECT_EQ(eng_trail_draw_debug(&t, &lines), 4U + 3U); /* 4 lames + 3 liaisons de pointes */
    EXPECT_EQ(eng_particles_draw_debug(NULL, &lines), 0U);
    EXPECT_EQ(eng_particles_draw_debug(&p, NULL), 0U);
    EXPECT_EQ(eng_trail_draw_debug(NULL, &lines), 0U);
    EXPECT_EQ(eng_trail_draw_debug(&t, NULL), 0U);
    eng_arena_shutdown(&arena);
}
