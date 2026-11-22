#include <SDL3/SDL.h>

#include "engine/clock.h"
#include "utest.h"

#define FREQ ((uint64_t)1000000000) /* 1 tick = 1 ns */

UTEST(eng_clock, dt_and_frame_counter_with_simulated_time)
{
    EngClock clock;
    eng_clock_init_at(&clock, FREQ, 5000);
    eng_clock_tick_at(&clock, 5000 + 16666667); /* ~60 Hz */
    EXPECT_NEAR(clock.dt, 0.016666667, 1e-12);
    EXPECT_EQ(clock.dt_counter, (uint64_t)16666667);
    EXPECT_EQ(clock.frame, (uint64_t)1);
    eng_clock_tick_at(&clock, 5000 + 16666667 + 8333333); /* ~120 Hz */
    EXPECT_NEAR(clock.dt, 0.008333333, 1e-12);
    EXPECT_NEAR(clock.time, 0.025, 1e-9);
    EXPECT_EQ(clock.frame, (uint64_t)2);
}

UTEST(eng_clock, precision_is_kept_with_huge_counter_values)
{
    /* Un PC allumé depuis des semaines : le compteur dépasse 2^53, limite de précision d'un
     * double. Le dt doit rester exact. */
    uint64_t base = (uint64_t)1 << 60;
    EngClock clock;
    eng_clock_init_at(&clock, FREQ, base);
    eng_clock_tick_at(&clock, base + 16666667);
    EXPECT_NEAR(clock.dt, 0.016666667, 1e-12);
}

UTEST(eng_clock, backwards_counter_gives_zero_dt)
{
    EngClock clock;
    eng_clock_init_at(&clock, FREQ, 1000);
    eng_clock_tick_at(&clock, 500);
    EXPECT_EQ(clock.dt, 0.0);
    EXPECT_EQ(clock.dt_counter, (uint64_t)0);
}

UTEST(eng_clock, non_integer_frequency_ratio)
{
    EngClock clock;
    eng_clock_init_at(&clock, 10000000, 0); /* 10 MHz (valeur courante sous Windows) */
    eng_clock_tick_at(&clock, 166667);
    EXPECT_NEAR(clock.dt, 0.0166667, 1e-9);
}

UTEST(eng_clock, stats_once_per_second_at_60_fps)
{
    EngClock clock;
    eng_clock_init_at(&clock, FREQ, 0);
    EngClockStats stats;
    int reports = 0;
    uint64_t t = 0;
    for (int i = 0; i < 180; i++) { /* 3 s à 60 Hz */
        t += 16666667;
        eng_clock_tick_at(&clock, t);
        if (eng_clock_take_stats(&clock, &stats)) {
            reports++;
            EXPECT_NEAR(stats.fps, 60.0, 0.5);
            EXPECT_NEAR(stats.avg_dt_ms, 16.67, 0.05);
        }
    }
    EXPECT_TRUE(reports >= 2 && reports <= 3);
}

UTEST(eng_clock, stats_report_the_worst_frame)
{
    EngClock clock;
    eng_clock_init_at(&clock, FREQ, 0);
    uint64_t t = 0;
    for (int i = 0; i < 59; i++) {
        t += 16000000;
        eng_clock_tick_at(&clock, t);
    }
    t += 120000000; /* un accroc de 120 ms */
    eng_clock_tick_at(&clock, t);
    t += 16000000;
    eng_clock_tick_at(&clock, t);
    EngClockStats stats;
    ASSERT_TRUE(eng_clock_take_stats(&clock, &stats));
    EXPECT_NEAR(stats.max_dt_ms, 120.0, 0.001);
    EXPECT_FALSE(eng_clock_take_stats(&clock, &stats)); /* fenêtre repartie de zéro */
}

UTEST(eng_clock, no_stats_before_one_second)
{
    EngClock clock;
    eng_clock_init_at(&clock, FREQ, 0);
    eng_clock_tick_at(&clock, 500000000);
    EngClockStats stats;
    EXPECT_FALSE(eng_clock_take_stats(&clock, &stats));
}

UTEST(eng_clock, real_clock_measures_a_delay)
{
    EngClock clock;
    eng_clock_init(&clock);
    SDL_Delay(20);
    eng_clock_tick(&clock);
    EXPECT_TRUE(clock.dt >= 0.019);
    EXPECT_TRUE(clock.dt < 0.5); /* large : un CI chargé peut rallonger le délai */
    double first = clock.time;
    SDL_Delay(5);
    eng_clock_tick(&clock);
    EXPECT_TRUE(clock.time > first);
}

UTEST(eng_clock, ms_between_converts_counter_readings_exactly)
{
    EngClock clock;
    eng_clock_init_at(&clock, FREQ, 0);
    EXPECT_NEAR(eng_clock_ms_between(&clock, 1000, 1000 + 2500000), 2.5, 1e-9);
    EXPECT_NEAR(eng_clock_ms_between(&clock, 7, 7), 0.0, 1e-12);
    EXPECT_NEAR(eng_clock_ms_between(&clock, 900, 100), 0.0, 1e-12); /* à l'envers : 0 */
    eng_clock_init_at(&clock, 1000, 0);                              /* 1 kHz : 1 tick = 1 ms */
    EXPECT_NEAR(eng_clock_ms_between(&clock, 10, 35), 25.0, 1e-9);
}

UTEST(eng_clock, the_real_counter_never_goes_backwards)
{
    const uint64_t a = eng_clock_counter();
    const uint64_t b = eng_clock_counter();
    EXPECT_GE(b, a);
    EXPECT_GT(a, (uint64_t)0);
}
