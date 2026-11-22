/* Boss spéciaux : phases du cavalier (cheval, cinématique, à pied) et souffle du dragon. */
#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#include "engine/boss_special.h"
#include "utest.h"

UTEST(boss_special, the_horse_carries_the_first_forty_percent_then_the_knight_fights_on_foot)
{
    EngRider r;
    eng_rider_init(&r);
    float hp = 200.0F;
    EXPECT_FALSE(eng_rider_step(&r, &hp, 200.0F));
    EXPECT_EQ(r.phase, ENG_RIDER_MOUNTED);
    EXPECT_NEAR(eng_rider_horse_hp(&r, 200.0F, 200.0F), 1.0F, 1e-6F);
    EXPECT_NEAR(eng_rider_horse_hp(&r, 160.0F, 200.0F), 0.5F, 1e-6F);
    EXPECT_EQ(eng_rider_horse_fall(&r), 0.0F);
    hp = 121.0F; /* le cheval a encore 1 PV */
    EXPECT_FALSE(eng_rider_step(&r, &hp, 200.0F));
    hp = 70.0F; /* un gros coup : le cheval meurt, le surplus ne touche pas le chevalier */
    EXPECT_TRUE(eng_rider_step(&r, &hp, 200.0F));
    EXPECT_NEAR(hp, 120.0F, 1e-4F);
    EXPECT_EQ(r.phase, ENG_RIDER_FALLING);
    EXPECT_TRUE(eng_rider_cinematic(&r));
    EXPECT_EQ(eng_rider_horse_hp(&r, hp, 200.0F), 0.0F);
    /* cinématique : PV tenus, le cheval se couche, le chevalier tombe puis se relève */
    bool was_down = false;
    float fall_before = 0.0F;
    for (uint32_t i = 0; i < ENG_RIDER_FALL_TICKS - 1U; i++) {
        hp = 5.0F; /* un coup pendant la cinématique est annulé */
        EXPECT_FALSE(eng_rider_step(&r, &hp, 200.0F));
        EXPECT_NEAR(hp, 120.0F, 1e-4F);
        ASSERT_TRUE(eng_rider_cinematic(&r));
        EXPECT_GE(eng_rider_horse_fall(&r), fall_before);
        fall_before = eng_rider_horse_fall(&r);
        was_down = was_down || eng_rider_down(&r);
    }
    EXPECT_TRUE(was_down);
    EXPECT_FALSE(eng_rider_down(&r)); /* relevé avant la fin */
    EXPECT_NEAR(fall_before, 1.0F, 1e-6F);
    EXPECT_FALSE(eng_rider_step(&r, &hp, 200.0F));
    EXPECT_EQ(r.phase, ENG_RIDER_ON_FOOT);
    EXPECT_FALSE(eng_rider_cinematic(&r));
    hp = 40.0F; /* à pied : les coups passent */
    EXPECT_FALSE(eng_rider_step(&r, &hp, 200.0F));
    EXPECT_NEAR(hp, 40.0F, 1e-6F);
    EXPECT_NEAR(eng_rider_horse_fall(&r), 1.0F, 1e-6F);
    /* arguments invalides : rien ne change */
    eng_rider_init(&r);
    hp = 1.0F;
    EXPECT_FALSE(eng_rider_step(NULL, &hp, 200.0F));
    EXPECT_FALSE(eng_rider_step(&r, NULL, 200.0F));
    EXPECT_FALSE(eng_rider_step(&r, &hp, 0.0F));
    EXPECT_FALSE(eng_rider_step(&r, &hp, NAN));
    EXPECT_EQ(r.phase, ENG_RIDER_MOUNTED);
    EXPECT_FALSE(eng_rider_cinematic(NULL));
}

UTEST(boss_special, the_flame_charges_burns_nine_times_at_most_then_cools_down)
{
    EngFlame f;
    eng_flame_init(&f);
    EXPECT_FALSE(eng_flame_step(&f, false));
    EXPECT_EQ(f.state, ENG_FLAME_IDLE);
    EXPECT_FALSE(eng_flame_busy(&f));
    EXPECT_FALSE(eng_flame_step(&f, true));
    ASSERT_EQ(f.state, ENG_FLAME_CHARGE);
    EXPECT_TRUE(eng_flame_busy(&f));
    float charge = 0.0F;
    for (uint32_t i = 0; i < ENG_FLAME_CHARGE_TICKS - 1U; i++) { /* la charge ne brûle pas */
        EXPECT_FALSE(eng_flame_step(&f, false));
        EXPECT_GE(eng_flame_charge(&f), charge);
        charge = eng_flame_charge(&f);
        EXPECT_EQ(eng_flame_jet(&f), 0.0F);
    }
    EXPECT_FALSE(eng_flame_step(&f, false));
    ASSERT_EQ(f.state, ENG_FLAME_BURN);
    uint32_t burns = 0;
    float jet_max = 0.0F;
    for (uint32_t i = 0; i < ENG_FLAME_BURN_TICKS; i++) {
        jet_max = fmaxf(jet_max, eng_flame_jet(&f));
        burns += eng_flame_step(&f, true) ? 1U : 0U;
    }
    EXPECT_EQ(burns, (ENG_FLAME_BURN_TICKS + ENG_FLAME_HIT_EVERY - 1U) / ENG_FLAME_HIT_EVERY);
    EXPECT_NEAR(jet_max, 1.0F, 1e-6F);
    ASSERT_EQ(f.state, ENG_FLAME_COOLDOWN);
    EXPECT_FALSE(eng_flame_busy(&f));
    EXPECT_EQ(eng_flame_jet(&f), 0.0F);
    for (uint32_t i = 0; i < ENG_FLAME_COOLDOWN_TICKS - 1U; i++) { /* pas de nouveau souffle */
        EXPECT_FALSE(eng_flame_step(&f, true));
        ASSERT_EQ(f.state, ENG_FLAME_COOLDOWN);
    }
    EXPECT_FALSE(eng_flame_step(&f, true));
    EXPECT_EQ(f.state, ENG_FLAME_IDLE);
    EXPECT_FALSE(eng_flame_step(NULL, true));
}

UTEST(boss_special, the_flame_cone_reaches_only_in_front_between_near_and_far)
{
    const float src[2] = {10.0F, 20.0F};
    const float half = 0.45F;
    /* lacet 0 : le dragon regarde -Z */
    EXPECT_TRUE(eng_flame_reaches(src, 0.0F, (float[2]){10.0F, 12.0F}, 3.0F, 14.0F, half));
    EXPECT_FALSE(
        eng_flame_reaches(src, 0.0F, (float[2]){10.0F, 28.0F}, 3.0F, 14.0F, half)); /* dos */
    EXPECT_FALSE(
        eng_flame_reaches(src, 0.0F, (float[2]){18.0F, 20.0F}, 3.0F, 14.0F, half)); /* flanc */
    EXPECT_FALSE(
        eng_flame_reaches(src, 0.0F, (float[2]){10.0F, 18.0F}, 3.0F, 14.0F, half)); /* trop près */
    EXPECT_FALSE(
        eng_flame_reaches(src, 0.0F, (float[2]){10.0F, 4.0F}, 3.0F, 14.0F, half)); /* trop loin */
    /* bord du cône : 8 m devant, 3 m de côté (20,6 degrés) dedans ; 5 m de côté (32 degrés) dehors
     */
    EXPECT_TRUE(eng_flame_reaches(src, 0.0F, (float[2]){13.0F, 12.0F}, 3.0F, 14.0F, half));
    EXPECT_FALSE(eng_flame_reaches(src, 0.0F, (float[2]){15.0F, 12.0F}, 3.0F, 14.0F, half));
    /* lacet -pi/2 : le dragon regarde +X (convention atan2(-dx, -dz)) */
    EXPECT_TRUE(eng_flame_reaches(src, -1.5707963F, (float[2]){18.0F, 20.0F}, 3.0F, 14.0F, half));
    EXPECT_FALSE(eng_flame_reaches(src, -1.5707963F, (float[2]){2.0F, 20.0F}, 3.0F, 14.0F, half));
    EXPECT_FALSE(eng_flame_reaches(src, NAN, (float[2]){10.0F, 12.0F}, 3.0F, 14.0F, half));
    EXPECT_FALSE(eng_flame_reaches(NULL, 0.0F, (float[2]){10.0F, 12.0F}, 3.0F, 14.0F, half));
}

/* Le souffle au sol du dragon géant : une bande devant lui, de plus en plus large. */
UTEST(boss_special, the_flame_strip_widens_from_the_mouth_to_the_end_of_the_jet)
{
    const float src[2] = {10.0F, 20.0F};
    /* cap 0 : il regarde -Z ; bande de 60 à 160 m, large de 2 x 5 m puis 2 x 20 m */
    EXPECT_TRUE(eng_flame_strip(src, 0.0F, (float[2]){10.0F, -90.0F}, 60.0F, 160.0F, 5.0F, 20.0F));
    EXPECT_TRUE(eng_flame_strip(src, 0.0F, (float[2]){10.0F, -40.0F}, 60.0F, 160.0F, 5.0F, 20.0F));
    EXPECT_FALSE(eng_flame_strip(src, 0.0F, (float[2]){10.0F, -39.0F}, 60.0F, 160.0F, 5.0F, 20.0F));
    EXPECT_TRUE(eng_flame_strip(src, 0.0F, (float[2]){10.0F, -140.0F}, 60.0F, 160.0F, 5.0F, 20.0F));
    EXPECT_FALSE(
        eng_flame_strip(src, 0.0F, (float[2]){10.0F, -141.0F}, 60.0F, 160.0F, 5.0F, 20.0F));
    EXPECT_FALSE(eng_flame_strip(src, 0.0F, (float[2]){10.0F, 110.0F}, 60.0F, 160.0F, 5.0F,
                                 20.0F)); /* dans son dos */
    /* près de la gueule : 5 m de chaque côté ; au bout : 20 m ; au milieu : 12,5 m */
    EXPECT_TRUE(eng_flame_strip(src, 0.0F, (float[2]){14.9F, -40.0F}, 60.0F, 160.0F, 5.0F, 20.0F));
    EXPECT_FALSE(eng_flame_strip(src, 0.0F, (float[2]){15.2F, -40.0F}, 60.0F, 160.0F, 5.0F, 20.0F));
    EXPECT_TRUE(eng_flame_strip(src, 0.0F, (float[2]){-9.9F, -140.0F}, 60.0F, 160.0F, 5.0F, 20.0F));
    EXPECT_FALSE(
        eng_flame_strip(src, 0.0F, (float[2]){-10.2F, -140.0F}, 60.0F, 160.0F, 5.0F, 20.0F));
    EXPECT_TRUE(eng_flame_strip(src, 0.0F, (float[2]){22.4F, -90.0F}, 60.0F, 160.0F, 5.0F, 20.0F));
    EXPECT_FALSE(eng_flame_strip(src, 0.0F, (float[2]){22.6F, -90.0F}, 60.0F, 160.0F, 5.0F, 20.0F));
    /* cap pi/2 : il regarde -X */
    EXPECT_TRUE(
        eng_flame_strip(src, 1.5707963F, (float[2]){-90.0F, 22.0F}, 60.0F, 160.0F, 5.0F, 20.0F));
    EXPECT_FALSE(
        eng_flame_strip(src, 1.5707963F, (float[2]){10.0F, -90.0F}, 60.0F, 160.0F, 5.0F, 20.0F));
    /* arguments absents, non finis, bande vide */
    EXPECT_FALSE(
        eng_flame_strip(NULL, 0.0F, (float[2]){10.0F, -90.0F}, 60.0F, 160.0F, 5.0F, 20.0F));
    EXPECT_FALSE(eng_flame_strip(src, 0.0F, NULL, 60.0F, 160.0F, 5.0F, 20.0F));
    EXPECT_FALSE(eng_flame_strip(src, NAN, (float[2]){10.0F, -90.0F}, 60.0F, 160.0F, 5.0F, 20.0F));
    EXPECT_FALSE(eng_flame_strip(src, 0.0F, (float[2]){NAN, -90.0F}, 60.0F, 160.0F, 5.0F, 20.0F));
    EXPECT_FALSE(
        eng_flame_strip(src, 0.0F, (float[2]){10.0F, -90.0F}, 160.0F, 160.0F, 5.0F, 20.0F));
    EXPECT_FALSE(eng_flame_strip(src, 0.0F, (float[2]){10.0F, -90.0F}, 60.0F, 160.0F, NAN, 20.0F));
}
