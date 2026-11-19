/* Sensation de frappe (M5-14). */
#include <math.h>
#include <string.h>

#include "engine/game_feel.h"
#include "utest.h"

static EngSimEvent ev(uint32_t att, uint32_t def, EngHitKind k, uint32_t hitstop)
{
    EngSimEvent e;
    memset(&e, 0, sizeof e);
    e.attacker = att;
    e.defender = def;
    e.kind = k;
    e.hitstop = hitstop;
    return e;
}

static float mag(const float o[3])
{
    return sqrtf(o[0] * o[0] + o[1] * o[1] + o[2] * o[2]);
}

UTEST(game_feel, defaults_are_valid_and_out_of_range_values_are_not)
{
    EngFeelParams p;
    eng_feel_params_default(&p);
    EXPECT_TRUE(eng_feel_params_valid(&p));
    EXPECT_FALSE(eng_feel_params_valid(NULL));
    EngFeelParams q = p;
    q.hitstop_scale = 3.1F;
    EXPECT_FALSE(eng_feel_params_valid(&q));
    q = p;
    q.hitstop_scale = NAN;
    EXPECT_FALSE(eng_feel_params_valid(&q));
    q = p;
    q.shake_heavy = 0.21F;
    EXPECT_FALSE(eng_feel_params_valid(&q));
    q = p;
    q.shake_ticks = 0;
    EXPECT_FALSE(eng_feel_params_valid(&q));
    q = p;
    q.shake_ticks = 61;
    EXPECT_FALSE(eng_feel_params_valid(&q));
    q = p;
    q.parry_slowmo_scale = 0.04F;
    EXPECT_FALSE(eng_feel_params_valid(&q));
    q = p;
    q.parry_slowmo_ticks = -1;
    EXPECT_FALSE(eng_feel_params_valid(&q));
    q = p;
    q.fov_kick_heavy = 16.0F;
    EXPECT_FALSE(eng_feel_params_valid(&q));
    q = p;
    q.hitstop_scale = 0.0F;
    q.shake_ticks = 1;
    q.parry_slowmo_scale = 1.0F;
    q.parry_slowmo_ticks = 0;
    q.fov_kick_heavy = 0.0F;
    EXPECT_TRUE(eng_feel_params_valid(&q)); /* bornes incluses */
}

UTEST(game_feel, hitstop_is_scaled_rounded_and_capped)
{
    EngFeel f;
    eng_feel_init(&f, 0);
    EXPECT_EQ(eng_feel_hitstop(&f, 3), 3U);
    f.params.hitstop_scale = 1.5F;
    EXPECT_EQ(eng_feel_hitstop(&f, 3), 5U); /* 4,5 → 5 */
    f.params.hitstop_scale = 0.0F;
    EXPECT_EQ(eng_feel_hitstop(&f, 6), 0U);
    f.params.hitstop_scale = 3.0F;
    EXPECT_EQ(eng_feel_hitstop(&f, 20), ENG_ATTACK_MAX_HITSTOP);
    EXPECT_EQ(eng_feel_hitstop(&f, 10), 30U); /* 30 exactement */
    f.params.hitstop_scale = NAN;
    EXPECT_EQ(eng_feel_hitstop(&f, 4), 4U);
    EXPECT_EQ(eng_feel_hitstop(NULL, 4), 4U);
}

UTEST(game_feel, a_hit_shakes_the_camera_then_it_decays_to_exactly_zero)
{
    EngFeel f;
    eng_feel_init(&f, 0);
    float off[3];
    float ts = 0.0F;
    float fov = 1.0F;
    eng_feel_tick(&f, off, &ts, &fov);
    EXPECT_NEAR(mag(off), 0.0F, 1e-9F); /* calme au départ */
    EXPECT_NEAR(ts, 1.0F, 1e-9F);
    EXPECT_NEAR(fov, 0.0F, 1e-9F);
    const EngSimEvent e = ev(0, 1, ENG_HITKIND_DAMAGE, 3);
    eng_feel_on_hit(&f, &e);
    float peak = 0.0F;
    float prev_bound = 1e9F;
    for (int i = 0; i < 12; i++) {
        eng_feel_tick(&f, off, NULL, NULL);
        const float bound =
            f.params.shake_light * (float)(12 - i) / 12.0F * (float)(12 - i) / 12.0F;
        EXPECT_LE(fabsf(off[0]), bound + 1e-7F);
        EXPECT_LE(bound, prev_bound);
        prev_bound = bound;
        if (mag(off) > peak) {
            peak = mag(off);
        }
    }
    EXPECT_GT(peak, 0.0F);
    eng_feel_tick(&f, off, NULL, &fov);
    EXPECT_NEAR(mag(off), 0.0F, 1e-9F);
    EXPECT_NEAR(fov, 0.0F, 1e-9F);
    EXPECT_NEAR(f.shake_amp, 0.0F, 1e-9F);
}

UTEST(game_feel, heavy_hits_shake_more_and_kick_the_fov_while_blocks_shake_less)
{
    EngFeel f;
    eng_feel_init(&f, 0);
    float fov = 0.0F;
    EngSimEvent e = ev(0, 1, ENG_HITKIND_DAMAGE, 3);
    eng_feel_on_hit(&f, &e);
    EXPECT_NEAR(f.shake_amp, 0.01F, 1e-9F);
    EXPECT_NEAR(f.fov_amp, 0.0F, 1e-9F);
    e = ev(0, 1, ENG_HITKIND_DAMAGE, 6); /* lourde : plus fort, remplace */
    eng_feel_on_hit(&f, &e);
    EXPECT_NEAR(f.shake_amp, 0.03F, 1e-9F);
    EXPECT_NEAR(f.fov_amp, 3.0F, 1e-9F);
    e = ev(0, 1, ENG_HITKIND_DAMAGE, 3); /* plus faible : ne diminue pas la secousse en cours */
    eng_feel_on_hit(&f, &e);
    EXPECT_NEAR(f.shake_amp, 0.03F, 1e-9F);
    eng_feel_tick(&f, NULL, NULL, &fov);
    EXPECT_NEAR(fov, 3.0F, 1e-6F); /* k = 12/12 au 1er tick */

    EngFeel g;
    eng_feel_init(&g, 0);
    e = ev(0, 1, ENG_HITKIND_BLOCKED, 3);
    eng_feel_on_hit(&g, &e);
    EXPECT_NEAR(g.shake_amp, 0.008F, 1e-9F);
    EngFeel h;
    eng_feel_init(&h, 0);
    e = ev(0, 1, ENG_HITKIND_DAMAGE, 5); /* seuil : 5 = lourde */
    eng_feel_on_hit(&h, &e);
    EXPECT_NEAR(h.shake_amp, 0.03F, 1e-9F);
    EngFeel i;
    eng_feel_init(&i, 0);
    e = ev(0, 1, ENG_HITKIND_DAMAGE, 4);
    eng_feel_on_hit(&i, &e);
    EXPECT_NEAR(i.shake_amp, 0.01F, 1e-9F);
}

UTEST(game_feel, a_parry_slows_time_for_exactly_the_configured_ticks)
{
    EngFeel f;
    eng_feel_init(&f, 1); /* le joueur est le combattant 1 (le défenseur) */
    const EngSimEvent e = ev(0, 1, ENG_HITKIND_PARRIED, 3);
    eng_feel_on_hit(&f, &e);
    EXPECT_NEAR(f.shake_amp, 0.02F, 1e-9F);
    int slow = 0;
    for (int i = 0; i < 30; i++) {
        float ts = 0.0F;
        eng_feel_tick(&f, NULL, &ts, NULL);
        if (ts < 1.0F) {
            EXPECT_NEAR(ts, 0.3F, 1e-6F);
            slow++;
        }
    }
    EXPECT_EQ(slow, 12);
    f.params.parry_slowmo_ticks = 0;
    eng_feel_on_hit(&f, &e);
    float ts = 0.0F;
    eng_feel_tick(&f, NULL, &ts, NULL);
    EXPECT_NEAR(ts, 1.0F, 1e-9F);
}

UTEST(game_feel, events_not_involving_the_player_and_bad_arguments_are_ignored)
{
    EngFeel f;
    eng_feel_init(&f, 2);
    EngSimEvent e = ev(0, 1, ENG_HITKIND_DAMAGE, 6);
    eng_feel_on_hit(&f, &e);
    EXPECT_EQ(f.shake_left, 0U);
    e = ev(2, 1, ENG_HITKIND_DAMAGE, 6); /* le joueur frappe : on le ressent aussi */
    eng_feel_on_hit(&f, &e);
    EXPECT_GT(f.shake_left, 0U);
    EngFeel g;
    eng_feel_init(&g, 0);
    e = ev(0, 1, ENG_HITKIND_IGNORED, 3);
    eng_feel_on_hit(&g, &e);
    EXPECT_EQ(g.shake_left, 0U);
    eng_feel_on_hit(NULL, &e);
    eng_feel_on_hit(&g, NULL);
    eng_feel_tick(NULL, NULL, NULL, NULL);
    float off[3] = {9, 9, 9};
    float ts = 0;
    eng_feel_tick(NULL, off, &ts, NULL);
    EXPECT_NEAR(mag(off), 0.0F, 1e-9F);
    EXPECT_NEAR(ts, 1.0F, 1e-9F);
    eng_feel_init(NULL, 0);
}

UTEST(game_feel, the_shake_is_deterministic_for_the_same_history)
{
    EngFeel a;
    EngFeel b;
    eng_feel_init(&a, 0);
    eng_feel_init(&b, 0);
    const EngSimEvent e = ev(0, 1, ENG_HITKIND_DAMAGE, 6);
    float oa[3];
    float ob[3];
    for (int i = 0; i < 30; i++) {
        if (i == 3 || i == 10) {
            eng_feel_on_hit(&a, &e);
            eng_feel_on_hit(&b, &e);
        }
        eng_feel_tick(&a, oa, NULL, NULL);
        eng_feel_tick(&b, ob, NULL, NULL);
        EXPECT_TRUE(oa[0] == ob[0] && oa[1] == ob[1] && oa[2] == ob[2]);
    }
}

UTEST(game_feel, tweaks_are_registered_by_name_with_bounds_and_edit_the_live_params)
{
    EngFeel f;
    EngTweaks t;
    eng_feel_init(&f, 0);
    eng_tweaks_init(&t);
    ASSERT_TRUE(eng_feel_register_tweaks(&f, &t));
    EXPECT_EQ(t.count, 9U);
    EXPECT_TRUE(eng_tweaks_set_float(&t, "feel.hitstop_scale", 2.0F));
    EXPECT_NEAR(f.params.hitstop_scale, 2.0F, 1e-9F);
    EXPECT_EQ(eng_feel_hitstop(&f, 3), 6U);
    EXPECT_TRUE(eng_tweaks_set_float(&t, "feel.hitstop_scale", 99.0F));
    EXPECT_NEAR(f.params.hitstop_scale, 3.0F, 1e-9F); /* borné */
    EXPECT_TRUE(eng_tweaks_set_int(&t, "feel.shake_ticks", 30));
    EXPECT_EQ(f.params.shake_ticks, 30);
    EXPECT_TRUE(eng_tweaks_set_int(&t, "feel.parry_slowmo_ticks", 5));
    EXPECT_TRUE(eng_tweaks_set_float(&t, "feel.fov_kick_heavy", 1.0F));
    EXPECT_TRUE(eng_tweaks_set_float(&t, "feel.shake_light", 0.1F));
    EXPECT_TRUE(eng_tweaks_set_float(&t, "feel.shake_heavy", 0.1F));
    EXPECT_TRUE(eng_tweaks_set_float(&t, "feel.shake_block", 0.1F));
    EXPECT_TRUE(eng_tweaks_set_float(&t, "feel.shake_parry", 0.1F));
    EXPECT_TRUE(eng_tweaks_set_float(&t, "feel.parry_slowmo_scale", 0.5F));
    EXPECT_TRUE(eng_feel_params_valid(&f.params));
    eng_tweaks_reset_all(&t);
    EXPECT_NEAR(f.params.hitstop_scale, 1.0F, 1e-9F);
    EXPECT_EQ(f.params.shake_ticks, 12);
    /* un second enregistrement échoue (noms déjà pris) ; arguments nuls refusés */
    EXPECT_FALSE(eng_feel_register_tweaks(&f, &t));
    EXPECT_FALSE(eng_feel_register_tweaks(NULL, &t));
    EXPECT_FALSE(eng_feel_register_tweaks(&f, NULL));
}
