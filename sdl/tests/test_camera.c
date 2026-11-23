/* Caméra libre de debug (M1-09) : orientation, matrices, déplacement, vitesse, interpolation, et
 * le chemin complet entrées → EngInputState → caméra. Valeurs attendues calculées à la main. */
#include <math.h>

#include <SDL3/SDL_scancode.h>

#include "engine/camera.h"
#include "engine/input.h"
#include "utest.h"

#define EPS 1e-4F

#define EXPECT_VEC3(v, ex, ey, ez)                                                                 \
    do {                                                                                           \
        EXPECT_NEAR((v)[0], (ex), EPS);                                                            \
        EXPECT_NEAR((v)[1], (ey), EPS);                                                            \
        EXPECT_NEAR((v)[2], (ez), EPS);                                                            \
    } while (0)

static EngCamera make_camera(float yaw, float pitch)
{
    EngCamera c;
    eng_camera_init(&c, (float[3]){1.0F, 2.0F, 3.0F}, yaw, pitch);
    return c;
}

static EngInputState state_move(float x, float y)
{
    EngInputState s = {0};
    s.axis[ENG_AXIS_MOVE] = (EngAxisValue){x, y};
    return s;
}

static EngInputState state_held(EngAction action)
{
    EngInputState s = {0};
    s.held = 1U << action;
    return s;
}

/* Distance parcourue depuis (1, 2, 3). */
static float travelled(const EngCamera *c)
{
    const float dx = c->position[0] - 1.0F;
    const float dy = c->position[1] - 2.0F;
    const float dz = c->position[2] - 3.0F;
    return sqrtf((dx * dx) + (dy * dy) + (dz * dz));
}

UTEST(eng_camera, init_sets_defaults_and_clamps_pitch)
{
    EngCamera c = make_camera(0.5F, 0.25F);
    EXPECT_VEC3(c.position, 1.0F, 2.0F, 3.0F);
    EXPECT_NEAR(c.yaw, 0.5F, EPS);
    EXPECT_NEAR(c.pitch, 0.25F, EPS);
    EXPECT_NEAR(c.fov_y, glm_rad(60.0F), EPS);
    EXPECT_NEAR(c.near_z, 0.1F, EPS);
    EXPECT_NEAR(c.far_z, 1000.0F, EPS);
    EXPECT_NEAR(c.speed, ENG_CAMERA_SPEED_DEFAULT, EPS);
    EXPECT_NEAR(make_camera(0.0F, 3.0F).pitch, ENG_CAMERA_MAX_PITCH, EPS);
    EXPECT_NEAR(make_camera(0.0F, -3.0F).pitch, -ENG_CAMERA_MAX_PITCH, EPS);
}

UTEST(eng_camera, default_orientation_looks_along_minus_z)
{
    EngCamera c = make_camera(0.0F, 0.0F);
    float f[3];
    float r[3];
    eng_camera_forward(&c, f);
    eng_camera_right(&c, r);
    EXPECT_VEC3(f, 0.0F, 0.0F, -1.0F);
    EXPECT_VEC3(r, 1.0F, 0.0F, 0.0F);
}

UTEST(eng_camera, positive_yaw_turns_left_and_positive_pitch_looks_up)
{
    float f[3];
    float r[3];
    EngCamera left = make_camera((float)GLM_PI_2, 0.0F);
    eng_camera_forward(&left, f);
    eng_camera_right(&left, r);
    EXPECT_VEC3(f, -1.0F, 0.0F, 0.0F); /* vers -X : la gauche de la vue par défaut */
    EXPECT_VEC3(r, 0.0F, 0.0F, -1.0F); /* droite = avant × haut */

    EngCamera up = make_camera(0.0F, (float)GLM_PI_4);
    eng_camera_forward(&up, f);
    EXPECT_VEC3(f, 0.0F, sqrtf(0.5F), -sqrtf(0.5F));
    eng_camera_right(&up, r);
    EXPECT_VEC3(r, 1.0F, 0.0F, 0.0F); /* le pitch n'incline pas la droite */
}

UTEST(eng_camera, forward_and_right_are_unit_and_orthogonal_for_any_orientation)
{
    for (int yi = -7; yi <= 7; yi++) {
        for (int pi = -3; pi <= 3; pi++) {
            EngCamera c = make_camera((float)yi * 0.9F, (float)pi * 0.5F);
            float f[3];
            float r[3];
            eng_camera_forward(&c, f);
            eng_camera_right(&c, r);
            EXPECT_NEAR(glm_vec3_norm(f), 1.0F, EPS);
            EXPECT_NEAR(glm_vec3_norm(r), 1.0F, EPS);
            EXPECT_NEAR(glm_vec3_dot(f, r), 0.0F, EPS);
            EXPECT_NEAR(r[1], 0.0F, EPS);
        }
    }
}

UTEST(eng_camera, rotate_accumulates_yaw_without_wrapping_and_clamps_pitch)
{
    EngCamera c = make_camera(0.0F, 0.0F);
    eng_camera_rotate(&c, 4.0F, 0.1F);
    eng_camera_rotate(&c, 4.0F, 0.1F);
    EXPECT_NEAR(c.yaw, 8.0F, EPS); /* > 2π : pas de repli, l'interpolation reste continue */
    EXPECT_NEAR(c.pitch, 0.2F, EPS);
    eng_camera_rotate(&c, 0.0F, 10.0F);
    EXPECT_NEAR(c.pitch, ENG_CAMERA_MAX_PITCH, EPS);
    eng_camera_rotate(&c, 0.0F, -100.0F);
    EXPECT_NEAR(c.pitch, -ENG_CAMERA_MAX_PITCH, EPS);
    EXPECT_LT(ENG_CAMERA_MAX_PITCH, (float)GLM_PI_2); /* jamais la verticale exacte */
}

UTEST(eng_camera, view_matrix_puts_the_camera_at_the_origin_looking_down_minus_z)
{
    EngCamera c = make_camera(0.7F, -0.3F);
    mat4 view;
    eng_camera_view(&c, view);
    float f[3];
    float r[3];
    eng_camera_forward(&c, f);
    eng_camera_right(&c, r);
    vec3 p;
    /* un point 5 m devant : dans l'espace de la caméra il est en (0, 0, -5) */
    const float ahead[3] = {c.position[0] + (5.0F * f[0]), c.position[1] + (5.0F * f[1]),
                            c.position[2] + (5.0F * f[2])};
    glm_mat4_mulv3(view, (float *)ahead, 1.0F, p);
    EXPECT_VEC3(p, 0.0F, 0.0F, -5.0F);
    /* un point 2 m à droite : en (+2, 0, 0) */
    const float side[3] = {c.position[0] + (2.0F * r[0]), c.position[1] + (2.0F * r[1]),
                           c.position[2] + (2.0F * r[2])};
    glm_mat4_mulv3(view, (float *)side, 1.0F, p);
    EXPECT_VEC3(p, 2.0F, 0.0F, 0.0F);
}

UTEST(eng_camera, projection_uses_the_camera_lens_and_aspect)
{
    EngCamera c = make_camera(0.0F, 0.0F);
    mat4 view;
    mat4 proj;
    mat4 vp;
    eng_camera_view(&c, view);
    eng_camera_projection(&c, 2.0F, proj);
    glm_mat4_mul(proj, view, vp);
    vec3 ndc = {0.0F, 0.0F, 0.0F};
    /* devant, à 10 m : au centre */
    EXPECT_TRUE(eng_project_point(vp, (float[3]){1.0F, 2.0F, -7.0F}, ndc));
    EXPECT_NEAR(ndc[0], 0.0F, EPS);
    EXPECT_NEAR(ndc[1], 0.0F, EPS);
    EXPECT_GT(ndc[2], 0.0F);
    EXPECT_LT(ndc[2], 1.0F);
    /* à droite et en haut du centre : x > 0 et y > 0 (y vers le haut) */
    EXPECT_TRUE(eng_project_point(vp, (float[3]){2.0F, 3.0F, -7.0F}, ndc));
    EXPECT_GT(ndc[0], 0.0F);
    EXPECT_GT(ndc[1], 0.0F);
    /* demi-hauteur visible à 10 m : tan(30°) × 10 ; le bord haut est à y = 1 exactement */
    const float half = tanf(c.fov_y * 0.5F) * 10.0F;
    EXPECT_TRUE(eng_project_point(vp, (float[3]){1.0F, 2.0F + half, -7.0F}, ndc));
    EXPECT_NEAR(ndc[1], 1.0F, EPS);
    /* aspect 2 : le bord droit est deux fois plus loin que le bord haut */
    EXPECT_TRUE(eng_project_point(vp, (float[3]){1.0F + (2.0F * half), 2.0F, -7.0F}, ndc));
    EXPECT_NEAR(ndc[0], 1.0F, EPS);
    /* derrière la caméra : refusé */
    EXPECT_FALSE(eng_project_point(vp, (float[3]){1.0F, 2.0F, 10.0F}, ndc));
}

UTEST(eng_camera, projection_maps_the_near_plane_to_depth_0_and_the_far_plane_to_1)
{
    EngCamera c = make_camera(0.0F, 0.0F);
    c.near_z = 0.5F;
    c.far_z = 200.0F;
    mat4 view;
    mat4 proj;
    mat4 vp;
    eng_camera_view(&c, view);
    eng_camera_projection(&c, 1.0F, proj);
    glm_mat4_mul(proj, view, vp);
    vec3 ndc = {0.0F, 0.0F, 0.0F};
    EXPECT_TRUE(eng_project_point(vp, (float[3]){1.0F, 2.0F, 3.0F - 0.5F}, ndc));
    EXPECT_NEAR(ndc[2], 0.0F, 1e-3F);
    EXPECT_TRUE(eng_project_point(vp, (float[3]){1.0F, 2.0F, 3.0F - 200.0F}, ndc));
    EXPECT_NEAR(ndc[2], 1.0F, 1e-3F);
    /* à mi-chemin (en distance), la profondeur est non linéaire : 100,25 m → (f(d-n))/(d(f-n)) */
    const float d = 100.25F;
    EXPECT_TRUE(eng_project_point(vp, (float[3]){1.0F, 2.0F, 3.0F - d}, ndc));
    EXPECT_NEAR(ndc[2], (200.0F * (d - 0.5F)) / (d * (200.0F - 0.5F)), 1e-4F);
}

UTEST(eng_camera, no_input_means_no_movement)
{
    EngCamera c = make_camera(0.3F, 0.2F);
    EngInputState s = {0};
    eng_camera_update(&c, &s, 1.0F);
    EXPECT_NEAR(travelled(&c), 0.0F, EPS);
    EXPECT_NEAR(c.yaw, 0.3F, EPS);
    EXPECT_NEAR(c.pitch, 0.2F, EPS);
    EXPECT_NEAR(c.speed, ENG_CAMERA_SPEED_DEFAULT, EPS);
}

UTEST(eng_camera, forward_moves_along_the_view_direction_at_speed_times_dt)
{
    EngCamera c = make_camera(0.0F, 0.0F);
    EngInputState s = state_move(0.0F, 1.0F);
    eng_camera_update(&c, &s, 0.5F);
    EXPECT_VEC3(c.position, 1.0F, 2.0F, 3.0F - (0.5F * ENG_CAMERA_SPEED_DEFAULT));

    EngCamera back = make_camera(0.0F, 0.0F);
    s = state_move(0.0F, -1.0F);
    eng_camera_update(&back, &s, 0.5F);
    EXPECT_VEC3(back.position, 1.0F, 2.0F, 3.0F + (0.5F * ENG_CAMERA_SPEED_DEFAULT));
}

UTEST(eng_camera, forward_follows_yaw_and_pitch)
{
    EngCamera yawed = make_camera((float)GLM_PI_2, 0.0F);
    EngInputState s = state_move(0.0F, 1.0F);
    eng_camera_update(&yawed, &s, 1.0F);
    EXPECT_VEC3(yawed.position, 1.0F - ENG_CAMERA_SPEED_DEFAULT, 2.0F, 3.0F);

    EngCamera pitched = make_camera(0.0F, (float)GLM_PI_4);
    eng_camera_update(&pitched, &s, 1.0F);
    const float d = ENG_CAMERA_SPEED_DEFAULT * sqrtf(0.5F);
    EXPECT_VEC3(pitched.position, 1.0F, 2.0F + d,
                3.0F - d); /* vol libre : monte en regardant en haut */
}

UTEST(eng_camera, strafing_moves_along_the_horizontal_right_vector)
{
    EngCamera c = make_camera(0.0F, 0.6F);
    EngInputState s = state_move(1.0F, 0.0F);
    eng_camera_update(&c, &s, 1.0F);
    EXPECT_VEC3(c.position, 1.0F + ENG_CAMERA_SPEED_DEFAULT, 2.0F, 3.0F); /* pitch sans effet */

    EngCamera rot = make_camera((float)GLM_PI_2, 0.0F);
    eng_camera_update(&rot, &s, 1.0F);
    EXPECT_VEC3(rot.position, 1.0F, 2.0F, 3.0F - ENG_CAMERA_SPEED_DEFAULT);
}

UTEST(eng_camera, diagonal_is_not_faster_than_straight)
{
    EngCamera c = make_camera(0.0F, 0.0F);
    /* MOVE est déjà de longueur <= 1 ; on force 1,0 + 1,0 pour tester la normalisation propre */
    EngInputState s = state_move(1.0F, 1.0F);
    eng_camera_update(&c, &s, 1.0F);
    EXPECT_NEAR(travelled(&c), ENG_CAMERA_SPEED_DEFAULT, EPS);
    EXPECT_NEAR(c.position[0] - 1.0F, ENG_CAMERA_SPEED_DEFAULT * sqrtf(0.5F), EPS);
    EXPECT_NEAR(c.position[2] - 3.0F, -ENG_CAMERA_SPEED_DEFAULT * sqrtf(0.5F), EPS);
}

UTEST(eng_camera, partial_stick_deflection_moves_proportionally)
{
    EngCamera c = make_camera(0.0F, 0.0F);
    EngInputState s = state_move(0.0F, 0.25F);
    eng_camera_update(&c, &s, 1.0F);
    EXPECT_NEAR(travelled(&c), 0.25F * ENG_CAMERA_SPEED_DEFAULT, EPS);
}

UTEST(eng_camera, jump_goes_up_dodge_goes_down_both_cancel)
{
    EngCamera up = make_camera(1.0F, 0.5F); /* l'orientation n'a pas d'effet sur la verticale */
    EngInputState s = state_held(ENG_ACTION_JUMP);
    eng_camera_update(&up, &s, 1.0F);
    EXPECT_VEC3(up.position, 1.0F, 2.0F + ENG_CAMERA_SPEED_DEFAULT, 3.0F);

    EngCamera down = make_camera(1.0F, 0.5F);
    s = state_held(ENG_ACTION_DODGE);
    eng_camera_update(&down, &s, 1.0F);
    EXPECT_VEC3(down.position, 1.0F, 2.0F - ENG_CAMERA_SPEED_DEFAULT, 3.0F);

    EngCamera both = make_camera(1.0F, 0.5F);
    s.held = (1U << ENG_ACTION_JUMP) | (1U << ENG_ACTION_DODGE);
    eng_camera_update(&both, &s, 1.0F);
    EXPECT_NEAR(travelled(&both), 0.0F, EPS);
}

UTEST(eng_camera, only_held_actions_count_not_pressed_or_released)
{
    EngCamera c = make_camera(0.0F, 0.0F);
    EngInputState s = {0};
    s.pressed = (1U << ENG_ACTION_JUMP) | (1U << ENG_ACTION_SPRINT);
    s.released = (1U << ENG_ACTION_DODGE);
    eng_camera_update(&c, &s, 1.0F);
    EXPECT_NEAR(travelled(&c), 0.0F, EPS);
}

UTEST(eng_camera, sprint_multiplies_speed_for_every_direction)
{
    EngCamera c = make_camera(0.0F, 0.0F);
    EngInputState s = state_move(0.0F, 1.0F);
    s.held = 1U << ENG_ACTION_SPRINT;
    eng_camera_update(&c, &s, 1.0F);
    EXPECT_NEAR(travelled(&c), ENG_CAMERA_SPEED_DEFAULT * ENG_CAMERA_SPRINT_FACTOR, EPS);

    EngCamera v = make_camera(0.0F, 0.0F);
    s = state_held(ENG_ACTION_JUMP);
    s.held |= 1U << ENG_ACTION_SPRINT;
    eng_camera_update(&v, &s, 1.0F);
    EXPECT_NEAR(v.position[1] - 2.0F, ENG_CAMERA_SPEED_DEFAULT * ENG_CAMERA_SPRINT_FACTOR, EPS);
    EXPECT_GT(ENG_CAMERA_SPRINT_FACTOR, 1.0F);
}

UTEST(eng_camera, movement_is_proportional_to_dt_and_to_speed)
{
    EngCamera a = make_camera(0.0F, 0.0F);
    EngCamera b = make_camera(0.0F, 0.0F);
    EngInputState s = state_move(0.0F, 1.0F);
    eng_camera_update(&a, &s, 1.0F / 60.0F);
    for (int i = 0; i < 60; i++) {
        eng_camera_update(&b, &s, 1.0F / 60.0F);
    }
    EXPECT_NEAR(travelled(&a), ENG_CAMERA_SPEED_DEFAULT / 60.0F, EPS);
    EXPECT_NEAR(travelled(&b), ENG_CAMERA_SPEED_DEFAULT, 1e-3F);

    EngCamera fast = make_camera(0.0F, 0.0F);
    eng_camera_set_speed(&fast, 20.0F);
    eng_camera_update(&fast, &s, 0.5F);
    EXPECT_NEAR(travelled(&fast), 10.0F, EPS);
}

UTEST(eng_camera, look_axis_turns_by_the_look_unit)
{
    EngCamera c = make_camera(0.0F, 0.0F);
    EngInputState s = {0};
    s.axis[ENG_AXIS_LOOK] = (EngAxisValue){1.0F, 0.0F}; /* droite : le yaw diminue */
    eng_camera_update(&c, &s, 1.0F);
    EXPECT_NEAR(c.yaw, -ENG_CAMERA_LOOK_UNIT, EPS);
    EXPECT_NEAR(c.pitch, 0.0F, EPS);

    s.axis[ENG_AXIS_LOOK] = (EngAxisValue){0.0F, 2.0F}; /* haut : le pitch augmente */
    eng_camera_update(&c, &s, 1.0F);
    EXPECT_NEAR(c.yaw, -ENG_CAMERA_LOOK_UNIT, EPS);
    EXPECT_NEAR(c.pitch, 2.0F * ENG_CAMERA_LOOK_UNIT, EPS);

    s.axis[ENG_AXIS_LOOK] = (EngAxisValue){0.0F, -1000.0F}; /* borné */
    eng_camera_update(&c, &s, 1.0F);
    EXPECT_NEAR(c.pitch, -ENG_CAMERA_MAX_PITCH, EPS);
}

UTEST(eng_camera, looking_does_not_depend_on_dt)
{
    EngCamera c = make_camera(0.0F, 0.0F);
    EngInputState s = {0};
    s.axis[ENG_AXIS_LOOK] = (EngAxisValue){-1.0F, 0.0F};
    eng_camera_update(&c, &s, 0.0F); /* les entrées de regard sont déjà des déplacements */
    EXPECT_NEAR(c.yaw, ENG_CAMERA_LOOK_UNIT, EPS);
}

UTEST(eng_camera, wheel_scales_speed_geometrically)
{
    EngCamera c = make_camera(0.0F, 0.0F);
    EngInputState s = {0};
    s.wheel = 1.0F;
    eng_camera_update(&c, &s, 0.0F);
    EXPECT_NEAR(c.speed, ENG_CAMERA_SPEED_DEFAULT * ENG_CAMERA_WHEEL_STEP, EPS);
    s.wheel = -1.0F;
    eng_camera_update(&c, &s, 0.0F);
    EXPECT_NEAR(c.speed, ENG_CAMERA_SPEED_DEFAULT, EPS);
    s.wheel = 3.0F;
    eng_camera_update(&c, &s, 0.0F);
    EXPECT_NEAR(c.speed, ENG_CAMERA_SPEED_DEFAULT * powf(ENG_CAMERA_WHEEL_STEP, 3.0F), 1e-3F);
    EXPECT_GT(ENG_CAMERA_WHEEL_STEP, 1.0F);
}

UTEST(eng_camera, wheel_speed_is_clamped_and_applies_to_the_same_step_movement)
{
    EngCamera c = make_camera(0.0F, 0.0F);
    EngInputState s = {0};
    s.wheel = 1000.0F;
    eng_camera_update(&c, &s, 0.0F);
    EXPECT_NEAR(c.speed, ENG_CAMERA_SPEED_MAX, EPS);
    s.wheel = -1000.0F;
    eng_camera_update(&c, &s, 0.0F);
    EXPECT_NEAR(c.speed, ENG_CAMERA_SPEED_MIN, EPS);

    EngCamera m = make_camera(0.0F, 0.0F);
    s = state_move(0.0F, 1.0F);
    s.wheel = 1.0F;
    eng_camera_update(&m, &s, 1.0F);
    EXPECT_NEAR(travelled(&m), ENG_CAMERA_SPEED_DEFAULT * ENG_CAMERA_WHEEL_STEP, EPS);
}

UTEST(eng_camera, set_speed_clamps_and_ignores_nan)
{
    EngCamera c = make_camera(0.0F, 0.0F);
    eng_camera_set_speed(&c, 12.0F);
    EXPECT_NEAR(c.speed, 12.0F, EPS);
    eng_camera_set_speed(&c, 0.0F);
    EXPECT_NEAR(c.speed, ENG_CAMERA_SPEED_MIN, EPS);
    eng_camera_set_speed(&c, -5.0F);
    EXPECT_NEAR(c.speed, ENG_CAMERA_SPEED_MIN, EPS);
    eng_camera_set_speed(&c, 1.0e9F);
    EXPECT_NEAR(c.speed, ENG_CAMERA_SPEED_MAX, EPS);
    eng_camera_set_speed(&c, 12.0F);
    eng_camera_set_speed(&c, NAN);
    EXPECT_NEAR(c.speed, 12.0F, EPS);
}

UTEST(eng_camera, lerp_interpolates_pose_and_takes_the_rest_from_the_target)
{
    EngCamera a = make_camera(1.0F, 0.0F);
    EngCamera b = make_camera(3.0F, 0.4F);
    b.position[0] = 11.0F;
    b.position[1] = 12.0F;
    b.position[2] = 13.0F;
    b.speed = 42.0F;
    b.fov_y = 1.0F;
    EngCamera out;
    eng_camera_lerp(&a, &b, 0.0F, &out);
    EXPECT_VEC3(out.position, 1.0F, 2.0F, 3.0F);
    EXPECT_NEAR(out.yaw, 1.0F, EPS);
    eng_camera_lerp(&a, &b, 1.0F, &out);
    EXPECT_VEC3(out.position, 11.0F, 12.0F, 13.0F);
    EXPECT_NEAR(out.yaw, 3.0F, EPS);
    EXPECT_NEAR(out.pitch, 0.4F, EPS);
    eng_camera_lerp(&a, &b, 0.25F, &out);
    EXPECT_VEC3(out.position, 3.5F, 4.5F, 5.5F);
    EXPECT_NEAR(out.yaw, 1.5F, EPS);
    EXPECT_NEAR(out.pitch, 0.1F, EPS);
    EXPECT_NEAR(out.speed, 42.0F, EPS);
    EXPECT_NEAR(out.fov_y, 1.0F, EPS);
    eng_camera_lerp(&a, &b, 7.0F, &out); /* t borné */
    EXPECT_NEAR(out.yaw, 3.0F, EPS);
    eng_camera_lerp(&a, &b, -7.0F, &out);
    EXPECT_NEAR(out.yaw, 1.0F, EPS);
}

UTEST(eng_camera, lerp_is_safe_when_output_aliases_an_input)
{
    EngCamera a = make_camera(0.0F, 0.0F);
    EngCamera b = make_camera(2.0F, 0.0F);
    eng_camera_lerp(&a, &b, 0.5F, &a);
    EXPECT_NEAR(a.yaw, 1.0F, EPS);
}

/* ── Chemin complet : événements bruts → liaisons par défaut → EngInputState → caméra ── */

static EngInputState tick(EngInput *input)
{
    EngInputState s;
    eng_input_begin_tick(input, &s);
    return s;
}

UTEST(eng_camera, default_wasd_bindings_fly_the_camera)
{
    EngInput input;
    eng_input_init(&input);
    eng_input_set_default_bindings(&input);
    EngCamera c = make_camera(0.0F, 0.0F);

    eng_input_key(&input, SDL_SCANCODE_W, true);
    EngInputState s = tick(&input);
    eng_camera_update(&c, &s, 1.0F);
    EXPECT_VEC3(c.position, 1.0F, 2.0F, 3.0F - ENG_CAMERA_SPEED_DEFAULT);
    eng_input_key(&input, SDL_SCANCODE_W, false);

    eng_input_key(&input, SDL_SCANCODE_D, true);
    eng_input_key(&input, SDL_SCANCODE_LSHIFT, true);
    s = tick(&input);
    eng_camera_update(&c, &s, 1.0F);
    EXPECT_NEAR(c.position[0], 1.0F + (ENG_CAMERA_SPEED_DEFAULT * ENG_CAMERA_SPRINT_FACTOR), EPS);
    eng_input_key(&input, SDL_SCANCODE_D, false);
    eng_input_key(&input, SDL_SCANCODE_LSHIFT, false);

    const float y0 = c.position[1];
    eng_input_key(&input, SDL_SCANCODE_SPACE, true);
    s = tick(&input);
    eng_camera_update(&c, &s, 1.0F);
    EXPECT_NEAR(c.position[1], y0 + ENG_CAMERA_SPEED_DEFAULT, EPS);
    eng_input_key(&input, SDL_SCANCODE_SPACE, false);
    eng_input_key(&input, SDL_SCANCODE_LCTRL, true);
    s = tick(&input);
    eng_camera_update(&c, &s, 1.0F);
    EXPECT_NEAR(c.position[1], y0, EPS);
}

UTEST(eng_camera, mouse_motion_turns_the_view_by_pixels_times_sensitivity)
{
    EngInput input;
    eng_input_init(&input);
    eng_input_set_default_bindings(&input);
    /* comme le lanceur : sensibilité en radians par pixel (0,002) convertie en unités de regard */
    input.axes[ENG_AXIS_LOOK].mouse_sensitivity = 0.002F / ENG_CAMERA_LOOK_UNIT;
    EngCamera c = make_camera(0.0F, 0.0F);

    eng_input_mouse_motion(&input, 100.0F, 0.0F); /* 100 px vers la droite */
    EngInputState s = tick(&input);
    eng_camera_update(&c, &s, 1.0F / 60.0F);
    EXPECT_NEAR(c.yaw, -0.2F, EPS);
    eng_input_mouse_motion(&input, 0.0F, -50.0F); /* 50 px vers le haut de l'écran */
    s = tick(&input);
    eng_camera_update(&c, &s, 1.0F / 60.0F);
    EXPECT_NEAR(c.pitch, 0.1F, EPS);
    s = tick(&input); /* rien de nouveau : la vue ne dérive pas */
    eng_camera_update(&c, &s, 1.0F / 60.0F);
    EXPECT_NEAR(c.yaw, -0.2F, EPS);
    EXPECT_NEAR(c.pitch, 0.1F, EPS);
}

UTEST(eng_camera, mouse_sensitivity_zero_ignores_the_mouse)
{
    EngInput input;
    eng_input_init(&input);
    eng_input_set_default_bindings(&input);
    input.axes[ENG_AXIS_LOOK].mouse_sensitivity = 0.0F; /* souris non capturée (lanceur) */
    EngCamera c = make_camera(0.0F, 0.0F);
    eng_input_mouse_motion(&input, 500.0F, 500.0F);
    EngInputState s = tick(&input);
    eng_camera_update(&c, &s, 1.0F);
    EXPECT_NEAR(c.yaw, 0.0F, EPS);
    EXPECT_NEAR(c.pitch, 0.0F, EPS);
}

UTEST(eng_camera, right_stick_look_is_gentle_with_the_look_unit)
{
    EngInput input;
    eng_input_init(&input);
    eng_input_set_default_bindings(&input);
    EngCamera c = make_camera(0.0F, 0.0F);
    eng_input_pad_axis(&input, ENG_PAD_AXIS_RIGHT_X, 32767); /* stick droit à fond à droite */
    EngInputState s = tick(&input);
    eng_camera_update(&c, &s, 1.0F / 60.0F);
    /* ≈ 0,03 rad par pas, soit environ 103°/s à 60 Hz : vitesse de visée raisonnable */
    EXPECT_NEAR(c.yaw, -ENG_CAMERA_LOOK_UNIT, 1e-3F);
}

UTEST(eng_camera, wheel_notches_accumulate_per_tick_and_reset)
{
    EngInput input;
    eng_input_init(&input);
    eng_input_mouse_wheel(&input, 1.0F);
    eng_input_mouse_wheel(&input, 2.0F);
    eng_input_mouse_wheel(&input, -0.5F);
    EngInputState s = tick(&input);
    EXPECT_NEAR(s.wheel, 2.5F, EPS);
    s = tick(&input);
    EXPECT_NEAR(s.wheel, 0.0F, EPS);
}
