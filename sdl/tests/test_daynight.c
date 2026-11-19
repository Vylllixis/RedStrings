#include <math.h>
#include <string.h>

#include "engine/daynight.h"
#include "engine/lighting.h"
#include "utest.h"

UTEST(daynight, the_sun_rises_east_peaks_south_at_noon_and_sets_west)
{
    EngDayNight d;
    eng_daynight_init(&d, 6.0);
    float az = 0.0F;
    float el = 0.0F;
    eng_daynight_sun(&d, &az, &el);
    EXPECT_NEAR(el, 0.0F, 1e-4F);
    EXPECT_NEAR(az, 90.0F, 1e-4F);
    EngLighting l;
    eng_lighting_default(&l);
    ASSERT_TRUE(eng_lighting_set_sun_angles(&l, az, el));
    EXPECT_NEAR(l.sun_direction[0], 1.0F, 1e-4F); /* lever : vers +X */
    ASSERT_TRUE(eng_daynight_set_hour(&d, 12.0));
    eng_daynight_sun(&d, &az, &el);
    EXPECT_NEAR(el, 62.0F, 1e-4F);
    ASSERT_TRUE(eng_lighting_set_sun_angles(&l, az, el));
    EXPECT_LT(l.sun_direction[2], -0.4F); /* midi : vers -Z, en hauteur */
    EXPECT_NEAR(l.sun_direction[0], 0.0F, 1e-4F);
    ASSERT_TRUE(eng_daynight_set_hour(&d, 18.0));
    eng_daynight_sun(&d, &az, &el);
    EXPECT_NEAR(el, 0.0F, 1e-4F);
    ASSERT_TRUE(eng_lighting_set_sun_angles(&l, az, el));
    EXPECT_NEAR(l.sun_direction[0], -1.0F, 1e-4F); /* coucher : vers -X */
    ASSERT_TRUE(eng_daynight_set_hour(&d, 0.0));
    eng_daynight_sun(&d, &az, &el);
    EXPECT_NEAR(el, -62.0F, 1e-4F); /* minuit : sous l'horizon */
    /* l'élévation croît strictement de 0 h à midi */
    float prev = -100.0F;
    for (int i = 0; i <= 48; i++) {
        ASSERT_TRUE(eng_daynight_set_hour(&d, (double)i * 0.25));
        eng_daynight_sun(&d, NULL, &el);
        EXPECT_GT(el, prev);
        prev = el;
    }
}

UTEST(daynight, the_clock_advances_wraps_and_ignores_bad_steps)
{
    EngDayNight d;
    eng_daynight_init(&d, 23.5);
    eng_daynight_tick(&d, 60.0); /* 1 min réelle = 1 h de jeu */
    EXPECT_NEAR(d.hour, 0.5, 1e-9);
    eng_daynight_tick(&d, -5.0);
    eng_daynight_tick(&d, NAN);
    eng_daynight_tick(&d, INFINITY);
    EXPECT_NEAR(d.hour, 0.5, 1e-9);
    d.speed = 0.0F;
    eng_daynight_tick(&d, 600.0);
    EXPECT_NEAR(d.hour, 0.5, 1e-9);
    d.speed = 2.0F;
    eng_daynight_tick(&d, 30.0);
    EXPECT_NEAR(d.hour, 1.5, 1e-9);
    eng_daynight_tick(&d, 1440.0 * 10.0 + 60.0); /* dix journées et une heure */
    EXPECT_NEAR(d.hour, 3.5, 1e-6);
    EXPECT_FALSE(eng_daynight_set_hour(&d, NAN));
    EXPECT_TRUE(eng_daynight_set_hour(&d, -1.0));
    EXPECT_NEAR(d.hour, 23.0, 1e-9);
    EXPECT_TRUE(eng_daynight_set_hour(&d, 48.25));
    EXPECT_NEAR(d.hour, 0.25, 1e-9);
    char text[8];
    EXPECT_EQ(eng_daynight_format(&d, text, sizeof text), 5U);
    EXPECT_STREQ(text, "00:15");
    EXPECT_TRUE(eng_daynight_set_hour(&d, 13.999));
    (void)eng_daynight_format(&d, text, sizeof text);
    EXPECT_STREQ(text, "13:59");
    EXPECT_EQ(eng_daynight_format(&d, text, 5U), 0U);
}

UTEST(daynight, night_weight_and_exposure_are_smooth_and_bounded)
{
    EXPECT_EQ(eng_daynight_night_weight(30.0F), 0.0F);
    EXPECT_EQ(eng_daynight_night_weight(2.0F), 0.0F);
    EXPECT_NEAR(eng_daynight_night_weight(-4.0F), 0.5F, 1e-5F);
    EXPECT_NEAR(eng_daynight_night_weight(-7.0F), 0.84375F, 1e-5F); /* smoothstep(0,75), pas 0,75 */
    EXPECT_EQ(eng_daynight_night_weight(-10.0F), 1.0F);
    EXPECT_EQ(eng_daynight_night_weight(-60.0F), 1.0F);
    EXPECT_EQ(eng_daynight_night_weight(NAN), 0.0F);
    float prev = 0.0F;
    for (int i = 0; i <= 60; i++) {
        const float w = eng_daynight_night_weight(5.0F - (float)i * 0.25F);
        EXPECT_GE(w, prev);
        EXPECT_LE(w - prev, 0.07F); /* pas de saut */
        prev = w;
    }
    EngDayNight d;
    eng_daynight_init(&d, 12.0);
    EXPECT_EQ(eng_daynight_exposure_bias(&d, 40.0F), 0.0F);
    EXPECT_NEAR(eng_daynight_exposure_bias(&d, -30.0F), 1.5F, 1e-6F);
    d.night_exposure_ev = -3.0F;
    EXPECT_EQ(eng_daynight_exposure_bias(&d, -30.0F), 0.0F);
}
