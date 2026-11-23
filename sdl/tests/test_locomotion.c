/* Déplacement relatif à la caméra (M4-07) : accélération, freinage, demi-tour sans glisse
 * excessive, rotation lissée. Valeurs attendues calculées à la main. */
#include <math.h>
#include <string.h>

#include "engine/locomotion.h"
#include "utest.h"

#define DT (1.0F / 60.0F)
#define EPS 1e-4F
#define PI_F 3.14159265F

/* Caméra qui regarde vers -Z (yaw 0) : avant = -Z, droite = +X. */
static const float FWD[3] = {0.0F, 0.0F, -1.0F};
static const float RIGHT[3] = {1.0F, 0.0F, 0.0F};

static EngInputState stick(float x, float y)
{
    EngInputState s;
    memset(&s, 0, sizeof(s));
    s.axis[ENG_AXIS_MOVE] = (EngAxisValue){x, y};
    return s;
}

static EngLocomotion make(float facing)
{
    EngLocomotion l;
    (void)eng_locomotion_init(&l, facing);
    return l;
}

static void run(EngLocomotion *l, const EngInputState *in, int steps)
{
    for (int i = 0; i < steps; i++) {
        (void)eng_locomotion_update(l, in, FWD, RIGHT, 1.0F, 1.0F, DT);
    }
}

UTEST(locomotion, move_is_relative_to_the_camera)
{
    EngLocomotion l = make(0.0F);
    EngInputState in = stick(0.0F, 1.0F);
    run(&l, &in, 60);
    float v[3];
    eng_locomotion_velocity(&l, v);
    EXPECT_NEAR(v[0], 0.0F, EPS);
    EXPECT_EQ(v[1], 0.0F);
    EXPECT_NEAR(v[2], -ENG_LOCOMOTION_MAX_SPEED_DEFAULT, EPS);

    l = make(0.0F);
    in = stick(1.0F, 0.0F);
    run(&l, &in, 60);
    eng_locomotion_velocity(&l, v);
    EXPECT_NEAR(v[0], ENG_LOCOMOTION_MAX_SPEED_DEFAULT, EPS);
    EXPECT_NEAR(v[2], 0.0F, EPS);

    /* caméra tournée de 90 degrés vers la gauche : avant = -X, droite = -Z */
    const float fwd90[3] = {-1.0F, 0.0F, 0.0F};
    const float right90[3] = {0.0F, 0.0F, -1.0F};
    l = make(0.0F);
    in = stick(0.0F, 1.0F);
    for (int i = 0; i < 60; i++) {
        EXPECT_TRUE(eng_locomotion_update(&l, &in, fwd90, right90, 1.0F, 1.0F, DT));
    }
    eng_locomotion_velocity(&l, v);
    EXPECT_NEAR(v[0], -ENG_LOCOMOTION_MAX_SPEED_DEFAULT, EPS);
    EXPECT_NEAR(v[2], 0.0F, EPS);
}

UTEST(locomotion, camera_direction_is_flattened_and_renormalised)
{
    EngLocomotion l = make(0.0F);
    const float fwd_up[3] = {0.0F, 0.8F, -0.1F}; /* regard très incliné vers le haut */
    const EngInputState in = stick(0.0F, 1.0F);
    for (int i = 0; i < 60; i++) {
        EXPECT_TRUE(eng_locomotion_update(&l, &in, fwd_up, RIGHT, 1.0F, 1.0F, DT));
    }
    EXPECT_NEAR(eng_locomotion_speed(&l), ENG_LOCOMOTION_MAX_SPEED_DEFAULT, EPS);
    float v[3];
    eng_locomotion_velocity(&l, v);
    EXPECT_NEAR(v[2], -ENG_LOCOMOTION_MAX_SPEED_DEFAULT, EPS);
}

UTEST(locomotion, accelerates_linearly_and_stops_at_max_speed)
{
    EngLocomotion l = make(0.0F);
    const EngInputState in = stick(0.0F, 1.0F);
    run(&l, &in, 3); /* 0,05 s */
    EXPECT_NEAR(eng_locomotion_speed(&l), ENG_LOCOMOTION_ACCEL_DEFAULT * 0.05F, EPS);
    run(&l, &in, 60);
    EXPECT_NEAR(eng_locomotion_speed(&l), ENG_LOCOMOTION_MAX_SPEED_DEFAULT, EPS);
}

UTEST(locomotion, stick_tilt_sets_the_pace_and_diagonals_are_not_faster)
{
    EngLocomotion l = make(0.0F);
    EngInputState in = stick(0.0F, 0.5F);
    run(&l, &in, 60);
    EXPECT_NEAR(eng_locomotion_speed(&l), 0.5F * ENG_LOCOMOTION_MAX_SPEED_DEFAULT, EPS);
    l = make(0.0F);
    in = stick(1.0F, 1.0F); /* ZQSD en diagonale : longueur ramenée à 1 */
    run(&l, &in, 60);
    EXPECT_NEAR(eng_locomotion_speed(&l), ENG_LOCOMOTION_MAX_SPEED_DEFAULT, EPS);
}

UTEST(locomotion, release_brakes_to_a_stop_within_the_expected_distance)
{
    EngLocomotion l = make(0.0F);
    EngInputState in = stick(0.0F, 1.0F);
    run(&l, &in, 60);
    in = stick(0.0F, 0.0F);
    float travelled = 0.0F;
    int steps = 0;
    while (eng_locomotion_speed(&l) > 0.0F && steps < 100) {
        EXPECT_TRUE(eng_locomotion_update(&l, &in, FWD, RIGHT, 1.0F, 1.0F, DT));
        travelled += eng_locomotion_speed(&l) * DT;
        steps++;
    }
    const float v = ENG_LOCOMOTION_MAX_SPEED_DEFAULT;
    /* arrêt en v / decel = 0,1125 s, soit 7 pas ; distance v²/(2 decel) = 0,253 m (+ pas) */
    EXPECT_EQ(steps, 7);
    EXPECT_NEAR(travelled, (v * v) / (2.0F * ENG_LOCOMOTION_DECEL_DEFAULT), 0.05F);
}

/* Demi-tour : on courait vers -Z, on pousse vers +Z. La glisse est la distance parcourue encore
 * vers -Z jusqu'à l'inversion de la vitesse. */
static void reversal(float speed_scale, float *slide, float *time_to_reverse)
{
    EngLocomotion l = make(0.0F);
    EngInputState in = stick(0.0F, 1.0F);
    for (int i = 0; i < 90; i++) {
        (void)eng_locomotion_update(&l, &in, FWD, RIGHT, speed_scale, 1.0F, DT);
    }
    in = stick(0.0F, -1.0F);
    float z = 0.0F;
    float max_z = 0.0F;
    int steps = 0;
    float v[3];
    eng_locomotion_velocity(&l, v);
    const float full = -v[2];
    *time_to_reverse = -1.0F;
    for (; steps < 120; steps++) {
        (void)eng_locomotion_update(&l, &in, FWD, RIGHT, speed_scale, 1.0F, DT);
        eng_locomotion_velocity(&l, v);
        z += v[2] * DT; /* v[2] < 0 : toujours vers l'avant ; > 0 : on est reparti */
        if (z < max_z) {
            max_z = z;
        }
        if (v[2] >= full - 1e-3F) {
            *time_to_reverse = (float)(steps + 1) * DT;
            break;
        }
    }
    *slide = -max_z;
}

UTEST(locomotion, u_turn_has_little_slide_and_is_quick)
{
    float slide;
    float t;
    reversal(1.0F, &slide, &t);
    /* v / reverse_decel = 0,075 s de freinage, v²/(2 reverse_decel) = 0,169 m de glisse */
    EXPECT_LT(slide, 0.22F);
    EXPECT_GT(slide, 0.10F); /* et ce n'est pas un arrêt instantané non plus */
    /* pleine vitesse arrière : freinage 4,5/60 = 0,075 s, puis accélération 4,5/30 = 0,15 s */
    EXPECT_NEAR(t, 0.225F, 0.02F);
    reversal(1.6F, &slide, &t); /* en sprint : 7,2 m/s */
    EXPECT_LT(slide, 0.5F);
    EXPECT_NEAR(t, (7.2F / 60.0F) + (7.2F / 30.0F), 0.02F);
}

UTEST(locomotion, speed_scale_and_control_scale_the_limits)
{
    EngLocomotion l = make(0.0F);
    const EngInputState in = stick(0.0F, 1.0F);
    for (int i = 0; i < 120; i++) {
        EXPECT_TRUE(eng_locomotion_update(&l, &in, FWD, RIGHT, 1.6F, 1.0F, DT));
    }
    EXPECT_NEAR(eng_locomotion_speed(&l), 1.6F * ENG_LOCOMOTION_MAX_SPEED_DEFAULT, EPS);
    l = make(0.0F);
    for (int i = 0; i < 3; i++) { /* control 0,5 : accélération moitié moindre */
        EXPECT_TRUE(eng_locomotion_update(&l, &in, FWD, RIGHT, 1.0F, 0.5F, DT));
    }
    EXPECT_NEAR(eng_locomotion_speed(&l), 0.5F * ENG_LOCOMOTION_ACCEL_DEFAULT * 0.05F, EPS);
}

/* Rotation : demi-tour complet en moins de 0,3 s, sans dépassement, sans pas supérieur au maximum.
 */
UTEST(locomotion, facing_turns_smoothly_to_the_walking_direction)
{
    EngLocomotion l = make(0.0F);
    const EngInputState in = stick(0.0F, -1.0F); /* vers +Z : lacet π */
    float prev = l.facing;
    float max_step = 0.0F;
    float reach = -1.0F;
    for (int i = 0; i < 60; i++) {
        EXPECT_TRUE(eng_locomotion_update(&l, &in, FWD, RIGHT, 1.0F, 1.0F, DT));
        max_step = fmaxf(max_step, fabsf(l.facing - prev));
        EXPECT_LE(l.facing, PI_F + 1e-5F); /* pas de dépassement */
        EXPECT_GE(l.facing, prev - 1e-6F); /* monotone */
        if (reach < 0.0F && l.facing > PI_F - (5.0F * PI_F / 180.0F)) {
            reach = (float)(i + 1) * DT;
        }
        prev = l.facing;
    }
    EXPECT_LE(max_step, ENG_LOCOMOTION_TURN_MAX_DEFAULT * DT + 1e-5F);
    EXPECT_GT(reach, 0.0F);
    EXPECT_LT(reach, 0.30F);
    EXPECT_GT(reach, 0.15F); /* pas un claquement : au moins (180° - 5°) / 1080°/s */
}

UTEST(locomotion, facing_follows_right_and_left_with_the_camera_convention)
{
    EngLocomotion l = make(0.0F);
    EngInputState in = stick(1.0F, 0.0F); /* à droite : le lacet diminue, vers -π/2 */
    run(&l, &in, 60);
    EXPECT_NEAR(l.facing, -PI_F / 2.0F, 1e-3F);
    l = make(0.0F);
    in = stick(-1.0F, 0.0F);
    run(&l, &in, 60);
    EXPECT_NEAR(l.facing, PI_F / 2.0F, 1e-3F);
}

UTEST(locomotion, facing_ignores_small_stick_and_keeps_direction_after_release)
{
    EngLocomotion l = make(0.7F);
    EngInputState in = stick(0.0F, 0.09F); /* sous la zone morte : pas de rotation */
    run(&l, &in, 30);
    EXPECT_EQ(l.facing, 0.7F);
    in = stick(1.0F, 0.0F);
    run(&l, &in, 60);
    const float turned = l.facing;
    in = stick(0.0F, 0.0F);
    run(&l, &in, 60);
    EXPECT_EQ(l.facing, turned);
}

UTEST(locomotion, facing_lerp_takes_the_short_way_round)
{
    EXPECT_NEAR(eng_locomotion_facing_lerp(0.0F, 1.0F, 0.5F), 0.5F, EPS);
    /* de 3,0 à -3,0 rad : le plus court est de passer par π (0,283 rad), pas par 0 */
    const float mid = eng_locomotion_facing_lerp(3.0F, -3.0F, 0.5F);
    EXPECT_GT(fabsf(mid), 3.1F);
    EXPECT_NEAR(eng_locomotion_facing_lerp(3.0F, -3.0F, 0.0F), 3.0F, EPS);
    EXPECT_NEAR(eng_locomotion_facing_lerp(3.0F, -3.0F, 1.0F), -3.0F, 1e-3F);
    EXPECT_NEAR(eng_locomotion_facing_lerp(0.0F, 1.0F, -2.0F), 0.0F, EPS); /* t borné */
    EXPECT_NEAR(eng_locomotion_facing_lerp(0.0F, 1.0F, 5.0F), 1.0F, EPS);
}

UTEST(locomotion, same_speed_at_60_and_120_hz)
{
    EngLocomotion a = make(0.0F);
    EngLocomotion b = make(0.0F);
    const EngInputState in = stick(0.3F, 1.0F);
    for (int i = 0; i < 30; i++) {
        EXPECT_TRUE(eng_locomotion_update(&a, &in, FWD, RIGHT, 1.0F, 1.0F, 1.0F / 60.0F));
    }
    for (int i = 0; i < 60; i++) {
        EXPECT_TRUE(eng_locomotion_update(&b, &in, FWD, RIGHT, 1.0F, 1.0F, 1.0F / 120.0F));
    }
    EXPECT_NEAR(a.vx, b.vx, 1e-3F);
    EXPECT_NEAR(a.vz, b.vz, 1e-3F);
}

static bool same(const EngLocomotion *a, const EngLocomotion *b)
{
    return a->vx == b->vx && a->vz == b->vz && a->facing == b->facing &&
           a->params.max_speed == b->params.max_speed && a->params.accel == b->params.accel;
}

UTEST(locomotion, invalid_arguments_change_nothing)
{
    EngLocomotion l = make(0.4F);
    const EngInputState in = stick(0.0F, 1.0F);
    run(&l, &in, 10);
    const EngLocomotion before = l;
    const float zero[3] = {0.0F, 0.0F, 0.0F};
    const float nanv[3] = {NAN, 0.0F, 0.0F};
    EXPECT_FALSE(eng_locomotion_update(NULL, &in, FWD, RIGHT, 1.0F, 1.0F, DT));
    EXPECT_FALSE(eng_locomotion_update(&l, NULL, FWD, RIGHT, 1.0F, 1.0F, DT));
    EXPECT_FALSE(eng_locomotion_update(&l, &in, NULL, RIGHT, 1.0F, 1.0F, DT));
    EXPECT_FALSE(eng_locomotion_update(&l, &in, FWD, NULL, 1.0F, 1.0F, DT));
    EXPECT_FALSE(eng_locomotion_update(&l, &in, zero, RIGHT, 1.0F, 1.0F, DT)); /* regard vertical */
    EXPECT_FALSE(eng_locomotion_update(&l, &in, FWD, nanv, 1.0F, 1.0F, DT));
    EXPECT_FALSE(eng_locomotion_update(&l, &in, FWD, RIGHT, -0.1F, 1.0F, DT));
    EXPECT_FALSE(eng_locomotion_update(&l, &in, FWD, RIGHT, 4.5F, 1.0F, DT));
    EXPECT_FALSE(eng_locomotion_update(&l, &in, FWD, RIGHT, NAN, 1.0F, DT));
    EXPECT_FALSE(eng_locomotion_update(&l, &in, FWD, RIGHT, 1.0F, 0.0F, DT));
    EXPECT_FALSE(eng_locomotion_update(&l, &in, FWD, RIGHT, 1.0F, 1.5F, DT));
    EXPECT_FALSE(eng_locomotion_update(&l, &in, FWD, RIGHT, 1.0F, 1.0F, 0.0F));
    EXPECT_FALSE(eng_locomotion_update(&l, &in, FWD, RIGHT, 1.0F, 1.0F, 0.11F));
    EXPECT_FALSE(eng_locomotion_update(&l, &in, FWD, RIGHT, 1.0F, 1.0F, NAN));
    EXPECT_TRUE(same(&l, &before));
    EXPECT_FALSE(eng_locomotion_init(&l, NAN));
    EXPECT_FALSE(eng_locomotion_init(NULL, 0.0F));
    EXPECT_TRUE(same(&l, &before));
    /* bornes incluses acceptées */
    EXPECT_TRUE(eng_locomotion_update(&l, &in, FWD, RIGHT, 0.0F, 1.0F, 0.1F));
    EXPECT_TRUE(eng_locomotion_update(&l, &in, FWD, RIGHT, 4.0F, 1.0F, DT));
    /* un stick non fini compte pour un stick au repos */
    EngLocomotion m = make(0.0F);
    EngInputState bad = stick(NAN, 1.0F);
    run(&m, &bad, 10);
    EXPECT_EQ(eng_locomotion_speed(&m), 0.0F);
}

UTEST(locomotion, params_are_validated)
{
    EngLocomotion l = make(0.0F);
    EngLocomotionParams p = l.params;
    p.max_speed = 6.0F;
    EXPECT_TRUE(eng_locomotion_set_params(&l, &p));
    EXPECT_EQ(l.params.max_speed, 6.0F);
    p.turn_tau = 0.0F; /* sans lissage : permis */
    p.deadzone = 0.0F;
    EXPECT_TRUE(eng_locomotion_set_params(&l, &p));
    const EngLocomotionParams good = l.params;
    EngLocomotionParams q = good;
    q.max_speed = 0.0F;
    EXPECT_FALSE(eng_locomotion_set_params(&l, &q));
    q = good;
    q.accel = -1.0F;
    EXPECT_FALSE(eng_locomotion_set_params(&l, &q));
    q = good;
    q.decel = NAN;
    EXPECT_FALSE(eng_locomotion_set_params(&l, &q));
    q = good;
    q.reverse_decel = 0.0F;
    EXPECT_FALSE(eng_locomotion_set_params(&l, &q));
    q = good;
    q.turn_tau = -0.1F;
    EXPECT_FALSE(eng_locomotion_set_params(&l, &q));
    q = good;
    q.turn_max = 0.0F;
    EXPECT_FALSE(eng_locomotion_set_params(&l, &q));
    q = good;
    q.deadzone = 1.0F;
    EXPECT_FALSE(eng_locomotion_set_params(&l, &q));
    q = good;
    q.deadzone = -0.1F;
    EXPECT_FALSE(eng_locomotion_set_params(&l, &q));
    EXPECT_FALSE(eng_locomotion_set_params(NULL, &good));
    EXPECT_FALSE(eng_locomotion_set_params(&l, NULL));
    EXPECT_EQ(l.params.max_speed, 6.0F);
    /* sans lissage de rotation, la rotation est bornée par turn_max seul */
    p.turn_max = 1.0F;
    EXPECT_TRUE(eng_locomotion_set_params(&l, &p));
    const EngInputState in = stick(0.0F, -1.0F);
    EXPECT_TRUE(eng_locomotion_update(&l, &in, FWD, RIGHT, 1.0F, 1.0F, DT));
    EXPECT_NEAR(l.facing, 1.0F * DT, 1e-6F);
}

UTEST(locomotion, stop_zeroes_velocity_and_keeps_facing)
{
    EngLocomotion l = make(0.0F);
    const EngInputState in = stick(1.0F, 0.0F);
    run(&l, &in, 30);
    const float f = l.facing;
    eng_locomotion_stop(&l);
    EXPECT_EQ(eng_locomotion_speed(&l), 0.0F);
    EXPECT_EQ(l.facing, f);
}

UTEST(locomotion, init_keeps_pi_and_wraps_other_angles_into_the_half_open_interval)
{
    EngLocomotion l;
    EXPECT_TRUE(eng_locomotion_init(&l, PI_F));
    EXPECT_NEAR(l.facing, PI_F, 1e-6F);
    EXPECT_GT(l.facing, 0.0F); /* ]-π ; π] : π est inclus, -π ne l'est pas */
    EXPECT_TRUE(eng_locomotion_init(&l, -PI_F));
    EXPECT_GT(l.facing, 0.0F);
    EXPECT_TRUE(eng_locomotion_init(&l, 7.0F)); /* 7 - 2π */
    EXPECT_NEAR(l.facing, 7.0F - (2.0F * PI_F), 1e-5F);
    EXPECT_TRUE(eng_locomotion_init(&l, -7.0F));
    EXPECT_NEAR(l.facing, -7.0F + (2.0F * PI_F), 1e-5F);
}

/* Les cas ci-dessous couvrent l'axe x, les directions obliques et les changements d'allure : les
 * premiers tests n'utilisaient que l'axe z de l'écran. */
UTEST(locomotion, every_axis_accelerates_and_brakes_the_same_way)
{
    EngLocomotion l = make(0.0F);
    EngInputState in = stick(1.0F, 0.0F);
    run(&l, &in, 3);
    EXPECT_NEAR(l.vx, ENG_LOCOMOTION_ACCEL_DEFAULT * 0.05F, EPS);
    EXPECT_NEAR(l.vz, 0.0F, EPS);
    l = make(0.0F);
    in = stick(-1.0F, 1.0F); /* diagonale avant-gauche : (-0.707, -0.707) en x, z */
    run(&l, &in, 3);
    EXPECT_NEAR(l.vx, -ENG_LOCOMOTION_ACCEL_DEFAULT * 0.05F * 0.70710678F, EPS);
    EXPECT_NEAR(l.vz, -ENG_LOCOMOTION_ACCEL_DEFAULT * 0.05F * 0.70710678F, EPS);
    l = make(0.0F);
    in = stick(0.5F, 0.0F); /* demi-stick sur x */
    run(&l, &in, 60);
    EXPECT_NEAR(l.vx, 0.5F * ENG_LOCOMOTION_MAX_SPEED_DEFAULT, EPS);
}

UTEST(locomotion, easing_off_the_stick_uses_the_braking_rate)
{
    EngLocomotion l = make(0.0F);
    EngInputState in = stick(0.0F, 1.0F);
    run(&l, &in, 60); /* pleine vitesse */
    in = stick(0.0F, 0.5F);
    run(&l, &in, 3);
    /* 4,5 - 3 x 40/60 = 2,5 m/s (au taux d'accélération ce serait 3,0) */
    EXPECT_NEAR(eng_locomotion_speed(&l), ENG_LOCOMOTION_MAX_SPEED_DEFAULT - 2.0F, EPS);
}

UTEST(locomotion, u_turn_works_on_x_and_diagonals_too)
{
    const float dirs[3][2] = {{1.0F, 0.0F}, {0.70710678F, 0.70710678F}, {-1.0F, 0.0F}};
    for (int k = 0; k < 3; k++) {
        EngLocomotion l = make(0.0F);
        EngInputState in = stick(dirs[k][0], dirs[k][1]);
        run(&l, &in, 90);
        float v[3];
        eng_locomotion_velocity(&l, v);
        const float dx = v[0] / ENG_LOCOMOTION_MAX_SPEED_DEFAULT;
        const float dz = v[2] / ENG_LOCOMOTION_MAX_SPEED_DEFAULT;
        in = stick(-dirs[k][0], -dirs[k][1]);
        float along = 0.0F;
        float max_along = 0.0F;
        for (int i = 0; i < 40; i++) {
            EXPECT_TRUE(eng_locomotion_update(&l, &in, FWD, RIGHT, 1.0F, 1.0F, DT));
            eng_locomotion_velocity(&l, v);
            along += ((v[0] * dx) + (v[2] * dz)) * DT; /* progression dans l'ancien sens */
            if (along > max_along) {
                max_along = along;
            }
        }
        EXPECT_GT(max_along, 0.10F);
        EXPECT_LT(max_along, 0.22F);
        EXPECT_NEAR(((v[0] * dx) + (v[2] * dz)), -ENG_LOCOMOTION_MAX_SPEED_DEFAULT, 1e-3F);
    }
}

UTEST(locomotion, right_vector_of_a_rotated_camera_is_used_for_sideways_input)
{
    const float fwd90[3] = {-1.0F, 0.0F, 0.0F};
    const float right90[3] = {0.0F, 0.0F, -1.0F};
    EngLocomotion l = make(0.0F);
    const EngInputState in = stick(1.0F, 0.0F); /* à droite de l'écran : -Z */
    for (int i = 0; i < 60; i++) {
        EXPECT_TRUE(eng_locomotion_update(&l, &in, fwd90, right90, 1.0F, 1.0F, DT));
    }
    EXPECT_NEAR(l.vz, -ENG_LOCOMOTION_MAX_SPEED_DEFAULT, EPS);
    EXPECT_NEAR(l.vx, 0.0F, EPS);
    const EngInputState diag = stick(1.0F, 1.0F);
    l = make(0.0F);
    for (int i = 0; i < 60; i++) {
        EXPECT_TRUE(eng_locomotion_update(&l, &diag, fwd90, right90, 1.0F, 1.0F, DT));
    }
    EXPECT_NEAR(l.vx, -ENG_LOCOMOTION_MAX_SPEED_DEFAULT * 0.70710678F, EPS);
    EXPECT_NEAR(l.vz, -ENG_LOCOMOTION_MAX_SPEED_DEFAULT * 0.70710678F, EPS);
}

UTEST(locomotion, zero_acceleration_or_deceleration_is_refused)
{
    EngLocomotion l = make(0.0F);
    EngLocomotionParams p = l.params;
    p.accel = 0.0F;
    EXPECT_FALSE(eng_locomotion_set_params(&l, &p));
    p = l.params;
    p.decel = 0.0F;
    EXPECT_FALSE(eng_locomotion_set_params(&l, &p));
}
