#include <math.h>
#include <stdint.h>

#include "engine/physics.h"
#include "utest.h"

#define DT (1.0F / 60.0F)

/* Les tests de simulation n'ont de sens qu'avec Jolt compilé. */
#define NEED_PHYSICS()                                                                             \
    if (!eng_physics_available()) {                                                                \
        return;                                                                                    \
    }

UTEST(physics, create_destroy_null_safe)
{
    eng_physics_destroy(NULL);
    EngPhysics *w = eng_physics_create(NULL);
    if (!eng_physics_available()) {
        ASSERT_TRUE(w == NULL);
        ASSERT_FALSE(eng_physics_step(w, DT));
        return;
    }
    ASSERT_TRUE(w != NULL);
    ASSERT_EQ(eng_physics_body_count(w), 0U);
    eng_physics_destroy(w);
}

UTEST(physics, two_worlds_coexist)
{
    NEED_PHYSICS();
    EngPhysics *a = eng_physics_create(NULL);
    EngPhysics *b = eng_physics_create(NULL);
    ASSERT_TRUE(a != NULL);
    ASSERT_TRUE(b != NULL);
    const float p[3] = {0, 5, 0};
    ASSERT_NE(eng_physics_add_sphere(a, 0.5F, p, ENG_LAYER_DYNAMIC), ENG_BODY_INVALID);
    ASSERT_EQ(eng_physics_body_count(a), 1U);
    ASSERT_EQ(eng_physics_body_count(b), 0U);
    eng_physics_destroy(a); /* détruire le premier ne casse pas le second */
    const EngBody ball = eng_physics_add_sphere(b, 0.5F, p, ENG_LAYER_DYNAMIC);
    for (int i = 0; i < 30; i++) { /* un monde avec un corps en mouvement utilise l'état global */
        ASSERT_TRUE(eng_physics_step(b, DT));
    }
    float pos[3];
    ASSERT_TRUE(eng_physics_body_position(b, ball, pos));
    ASSERT_LT(pos[1], 4.0F);
    eng_physics_destroy(b);
    EngPhysics *c = eng_physics_create(NULL); /* ré-initialisation après libération totale */
    ASSERT_TRUE(c != NULL);
    eng_physics_destroy(c);
}

UTEST(physics, invalid_arguments_rejected)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    ASSERT_TRUE(w != NULL);
    const float p[3] = {0, 0, 0};
    const float nanp[3] = {NAN, 0, 0};
    const float he_bad[3] = {1, 0, 1};
    const float he_ok[3] = {1, 1, 1};
    ASSERT_EQ(eng_physics_add_sphere(w, 0.0F, p, ENG_LAYER_DYNAMIC), ENG_BODY_INVALID);
    ASSERT_EQ(eng_physics_add_sphere(w, -1.0F, p, ENG_LAYER_DYNAMIC), ENG_BODY_INVALID);
    ASSERT_EQ(eng_physics_add_sphere(w, NAN, p, ENG_LAYER_DYNAMIC), ENG_BODY_INVALID);
    ASSERT_EQ(eng_physics_add_sphere(w, 1.0F, nanp, ENG_LAYER_DYNAMIC), ENG_BODY_INVALID);
    ASSERT_EQ(eng_physics_add_sphere(w, 1.0F, NULL, ENG_LAYER_DYNAMIC), ENG_BODY_INVALID);
    ASSERT_EQ(eng_physics_add_sphere(w, 1.0F, p, ENG_LAYER_COUNT), ENG_BODY_INVALID);
    ASSERT_EQ(eng_physics_add_box(w, he_bad, p, ENG_LAYER_STATIC), ENG_BODY_INVALID);
    for (int axis = 0; axis < 3; axis++) { /* un demi-côté nul sur chaque axe */
        float he0[3] = {1, 1, 1};
        he0[axis] = 0.0F;
        ASSERT_EQ(eng_physics_add_box(w, he0, p, ENG_LAYER_STATIC), ENG_BODY_INVALID);
    }
    ASSERT_EQ(eng_physics_add_box(w, he_ok, p, ENG_LAYER_COUNT), ENG_BODY_INVALID);
    ASSERT_EQ(eng_physics_add_box(w, NULL, p, ENG_LAYER_STATIC), ENG_BODY_INVALID);
    ASSERT_NE(eng_physics_add_box(w, he_ok, p, ENG_LAYER_STATIC), ENG_BODY_INVALID);
    ASSERT_EQ(eng_physics_body_count(w), 1U);
    ASSERT_FALSE(eng_physics_step(w, 0.0F));
    ASSERT_FALSE(eng_physics_step(w, -DT));
    ASSERT_FALSE(eng_physics_step(w, 0.5F));
    ASSERT_TRUE(eng_physics_step(w, 0.1F));     /* le pas maximal est accepté... */
    ASSERT_FALSE(eng_physics_step(w, 0.1001F)); /* ...pas au-delà */
    ASSERT_FALSE(eng_physics_step(w, NAN));
    ASSERT_FALSE(eng_physics_step(NULL, DT));
    EngPhysicsDesc bad = {.gravity = {INFINITY, 0, 0}};
    ASSERT_TRUE(eng_physics_create(&bad) == NULL);
    eng_physics_destroy(w);
}

UTEST(physics, unknown_body_is_harmless)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    float out[3] = {7, 8, 9};
    const float v[3] = {1, 2, 3};
    ASSERT_FALSE(eng_physics_body_position(w, ENG_BODY_INVALID, out));
    ASSERT_FALSE(eng_physics_body_position(w, 12345U, out));
    ASSERT_FALSE(eng_physics_body_velocity(w, 12345U, out));
    ASSERT_FALSE(eng_physics_set_body_velocity(w, 12345U, v));
    ASSERT_FALSE(eng_physics_body_is_active(w, 12345U));
    eng_physics_remove_body(w, 12345U);
    eng_physics_remove_body(w, ENG_BODY_INVALID);
    eng_physics_remove_body(NULL, 1U);
    ASSERT_EQ(out[0], 7.0F); /* sorties inchangées */
    ASSERT_EQ(out[1], 8.0F);
    ASSERT_EQ(out[2], 9.0F);
    eng_physics_destroy(w);
}

UTEST(physics, remove_body_invalidates_id)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float p[3] = {0, 1, 0};
    const EngBody b = eng_physics_add_sphere(w, 0.5F, p, ENG_LAYER_DYNAMIC);
    float out[3];
    ASSERT_TRUE(eng_physics_body_position(w, b, out));
    eng_physics_remove_body(w, b);
    ASSERT_EQ(eng_physics_body_count(w), 0U);
    ASSERT_FALSE(eng_physics_body_position(w, b, out));
    eng_physics_remove_body(w, b); /* double retrait sans effet */
    eng_physics_destroy(w);
}

/* Amortissement linéaire par défaut de Jolt : v *= (1 - 0,05 dt) à chaque pas (après la gravité).
 */
#define JOLT_LINEAR_DAMPING 0.05F

/* Référence : intégration semi-implicite exacte de Jolt (vitesse = (vitesse + g dt) x (1 - c dt),
 * puis position += vitesse dt), pour `n` pas depuis (y0, vy0). */
static void reference_fall(float y0, float vy0, float vx0, float gravity, int n, float *y,
                           float *vy, float *vx)
{
    float py = y0;
    float v = vy0;
    float u = vx0;
    for (int i = 0; i < n; i++) {
        v = (v + (gravity * DT)) * (1.0F - (JOLT_LINEAR_DAMPING * DT));
        u *= 1.0F - (JOLT_LINEAR_DAMPING * DT);
        py += v * DT;
    }
    *y = py;
    *vy = v;
    *vx = u;
}

/* « HelloWorld » : une sphère de 0,5 m lâchée de 10 m, sans sol : chute libre. Deux références :
 * la récurrence exacte du pas de Jolt (précise à ~1e-4) et la loi continue avec traînée
 * v(t) = -(g/c)(1 - e^(-c t)) (précise à quelques 1e-2 : l'intégration est du premier ordre). */
UTEST(physics, free_fall_matches_kinematics)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float p[3] = {1.0F, 10.0F, -2.0F};
    const EngBody b = eng_physics_add_sphere(w, 0.5F, p, ENG_LAYER_DYNAMIC);
    ASSERT_NE(b, ENG_BODY_INVALID);
    const int steps = 60; /* 1 s */
    for (int i = 0; i < steps; i++) {
        ASSERT_TRUE(eng_physics_step(w, DT));
    }
    float pos[3];
    float vel[3];
    ASSERT_TRUE(eng_physics_body_position(w, b, pos));
    ASSERT_TRUE(eng_physics_body_velocity(w, b, vel));
    float ry;
    float rvy;
    float rvx;
    reference_fall(10.0F, 0.0F, 0.0F, -9.81F, steps, &ry, &rvy, &rvx);
    ASSERT_NEAR(pos[1], ry, 1e-3F);
    ASSERT_NEAR(vel[1], rvy, 1e-3F);
    ASSERT_NEAR(pos[0], 1.0F, 1e-5F); /* aucune dérive latérale */
    ASSERT_NEAR(pos[2], -2.0F, 1e-5F);
    const float c = JOLT_LINEAR_DAMPING;
    ASSERT_NEAR(vel[1], -(9.81F / c) * (1.0F - expf(-c)), 0.03F);
    ASSERT_NEAR(pos[1], 10.0F - (0.5F * 9.81F), 0.15F); /* ordre de grandeur : y0 - g t^2 / 2 */
    eng_physics_destroy(w);
}

UTEST(physics, sphere_lands_on_floor_and_sleeps)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float floor_he[3] = {50.0F, 1.0F, 50.0F};
    const float floor_pos[3] = {0.0F, -1.0F, 0.0F}; /* dessus à y = 0 */
    ASSERT_NE(eng_physics_add_box(w, floor_he, floor_pos, ENG_LAYER_STATIC), ENG_BODY_INVALID);
    const float p[3] = {0.0F, 2.0F, 0.0F};
    const EngBody ball = eng_physics_add_sphere(w, 0.5F, p, ENG_LAYER_DYNAMIC);
    for (int i = 0; i < 600; i++) { /* 10 s */
        ASSERT_TRUE(eng_physics_step(w, DT));
    }
    float pos[3];
    ASSERT_TRUE(eng_physics_body_position(w, ball, pos));
    ASSERT_NEAR(pos[1], 0.5F, 0.02F); /* repose sur le sol : centre à un rayon */
    ASSERT_NEAR(pos[0], 0.0F, 1e-3F);
    ASSERT_FALSE(eng_physics_body_is_active(w, ball)); /* endormie */
    eng_physics_destroy(w);
}

UTEST(physics, static_body_never_moves)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float he[3] = {1, 1, 1};
    const float p[3] = {3, 4, 5};
    const EngBody b = eng_physics_add_box(w, he, p, ENG_LAYER_STATIC);
    for (int i = 0; i < 120; i++) {
        eng_physics_step(w, DT);
    }
    float out[3];
    ASSERT_TRUE(eng_physics_body_position(w, b, out));
    ASSERT_EQ(out[0], 3.0F);
    ASSERT_EQ(out[1], 4.0F);
    ASSERT_EQ(out[2], 5.0F);
    eng_physics_destroy(w);
}

UTEST(physics, custom_gravity_and_velocity)
{
    NEED_PHYSICS();
    EngPhysicsDesc d = {.gravity = {0.0F, -1.62F, 0.0F}}; /* Lune */
    EngPhysics *w = eng_physics_create(&d);
    const float p[3] = {0, 0, 0};
    const EngBody b = eng_physics_add_sphere(w, 0.25F, p, ENG_LAYER_DYNAMIC);
    const float v[3] = {3.0F, 0.0F, 0.0F};
    ASSERT_TRUE(eng_physics_set_body_velocity(w, b, v));
    for (int i = 0; i < 60; i++) {
        eng_physics_step(w, DT);
    }
    float vel[3];
    eng_physics_body_velocity(w, b, vel);
    float ry;
    float rvy;
    float rvx;
    reference_fall(0.0F, 0.0F, 3.0F, -1.62F, 60, &ry, &rvy, &rvx);
    ASSERT_NEAR(vel[0], rvx, 1e-4F);
    ASSERT_NEAR(vel[1], rvy, 1e-4F);
    ASSERT_NE(vel[1], -9.81F * 1.0F); /* la gravité demandée est bien utilisée, pas la terrestre */
    ASSERT_LT(vel[1], -1.5F);
    ASSERT_GT(vel[1], -1.7F);
    eng_physics_destroy(w);
}

/* Le résultat ne dépend pas du nombre de fils de travail. */
UTEST(physics, same_result_for_any_thread_count)
{
    NEED_PHYSICS();
    float results[3][3];
    const int threads[3] = {0, 1, 3};
    for (int k = 0; k < 3; k++) {
        EngPhysicsDesc d = {.worker_threads = threads[k]};
        EngPhysics *w = eng_physics_create(&d);
        const float fh[3] = {20, 1, 20};
        const float fp[3] = {0, -1, 0};
        eng_physics_add_box(w, fh, fp, ENG_LAYER_STATIC);
        EngBody last = ENG_BODY_INVALID;
        for (int i = 0; i < 12; i++) {
            const float p[3] = {(float)(i % 4) * 0.7F, 1.0F + ((float)i * 0.9F),
                                floorf((float)i / 4.0F) * 0.6F};
            last = eng_physics_add_sphere(w, 0.4F, p, ENG_LAYER_DYNAMIC);
        }
        for (int i = 0; i < 240; i++) {
            eng_physics_step(w, DT);
        }
        eng_physics_body_position(w, last, results[k]);
        eng_physics_destroy(w);
    }
    for (int k = 1; k < 3; k++) {
        for (int c = 0; c < 3; c++) {
            ASSERT_NEAR(results[k][c], results[0][c], 0.0F);
        }
    }
}

/* Les capacités internes suivent max_bodies : une rangée dense de 32 sphères en contact (une
 * trentaine de paires) ne dépasse pas la capacité de paires d'un monde de 32 corps. */
UTEST(physics, dense_row_fits_pair_capacity)
{
    NEED_PHYSICS();
    EngPhysicsDesc d = {.max_bodies = 32};
    EngPhysics *w = eng_physics_create(&d);
    for (int i = 0; i < 32; i++) {
        const float q[3] = {(float)i * 0.9F, 0.0F, 0.0F}; /* rayon 0,5 : léger recouvrement */
        ASSERT_NE(eng_physics_add_sphere(w, 0.5F, q, ENG_LAYER_DYNAMIC), ENG_BODY_INVALID);
    }
    for (int i = 0; i < 10; i++) {
        ASSERT_TRUE(eng_physics_step(w, DT));
    }
    eng_physics_destroy(w);
}

UTEST(physics, world_full_is_reported)
{
    NEED_PHYSICS();
    EngPhysicsDesc d = {.max_bodies = 4};
    EngPhysics *w = eng_physics_create(&d);
    const float p[3] = {0, 0, 0};
    int ok = 0;
    for (int i = 0; i < 8; i++) {
        const float q[3] = {(float)i * 2.0F, 0, 0};
        if (eng_physics_add_sphere(w, 0.5F, q, ENG_LAYER_STATIC) != ENG_BODY_INVALID) {
            ok++;
        }
    }
    (void)p;
    ASSERT_EQ(ok, 4);
    ASSERT_EQ(eng_physics_body_count(w), 4U);
    eng_physics_destroy(w);
}
