/* Ambiance et musique synthétisées (M8-01). */
#include <math.h>
#include <string.h>

#include "engine/ambience.h"
#include "engine/arena.h"
#include "utest.h"

static EngArena g_arena;
static EngAmbience g_amb;

static bool ready(void)
{
    static bool done;
    if (!done) {
        if (!eng_arena_init(&g_arena, "test-ambience", (size_t)24 << 20) ||
            !eng_ambience_init(&g_amb, &g_arena, 48000U)) {
            return false;
        }
        done = true;
    }
    return true;
}

UTEST(ambience, every_layer_is_synthesised_finite_audible_and_without_clipping)
{
    ASSERT_TRUE(ready());
    for (uint32_t l = 0; l < ENG_AMB_LAYER_COUNT; l++) {
        double energy = 0.0;
        float peak = 0.0F;
        for (uint32_t i = 0; i < g_amb.length[l]; i++) {
            const float v = g_amb.data[l][i];
            ASSERT_TRUE(isfinite(v));
            energy += (double)v * (double)v;
            peak = fmaxf(peak, fabsf(v));
        }
        const double rms = sqrt(energy / (double)g_amb.length[l]);
        EXPECT_GT(rms, 0.002); /* audible */
        EXPECT_LT(peak, 1.0F); /* pas d'écrêtage seul */
    }
    EXPECT_EQ(g_amb.length[ENG_AMB_MUSIC], 32U * 48000U);
}

/* Le raccord de chaque boucle (dernier échantillon -> premier) ne fait pas de saut plus grand
 * qu'un pas ordinaire du signal : pas de « clic » à chaque tour. */
UTEST(ambience, each_loop_joins_its_end_to_its_start_without_a_click)
{
    ASSERT_TRUE(ready());
    for (uint32_t l = 0; l < ENG_AMB_LAYER_COUNT; l++) {
        const float *d = g_amb.data[l];
        const uint32_t n = g_amb.length[l];
        float worst_step = 0.0F;
        for (uint32_t i = 1; i < n; i++) {
            worst_step = fmaxf(worst_step, fabsf(d[i] - d[i - 1U]));
        }
        const float join = fabsf(d[0] - d[n - 1U]);
        EXPECT_LE(join, worst_step * 1.5F + 1e-4F);
    }
}

UTEST(ambience, birds_sing_by_day_crickets_by_night_and_music_does_not_depend_on_the_hour)
{
    EXPECT_GT(eng_ambience_gain(ENG_AMB_BIRDS, 0.0F), 0.5F);
    EXPECT_EQ(eng_ambience_gain(ENG_AMB_BIRDS, 1.0F), 0.0F);
    EXPECT_EQ(eng_ambience_gain(ENG_AMB_CRICKETS, 0.0F), 0.0F);
    EXPECT_GT(eng_ambience_gain(ENG_AMB_CRICKETS, 1.0F), 0.5F);
    EXPECT_EQ(eng_ambience_gain(ENG_AMB_MUSIC, 0.0F), eng_ambience_gain(ENG_AMB_MUSIC, 1.0F));
    EXPECT_GT(eng_ambience_gain(ENG_AMB_WIND, 0.5F), 0.0F);
    EXPECT_EQ(eng_ambience_gain(ENG_AMB_BIRDS, NAN), eng_ambience_gain(ENG_AMB_BIRDS, 0.0F));
}

/* Ordonnancement : chaque couche est relancée EXACTEMENT à la fin de la précédente ; une couche
 * muette (oiseaux la nuit) avance sans prendre de voix ; la musique va sur son bus. */
UTEST(ambience, loops_are_chained_back_to_back_on_their_buses)
{
    ASSERT_TRUE(ready());
    EngAmbience a = g_amb;
    a.started = false;
    memset(a.loops, 0, sizeof a.loops);
    EngMixer m;
    ASSERT_TRUE(eng_mixer_init(&m, 48000U));
    eng_ambience_update(&a, &m, 1.0F, 960U); /* nuit */
    EXPECT_EQ(a.loops[ENG_AMB_WIND], 1U);
    EXPECT_EQ(a.next_start[ENG_AMB_WIND], (uint64_t)a.length[ENG_AMB_WIND]);
    uint32_t music = 0;
    uint32_t ambience = 0;
    for (uint32_t v = 0; v < ENG_MIXER_VOICES; v++) {
        if (m.voices[v].active) {
            music += m.voices[v].bus == ENG_BUS_MUSIC ? 1U : 0U;
            ambience += m.voices[v].bus == ENG_BUS_AMBIENCE ? 1U : 0U;
        }
    }
    EXPECT_EQ(music, 1U);
    EXPECT_EQ(ambience, 2U); /* vent + grillons ; pas d'oiseaux la nuit */
    /* le mixeur avance jusqu'à 10 ms de la fin du vent : la boucle suivante part pile à la fin */
    m.frames_mixed = (uint64_t)a.length[ENG_AMB_WIND] - 480U;
    eng_ambience_update(&a, &m, 1.0F, 960U);
    EXPECT_EQ(a.loops[ENG_AMB_WIND], 2U);
    bool found = false;
    for (uint32_t v = 0; v < ENG_MIXER_VOICES; v++) {
        found = found || (m.voices[v].active && m.voices[v].delay == 480U &&
                          m.voices[v].data == a.data[ENG_AMB_WIND]);
    }
    EXPECT_TRUE(found);
    eng_ambience_update(NULL, &m, 0.0F, 960U); /* sans effet */
}
