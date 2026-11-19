#include <math.h>
#include <stdint.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/debug_lines.h"
#include "engine/jump.h"
#include "engine/physics.h"
#include "utest.h"

#define DT (1.0F / 60.0F)

#define NEED_PHYSICS()                                                                             \
    if (!eng_physics_available()) {                                                                \
        return;                                                                                    \
    }

/* Sol : dessus de la boîte en y = 0, de z = -10 à z = +10 et x = -10 à 10. */
static void add_floor(EngPhysics *w)
{
    const float half[3] = {10.0F, 0.5F, 10.0F};
    const float pos[3] = {0.0F, -0.5F, 0.0F};
    (void)eng_physics_add_box(w, half, pos, ENG_LAYER_STATIC);
}

static EngCharacter *make_char(EngPhysics *w, float x, float y, float z)
{
    EngCharacterDesc d;
    memset(&d, 0, sizeof(d));
    d.position[0] = x;
    d.position[1] = y;
    d.position[2] = z;
    return eng_character_create(w, &d);
}

static void run(EngPhysics *w, EngCharacter *c, int steps, float vx, float vz)
{
    const float move[3] = {vx, 0.0F, vz};
    for (int i = 0; i < steps; i++) {
        (void)eng_physics_step(w, DT);
        (void)eng_character_update(c, DT, move);
    }
}

UTEST(character, falls_and_lands_on_the_floor)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    add_floor(w);
    EngCharacter *c = make_char(w, 0.0F, 2.0F, 0.0F);
    ASSERT_TRUE(c != NULL);
    ASSERT_EQ(eng_character_ground(c), ENG_GROUND_AIR);
    run(w, c, 120, 0.0F, 0.0F);
    float f[3];
    ASSERT_TRUE(eng_character_position(c, f));
    ASSERT_NEAR(f[1], 0.0F, 0.05F);
    ASSERT_EQ(eng_character_ground(c), ENG_GROUND_SUPPORTED);
    float n[3];
    ASSERT_TRUE(eng_character_ground_normal(c, n));
    ASSERT_NEAR(n[1], 1.0F, 1e-3F);
    eng_character_destroy(c);
    eng_physics_destroy(w);
}

UTEST(character, walks_on_flat_ground_at_the_requested_speed)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    add_floor(w);
    EngCharacter *c = make_char(w, 0.0F, 0.0F, -5.0F);
    run(w, c, 30, 0.0F, 0.0F); /* se pose */
    float a[3];
    float b[3];
    (void)eng_character_position(c, a);
    run(w, c, 60, 0.0F, 3.0F);
    (void)eng_character_position(c, b);
    ASSERT_NEAR(b[2] - a[2], 3.0F, 0.1F);
    ASSERT_NEAR(b[0] - a[0], 0.0F, 1e-3F);
    ASSERT_NEAR(b[1], 0.0F, 0.05F);
    eng_character_destroy(c);
    eng_physics_destroy(w);
}

/* Rampe montante vers +z à partir de z = 0, inclinée de `deg`, large de 6 m, longue de 8 m. */
static EngPhysicsShape *make_ramp(EngPhysics *w, float deg)
{
    const float len = 8.0F;
    const float h = len * tanf(deg * 3.14159265F / 180.0F);
    const float v[12] = {-3.0F, 0.0F, 0.0F, 3.0F, 0.0F, 0.0F, -3.0F, h, len, 3.0F, h, len};
    const uint32_t idx[6] = {0, 2, 1, 1, 2, 3};
    return eng_physics_shape_mesh(w, v, 12U, 4U, idx, 6U);
}

static float slope_test(float deg, float *out_z)
{
    EngPhysics *w = eng_physics_create(NULL);
    add_floor(w);
    EngPhysicsShape *ramp = make_ramp(w, deg);
    (void)eng_physics_add_static_shape(w, ramp, NULL);
    EngCharacter *c = make_char(w, 0.0F, 0.0F, -2.0F);
    run(w, c, 30, 0.0F, 0.0F);
    run(w, c, 180, 0.0F, 2.0F);
    float f[3];
    (void)eng_character_position(c, f);
    *out_z = f[2];
    eng_character_destroy(c);
    eng_physics_shape_destroy(ramp);
    eng_physics_destroy(w);
    return f[1];
}

UTEST(character, climbs_a_40_degree_slope)
{
    NEED_PHYSICS();
    float z;
    const float y = slope_test(40.0F, &z);
    /* la vitesse horizontale baisse sur une pente (la vitesse voulue est projetée sur le plan) :
     * on ne vérifie pas la distance parcourue, seulement qu'il est bien monté, sur la surface */
    ASSERT_GT(z, 1.5F);
    ASSERT_NEAR(y, z * tanf(40.0F * 3.14159265F / 180.0F), 0.15F);
}

/* La vitesse le long du sol reste celle qu'on demande, en montée comme en descente (sans cela, Jolt
 * projetterait la vitesse horizontale sur la pente et le personnage ralentirait de cos² 30 = 0,75
 * en montée). */
UTEST(character, keeps_its_speed_along_a_30_degree_slope_up_and_down)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    add_floor(w);
    EngPhysicsShape *ramp = make_ramp(w, 30.0F);
    (void)eng_physics_add_static_shape(w, ramp, NULL);
    const float z0 = 2.0F;
    EngCharacter *c = make_char(w, 0.0F, z0 * tanf(30.0F * 3.14159265F / 180.0F), z0);
    run(w, c, 30, 0.0F, 0.0F);
    ASSERT_EQ(eng_character_ground(c), ENG_GROUND_SUPPORTED);
    float a[3];
    float b[3];
    (void)eng_character_position(c, a);
    run(w, c, 60, 0.0F, 3.0F);
    (void)eng_character_position(c, b);
    const float up = sqrtf(((b[0] - a[0]) * (b[0] - a[0])) + ((b[1] - a[1]) * (b[1] - a[1])) +
                           ((b[2] - a[2]) * (b[2] - a[2])));
    ASSERT_NEAR(up, 3.0F, 0.07F);
    ASSERT_GT(b[1], a[1]);
    int grounded = 0;
    float p[3];
    (void)eng_character_position(c, a);
    for (int i = 0; i < 60; i++) {
        run(w, c, 1, 0.0F, -3.0F);
        grounded += eng_character_ground(c) == ENG_GROUND_SUPPORTED ? 1 : 0;
    }
    (void)eng_character_position(c, p);
    const float down = sqrtf(((p[0] - a[0]) * (p[0] - a[0])) + ((p[1] - a[1]) * (p[1] - a[1])) +
                             ((p[2] - a[2]) * (p[2] - a[2])));
    ASSERT_NEAR(down, 3.0F, 0.07F);
    ASSERT_LT(p[1], a[1]);
    ASSERT_GE(grounded,
              58); /* colle à la pente en descente : jamais en l'air plus d'un pas ou deux */
    eng_character_destroy(c);
    eng_physics_shape_destroy(ramp);
    eng_physics_destroy(w);
}

static float dist3(const float a[3], const float b[3])
{
    const float dx = a[0] - b[0];
    const float dy = a[1] - b[1];
    const float dz = a[2] - b[2];
    return sqrtf((dx * dx) + (dy * dy) + (dz * dz));
}

/* Même règle en diagonale et en travers de la pente (la rampe monte vers +z : la ligne de niveau
 * est l'axe x). */
UTEST(character, keeps_its_speed_along_the_ground_diagonally_and_across_a_slope)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    add_floor(w);
    EngPhysicsShape *ramp = make_ramp(w, 30.0F);
    (void)eng_physics_add_static_shape(w, ramp, NULL);
    const float z0 = 2.0F;
    EngCharacter *c = make_char(w, -1.0F, z0 * tanf(30.0F * 3.14159265F / 180.0F), z0);
    run(w, c, 30, 0.0F, 0.0F);
    float a[3];
    float b[3];
    (void)eng_character_position(c, a);
    run(w, c, 60, 2.0F, 2.0F); /* en diagonale : 2,83 m/s horizontaux */
    (void)eng_character_position(c, b);
    ASSERT_NEAR(dist3(a, b), 2.0F * sqrtf(2.0F), 0.07F);
    ASSERT_GT(b[0] - a[0], b[2] - a[2]); /* la montée ronge la composante z, pas x */
    ASSERT_GT(b[1], a[1] + 0.5F);

    ASSERT_TRUE(eng_character_teleport(c, a));
    run(w, c, 30, 0.0F, 0.0F);
    (void)eng_character_position(c, a);
    run(w, c, 60, 3.0F, 0.0F); /* en travers : ni montée ni descente */
    (void)eng_character_position(c, b);
    ASSERT_NEAR(dist3(a, b), 3.0F, 0.07F);
    ASSERT_NEAR(b[1], a[1], 0.05F);
    eng_character_destroy(c);
    eng_physics_shape_destroy(ramp);
    eng_physics_destroy(w);
}

/* Rampe tournée de 45 degrés autour de Y : la normale du sol a une composante x non nulle. */
UTEST(character, keeps_its_speed_on_a_slope_whose_normal_points_sideways)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    add_floor(w);
    EngPhysicsShape *ramp = make_ramp(w, 30.0F);
    EngPhysicsPose pose = eng_physics_pose_identity();
    pose.rotation[1] = sinf(22.5F * 3.14159265F / 180.0F);
    pose.rotation[3] = cosf(22.5F * 3.14159265F / 180.0F);
    (void)eng_physics_add_static_shape(w, ramp, &pose);
    const float s45 = 0.70710678F;
    const float local_z = 2.0F; /* sur la pente, à 2 m du bas, dans l'axe de la rampe */
    EngCharacter *c =
        make_char(w, s45 * local_z, local_z * tanf(30.0F * 3.14159265F / 180.0F), s45 * local_z);
    run(w, c, 30, 0.0F, 0.0F);
    ASSERT_EQ(eng_character_ground(c), ENG_GROUND_SUPPORTED);
    float n[3];
    ASSERT_TRUE(eng_character_ground_normal(c, n));
    ASSERT_GT(fabsf(n[0]), 0.3F); /* la normale penche bien vers le côté */
    float a[3];
    float b[3];
    (void)eng_character_position(c, a);
    run(w, c, 60, 3.0F * s45, 3.0F * s45); /* vers le haut de la pente : 3 m/s */
    (void)eng_character_position(c, b);
    ASSERT_NEAR(dist3(a, b), 3.0F, 0.07F);
    ASSERT_GT(b[1], a[1] + 1.0F);
    eng_character_destroy(c);
    eng_physics_shape_destroy(ramp);
    eng_physics_destroy(w);
}

UTEST(character, does_not_climb_a_50_degree_slope)
{
    NEED_PHYSICS();
    float z;
    const float y = slope_test(50.0F, &z);
    ASSERT_LT(z, 0.5F);
    ASSERT_LT(y, 0.6F);
}

static void step_test(float height, float *out_z, float *out_y)
{
    EngPhysics *w = eng_physics_create(NULL);
    add_floor(w);
    const float half[3] = {5.0F, height * 0.5F, 3.0F};
    const float pos[3] = {0.0F, height * 0.5F, 3.0F};
    (void)eng_physics_add_box(w, half, pos, ENG_LAYER_STATIC);
    EngCharacter *c = make_char(w, 0.0F, 0.0F, -1.5F);
    run(w, c, 30, 0.0F, 0.0F);
    run(w, c, 150, 0.0F, 1.5F);
    float f[3];
    (void)eng_character_position(c, f);
    *out_z = f[2];
    *out_y = f[1];
    eng_character_destroy(c);
    eng_physics_destroy(w);
}

UTEST(character, steps_over_a_30_cm_ledge)
{
    NEED_PHYSICS();
    float z;
    float y;
    step_test(0.30F, &z, &y);
    ASSERT_GT(z, 1.0F);
    ASSERT_NEAR(y, 0.30F, 0.03F);
}

/* Marche franchie à vitesse de jeu (mode jouable). Régression : à 3,5 m/s et plus, l'arête de la
 * marche donnait à la capsule une vitesse verticale de +2 à +4 m/s que Jolt laissait en place : le
 * personnage décollait et décrivait un arc de 30 cm au-dessus de la marche. */
static void step_at_speed(float height, float speed, float *max_y, float *end_y, float *end_z)
{
    EngPhysics *w = eng_physics_create(NULL);
    add_floor(w);
    const float half[3] = {5.0F, height * 0.5F, 20.0F}; /* marche longue : z de 0 à 40 */
    const float pos[3] = {0.0F, height * 0.5F, 20.0F};
    (void)eng_physics_add_box(w, half, pos, ENG_LAYER_STATIC);
    EngCharacter *c = make_char(w, 0.0F, 0.0F, -1.5F);
    run(w, c, 30, 0.0F, 0.0F);
    *max_y = 0.0F;
    const float move[3] = {0.0F, 0.0F, speed};
    for (int i = 0; i < 90; i++) { /* 1,5 s : arrive sur la marche et continue dessus */
        (void)eng_physics_step(w, DT);
        (void)eng_character_update(c, DT, move);
        float f[3];
        (void)eng_character_position(c, f);
        if (f[1] > *max_y) {
            *max_y = f[1];
        }
    }
    float f[3];
    (void)eng_character_position(c, f);
    *end_y = f[1];
    *end_z = f[2];
    eng_character_destroy(c);
    eng_physics_destroy(w);
}

UTEST(character, steps_do_not_launch_the_character_at_game_speeds)
{
    NEED_PHYSICS();
    /* jusqu'au sprint (7,2 m/s). Mesuré, hors test : à 12 m/s une marche de 0,30 m fait encore
     * rebondir de 0,19 m. */
    static const float speeds[] = {1.5F, 3.5F, 4.5F, 7.2F};
    static const float heights[] = {0.15F, 0.30F};
    for (size_t h = 0; h < 2; h++) {
        for (size_t i = 0; i < 4; i++) {
            float max_y;
            float end_y;
            float end_z;
            step_at_speed(heights[h], speeds[i], &max_y, &end_y, &end_z);
            ASSERT_GT(end_z, 0.3F); /* il est monté sur la marche (qui commence à z = 0) */
            ASSERT_NEAR(end_y, heights[h], 0.03F);
            ASSERT_LT(max_y, heights[h] + 0.08F); /* jamais d'arc au-dessus de la marche */
        }
    }
}

/* La limite réelle est un peu au-dessus de step_height : le bas de la capsule est arrondi, un appui
 * dont la normale est à moins de 45 de la verticale compte comme sol, soit jusqu'à
 * step_height + rayon x (1 - cos 45) = 0,39 m. 45 cm est donc franchement infranchissable. */
UTEST(character, is_stopped_by_a_45_cm_ledge)
{
    NEED_PHYSICS();
    float z;
    float y;
    step_test(0.45F, &z, &y);
    ASSERT_LT(z, 0.0F); /* le rayon de 0,3 m le garde en deçà de la marche à z = 0 */
    ASSERT_NEAR(y, 0.0F, 0.03F);
}

/* À 30 m/s et 60 Hz, le personnage avance de 0,5 m par pas : il traverserait un mur de 5 cm s'il
 * ne balayait pas son volume. */
UTEST(character, does_not_tunnel_through_a_thin_wall)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    add_floor(w);
    const float half[3] = {5.0F, 3.0F, 0.025F};
    const float pos[3] = {0.0F, 3.0F, 8.0F};
    (void)eng_physics_add_box(w, half, pos, ENG_LAYER_STATIC);
    EngCharacter *c = make_char(w, 0.0F, 0.0F, 0.0F);
    run(w, c, 20, 0.0F, 0.0F);
    run(w, c, 60, 0.0F, 30.0F);
    float f[3];
    (void)eng_character_position(c, f);
    ASSERT_LT(f[2], 8.0F - 0.025F - 0.3F + 0.02F);
    ASSERT_GT(f[2], 6.0F);
    eng_character_destroy(c);
    eng_physics_destroy(w);
}

UTEST(character, jump_reaches_the_expected_apex_and_is_refused_in_the_air)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    add_floor(w);
    EngCharacter *c = make_char(w, 0.0F, 0.0F, 0.0F);
    run(w, c, 30, 0.0F, 0.0F);
    ASSERT_FALSE(eng_character_jump(c, 0.0F));
    ASSERT_FALSE(eng_character_jump(c, NAN));
    ASSERT_FALSE(eng_character_jump(NULL, 5.0F));
    ASSERT_TRUE(eng_character_jump(c, 5.0F));
    float apex = 0.0F;
    bool refused_in_air = false;
    const float move[3] = {0.0F, 0.0F, 0.0F};
    for (int i = 0; i < 90; i++) {
        (void)eng_physics_step(w, DT);
        (void)eng_character_update(c, DT, move);
        float f[3];
        (void)eng_character_position(c, f);
        apex = fmaxf(apex, f[1]);
        if (i == 15) {
            refused_in_air = !eng_character_jump(c, 5.0F);
        }
    }
    ASSERT_TRUE(refused_in_air);
    ASSERT_NEAR(apex, (5.0F * 5.0F) / (2.0F * 9.81F), 0.1F);
    ASSERT_EQ(eng_character_ground(c), ENG_GROUND_SUPPORTED); /* revenu au sol */
    eng_character_destroy(c);
    eng_physics_destroy(w);
}

UTEST(character, inner_body_is_seen_by_sensors_and_counted)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    add_floor(w);
    const uint32_t before = eng_physics_body_count(w);
    EngCharacter *c = make_char(w, 0.0F, 0.0F, 0.0F);
    ASSERT_EQ(eng_physics_body_count(w), before + 1U);
    const EngBody inner = eng_character_body(c);
    ASSERT_NE(inner, ENG_BODY_INVALID);
    ASSERT_EQ(eng_physics_body_layer(w, inner), ENG_LAYER_CHARACTER);
    const float half[3] = {1.0F, 1.0F, 1.0F};
    const float pos[3] = {0.0F, 1.0F, 0.0F};
    const EngBody trig = eng_physics_add_box(w, half, pos, ENG_LAYER_TRIGGER);
    run(w, c, 5, 0.0F, 0.0F);
    EngBody out[4];
    const uint32_t n = eng_physics_overlaps(w, trig, out, 4U);
    bool found = false;
    for (uint32_t i = 0; i < n && i < 4U; i++) {
        found = found || out[i] == inner;
    }
    ASSERT_TRUE(found);
    eng_character_destroy(c);
    ASSERT_EQ(eng_physics_body_count(w),
              before + 1U); /* le sol et le déclencheur : le corps du personnage est retiré */
    ASSERT_EQ(eng_physics_body_layer(w, inner), ENG_LAYER_COUNT);
    eng_physics_destroy(w);
}

UTEST(character, rejects_invalid_arguments)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    add_floor(w);
    EngCharacterDesc d;
    memset(&d, 0, sizeof(d));
    ASSERT_TRUE(eng_character_create(NULL, &d) == NULL);
    ASSERT_TRUE(eng_character_create(w, NULL) == NULL);
    d.radius = -0.3F;
    ASSERT_TRUE(eng_character_create(w, &d) == NULL);
    d.radius = 0.5F;
    d.height = 1.0F; /* <= 2 x rayon */
    ASSERT_TRUE(eng_character_create(w, &d) == NULL);
    memset(&d, 0, sizeof(d));
    d.max_slope_deg = 90.0F;
    ASSERT_TRUE(eng_character_create(w, &d) == NULL);
    d.max_slope_deg = -5.0F;
    ASSERT_TRUE(eng_character_create(w, &d) == NULL);
    memset(&d, 0, sizeof(d));
    d.step_height = -0.1F;
    ASSERT_TRUE(eng_character_create(w, &d) == NULL);
    memset(&d, 0, sizeof(d));
    d.mass = NAN;
    ASSERT_TRUE(eng_character_create(w, &d) == NULL);
    memset(&d, 0, sizeof(d));
    d.position[1] = INFINITY;
    ASSERT_TRUE(eng_character_create(w, &d) == NULL);
    ASSERT_EQ(eng_physics_body_count(w), 1U); /* seul le sol */

    EngCharacter *c = make_char(w, 0.0F, 0.0F, 0.0F);
    const float ok[3] = {1.0F, 0.0F, 0.0F};
    const float nan3[3] = {NAN, 0.0F, 0.0F};
    float tmp3[3];
    ASSERT_FALSE(eng_character_update(NULL, DT, ok));
    ASSERT_FALSE(eng_character_update(c, DT, NULL));
    ASSERT_FALSE(eng_character_update(c, DT, nan3));
    ASSERT_FALSE(eng_character_update(c, 0.0F, ok));
    ASSERT_FALSE(eng_character_update(c, -DT, ok));
    ASSERT_FALSE(eng_character_update(c, 0.2F, ok));
    ASSERT_FALSE(eng_character_update(c, NAN, ok));
    ASSERT_TRUE(eng_character_update(c, 0.1F, ok));
    ASSERT_FALSE(eng_character_teleport(c, nan3));
    ASSERT_FALSE(eng_character_position(c, NULL));
    ASSERT_FALSE(eng_character_velocity(NULL, NULL));
    ASSERT_FALSE(eng_character_interpolated(c, -0.1, tmp3));
    ASSERT_FALSE(eng_character_interpolated(c, 1.1, tmp3));
    ASSERT_EQ(eng_character_body(NULL), ENG_BODY_INVALID);
    ASSERT_EQ(eng_character_ground(NULL), ENG_GROUND_AIR);
    eng_character_destroy(NULL);
    eng_character_destroy(c);
    eng_physics_destroy(w);
}

UTEST(character, without_jolt_everything_fails_cleanly)
{
    if (eng_physics_available()) {
        return;
    }
    EngCharacterDesc d;
    memset(&d, 0, sizeof(d));
    ASSERT_TRUE(eng_character_create(NULL, &d) == NULL);
    ASSERT_FALSE(eng_character_update(NULL, DT, (const float[3]){0.0F, 0.0F, 0.0F}));
    ASSERT_EQ(eng_character_body(NULL), ENG_BODY_INVALID);
    eng_character_destroy(NULL);
}

UTEST(character, interpolation_runs_between_the_last_two_steps)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    add_floor(w);
    EngCharacter *c = make_char(w, 0.0F, 0.0F, 0.0F);
    run(w, c, 30, 0.0F, 0.0F);
    float a[3];
    float b[3];
    float m[3];
    float i0[3];
    float i1[3];
    (void)eng_character_position(c, a);
    run(w, c, 1, 0.0F, 6.0F);
    (void)eng_character_position(c, b);
    ASSERT_GT(b[2] - a[2], 0.05F);
    ASSERT_TRUE(eng_character_interpolated(c, 0.0, i0));
    ASSERT_TRUE(eng_character_interpolated(c, 1.0, i1));
    ASSERT_TRUE(eng_character_interpolated(c, 0.5, m));
    ASSERT_NEAR(i0[2], a[2], 1e-5F);
    ASSERT_EQ(i1[2], b[2]);
    ASSERT_NEAR(m[2], 0.5F * (a[2] + b[2]), 1e-4F);
    /* après un téléport : pas d'interpolation depuis l'ancien endroit */
    const float t[3] = {4.0F, 1.0F, 4.0F};
    ASSERT_TRUE(eng_character_teleport(c, t));
    ASSERT_TRUE(eng_character_interpolated(c, 0.0, i0));
    ASSERT_NEAR(i0[0], 4.0F, 1e-5F);
    ASSERT_NEAR(i0[2], 4.0F, 1e-5F);
    float v[3];
    ASSERT_TRUE(eng_character_velocity(c, v));
    ASSERT_EQ(v[0], 0.0F);
    eng_character_destroy(c);
    eng_physics_destroy(w);
}

static uint32_t bits(float f)
{
    uint32_t u;
    memcpy(&u, &f, sizeof(u));
    return u;
}

/* Parcours : sol, marche, rampe, saut. Renvoie les pieds après `steps` pas, découpés en `chunk`. */
static void scenario(int chunk, float out[3])
{
    EngPhysics *w = eng_physics_create(NULL);
    add_floor(w);
    EngPhysicsShape *ramp = make_ramp(w, 30.0F);
    (void)eng_physics_add_static_shape(w, ramp, NULL);
    EngCharacter *c = make_char(w, 0.3F, 1.0F, -4.0F);
    const float move[3] = {0.4F, 0.0F, 2.5F};
    int done = 0;
    while (done < 240) {
        for (int i = 0; i < chunk && done < 240; i++, done++) {
            if (done == 100) {
                (void)eng_character_jump(c, 4.0F);
            }
            (void)eng_physics_step(w, DT);
            (void)eng_character_update(c, DT, move);
        }
    }
    (void)eng_character_position(c, out);
    eng_character_destroy(c);
    eng_physics_shape_destroy(ramp);
    eng_physics_destroy(w);
}

UTEST(character, simulation_is_bit_identical_across_runs_and_chunking)
{
    NEED_PHYSICS();
    float a[3];
    float b[3];
    float c[3];
    scenario(240, a);
    scenario(240, b);
    scenario(7, c); /* même suite de pas, regroupés autrement */
    for (int k = 0; k < 3; k++) {
        ASSERT_EQ(bits(a[k]), bits(b[k]));
        ASSERT_EQ(bits(a[k]), bits(c[k]));
    }
    ASSERT_GT(a[2], 0.0F); /* il a bien avancé : le test n'est pas trivial */
}

UTEST(character, debug_draw_shows_a_cyan_capsule)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    EngCharacter *c = make_char(w, 0.0F, 0.0F, 0.0F);
    EngArena arena;
    EngDebugLines lines;
    ASSERT_TRUE(eng_arena_init(&arena, "test-character", 1U << 20));
    ASSERT_TRUE(eng_debug_lines_init(&lines, &arena, 4096U));
    EngPhysicsDebugDraw o;
    memset(&o, 0, sizeof(o));
    o.layer_mask = 1U << ENG_LAYER_CHARACTER;
    ASSERT_EQ(eng_physics_debug_draw(w, &lines, &o), 1U);
    ASSERT_GT(eng_debug_lines_count(&lines), 0U);
    float ymin = 1e9F;
    float ymax = -1e9F;
    for (uint32_t i = 0; i < lines.depth_count; i++) {
        ymin = fminf(ymin, lines.depth[i].position[1]);
        ymax = fmaxf(ymax, lines.depth[i].position[1]);
    }
    /* Jolt décale le corps intérieur de `characterPadding` (2 cm) vers le haut */
    ASSERT_NEAR(ymin, 0.02F, 0.005F);
    ASSERT_NEAR(ymax, 1.82F, 0.005F);
    eng_character_destroy(c);
    eng_physics_destroy(w);
    eng_arena_shutdown(&arena);
}

UTEST(character, gravity_scale_shortens_the_rise_and_the_fall)
{
    NEED_PHYSICS();
    float apex[2];
    float fall_steps[2];
    const float scales[2][2] = {{1.0F, 1.0F}, {2.0F, 3.0F}}; /* montée, chute */
    for (int k = 0; k < 2; k++) {
        EngPhysics *w = eng_physics_create(NULL);
        add_floor(w);
        EngCharacter *c = make_char(w, 0.0F, 0.0F, 0.0F);
        ASSERT_TRUE(eng_character_set_gravity_scale(c, scales[k][0], scales[k][1]));
        run(w, c, 30, 0.0F, 0.0F);
        ASSERT_TRUE(eng_character_jump(c, 6.0F));
        apex[k] = 0.0F;
        fall_steps[k] = 0.0F;
        bool descending = false;
        const float move[3] = {0.0F, 0.0F, 0.0F};
        for (int i = 0; i < 240 && !(descending && eng_character_ground(c) == ENG_GROUND_SUPPORTED);
             i++) {
            (void)eng_physics_step(w, DT);
            (void)eng_character_update(c, DT, move);
            float f[3];
            float v[3];
            (void)eng_character_position(c, f);
            (void)eng_character_velocity(c, v);
            if (f[1] > apex[k]) {
                apex[k] = f[1];
            } else if (v[1] < 0.0F) {
                descending = true;
            }
            if (descending) {
                fall_steps[k] += 1.0F;
            }
        }
        eng_character_destroy(c);
        eng_physics_destroy(w);
    }
    ASSERT_NEAR(apex[0], 36.0F / (2.0F * 9.81F), 0.1F);
    ASSERT_NEAR(apex[1], 36.0F / (2.0F * 9.81F * 2.0F), 0.1F); /* gravité x2 en montée */
    ASSERT_LT(fall_steps[1], fall_steps[0] * 0.6F);            /* et la chute est plus rapide */
}

UTEST(character, launch_works_in_the_air_and_cut_jump_shortens_a_rise)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    add_floor(w);
    EngCharacter *c = make_char(w, 0.0F, 3.0F, 0.0F); /* en l'air */
    ASSERT_FALSE(eng_character_jump(c, 5.0F));        /* jump refuse en l'air */
    ASSERT_TRUE(eng_character_launch(c, 5.0F));       /* launch non */
    float v[3];
    ASSERT_TRUE(eng_character_velocity(c, v));
    ASSERT_NEAR(v[1], 5.0F, 1e-4F);
    ASSERT_TRUE(eng_character_cut_jump(c, 0.5F));
    ASSERT_TRUE(eng_character_velocity(c, v));
    ASSERT_NEAR(v[1], 2.5F, 1e-4F);
    run(w, c, 120, 0.0F, 0.0F);                   /* retombe */
    ASSERT_TRUE(eng_character_cut_jump(c, 0.5F)); /* ne monte pas : sans effet */
    ASSERT_EQ(eng_character_ground(c), ENG_GROUND_SUPPORTED);
    eng_character_destroy(c);
    eng_physics_destroy(w);
}

UTEST(character, jump_extras_reject_invalid_arguments)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    EngCharacter *c = make_char(w, 0.0F, 3.0F, 0.0F);
    ASSERT_FALSE(eng_character_launch(NULL, 5.0F));
    ASSERT_FALSE(eng_character_launch(c, 0.0F));
    ASSERT_FALSE(eng_character_launch(c, -1.0F));
    ASSERT_FALSE(eng_character_launch(c, NAN));
    ASSERT_FALSE(eng_character_cut_jump(NULL, 0.5F));
    ASSERT_FALSE(eng_character_cut_jump(c, -0.1F));
    ASSERT_FALSE(eng_character_cut_jump(c, 1.0F));
    ASSERT_FALSE(eng_character_cut_jump(c, NAN));
    ASSERT_TRUE(eng_character_cut_jump(c, 0.0F));
    ASSERT_FALSE(eng_character_set_gravity_scale(NULL, 1.0F, 1.0F));
    ASSERT_FALSE(eng_character_set_gravity_scale(c, 0.0F, 1.0F));
    ASSERT_FALSE(eng_character_set_gravity_scale(c, 1.0F, 0.0F));
    ASSERT_FALSE(eng_character_set_gravity_scale(c, 5.1F, 1.0F));
    ASSERT_FALSE(eng_character_set_gravity_scale(c, 1.0F, 5.1F));
    ASSERT_FALSE(eng_character_set_gravity_scale(c, NAN, 1.0F));
    ASSERT_FALSE(eng_character_set_gravity_scale(c, 1.0F, NAN));
    ASSERT_TRUE(eng_character_set_gravity_scale(c, 5.0F, 5.0F));
    eng_character_destroy(c);
    eng_physics_destroy(w);
}

/* Critère d'acceptation de bout en bout : un personnage marche hors d'un rebord ; la touche de saut
 * est pressée `delay` pas après qu'il a quitté le sol. */
static bool ledge_jump(int delay)
{
    EngPhysics *w = eng_physics_create(NULL);
    const float half[3] = {5.0F, 0.5F, 5.0F}; /* sol x < 5 ; au-delà : le vide */
    const float pos[3] = {0.0F, -0.5F, 0.0F};
    (void)eng_physics_add_box(w, half, pos, ENG_LAYER_STATIC);
    EngCharacter *c = make_char(w, 3.0F, 0.0F, 0.0F);
    run(w, c, 30, 0.0F, 0.0F);
    EngJump jump;
    (void)eng_jump_init(&jump);
    const float move[3] = {3.0F, 0.0F, 0.0F};
    int since_left = -1;
    bool jumped = false;
    for (int i = 0; i < 200 && !jumped; i++) {
        (void)eng_physics_step(w, DT);
        const bool grounded = eng_character_ground(c) == ENG_GROUND_SUPPORTED;
        if (since_left >= 0) {
            since_left++;
        } else if (!grounded) {
            since_left = 1; /* premier pas observé en l'air */
        }
        EngJumpEvents ev;
        (void)eng_jump_update(&jump, grounded, since_left == delay, DT, &ev);
        if (ev.jump) {
            jumped = eng_character_launch(c, 5.0F);
        }
        (void)eng_character_update(c, DT, move);
        if (since_left > delay + 2) {
            break;
        }
    }
    float v[3];
    (void)eng_character_velocity(c, v);
    const bool rising = jumped && v[1] > 0.0F;
    eng_character_destroy(c);
    eng_physics_destroy(w);
    return rising;
}

UTEST(character, jump_is_accepted_100_ms_after_walking_off_a_ledge)
{
    NEED_PHYSICS();
    ASSERT_TRUE(ledge_jump(1));
    ASSERT_TRUE(ledge_jump(5));  /* 83 ms */
    ASSERT_FALSE(ledge_jump(9)); /* 150 ms : trop tard */
    ASSERT_FALSE(ledge_jump(12));
}
