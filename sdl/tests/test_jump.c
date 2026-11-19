/* Saut avec délai de grâce et mémorisation de la touche (M4-08), module pur. Les durées sont
 * comptées en pas de 1/60 s : 0,1 s = 6 pas, 0,12 s = 7,2 pas. */
#include <math.h>

#include "engine/jump.h"
#include "utest.h"

#define DT (1.0F / 60.0F)

static EngJump make(void)
{
    EngJump j;
    (void)eng_jump_init(&j);
    return j;
}

static EngJumpEvents step(EngJump *j, bool grounded, bool pressed)
{
    EngJumpEvents ev;
    (void)eng_jump_update(j, grounded, pressed, DT, &ev);
    return ev;
}

UTEST(jump, jumps_when_pressed_on_the_ground_and_not_otherwise)
{
    EngJump j = make();
    EXPECT_FALSE(step(&j, true, false).jump);
    EXPECT_TRUE(step(&j, true, true).jump);
}

/* Critère : saut accepté 100 ms après avoir quitté un rebord. */
UTEST(jump, is_accepted_100_ms_after_leaving_a_ledge_and_refused_after)
{
    for (int k = 1; k <= 8; k++) {
        EngJump j = make();
        step(&j, true, false);
        for (int i = 1; i < k; i++) {
            EXPECT_FALSE(step(&j, false, false).jump); /* en l'air depuis i pas */
        }
        const bool jumped = step(&j, false, true).jump; /* touche pressée k pas après le rebord */
        if (k <= 6) {
            EXPECT_TRUE(jumped); /* jusqu'à 6 x 1/60 = 100 ms */
        } else {
            EXPECT_FALSE(jumped); /* 116 ms, 133 ms : trop tard */
        }
    }
}

UTEST(jump, buffered_press_fires_on_landing_within_the_buffer_window)
{
    for (int k = 1; k <= 10; k++) {
        EngJump j = make();
        step(&j, true, false);
        /* en l'air : on a sauté ou on tombe d'un rebord depuis longtemps */
        for (int i = 0; i < 20; i++) {
            step(&j, false, false);
        }
        step(&j, false, true); /* la touche est pressée... */
        for (int i = 1; i < k; i++) {
            EXPECT_FALSE(step(&j, false, false).jump);
        }
        const bool jumped = step(&j, true, false).jump; /* ...puis on atterrit k pas plus tard */
        EXPECT_EQ(jumped, k <= 7); /* 7 pas = 117 ms <= 120 ms ; 8 pas = 133 ms */
    }
}

UTEST(jump, no_double_jump_and_coyote_is_disarmed_after_a_jump)
{
    EngJump j = make();
    EXPECT_TRUE(step(&j, true, true).jump);
    EXPECT_FALSE(step(&j, false, false).jump);
    EXPECT_FALSE(step(&j, false, true).jump); /* appui en l'air juste après le saut : refusé */
    for (int i = 0; i < 4; i++) {
        EXPECT_FALSE(step(&j, false, true).jump); /* même dans les 100 ms */
    }
    /* un appui mémorisé en l'air se déclenche à l'atterrissage, comme prévu */
    EXPECT_TRUE(step(&j, true, false).jump);
}

UTEST(jump, ground_still_reported_right_after_a_jump_does_not_allow_a_second_jump)
{
    EngJump j = make();
    EXPECT_TRUE(step(&j, true, true).jump);
    EXPECT_FALSE(step(&j, true, true).jump); /* le sol est encore signalé un pas : pas de rebond */
    EXPECT_FALSE(step(&j, true, true).jump);
    step(&j, false, false);
    /* vrai atterrissage : la touche pressée pendant ces pas (mémoire de 0,12 s) saute aussitôt */
    EXPECT_TRUE(step(&j, true, false).jump);
    EXPECT_FALSE(step(&j, true, false).jump);
    step(&j, false, false); /* un second cycle en l'air puis au sol réarme le saut */
    step(&j, true, false);
    EXPECT_TRUE(step(&j, true, true).jump);
}

UTEST(jump, events_report_landing_and_leaving_the_ground)
{
    EngJump j = make();
    EngJumpEvents ev = step(&j, true, false);
    EXPECT_FALSE(ev.landed); /* le personnage commence au sol : pas d'atterrissage au départ */
    EXPECT_FALSE(ev.left_ground);
    ev = step(&j, false, false);
    EXPECT_TRUE(ev.left_ground);
    EXPECT_FALSE(ev.landed);
    ev = step(&j, false, false);
    EXPECT_FALSE(ev.left_ground);
    ev = step(&j, true, false);
    EXPECT_TRUE(ev.landed);
    ev = step(&j, true, false);
    EXPECT_FALSE(ev.landed);
}

UTEST(jump, a_press_is_consumed_by_the_jump_it_triggers)
{
    EngJump j = make();
    EXPECT_TRUE(step(&j, true, true).jump);
    step(&j, false, false);
    step(&j, true, false); /* atterri : la vieille pression a été consommée */
    EXPECT_FALSE(step(&j, true, false).jump);
}

UTEST(jump, zero_windows_mean_exact_frame_only)
{
    EngJump j = make();
    const EngJumpParams p = {0.0F, 0.0F};
    EXPECT_TRUE(eng_jump_set_params(&j, &p));
    step(&j, true, false);
    EXPECT_FALSE(step(&j, false, true).jump); /* pas de délai de grâce */
    EXPECT_FALSE(step(&j, false, false).jump);
    EXPECT_FALSE(step(&j, true, false).jump); /* pas de mémoire de touche */
    EXPECT_TRUE(step(&j, true, true).jump);
}

UTEST(jump, windows_are_measured_in_time_not_in_frames)
{
    /* à 144 Hz, 100 ms = 14,4 pas : le 14e est accepté, le 15e non */
    const float dt = 1.0F / 144.0F;
    for (int k = 13; k <= 16; k++) {
        EngJump j = make();
        EngJumpEvents ev;
        EXPECT_TRUE(eng_jump_update(&j, true, false, dt, &ev));
        for (int i = 1; i < k; i++) {
            EXPECT_TRUE(eng_jump_update(&j, false, false, dt, &ev));
        }
        EXPECT_TRUE(eng_jump_update(&j, false, true, dt, &ev));
        EXPECT_EQ(ev.jump, k <= 14);
    }
}

UTEST(jump, params_and_arguments_are_validated)
{
    EngJump j = make();
    EngJumpParams p = {0.2F, 0.3F};
    EXPECT_TRUE(eng_jump_set_params(&j, &p));
    EXPECT_EQ(j.params.coyote, 0.2F);
    EXPECT_EQ(j.params.buffer, 0.3F);
    p = (EngJumpParams){0.5F, 0.5F}; /* bornes incluses */
    EXPECT_TRUE(eng_jump_set_params(&j, &p));
    const EngJumpParams bad[5] = {
        {-0.01F, 0.1F}, {0.1F, -0.01F}, {0.51F, 0.1F}, {0.1F, 0.51F}, {NAN, 0.1F}};
    for (int i = 0; i < 5; i++) {
        EXPECT_FALSE(eng_jump_set_params(&j, &bad[i]));
    }
    EXPECT_FALSE(eng_jump_set_params(&j, &(EngJumpParams){0.1F, NAN}));
    EXPECT_FALSE(eng_jump_set_params(NULL, &p));
    EXPECT_FALSE(eng_jump_set_params(&j, NULL));
    EXPECT_EQ(j.params.coyote, 0.5F);

    EXPECT_FALSE(eng_jump_init(NULL));
    EXPECT_FALSE(eng_jump_update(NULL, true, true, DT, NULL));
    EXPECT_FALSE(eng_jump_update(&j, true, true, 0.0F, NULL));
    EXPECT_FALSE(eng_jump_update(&j, true, true, -DT, NULL));
    EXPECT_FALSE(eng_jump_update(&j, true, true, 0.11F, NULL));
    EXPECT_FALSE(eng_jump_update(&j, true, true, NAN, NULL));
    EXPECT_TRUE(eng_jump_update(&j, true, true, 0.1F, NULL)); /* dt maximal, events NULL ok */
}
