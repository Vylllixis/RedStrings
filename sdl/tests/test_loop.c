#include <stdlib.h>

#include "engine/loop.h"
#include "utest.h"

#define FREQ ((uint64_t)1000000000)

/* Simule `seconds` secondes de temps réel affichées à `fps` images par seconde. Les instants
 * des images sont arrondis au tick près mais cumulent exactement `seconds * FREQ` au total.
 * L'objet test avance de 3 unités par pas de simulation. */
static void run_at_fps(EngLoop *loop, uint32_t fps, uint32_t seconds, long *position)
{
    uint64_t frames = (uint64_t)fps * seconds;
    uint64_t previous = 0;
    for (uint64_t i = 1; i <= frames; i++) {
        uint64_t now = i * FREQ * seconds / frames;
        uint32_t steps = eng_loop_advance(loop, now - previous);
        previous = now;
        for (uint32_t s = 0; s < steps; s++) {
            *position += 3;
        }
    }
}

/* Critère M0-16 : après N pas, l'objet test est à la même position à 30, 60 et 144 FPS. */
UTEST(eng_loop, same_state_at_30_60_and_144_fps)
{
    const uint32_t rates[] = {30, 60, 144, 75, 240, 59};
    long reference = -1;
    for (size_t i = 0; i < sizeof rates / sizeof rates[0]; i++) {
        EngLoop loop;
        ASSERT_TRUE(eng_loop_init(&loop, FREQ, 60, 0.25));
        long position = 0;
        run_at_fps(&loop, rates[i], 10, &position);
        EXPECT_EQ(loop.tick_count, (uint64_t)600); /* 10 s x 60 Hz, exactement */
        EXPECT_EQ(loop.clamped_frames, (uint64_t)0);
        if (reference < 0) {
            reference = position;
        }
        EXPECT_EQ(position, reference);
        EXPECT_EQ(position, 1800L);
    }
}

UTEST(eng_loop, irregular_frame_times_still_give_exact_tick_count)
{
    EngLoop loop;
    ASSERT_TRUE(eng_loop_init(&loop, FREQ, 60, 0.25));
    srand(7);
    uint64_t total = 0;
    uint64_t target = 4 * FREQ; /* 4 s au total */
    while (total < target) {
        uint64_t delta = (uint64_t)(1000000 + rand() % 40000000); /* 1 à 41 ms */
        if (delta > target - total) {
            delta = target - total;
        }
        total += delta;
        (void)eng_loop_advance(&loop, delta);
    }
    EXPECT_EQ(loop.tick_count, (uint64_t)240);
    EXPECT_EQ(loop.alpha, 0.0);
}

UTEST(eng_loop, alpha_is_the_fraction_of_a_step)
{
    EngLoop loop;
    ASSERT_TRUE(eng_loop_init(&loop, 1200, 60, 0.25)); /* 1 pas = 20 ticks exactement */
    uint64_t half_step = 10;
    EXPECT_EQ(eng_loop_advance(&loop, half_step), (uint32_t)0);
    EXPECT_NEAR(loop.alpha, 0.5, 1e-6);
    EXPECT_EQ(eng_loop_advance(&loop, half_step), (uint32_t)1);
    EXPECT_NEAR(loop.alpha, 0.0, 1e-6);
    for (int i = 0; i < 1000; i++) {
        (void)eng_loop_advance(&loop, 7777777);
        ASSERT_TRUE(loop.alpha >= 0.0 && loop.alpha < 1.0);
    }
}

/* Critère M0-16 : pas d'emballement si le jeu est mis en pause dans le débogueur. */
UTEST(eng_loop, debugger_pause_does_not_cause_a_catch_up_storm)
{
    EngLoop loop;
    ASSERT_TRUE(eng_loop_init(&loop, FREQ, 60, 0.25));
    uint32_t steps = eng_loop_advance(&loop, 30 * FREQ); /* 30 s à l'arrêt sur un point d'arrêt */
    EXPECT_TRUE(steps <= 15);                            /* 0,25 s x 60 Hz */
    EXPECT_EQ(loop.clamped_frames, (uint64_t)1);
    /* L'image suivante est normale : pas de dette restante. */
    uint32_t next = eng_loop_advance(&loop, FREQ / 60);
    EXPECT_TRUE(next <= 1);
    EXPECT_EQ(loop.clamped_frames, (uint64_t)1);
}

UTEST(eng_loop, extreme_delta_is_clamped_without_overflow)
{
    EngLoop loop;
    ASSERT_TRUE(eng_loop_init(&loop, FREQ, 240, 0.25));
    uint32_t steps = eng_loop_advance(&loop, UINT64_MAX);
    EXPECT_TRUE(steps <= 60);
    EXPECT_EQ(loop.clamped_frames, (uint64_t)1);
}

UTEST(eng_loop, slow_but_not_extreme_frame_runs_several_steps)
{
    EngLoop loop;
    ASSERT_TRUE(eng_loop_init(&loop, FREQ, 60, 0.25));
    EXPECT_EQ(eng_loop_advance(&loop, FREQ / 10 + 1), (uint32_t)6); /* 100 ms = 6 pas */
    EXPECT_EQ(loop.clamped_frames, (uint64_t)0);
}

UTEST(eng_loop, works_with_a_non_round_counter_frequency)
{
    EngLoop loop;
    ASSERT_TRUE(eng_loop_init(&loop, 10000000, 60, 0.25)); /* 10 MHz */
    for (int i = 0; i < 600; i++) {
        (void)eng_loop_advance(&loop, 10000000 / 144 + (i % 7 == 0 ? 1 : 0));
    }
    EXPECT_TRUE(loop.tick_count >= 240 && loop.tick_count <= 260);
}

UTEST(eng_loop, tick_seconds_and_invalid_parameters)
{
    EngLoop loop;
    ASSERT_TRUE(eng_loop_init(&loop, FREQ, 60, 0.25));
    EXPECT_NEAR(loop.tick_seconds, 1.0 / 60.0, 1e-15);
    EXPECT_FALSE(eng_loop_init(&loop, 0, 60, 0.25));
    EXPECT_FALSE(eng_loop_init(&loop, FREQ, 0, 0.25));
    EXPECT_FALSE(eng_loop_init(&loop, FREQ, 60, 0.0));
    EXPECT_FALSE(eng_loop_init(&loop, FREQ, 60, -1.0));
    EXPECT_FALSE(eng_loop_init(&loop, FREQ, 60, 1e30));
}
