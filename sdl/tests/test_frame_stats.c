#include <math.h>
#include <stdint.h>

#include "engine/frame_stats.h"
#include "utest.h"

/* Statistiques d'images : durées fournies à la main, résultats exacts. */

#define EXPECT_NEAR_F(a, b) EXPECT_LT(fabsf((a) - (b)), 1e-4F)

UTEST(eng_frame_stats, empty_summary_is_zero)
{
    EngFrameStats s;
    eng_frame_stats_init(&s);
    const EngFrameSummary sum = eng_frame_stats_summary(&s);
    EXPECT_EQ(sum.count, 0U);
    EXPECT_NEAR_F(sum.avg_frame_ms, 0.0F);
    EXPECT_NEAR_F(sum.max_frame_ms, 0.0F);
    EXPECT_NEAR_F(sum.avg_cpu_ms, 0.0F);
    EXPECT_NEAR_F(sum.max_cpu_ms, 0.0F);
    EXPECT_NEAR_F(sum.fps, 0.0F);
    EXPECT_NEAR_F(eng_frame_stats_frame_at(&s, 0), 0.0F);
    float out[4];
    EXPECT_EQ(eng_frame_stats_copy_frame(&s, out, 4), 0U);
}

UTEST(eng_frame_stats, summary_averages_and_maxima)
{
    EngFrameStats s;
    eng_frame_stats_init(&s);
    EXPECT_TRUE(eng_frame_stats_push(&s, 10.0F, 2.0F));
    EXPECT_TRUE(eng_frame_stats_push(&s, 20.0F, 6.0F));
    EXPECT_TRUE(eng_frame_stats_push(&s, 30.0F, 4.0F));
    const EngFrameSummary sum = eng_frame_stats_summary(&s);
    EXPECT_EQ(sum.count, 3U);
    EXPECT_NEAR_F(sum.avg_frame_ms, 20.0F);
    EXPECT_NEAR_F(sum.max_frame_ms, 30.0F);
    EXPECT_NEAR_F(sum.avg_cpu_ms, 4.0F);
    EXPECT_NEAR_F(sum.max_cpu_ms, 6.0F);
    EXPECT_NEAR_F(sum.fps, 50.0F); /* 1000 / 20 ms */
}

UTEST(eng_frame_stats, max_is_not_the_last_or_first)
{
    /* le maximum est au milieu : un « dernier » ou « premier » pris pour le max serait faux */
    EngFrameStats s;
    eng_frame_stats_init(&s);
    (void)eng_frame_stats_push(&s, 5.0F, 1.0F);
    (void)eng_frame_stats_push(&s, 40.0F, 9.0F);
    (void)eng_frame_stats_push(&s, 6.0F, 2.0F);
    const EngFrameSummary sum = eng_frame_stats_summary(&s);
    EXPECT_NEAR_F(sum.max_frame_ms, 40.0F);
    EXPECT_NEAR_F(sum.max_cpu_ms, 9.0F);
}

UTEST(eng_frame_stats, ring_keeps_only_the_latest_images)
{
    EngFrameStats s;
    eng_frame_stats_init(&s);
    /* 130 images : les valeurs 1..130 ; il doit rester 11..130 (les 120 dernières) */
    for (uint32_t i = 1; i <= 130; i++) {
        (void)eng_frame_stats_push(&s, (float)i, (float)i * 0.5F);
    }
    EXPECT_EQ(s.count, ENG_FRAME_STATS_HISTORY);
    EXPECT_EQ(s.total, (uint64_t)130);
    EXPECT_NEAR_F(eng_frame_stats_frame_at(&s, 0), 11.0F); /* la plus ancienne */
    EXPECT_NEAR_F(eng_frame_stats_frame_at(&s, ENG_FRAME_STATS_HISTORY - 1), 130.0F);
    EXPECT_NEAR_F(eng_frame_stats_cpu_at(&s, 0), 5.5F);
    EXPECT_NEAR_F(eng_frame_stats_cpu_at(&s, ENG_FRAME_STATS_HISTORY - 1), 65.0F);
    const EngFrameSummary sum = eng_frame_stats_summary(&s);
    EXPECT_NEAR_F(sum.avg_frame_ms, 70.5F); /* moyenne de 11..130 */
    EXPECT_NEAR_F(sum.max_frame_ms, 130.0F);
    /* hors limites */
    EXPECT_NEAR_F(eng_frame_stats_frame_at(&s, ENG_FRAME_STATS_HISTORY), 0.0F);
    EXPECT_NEAR_F(eng_frame_stats_cpu_at(&s, 1000), 0.0F);
}

UTEST(eng_frame_stats, exactly_full_then_one_more)
{
    EngFrameStats s;
    eng_frame_stats_init(&s);
    for (uint32_t i = 1; i <= ENG_FRAME_STATS_HISTORY; i++) {
        (void)eng_frame_stats_push(&s, (float)i, 0.0F);
    }
    EXPECT_NEAR_F(eng_frame_stats_frame_at(&s, 0), 1.0F);
    (void)eng_frame_stats_push(&s, 999.0F, 0.0F);
    EXPECT_NEAR_F(eng_frame_stats_frame_at(&s, 0), 2.0F);
    EXPECT_NEAR_F(eng_frame_stats_frame_at(&s, ENG_FRAME_STATS_HISTORY - 1), 999.0F);
}

UTEST(eng_frame_stats, invalid_durations_are_zeroed_and_reported)
{
    EngFrameStats s;
    eng_frame_stats_init(&s);
    EXPECT_FALSE(eng_frame_stats_push(&s, NAN, 1.0F));
    EXPECT_FALSE(eng_frame_stats_push(&s, 1.0F, INFINITY));
    EXPECT_FALSE(eng_frame_stats_push(&s, -3.0F, 1.0F));
    EXPECT_FALSE(eng_frame_stats_push(&s, 1.0F, -INFINITY));
    EXPECT_TRUE(eng_frame_stats_push(&s, 8.0F, 2.0F));
    EXPECT_EQ(s.count, 5U); /* l'image est comptée, avec une durée nulle */
    EXPECT_NEAR_F(eng_frame_stats_frame_at(&s, 0), 0.0F);
    EXPECT_NEAR_F(eng_frame_stats_cpu_at(&s, 1), 0.0F);
    EXPECT_NEAR_F(eng_frame_stats_cpu_at(&s, 0), 1.0F); /* l'autre valeur de la paire est gardée */
    const EngFrameSummary sum = eng_frame_stats_summary(&s);
    EXPECT_TRUE(isfinite(sum.avg_frame_ms));
    EXPECT_TRUE(isfinite(sum.fps));
    EXPECT_NEAR_F(sum.max_frame_ms, 8.0F);
}

UTEST(eng_frame_stats, a_slightly_negative_duration_is_invalid_and_zero_is_valid)
{
    EngFrameStats s;
    eng_frame_stats_init(&s);
    EXPECT_FALSE(eng_frame_stats_push(&s, -0.5F, 1.0F)); /* juste sous zéro : invalide */
    EXPECT_FALSE(eng_frame_stats_push(&s, 1.0F, -0.001F));
    EXPECT_TRUE(eng_frame_stats_push(&s, 0.0F, 0.0F)); /* zéro lui-même est valide */
    EXPECT_NEAR_F(eng_frame_stats_frame_at(&s, 0), 0.0F);
}

UTEST(eng_frame_stats, zero_durations_give_zero_fps_not_infinity)
{
    EngFrameStats s;
    eng_frame_stats_init(&s);
    (void)eng_frame_stats_push(&s, 0.0F, 0.0F);
    const EngFrameSummary sum = eng_frame_stats_summary(&s);
    EXPECT_NEAR_F(sum.fps, 0.0F);
}

UTEST(eng_frame_stats, copy_is_chronological_and_keeps_the_most_recent)
{
    EngFrameStats s;
    eng_frame_stats_init(&s);
    for (uint32_t i = 1; i <= 130; i++) { /* l'anneau a fait un tour */
        (void)eng_frame_stats_push(&s, (float)i, (float)(1000 + i));
    }
    float all[ENG_FRAME_STATS_HISTORY];
    EXPECT_EQ(eng_frame_stats_copy_frame(&s, all, ENG_FRAME_STATS_HISTORY),
              ENG_FRAME_STATS_HISTORY);
    EXPECT_NEAR_F(all[0], 11.0F);
    EXPECT_NEAR_F(all[ENG_FRAME_STATS_HISTORY - 1], 130.0F);
    for (uint32_t i = 1; i < ENG_FRAME_STATS_HISTORY; i++) {
        EXPECT_NEAR_F(all[i] - all[i - 1], 1.0F);
    }
    /* capacité plus petite : les 5 plus récentes, dans l'ordre */
    float few[5];
    EXPECT_EQ(eng_frame_stats_copy_frame(&s, few, 5), 5U);
    EXPECT_NEAR_F(few[0], 126.0F);
    EXPECT_NEAR_F(few[4], 130.0F);
    EXPECT_EQ(eng_frame_stats_copy_cpu(&s, few, 5), 5U);
    EXPECT_NEAR_F(few[0], 1126.0F);
    EXPECT_NEAR_F(few[4], 1130.0F);
    /* capacité nulle */
    EXPECT_EQ(eng_frame_stats_copy_frame(&s, few, 0), 0U);
    /* historique partiel : 3 images */
    EngFrameStats p;
    eng_frame_stats_init(&p);
    (void)eng_frame_stats_push(&p, 1.0F, 1.0F);
    (void)eng_frame_stats_push(&p, 2.0F, 2.0F);
    (void)eng_frame_stats_push(&p, 3.0F, 3.0F);
    EXPECT_EQ(eng_frame_stats_copy_frame(&p, few, 5), 3U);
    EXPECT_NEAR_F(few[0], 1.0F);
    EXPECT_NEAR_F(few[2], 3.0F);
}
