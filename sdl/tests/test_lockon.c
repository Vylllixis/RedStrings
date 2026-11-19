/* Verrouillage de cible (M5-15). */
#include <math.h>

#include "engine/lockon.h"
#include "utest.h"

#define DEG 0.01745329252F

/* candidat à `dist` mètres, `deg` degrés à droite de l'axe (caméra yaw 0 : avant = -Z) */
static EngLockCandidate at(float dist, float deg)
{
    EngLockCandidate c;
    c.pos[0] = dist * sinf(deg * DEG);
    c.pos[1] = -dist * cosf(deg * DEG);
    c.alive = true;
    return c;
}

UTEST(lockon, acquisition_picks_the_target_closest_to_the_view_axis)
{
    const float eye[2] = {0.0F, 0.0F};
    const EngLockCandidate c[3] = {at(5.0F, 20.0F), at(10.0F, -5.0F), at(3.0F, 25.0F)};
    EXPECT_EQ(eng_lockon_acquire(eye, 0.0F, c, 3, ENG_LOCK_NONE), 1);
}

UTEST(lockon, cone_and_range_boundaries)
{
    const float eye[2] = {0.0F, 0.0F};
    EngLockCandidate c[1] = {at(10.0F, 29.5F)};
    EXPECT_EQ(eng_lockon_acquire(eye, 0.0F, c, 1, ENG_LOCK_NONE), 0);
    c[0] = at(10.0F, 30.5F);
    EXPECT_EQ(eng_lockon_acquire(eye, 0.0F, c, 1, ENG_LOCK_NONE), ENG_LOCK_NONE);
    c[0] = at(10.0F, -30.5F);
    EXPECT_EQ(eng_lockon_acquire(eye, 0.0F, c, 1, ENG_LOCK_NONE), ENG_LOCK_NONE);
    c[0] = at(19.9F, 0.0F);
    EXPECT_EQ(eng_lockon_acquire(eye, 0.0F, c, 1, ENG_LOCK_NONE), 0);
    c[0] = at(20.1F, 0.0F);
    EXPECT_EQ(eng_lockon_acquire(eye, 0.0F, c, 1, ENG_LOCK_NONE), ENG_LOCK_NONE);
    c[0] = at(5.0F, 180.0F); /* derrière */
    EXPECT_EQ(eng_lockon_acquire(eye, 0.0F, c, 1, ENG_LOCK_NONE), ENG_LOCK_NONE);
}

UTEST(lockon, dead_self_and_invalid_candidates_are_never_chosen)
{
    const float eye[2] = {0.0F, 0.0F};
    EngLockCandidate c[3] = {at(5.0F, 0.0F), at(6.0F, 1.0F), at(7.0F, 2.0F)};
    c[0].alive = false;
    EXPECT_EQ(eng_lockon_acquire(eye, 0.0F, c, 3, 1), 2); /* 0 mort, 1 = soi-même */
    c[2].pos[0] = NAN;
    EXPECT_EQ(eng_lockon_acquire(eye, 0.0F, c, 3, 1), ENG_LOCK_NONE);
    EXPECT_EQ(eng_lockon_acquire(NULL, 0.0F, c, 3, 1), ENG_LOCK_NONE);
    EXPECT_EQ(eng_lockon_acquire(eye, NAN, c, 3, 1), ENG_LOCK_NONE);
    EXPECT_EQ(eng_lockon_acquire(eye, 0.0F, NULL, 3, 1), ENG_LOCK_NONE);
    EXPECT_EQ(eng_lockon_acquire(eye, 0.0F, NULL, 0, 1), ENG_LOCK_NONE);
}

UTEST(lockon, ties_go_to_the_nearest_then_the_lowest_index)
{
    const float eye[2] = {0.0F, 0.0F};
    EngLockCandidate c[3] = {at(10.0F, 0.0F), at(4.0F, 0.0F), at(4.0F, 0.0F)};
    EXPECT_EQ(eng_lockon_acquire(eye, 0.0F, c, 3, ENG_LOCK_NONE), 1);
}

UTEST(lockon, the_camera_yaw_rotates_the_cone)
{
    const float eye[2] = {0.0F, 0.0F};
    const EngLockCandidate c[1] = {{{10.0F, 0.0F}, true}}; /* en +X */
    EXPECT_EQ(eng_lockon_acquire(eye, 0.0F, c, 1, ENG_LOCK_NONE), ENG_LOCK_NONE);
    EXPECT_EQ(eng_lockon_acquire(eye, -1.57079633F, c, 1, ENG_LOCK_NONE), 0); /* avant = +X */
}

UTEST(lockon, switching_moves_to_the_neighbour_on_the_requested_side)
{
    const float eye[2] = {0.0F, 0.0F};
    const EngLockCandidate c[4] = {at(8.0F, -20.0F), at(8.0F, 0.0F), at(8.0F, 15.0F),
                                   at(8.0F, 40.0F)};
    EXPECT_EQ(eng_lockon_switch(eye, 0.0F, c, 4, ENG_LOCK_NONE, 1, +1), 2);
    EXPECT_EQ(eng_lockon_switch(eye, 0.0F, c, 4, ENG_LOCK_NONE, 2, +1), 3);
    EXPECT_EQ(eng_lockon_switch(eye, 0.0F, c, 4, ENG_LOCK_NONE, 3, +1), 3); /* pas de voisin */
    EXPECT_EQ(eng_lockon_switch(eye, 0.0F, c, 4, ENG_LOCK_NONE, 1, -1), 0);
    EXPECT_EQ(eng_lockon_switch(eye, 0.0F, c, 4, ENG_LOCK_NONE, 0, -1), 0);
}

UTEST(lockon, switching_ignores_dead_far_and_rear_targets_and_bad_arguments)
{
    const float eye[2] = {0.0F, 0.0F};
    EngLockCandidate c[4] = {at(8.0F, 0.0F), at(8.0F, 10.0F), at(8.0F, 20.0F), at(8.0F, 100.0F)};
    c[1].alive = false;
    EXPECT_EQ(eng_lockon_switch(eye, 0.0F, c, 4, ENG_LOCK_NONE, 0, +1), 2); /* saute le mort */
    c[2] = at(21.0F, 20.0F);
    EXPECT_EQ(eng_lockon_switch(eye, 0.0F, c, 4, ENG_LOCK_NONE, 0, +1),
              0); /* trop loin ; 3 derrière */
    EXPECT_EQ(eng_lockon_switch(eye, 0.0F, c, 4, ENG_LOCK_NONE, 0, 0), 0);
    EXPECT_EQ(eng_lockon_switch(eye, 0.0F, c, 4, ENG_LOCK_NONE, 9, +1), 9);
    EXPECT_EQ(eng_lockon_switch(eye, 0.0F, c, 4, ENG_LOCK_NONE, -1, +1), -1);
    EXPECT_EQ(eng_lockon_switch(eye, 0.0F, NULL, 4, ENG_LOCK_NONE, 0, +1), 0);
    EXPECT_EQ(eng_lockon_switch(eye, 0.0F, c, 4, 0, 0, +1), 0); /* la cible courante est soi */
}

UTEST(lockon, the_lock_is_released_beyond_25_metres_or_when_the_target_dies)
{
    const float p[2] = {0.0F, 0.0F};
    EngLockCandidate c[2] = {at(24.9F, 0.0F), at(25.1F, 0.0F)};
    EXPECT_TRUE(eng_lockon_valid(p, c, 2, 0));
    EXPECT_FALSE(eng_lockon_valid(p, c, 2, 1));
    c[0].alive = false;
    EXPECT_FALSE(eng_lockon_valid(p, c, 2, 0));
    EXPECT_FALSE(eng_lockon_valid(p, c, 2, 2));
    EXPECT_FALSE(eng_lockon_valid(p, c, 2, ENG_LOCK_NONE));
    EXPECT_FALSE(eng_lockon_valid(NULL, c, 2, 0));
    c[1].pos[0] = INFINITY;
    c[1].alive = true;
    EXPECT_FALSE(eng_lockon_valid(p, c, 2, 1));
    /* hystérésis : valide à 22 m alors qu'on ne l'acquerrait plus */
    const EngLockCandidate far_one[1] = {at(22.0F, 0.0F)};
    EXPECT_EQ(eng_lockon_acquire(p, 0.0F, far_one, 1, ENG_LOCK_NONE), ENG_LOCK_NONE);
    EXPECT_TRUE(eng_lockon_valid(p, far_one, 1, 0));
}

UTEST(lockon, framing_looks_at_the_target_and_backs_off_with_the_separation)
{
    const float p[2] = {0.0F, 0.0F};
    const float t[2] = {0.0F, -6.0F}; /* devant, vers -Z */
    float yaw = 9.0F;
    float dist = 0.0F;
    ASSERT_TRUE(eng_lockon_frame(p, t, 4.0F, 12.0F, &yaw, &dist));
    EXPECT_NEAR(yaw, 0.0F, 1e-5F);
    EXPECT_NEAR(dist, 7.0F, 1e-5F);    /* 4 + 6/2 */
    const float t2[2] = {10.0F, 0.0F}; /* vers +X : yaw = -90° */
    ASSERT_TRUE(eng_lockon_frame(p, t2, 4.0F, 12.0F, &yaw, &dist));
    EXPECT_NEAR(yaw, -1.57079633F, 1e-5F);
    EXPECT_NEAR(dist, 9.0F, 1e-5F);
    const float far_t[2] = {0.0F, -100.0F};
    ASSERT_TRUE(eng_lockon_frame(p, far_t, 4.0F, 12.0F, &yaw, &dist));
    EXPECT_NEAR(dist, 12.0F, 1e-6F); /* plafonnée */
    yaw = 1.25F;
    ASSERT_TRUE(eng_lockon_frame(p, p, 4.0F, 12.0F, &yaw, &dist)); /* même point : cap conservé */
    EXPECT_NEAR(yaw, 1.25F, 1e-9F);
    EXPECT_NEAR(dist, 4.0F, 1e-6F);
    EXPECT_FALSE(eng_lockon_frame(p, t, 0.0F, 12.0F, &yaw, &dist));
    EXPECT_FALSE(eng_lockon_frame(p, t, 5.0F, 4.0F, &yaw, &dist));
    EXPECT_FALSE(eng_lockon_frame(p, t, NAN, 4.0F, &yaw, &dist));
    EXPECT_FALSE(eng_lockon_frame(NULL, t, 4.0F, 12.0F, &yaw, &dist));
    EXPECT_FALSE(eng_lockon_frame(p, t, 4.0F, 12.0F, NULL, &dist));
}

UTEST(lockon, strafe_moves_relative_to_the_direction_of_the_target)
{
    const float p[2] = {0.0F, 0.0F};
    const float t[2] = {0.0F, -5.0F}; /* cible devant (-Z) */
    float out[2];
    const float fwd[2] = {0.0F, 1.0F};
    ASSERT_TRUE(eng_lockon_move_dir(p, t, fwd, out));
    EXPECT_NEAR(out[0], 0.0F, 1e-6F);
    EXPECT_NEAR(out[1], -1.0F, 1e-6F); /* vers la cible */
    const float right[2] = {1.0F, 0.0F};
    ASSERT_TRUE(eng_lockon_move_dir(p, t, right, out));
    EXPECT_NEAR(out[0], 1.0F, 1e-6F); /* à droite du joueur = +X quand on regarde -Z */
    EXPECT_NEAR(out[1], 0.0F, 1e-6F);
    const float t2[2] = {5.0F, 0.0F}; /* cible vers +X : la droite devient +Z */
    ASSERT_TRUE(eng_lockon_move_dir(p, t2, right, out));
    EXPECT_NEAR(out[0], 0.0F, 1e-6F);
    EXPECT_NEAR(out[1], 1.0F, 1e-6F);
    const float diag[2] = {0.6F, -0.8F}; /* norme conservée : recule et glisse */
    ASSERT_TRUE(eng_lockon_move_dir(p, t2, diag, out));
    EXPECT_NEAR(sqrtf(out[0] * out[0] + out[1] * out[1]), 1.0F, 1e-5F);
    EXPECT_NEAR(out[0], -0.8F, 1e-5F);
    EXPECT_NEAR(out[1], 0.6F, 1e-5F);
    ASSERT_TRUE(eng_lockon_move_dir(p, p, right, out)); /* cible confondue : avant = -Z */
    EXPECT_NEAR(out[0], 1.0F, 1e-6F);
    EXPECT_FALSE(eng_lockon_move_dir(p, t, NULL, out));
    EXPECT_FALSE(eng_lockon_move_dir(p, t, right, NULL));
    const float bad[2] = {NAN, 0.0F};
    EXPECT_FALSE(eng_lockon_move_dir(p, t, bad, out));
}

/* Même scénario avec joueur décalé et caméra tournée : les soustractions de position comptent. */
static EngLockCandidate at_from(const float eye[2], float yaw, float dist, float deg)
{
    /* avant caméra = (-sin yaw, -cos yaw) ; droite = (cos yaw, -sin yaw) */
    const float a = deg * DEG;
    const float fx = -sinf(yaw);
    const float fz = -cosf(yaw);
    const float rx = cosf(yaw);
    const float rz = -sinf(yaw);
    EngLockCandidate c;
    c.pos[0] = eye[0] + dist * (cosf(a) * fx + sinf(a) * rx);
    c.pos[1] = eye[1] + dist * (cosf(a) * fz + sinf(a) * rz);
    c.alive = true;
    return c;
}

UTEST(lockon, offset_eye_and_rotated_camera_behave_like_the_origin_case)
{
    const float eye[2] = {37.0F, -52.0F};
    const float yaw = 0.7F;
    EngLockCandidate c[4] = {at_from(eye, yaw, 8.0F, -20.0F), at_from(eye, yaw, 8.0F, 0.0F),
                             at_from(eye, yaw, 8.0F, 15.0F), at_from(eye, yaw, 8.0F, 40.0F)};
    EXPECT_EQ(eng_lockon_acquire(eye, yaw, c, 4, ENG_LOCK_NONE), 1);
    EXPECT_EQ(eng_lockon_switch(eye, yaw, c, 4, ENG_LOCK_NONE, 1, +1), 2);
    EXPECT_EQ(eng_lockon_switch(eye, yaw, c, 4, ENG_LOCK_NONE, 2, +1),
              3); /* le basculement n'est pas limité au cône */
    EXPECT_EQ(eng_lockon_switch(eye, yaw, c, 4, ENG_LOCK_NONE, 1, -1), 0);
    EngLockCandidate b[1] = {at_from(eye, yaw, 10.0F, 29.5F)};
    EXPECT_EQ(eng_lockon_acquire(eye, yaw, b, 1, ENG_LOCK_NONE), 0);
    b[0] = at_from(eye, yaw, 10.0F, 30.5F);
    EXPECT_EQ(eng_lockon_acquire(eye, yaw, b, 1, ENG_LOCK_NONE), ENG_LOCK_NONE);
    b[0] = at_from(eye, yaw, 10.0F, -30.5F);
    EXPECT_EQ(eng_lockon_acquire(eye, yaw, b, 1, ENG_LOCK_NONE), ENG_LOCK_NONE);
    b[0] = at_from(eye, yaw, 19.9F, 0.0F);
    EXPECT_EQ(eng_lockon_acquire(eye, yaw, b, 1, ENG_LOCK_NONE), 0);
    b[0] = at_from(eye, yaw, 20.1F, 0.0F);
    EXPECT_EQ(eng_lockon_acquire(eye, yaw, b, 1, ENG_LOCK_NONE), ENG_LOCK_NONE);
    /* valid : portée de relâche 25 m */
    b[0] = at_from(eye, yaw, 24.9F, 0.0F);
    EXPECT_TRUE(eng_lockon_valid(eye, b, 1, 0));
    b[0] = at_from(eye, yaw, 25.1F, 0.0F);
    EXPECT_FALSE(eng_lockon_valid(eye, b, 1, 0));
}

UTEST(lockon, frame_and_move_dir_with_offset_player)
{
    const float p[2] = {10.0F, 20.0F};
    const float t[2] = {10.0F, 10.0F}; /* cible en -Z : avant monde = -Z */
    float yaw = 9.0F;
    float dist = 0.0F;
    ASSERT_TRUE(eng_lockon_frame(p, t, 4.0F, 12.0F, &yaw, &dist));
    EXPECT_NEAR(yaw, 0.0F, 1e-4F);
    const float t2[2] = {20.0F, 20.0F}; /* cible en +X */
    ASSERT_TRUE(eng_lockon_frame(p, t2, 4.0F, 12.0F, &yaw, &dist));
    EXPECT_NEAR(yaw, -1.57079633F, 1e-4F);
    const float fwd[2] = {0.0F, 1.0F};
    float out[2] = {0.0F, 0.0F};
    ASSERT_TRUE(eng_lockon_move_dir(p, t2, fwd, out));
    EXPECT_NEAR(out[0], 1.0F, 1e-4F);
    EXPECT_NEAR(out[1], 0.0F, 1e-4F);
}
