/* Sphere cast (physique) et collision de la caméra orbitale (M4-11). Géométrie à la main :
 * pivot à l'origine, yaw = pitch = 0 : la caméra est derrière, en +Z. Mur : boîte de demi-épaisseur
 * 0,5 centrée en z = 2,5, face avant en z = 2. Sphère de 0,25 : son centre s'arrête en z = 1,75. */
#include <math.h>
#include <stdint.h>

#include "engine/orbit_camera.h"
#include "engine/physics.h"
#include "utest.h"

#define DT (1.0F / 60.0F)
#define STATIC_MASK (1U << ENG_LAYER_STATIC)

#define NEED_PHYSICS()                                                                             \
    if (!eng_physics_available()) {                                                                \
        return;                                                                                    \
    }

static const float ORIGIN[3] = {0.0F, 0.0F, 0.0F};
static const float BACK[3] = {0.0F, 0.0F, 1.0F};

static EngBody add_wall(EngPhysics *w)
{
    const float he[3] = {5.0F, 5.0F, 0.5F};
    const float pos[3] = {0.0F, 0.0F, 2.5F};
    return eng_physics_add_box(w, he, pos, ENG_LAYER_STATIC);
}

static EngOrbitCamera make(float yaw, float pitch, float distance)
{
    EngOrbitCamera c;
    (void)eng_orbit_camera_init(&c, ORIGIN, yaw, pitch, distance);
    return c;
}

UTEST(spherecast, stops_one_radius_before_the_wall_unlike_a_ray)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    ASSERT_TRUE(w != NULL);
    const EngBody wall = add_wall(w);
    EngRayHit hit;
    ASSERT_TRUE(eng_physics_spherecast(w, ORIGIN, BACK, 10.0F, 0.25F, STATIC_MASK, &hit));
    EXPECT_EQ(hit.body, wall);
    EXPECT_NEAR(hit.distance, 1.75F, 0.01F);
    EXPECT_NEAR(hit.point[2], 2.0F, 0.01F);
    EXPECT_NEAR(hit.normal[2], -1.0F, 0.01F); /* vers la sphère */
    EXPECT_NEAR(hit.normal[0], 0.0F, 0.01F);
    EngRayHit ray;
    ASSERT_TRUE(eng_physics_raycast(w, ORIGIN, BACK, 10.0F, STATIC_MASK, &ray));
    EXPECT_NEAR(ray.distance, 2.0F, 0.01F); /* le rayon, lui, va jusqu'à la paroi */
    ASSERT_TRUE(eng_physics_spherecast(w, ORIGIN, BACK, 10.0F, 0.5F, STATIC_MASK, &hit));
    EXPECT_NEAR(hit.distance, 1.5F, 0.01F); /* sphère plus grosse : s'arrête plus tôt */
    eng_physics_destroy(w);
}

UTEST(spherecast, range_mask_and_direction_scale)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    ASSERT_TRUE(w != NULL);
    (void)add_wall(w);
    EngRayHit hit;
    EXPECT_FALSE(eng_physics_spherecast(w, ORIGIN, BACK, 1.7F, 0.25F, STATIC_MASK, &hit));
    EXPECT_TRUE(eng_physics_spherecast(w, ORIGIN, BACK, 1.8F, 0.25F, STATIC_MASK, &hit));
    EXPECT_FALSE(
        eng_physics_spherecast(w, ORIGIN, BACK, 10.0F, 0.25F, 1U << ENG_LAYER_DYNAMIC, &hit));
    const float far_dir[3] = {0.0F, 0.0F, 40.0F}; /* direction non normalisée */
    ASSERT_TRUE(eng_physics_spherecast(w, ORIGIN, far_dir, 10.0F, 0.25F, STATIC_MASK, &hit));
    EXPECT_NEAR(hit.distance, 1.75F, 0.01F);
    const float away[3] = {0.0F, 0.0F, -1.0F};
    EXPECT_FALSE(eng_physics_spherecast(w, ORIGIN, away, 10.0F, 0.25F, STATIC_MASK, &hit));
    const float start[3] = {0.0F, 0.0F, 1.8F}; /* démarre en contact (0,2 m de la paroi < 0,25) */
    ASSERT_TRUE(eng_physics_spherecast(w, start, BACK, 5.0F, 0.25F, STATIC_MASK, &hit));
    EXPECT_NEAR(hit.distance, 0.0F, 0.01F);
    EXPECT_NEAR(hit.normal[2], -1.0F, 0.05F); /* normale utilisable même au contact initial */
    eng_physics_destroy(w);
}

/* Direction oblique, non normalisée : (2, 3, 6) / 7 ; le centre atteint z = 1,75 après
 * 1,75 x 7 / 6 mètres de course. Chaque composante compte. */
UTEST(spherecast, oblique_direction_uses_every_component)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    ASSERT_TRUE(w != NULL);
    (void)add_wall(w);
    const float dir[3] = {2.0F, 3.0F, 6.0F};
    EngRayHit hit;
    ASSERT_TRUE(eng_physics_spherecast(w, ORIGIN, dir, 10.0F, 0.25F, STATIC_MASK, &hit));
    EXPECT_NEAR(hit.distance, 1.75F * 7.0F / 6.0F, 0.01F);
    EXPECT_NEAR(hit.point[0], 2.0F / 7.0F * hit.distance, 0.02F);
    EXPECT_NEAR(hit.point[1], 3.0F / 7.0F * hit.distance, 0.02F);
    const float dx[3] = {3.0F, 0.0F, 4.0F};
    ASSERT_TRUE(eng_physics_spherecast(w, ORIGIN, dx, 10.0F, 0.25F, STATIC_MASK, &hit));
    EXPECT_NEAR(hit.distance, 1.75F / 0.8F, 0.01F);
    const float dy[3] = {0.0F, 3.0F, 4.0F};
    ASSERT_TRUE(eng_physics_spherecast(w, ORIGIN, dy, 10.0F, 0.25F, STATIC_MASK, &hit));
    EXPECT_NEAR(hit.distance, 1.75F / 0.8F, 0.01F);
    eng_physics_destroy(w);
}

/* Surface courbe : la normale a trois composantes non nulles. Sphère fixe de rayon 1 en
 * (0, 0, 5), sphère lancée de rayon 0,5 depuis (-0,8, 0,6, 0) vers +Z : contact quand les centres
 * sont à 1,5 m, soit dz = sqrt(2,25 - 1) = 1,118 donc z = 3,882 ; normale = (-0,8, 0,6, -1,118) /
 * 1,5. */
UTEST(spherecast, normal_on_a_curved_surface_points_to_the_moving_sphere)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    ASSERT_TRUE(w != NULL);
    const float centre[3] = {0.0F, 0.0F, 5.0F};
    ASSERT_TRUE(eng_physics_add_sphere(w, 1.0F, centre, ENG_LAYER_STATIC) != ENG_BODY_INVALID);
    const float from[3] = {-0.8F, 0.6F, 0.0F};
    EngRayHit hit;
    ASSERT_TRUE(eng_physics_spherecast(w, from, BACK, 10.0F, 0.5F, STATIC_MASK, &hit));
    const float dz = sqrtf(1.25F);
    EXPECT_NEAR(hit.distance, 5.0F - dz, 0.02F);
    EXPECT_NEAR(hit.normal[0], -0.8F / 1.5F, 0.02F);
    EXPECT_NEAR(hit.normal[1], 0.6F / 1.5F, 0.02F);
    EXPECT_NEAR(hit.normal[2], -dz / 1.5F, 0.02F);
    eng_physics_destroy(w);
}

UTEST(spherecast, invalid_arguments_are_refused)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    ASSERT_TRUE(w != NULL);
    (void)add_wall(w);
    EngRayHit hit;
    const float zero[3] = {0.0F, 0.0F, 0.0F};
    const float nan3[3] = {NAN, 0.0F, 1.0F};
    EXPECT_FALSE(eng_physics_spherecast(NULL, ORIGIN, BACK, 10.0F, 0.25F, STATIC_MASK, &hit));
    EXPECT_FALSE(eng_physics_spherecast(w, ORIGIN, BACK, 10.0F, 0.25F, STATIC_MASK, NULL));
    EXPECT_FALSE(eng_physics_spherecast(w, ORIGIN, zero, 10.0F, 0.25F, STATIC_MASK, &hit));
    EXPECT_FALSE(eng_physics_spherecast(w, nan3, BACK, 10.0F, 0.25F, STATIC_MASK, &hit));
    EXPECT_FALSE(eng_physics_spherecast(w, ORIGIN, nan3, 10.0F, 0.25F, STATIC_MASK, &hit));
    EXPECT_FALSE(eng_physics_spherecast(w, ORIGIN, BACK, 0.0F, 0.25F, STATIC_MASK, &hit));
    EXPECT_FALSE(eng_physics_spherecast(w, ORIGIN, BACK, INFINITY, 0.25F, STATIC_MASK, &hit));
    EXPECT_FALSE(eng_physics_spherecast(w, ORIGIN, BACK, 10.0F, 0.0F, STATIC_MASK, &hit));
    EXPECT_FALSE(eng_physics_spherecast(w, ORIGIN, BACK, 10.0F, -1.0F, STATIC_MASK, &hit));
    EXPECT_FALSE(eng_physics_spherecast(w, ORIGIN, BACK, 10.0F, 10.5F, STATIC_MASK, &hit));
    EXPECT_FALSE(eng_physics_spherecast(w, ORIGIN, BACK, 10.0F, NAN, STATIC_MASK, &hit));
    EXPECT_TRUE(eng_physics_spherecast(w, ORIGIN, BACK, 10.0F, 10.0F, STATIC_MASK, &hit));
    eng_physics_destroy(w);
}

/* Critère M4-11 : dos à un mur, la caméra se rapproche sans clipper. */
UTEST(camera_collision, back_to_a_wall_the_camera_comes_closer_without_clipping)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    ASSERT_TRUE(w != NULL);
    (void)add_wall(w);
    EngOrbitCamera c = make(0.0F, 0.0F, 4.0F);
    ASSERT_TRUE(eng_orbit_camera_collide(&c, w, STATIC_MASK, ENG_ORBIT_COLLISION_RADIUS, DT));
    EXPECT_NEAR(c.effective, 1.75F, 0.01F); /* d'un coup, dès le premier pas */
    float eye[3];
    eng_orbit_camera_eye(&c, eye);
    EXPECT_LE(eye[2] + ENG_ORBIT_COLLISION_RADIUS, 2.0F + 0.01F); /* la sphère reste devant */
    EXPECT_EQ(c.distance, 4.0F);                                  /* la demande du joueur reste */
    eng_physics_destroy(w);
}

UTEST(camera_collision, no_obstacle_keeps_the_requested_distance)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    ASSERT_TRUE(w != NULL);
    EngOrbitCamera c = make(0.0F, 0.0F, 4.0F);
    ASSERT_TRUE(eng_orbit_camera_collide(&c, w, STATIC_MASK, ENG_ORBIT_COLLISION_RADIUS, DT));
    EXPECT_EQ(c.effective, 4.0F);
    EXPECT_EQ(c.clear, 4.0F);
    eng_physics_destroy(w);
}

UTEST(camera_collision, recovers_smoothly_when_the_obstacle_goes_away)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    ASSERT_TRUE(w != NULL);
    const EngBody wall = add_wall(w);
    EngOrbitCamera c = make(0.0F, 0.0F, 4.0F);
    ASSERT_TRUE(eng_orbit_camera_collide(&c, w, STATIC_MASK, ENG_ORBIT_COLLISION_RADIUS, DT));
    const float blocked = c.effective;
    eng_physics_remove_body(w, wall);
    ASSERT_TRUE(eng_orbit_camera_collide(&c, w, STATIC_MASK, ENG_ORBIT_COLLISION_RADIUS, DT));
    /* filtre exact : + (4 - bloqué) x (1 - exp(-dt / 0,3)) */
    const float step = (4.0F - blocked) * (1.0F - expf(-DT / ENG_ORBIT_RECOVER_TAU));
    EXPECT_NEAR(c.effective, blocked + step, 1e-4F);
    float prev = c.effective;
    for (int i = 0; i < 300; i++) { /* 5 s */
        ASSERT_TRUE(eng_orbit_camera_collide(&c, w, STATIC_MASK, ENG_ORBIT_COLLISION_RADIUS, DT));
        EXPECT_GE(c.effective, prev);
        prev = c.effective;
    }
    EXPECT_NEAR(c.effective, 4.0F, 0.01F);
    eng_physics_destroy(w);
}

UTEST(camera_collision, the_wall_reappearing_pulls_the_camera_in_immediately)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    ASSERT_TRUE(w != NULL);
    EngOrbitCamera c = make(0.0F, 0.0F, 4.0F);
    for (int i = 0; i < 10; i++) {
        ASSERT_TRUE(eng_orbit_camera_collide(&c, w, STATIC_MASK, ENG_ORBIT_COLLISION_RADIUS, DT));
    }
    EXPECT_EQ(c.effective, 4.0F);
    (void)add_wall(w);
    ASSERT_TRUE(eng_orbit_camera_collide(&c, w, STATIC_MASK, ENG_ORBIT_COLLISION_RADIUS, DT));
    EXPECT_NEAR(c.effective, 1.75F, 0.01F);
    eng_physics_destroy(w);
}

UTEST(camera_collision, the_floor_limits_a_camera_looking_up_from_below)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    ASSERT_TRUE(w != NULL);
    const float he[3] = {50.0F, 0.5F, 50.0F};
    const float pos[3] = {0.0F, -1.5F, 0.0F}; /* dessus du sol à y = -1 */
    ASSERT_TRUE(eng_physics_add_box(w, he, pos, ENG_LAYER_STATIC) != ENG_BODY_INVALID);
    EngOrbitCamera c = make(0.0F, 0.6F, 6.0F); /* pitch + : l'oeil est sous le pivot */
    ASSERT_TRUE(eng_orbit_camera_collide(&c, w, STATIC_MASK, ENG_ORBIT_COLLISION_RADIUS, DT));
    /* le centre de l'oeil ne descend pas sous y = -0,75 : d x sin(0,6) = 0,75 */
    EXPECT_NEAR(c.effective, 0.75F / sinf(0.6F), 0.02F);
    float eye[3];
    eng_orbit_camera_eye(&c, eye);
    EXPECT_GE(eye[1] - ENG_ORBIT_COLLISION_RADIUS, -1.0F - 0.01F);
    eng_physics_destroy(w);
}

/* Propriété : pivot au centre d'une pièce fermée de 4 m de côté, quelles que soient les
 * orientations et la distance, l'oeil (plus son rayon) reste dans la pièce. */
UTEST(camera_collision, never_leaves_a_closed_room_at_any_angle)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    ASSERT_TRUE(w != NULL);
    for (int axis = 0; axis < 3; axis++) {
        for (int sign = -1; sign <= 1; sign += 2) {
            float he[3] = {2.6F, 2.6F, 2.6F};
            float pos[3] = {0.0F, 0.0F, 0.0F};
            he[axis] = 0.5F;
            pos[axis] = (float)sign * 2.5F;
            ASSERT_TRUE(eng_physics_add_box(w, he, pos, ENG_LAYER_STATIC) != ENG_BODY_INVALID);
        }
    }
    int checked = 0;
    for (int yi = 0; yi < 24; yi++) {
        for (int pi = 0; pi < 9; pi++) {
            const float yaw = (float)yi * (6.2831853F / 24.0F);
            const float pitch = -1.2F + ((float)pi * 0.2F);
            EngOrbitCamera c = make(yaw, pitch, ENG_ORBIT_DISTANCE_MAX);
            ASSERT_TRUE(
                eng_orbit_camera_collide(&c, w, STATIC_MASK, ENG_ORBIT_COLLISION_RADIUS, DT));
            float eye[3];
            eng_orbit_camera_eye(&c, eye);
            for (int k = 0; k < 3; k++) {
                EXPECT_LE(fabsf(eye[k]) + ENG_ORBIT_COLLISION_RADIUS, 2.0F + 0.02F);
            }
            checked++;
        }
    }
    EXPECT_EQ(checked, 216);
    eng_physics_destroy(w);
}

UTEST(camera_collision, the_character_layer_does_not_block_the_camera)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    ASSERT_TRUE(w != NULL);
    const float pos[3] = {0.0F, 0.0F, 2.0F};
    ASSERT_TRUE(eng_physics_add_capsule(w, 0.5F, 0.3F, pos, ENG_LAYER_CHARACTER) !=
                ENG_BODY_INVALID);
    EngOrbitCamera c = make(0.0F, 0.0F, 4.0F);
    ASSERT_TRUE(eng_orbit_camera_collide(&c, w, STATIC_MASK, ENG_ORBIT_COLLISION_RADIUS, DT));
    EXPECT_EQ(c.effective, 4.0F); /* masque statique seul : un corps CHARACTER est ignoré */
    eng_physics_destroy(w);
}

UTEST(camera_collision, a_pivot_touching_a_wall_keeps_a_minimum_distance_and_snap_resets)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    ASSERT_TRUE(w != NULL);
    (void)add_wall(w);
    const float against[3] = {0.0F, 0.0F, 1.9F};
    EngOrbitCamera c;
    ASSERT_TRUE(eng_orbit_camera_init(&c, against, 0.0F, 0.0F, 4.0F));
    ASSERT_TRUE(eng_orbit_camera_collide(&c, w, STATIC_MASK, ENG_ORBIT_COLLISION_RADIUS, DT));
    EXPECT_EQ(c.effective, ENG_ORBIT_COLLISION_MIN);
    ASSERT_TRUE(eng_orbit_camera_snap(&c, ORIGIN)); /* téléportation : plus de mémoire d'obstacle */
    EXPECT_EQ(c.clear, 4.0F);
    eng_physics_destroy(w);
}

UTEST(camera_collision, invalid_arguments_leave_the_camera_untouched)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    ASSERT_TRUE(w != NULL);
    (void)add_wall(w);
    EngOrbitCamera c = make(0.0F, 0.0F, 4.0F);
    EXPECT_FALSE(eng_orbit_camera_collide(NULL, w, STATIC_MASK, 0.25F, DT));
    EXPECT_FALSE(eng_orbit_camera_collide(&c, NULL, STATIC_MASK, 0.25F, DT));
    EXPECT_FALSE(eng_orbit_camera_collide(&c, w, STATIC_MASK, 0.0F, DT));
    EXPECT_FALSE(eng_orbit_camera_collide(&c, w, STATIC_MASK, 1.01F, DT));
    EXPECT_FALSE(eng_orbit_camera_collide(&c, w, STATIC_MASK, NAN, DT));
    EXPECT_FALSE(eng_orbit_camera_collide(&c, w, STATIC_MASK, 0.25F, 0.0F));
    EXPECT_FALSE(eng_orbit_camera_collide(&c, w, STATIC_MASK, 0.25F, 0.11F));
    EXPECT_FALSE(eng_orbit_camera_collide(&c, w, STATIC_MASK, 0.25F, NAN));
    EXPECT_EQ(c.effective, 4.0F);
    EXPECT_EQ(c.clear, 4.0F);
    EXPECT_TRUE(eng_orbit_camera_collide(&c, w, STATIC_MASK, 1.0F, 0.1F)); /* bornes incluses */
    eng_physics_destroy(w);
}
