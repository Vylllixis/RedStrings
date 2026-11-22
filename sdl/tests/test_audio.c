/* Sons procéduraux, mélangeur et calage des impacts (M5-19). */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "engine/combat_audio.h"
#include "utest.h"

static float rms(const float *x, uint32_t n)
{
    double s = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        s += (double)x[i] * (double)x[i];
    }
    return (float)sqrt(s / (double)n);
}

UTEST(audio, every_sfx_is_bounded_deterministic_non_silent_and_ends_at_zero)
{
    static float a[ENG_SFX_MAX_SAMPLES];
    static float b[ENG_SFX_MAX_SAMPLES];
    for (uint32_t s = 0; s < (uint32_t)ENG_SFX_COUNT; s++) {
        const uint32_t n = eng_sfx_synth((EngSfx)s, 48000, a, ENG_SFX_MAX_SAMPLES);
        ASSERT_GT(n, 1000U);
        EXPECT_EQ(n, eng_sfx_length((EngSfx)s, 48000));
        EXPECT_EQ(eng_sfx_synth((EngSfx)s, 48000, b, ENG_SFX_MAX_SAMPLES), n);
        EXPECT_EQ(memcmp(a, b, sizeof(float) * n), 0); /* déterministe */
        float peak = 0.0F;
        for (uint32_t i = 0; i < n; i++) {
            EXPECT_TRUE(isfinite(a[i]));
            peak = fmaxf(peak, fabsf(a[i]));
        }
        EXPECT_NEAR(peak, 0.9F, 0.02F);
        EXPECT_NEAR(a[n - 1U], 0.0F, 1e-9F);
        EXPECT_GT(rms(a, n), 0.02F);
        EXPECT_NE(eng_sfx_name((EngSfx)s)[0], '?');
    }
}

UTEST(audio, sounds_differ_and_the_heavy_hit_lasts_longer_than_the_light_one)
{
    static float light[ENG_SFX_MAX_SAMPLES];
    static float heavy[ENG_SFX_MAX_SAMPLES];
    const uint32_t nl = eng_sfx_synth(ENG_SFX_HIT_LIGHT, 48000, light, ENG_SFX_MAX_SAMPLES);
    const uint32_t nh = eng_sfx_synth(ENG_SFX_HIT_HEAVY, 48000, heavy, ENG_SFX_MAX_SAMPLES);
    EXPECT_GT(nh, nl);
    EXPECT_NE(light[10], heavy[10]);
    /* le coup lourd garde plus d'énergie dans sa seconde moitié */
    EXPECT_GT(rms(heavy + nh / 2U, nh / 2U), rms(light + nl / 2U, nl / 2U));
    EXPECT_NE(light[0], 0.0F); /* démarre sur le bruit : l'attaque est immédiate */
    EXPECT_EQ(eng_sfx_length(ENG_SFX_PARRY, 48000), ENG_SFX_MAX_SAMPLES);
}

UTEST(audio, invalid_synthesis_arguments_are_refused)
{
    static float buf[ENG_SFX_MAX_SAMPLES];
    EXPECT_EQ(eng_sfx_synth(ENG_SFX_COUNT, 48000, buf, ENG_SFX_MAX_SAMPLES), 0U);
    EXPECT_EQ(eng_sfx_synth(ENG_SFX_BLOCK, 7999, buf, ENG_SFX_MAX_SAMPLES), 0U);
    EXPECT_EQ(eng_sfx_synth(ENG_SFX_BLOCK, 48001, buf, ENG_SFX_MAX_SAMPLES), 0U);
    EXPECT_EQ(eng_sfx_synth(ENG_SFX_BLOCK, 48000, NULL, ENG_SFX_MAX_SAMPLES), 0U);
    EXPECT_EQ(eng_sfx_synth(ENG_SFX_BLOCK, 48000, buf, 10), 0U);
    EXPECT_EQ(eng_sfx_synth(ENG_SFX_BLOCK, 48000, buf, 7200), 7200U); /* capacité exacte */
    EXPECT_EQ(eng_sfx_length(ENG_SFX_BLOCK, 8000), 1200U);
    EXPECT_STREQ(eng_sfx_name(ENG_SFX_COUNT), "?");
}

UTEST(audio, a_voice_starts_exactly_at_its_delay_even_across_block_boundaries)
{
    EngMixer m;
    ASSERT_TRUE(eng_mixer_init(&m, 48000));
    const float one[4] = {0.5F, 0.5F, 0.5F, 0.5F};
    ASSERT_GE(eng_mixer_play(&m, one, 4, 1.0F, -1.0F, 1000), 0);
    float out[2 * 300];
    int64_t first = -1;
    for (uint32_t block = 0; block < 5; block++) {
        eng_mixer_mix(&m, out, 300);
        for (uint32_t f = 0; f < 300 && first < 0; f++) {
            if (out[(size_t)2 * f] != 0.0F) {
                first = (int64_t)block * 300 + (int64_t)f;
            }
        }
    }
    EXPECT_EQ(first, 1000);
    EXPECT_EQ(m.frames_mixed, 1500U);
    EXPECT_EQ(eng_mixer_active(&m), 0U); /* terminée → libérée */
}

UTEST(audio, pan_is_constant_power_and_gain_and_master_scale_the_output)
{
    EngMixer m;
    ASSERT_TRUE(eng_mixer_init(&m, 48000));
    const float s[1] = {0.5F};
    float out[2];
    ASSERT_GE(eng_mixer_play(&m, s, 1, 1.0F, -1.0F, 0), 0);
    eng_mixer_mix(&m, out, 1);
    EXPECT_NEAR(out[0], 0.5F, 1e-6F);
    EXPECT_NEAR(out[1], 0.0F, 1e-6F);
    ASSERT_GE(eng_mixer_play(&m, s, 1, 1.0F, 1.0F, 0), 0);
    eng_mixer_mix(&m, out, 1);
    EXPECT_NEAR(out[0], 0.0F, 1e-6F);
    EXPECT_NEAR(out[1], 0.5F, 1e-6F);
    ASSERT_GE(eng_mixer_play(&m, s, 1, 1.0F, 0.0F, 0), 0);
    eng_mixer_mix(&m, out, 1);
    EXPECT_NEAR(out[0], 0.5F * 0.70710678F, 1e-5F);
    EXPECT_NEAR(out[1], out[0], 1e-6F);
    for (int step = -4; step <= 4; step++) {
        const float pan = (float)step * 0.25F;
        ASSERT_GE(eng_mixer_play(&m, s, 1, 1.0F, pan, 0), 0);
        eng_mixer_mix(&m, out, 1);
        EXPECT_NEAR(out[0] * out[0] + out[1] * out[1], 0.25F, 1e-5F); /* puissance constante */
    }
    m.master = 0.5F;
    ASSERT_GE(eng_mixer_play(&m, s, 1, 2.0F, -1.0F, 0), 0);
    eng_mixer_mix(&m, out, 1);
    EXPECT_NEAR(out[0], 0.5F, 1e-6F); /* 0,5 × gain 2 × master 0,5 */
}

UTEST(audio, voices_are_summed_clipped_and_the_oldest_is_stolen_when_full)
{
    EngMixer m;
    ASSERT_TRUE(eng_mixer_init(&m, 48000));
    const float loud[1] = {0.9F};
    float out[2];
    ASSERT_GE(eng_mixer_play(&m, loud, 1, 1.0F, -1.0F, 0), 0);
    ASSERT_GE(eng_mixer_play(&m, loud, 1, 1.0F, -1.0F, 0), 0);
    eng_mixer_mix(&m, out, 1);
    EXPECT_NEAR(out[0], 1.0F, 1e-9F); /* 1,8 écrêté */
    EXPECT_EQ(m.clipped, 1U);
    const float neg[1] = {-0.9F};
    ASSERT_GE(eng_mixer_play(&m, neg, 1, 1.0F, -1.0F, 0), 0);
    ASSERT_GE(eng_mixer_play(&m, neg, 1, 1.0F, -1.0F, 0), 0);
    eng_mixer_mix(&m, out, 1);
    EXPECT_NEAR(out[0], -1.0F, 1e-9F);
    EXPECT_EQ(m.clipped, 2U);

    static float long_snd[1000];
    for (uint32_t i = 0; i < ENG_MIXER_VOICES; i++) {
        ASSERT_EQ(eng_mixer_play(&m, long_snd, 1000, 0.1F, 0.0F, 0), (int32_t)i);
    }
    EXPECT_EQ(eng_mixer_active(&m), ENG_MIXER_VOICES);
    EXPECT_EQ(eng_mixer_play(&m, long_snd, 1000, 0.1F, 0.0F, 0),
              0); /* vole la voix 0 (la + ancienne) */
    EXPECT_EQ(eng_mixer_play(&m, long_snd, 1000, 0.1F, 0.0F, 0), 1);
    EXPECT_EQ(eng_mixer_active(&m), ENG_MIXER_VOICES);
    eng_mixer_stop_all(&m);
    EXPECT_EQ(eng_mixer_active(&m), 0U);
}

UTEST(audio, mixer_rejects_invalid_arguments)
{
    EngMixer m;
    EXPECT_FALSE(eng_mixer_init(NULL, 48000));
    EXPECT_FALSE(eng_mixer_init(&m, 7999));
    EXPECT_FALSE(eng_mixer_init(&m, 192001));
    EXPECT_TRUE(eng_mixer_init(&m, 8000));
    EXPECT_TRUE(eng_mixer_init(&m, 192000));
    const float s[1] = {0.1F};
    EXPECT_EQ(eng_mixer_play(NULL, s, 1, 1, 0, 0), -1);
    EXPECT_EQ(eng_mixer_play(&m, NULL, 1, 1, 0, 0), -1);
    EXPECT_EQ(eng_mixer_play(&m, s, 0, 1, 0, 0), -1);
    EXPECT_EQ(eng_mixer_play(&m, s, 1, -0.1F, 0, 0), -1);
    EXPECT_EQ(eng_mixer_play(&m, s, 1, 4.1F, 0, 0), -1);
    EXPECT_EQ(eng_mixer_play(&m, s, 1, NAN, 0, 0), -1);
    EXPECT_EQ(eng_mixer_play(&m, s, 1, 1, 1.1F, 0), -1);
    EXPECT_EQ(eng_mixer_play(&m, s, 1, 1, -1.1F, 0), -1);
    EXPECT_EQ(eng_mixer_play(&m, s, 1, 1, NAN, 0), -1);
    EXPECT_GE(eng_mixer_play(&m, s, 1, 4.0F, 1.0F, 0), 0); /* bornes incluses */
    float out[4] = {9, 9, 9, 9};
    eng_mixer_mix(NULL, out, 2);
    EXPECT_NEAR(out[0], 0.0F, 1e-9F); /* silence, pas de crash */
    eng_mixer_mix(&m, NULL, 2);
    eng_mixer_mix(&m, out, 0);
    EXPECT_EQ(eng_mixer_active(NULL), 0U);
    eng_mixer_stop_all(NULL);
}

static EngSimEvent hit_event(uint32_t tick, EngHitKind k, uint32_t hitstop)
{
    EngSimEvent e;
    memset(&e, 0, sizeof e);
    e.tick = tick;
    e.kind = k;
    e.hitstop = hitstop;
    e.dealt = 10.0F;
    e.zone = ENG_ZONE_TORSO;
    return e;
}

UTEST(audio, an_impact_sound_starts_exactly_on_the_sample_of_its_tick)
{
    static EngMixer m;
    static EngCombatAudio ca;
    ASSERT_TRUE(eng_mixer_init(&m, 48000));
    ASSERT_TRUE(eng_combat_audio_init(&ca, &m));
    EXPECT_EQ(eng_audio_tick_frame(9, 48000), 7200U);
    EXPECT_EQ(eng_audio_tick_frame(60, 44100), 44100U);
    const float listener[2] = {0.0F, 0.0F};
    const EngSimEvent e = hit_event(9, ENG_HITKIND_DAMAGE, 3);
    ASSERT_TRUE(eng_combat_audio_on_event(&ca, &e, listener, 0.0F));
    float out[2 * 480];
    int64_t first = -1;
    for (uint32_t block = 0; block < 30; block++) { /* blocs de 480 images = 10 ms */
        eng_mixer_mix(&m, out, 480);
        for (uint32_t f = 0; f < 480 && first < 0; f++) {
            if (out[(size_t)2 * f] != 0.0F || out[(size_t)2 * f + 1] != 0.0F) {
                first = (int64_t)block * 480 + (int64_t)f;
            }
        }
    }
    EXPECT_EQ(first, 7200); /* ni avant, ni après */

    /* un événement dont le tick est déjà passé démarre immédiatement */
    static EngMixer m2;
    static EngCombatAudio ca2;
    ASSERT_TRUE(eng_mixer_init(&m2, 48000));
    ASSERT_TRUE(eng_combat_audio_init(&ca2, &m2));
    eng_mixer_mix(&m2, out, 480);
    const EngSimEvent late = hit_event(0, ENG_HITKIND_DAMAGE, 3);
    ASSERT_TRUE(eng_combat_audio_on_event(&ca2, &late, listener, 0.0F));
    EXPECT_EQ(m2.voices[0].delay, 0U);
}

UTEST(audio, sound_choice_gain_and_pan_follow_the_hit)
{
    static EngMixer m;
    static EngCombatAudio ca;
    ASSERT_TRUE(eng_mixer_init(&m, 48000));
    ASSERT_TRUE(eng_combat_audio_init(&ca, &m));
    const float listener[2] = {0.0F, 0.0F};
    EngSimEvent e = hit_event(1, ENG_HITKIND_DAMAGE, 6);
    ASSERT_TRUE(eng_combat_audio_on_event(&ca, &e, listener, 0.0F));
    EXPECT_TRUE(m.voices[0].data == ca.bank[ENG_SFX_HIT_HEAVY]);
    e = hit_event(1, ENG_HITKIND_DAMAGE, 3);
    ASSERT_TRUE(eng_combat_audio_on_event(&ca, &e, listener, 0.0F));
    EXPECT_TRUE(m.voices[1].data == ca.bank[ENG_SFX_HIT_LIGHT]);
    e = hit_event(1, ENG_HITKIND_BLOCKED, 3);
    ASSERT_TRUE(eng_combat_audio_on_event(&ca, &e, listener, 0.0F));
    EXPECT_TRUE(m.voices[2].data == ca.bank[ENG_SFX_BLOCK]);
    e = hit_event(1, ENG_HITKIND_PARRIED, 3);
    ASSERT_TRUE(eng_combat_audio_on_event(&ca, &e, listener, 0.0F));
    EXPECT_TRUE(m.voices[3].data == ca.bank[ENG_SFX_PARRY]);
    e = hit_event(1, ENG_HITKIND_IGNORED, 3);
    EXPECT_FALSE(eng_combat_audio_on_event(&ca, &e, listener, 0.0F));
    EXPECT_EQ(eng_mixer_active(&m), 4U);

    /* panoramique : contact à droite (+X, caméra vers -Z) → voix à droite */
    eng_mixer_stop_all(&m);
    e = hit_event(1, ENG_HITKIND_DAMAGE, 3);
    e.point[0] = 4.0F;
    ASSERT_TRUE(eng_combat_audio_on_event(&ca, &e, listener, 0.0F));
    EXPECT_GT(m.voices[0].gain_r, m.voices[0].gain_l);
    e.point[0] = -4.0F;
    ASSERT_TRUE(eng_combat_audio_on_event(&ca, &e, listener, 0.0F));
    EXPECT_GT(m.voices[1].gain_l, m.voices[1].gain_r);
    /* la même position, caméra tournée de 180° : les côtés s'inversent */
    e.point[0] = 4.0F;
    ASSERT_TRUE(eng_combat_audio_on_event(&ca, &e, listener, 3.14159265F));
    EXPECT_GT(m.voices[2].gain_l, m.voices[2].gain_r);
    /* atténuation avec la distance : plus loin = plus faible */
    eng_mixer_stop_all(&m);
    e = hit_event(1, ENG_HITKIND_DAMAGE, 3);
    ASSERT_TRUE(eng_combat_audio_on_event(&ca, &e, listener, 0.0F));
    e.point[2] = -30.0F;
    ASSERT_TRUE(eng_combat_audio_on_event(&ca, &e, listener, 0.0F));
    EXPECT_GT(m.voices[0].gain_l, m.voices[1].gain_l);
    /* dégâts plus forts = plus fort */
    eng_mixer_stop_all(&m);
    e = hit_event(1, ENG_HITKIND_DAMAGE, 3);
    e.dealt = 2.0F;
    ASSERT_TRUE(eng_combat_audio_on_event(&ca, &e, listener, 0.0F));
    e.dealt = 25.0F;
    ASSERT_TRUE(eng_combat_audio_on_event(&ca, &e, listener, 0.0F));
    EXPECT_GT(m.voices[1].gain_l, m.voices[0].gain_l);
}

UTEST(audio, combat_audio_rejects_invalid_arguments)
{
    static EngMixer m;
    static EngCombatAudio ca;
    EXPECT_FALSE(eng_combat_audio_init(NULL, &m));
    EXPECT_FALSE(eng_combat_audio_init(&ca, NULL));
    ASSERT_TRUE(eng_mixer_init(&m, 96000)); /* fréquence non gérée par les sons */
    EXPECT_FALSE(eng_combat_audio_init(&ca, &m));
    ASSERT_TRUE(eng_mixer_init(&m, 8000));
    EXPECT_TRUE(eng_combat_audio_init(&ca, &m));
    EXPECT_GT(ca.length[ENG_SFX_PARRY], 2000U);
    ASSERT_TRUE(eng_mixer_init(&m, 48000));
    ASSERT_TRUE(eng_combat_audio_init(&ca, &m));
    const float l[2] = {0.0F, 0.0F};
    const EngSimEvent e = hit_event(1, ENG_HITKIND_DAMAGE, 3);
    EXPECT_FALSE(eng_combat_audio_on_event(NULL, &e, l, 0.0F));
    EXPECT_FALSE(eng_combat_audio_on_event(&ca, NULL, l, 0.0F));
    EXPECT_FALSE(eng_combat_audio_on_event(&ca, &e, NULL, 0.0F));
    EXPECT_FALSE(eng_combat_audio_on_event(&ca, &e, l, NAN));
    const float bad[2] = {NAN, 0.0F};
    EXPECT_FALSE(eng_combat_audio_on_event(&ca, &e, bad, 0.0F));
    EXPECT_FALSE(eng_combat_audio_play(&ca, ENG_SFX_COUNT, 1, 0, 0));
    EXPECT_FALSE(eng_combat_audio_play(NULL, ENG_SFX_BLOCK, 1, 0, 0));
    EXPECT_TRUE(eng_combat_audio_play(&ca, ENG_SFX_WHOOSH, 1, 0, 0));
}
