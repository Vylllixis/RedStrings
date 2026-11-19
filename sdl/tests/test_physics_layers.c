#include <math.h>
#include <stdint.h>
#include <string.h>

#include "engine/physics.h"
#include "utest.h"

#define DT (1.0F / 60.0F)

#define NEED_PHYSICS()                                                                             \
    if (!eng_physics_available()) {                                                                \
        return;                                                                                    \
    }

enum {
    S = ENG_LAYER_STATIC,
    D = ENG_LAYER_DYNAMIC,
    C = ENG_LAYER_CHARACTER,
    H = ENG_LAYER_HITBOX,
    T = ENG_LAYER_TRIGGER
};

/* Un entier quelconque vu comme couche (valeurs hors de l'énumération, pour les tests de rejet). */
static EngPhysicsLayer layer_from_int(int value)
{
    EngPhysicsLayer layer;
    memcpy(&layer, &value, sizeof(layer));
    return layer;
}

/* La matrice, recopiée à la main depuis docs/physics.md (0 aucune, 1 solide, 2 détection), de façon
 * indépendante du tableau du moteur. */
static const int EXPECTED_MATRIX[5][5] = {
    {0, 1, 1, 0, 0}, /* STATIC    */
    {1, 1, 1, 2, 2}, /* DYNAMIC   */
    {1, 1, 1, 2, 2}, /* CHARACTER */
    {0, 2, 2, 0, 0}, /* HITBOX    */
    {0, 2, 2, 0, 0}, /* TRIGGER   */
};

UTEST(physics_layers, matrix_matches_documentation_and_is_symmetric)
{
    for (int a = 0; a < 5; a++) {
        for (int b = 0; b < 5; b++) {
            ASSERT_EQ((int)eng_physics_layer_interaction((EngPhysicsLayer)a, (EngPhysicsLayer)b),
                      EXPECTED_MATRIX[a][b]);
            ASSERT_EQ((int)eng_physics_layer_interaction((EngPhysicsLayer)a, (EngPhysicsLayer)b),
                      (int)eng_physics_layer_interaction((EngPhysicsLayer)b, (EngPhysicsLayer)a));
        }
    }
    ASSERT_EQ((int)eng_physics_layer_interaction(ENG_LAYER_COUNT, ENG_LAYER_STATIC), 0);
    ASSERT_EQ((int)eng_physics_layer_interaction(ENG_LAYER_STATIC, ENG_LAYER_COUNT), 0);
    ASSERT_EQ((int)eng_physics_layer_interaction(layer_from_int(-1), ENG_LAYER_STATIC), 0);
}

UTEST(physics_layers, a_sensor_is_never_solid)
{
    for (int a = 0; a < 5; a++) {
        for (int b = 0; b < 5; b++) {
            if (eng_physics_layer_is_sensor((EngPhysicsLayer)a) ||
                eng_physics_layer_is_sensor((EngPhysicsLayer)b)) {
                ASSERT_NE(
                    (int)eng_physics_layer_interaction((EngPhysicsLayer)a, (EngPhysicsLayer)b),
                    (int)ENG_INTERACT_SOLID);
            }
        }
    }
    ASSERT_TRUE(eng_physics_layer_is_sensor(ENG_LAYER_HITBOX));
    ASSERT_TRUE(eng_physics_layer_is_sensor(ENG_LAYER_TRIGGER));
    ASSERT_FALSE(eng_physics_layer_is_sensor(ENG_LAYER_STATIC));
    ASSERT_FALSE(eng_physics_layer_is_sensor(ENG_LAYER_DYNAMIC));
    ASSERT_FALSE(eng_physics_layer_is_sensor(ENG_LAYER_CHARACTER));
    ASSERT_FALSE(eng_physics_layer_is_sensor(ENG_LAYER_COUNT));
}

UTEST(physics_layers, names)
{
    ASSERT_STREQ(eng_physics_layer_name(ENG_LAYER_STATIC), "static");
    ASSERT_STREQ(eng_physics_layer_name(ENG_LAYER_DYNAMIC), "dynamic");
    ASSERT_STREQ(eng_physics_layer_name(ENG_LAYER_CHARACTER), "character");
    ASSERT_STREQ(eng_physics_layer_name(ENG_LAYER_HITBOX), "hitbox");
    ASSERT_STREQ(eng_physics_layer_name(ENG_LAYER_TRIGGER), "trigger");
    ASSERT_STREQ(eng_physics_layer_name(ENG_LAYER_COUNT), "?");
    ASSERT_STREQ(eng_physics_layer_name(layer_from_int(-3)), "?");
}

static EngBody add_floor(EngPhysics *w)
{
    const float he[3] = {50.0F, 1.0F, 50.0F};
    const float p[3] = {0.0F, -1.0F, 0.0F}; /* dessus à y = 0 */
    return eng_physics_add_box(w, he, p, ENG_LAYER_STATIC);
}

/* Un personnage (capsule de 1,6 m de haut) posé au sol à l'origine, immobile. */
static EngBody add_standing_character(EngPhysics *w)
{
    const float p[3] = {0.0F, 0.8F, 0.0F};
    const EngBody c = eng_physics_add_capsule(w, 0.5F, 0.3F, p, ENG_LAYER_CHARACTER);
    for (int i = 0; i < 90; i++) {
        eng_physics_step(w, DT);
    }
    return c;
}

/* Critère M4-02 : « une hitbox ne pousse jamais un personnage ». Un volume de 1 m x 2 m x 1 m
 * traverse le personnage à 10 m/s ; le personnage ne bouge pas d'un micron, et la hitbox le
 * détecte. */
UTEST(physics_layers, hitbox_never_pushes_character)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    add_floor(w);
    const EngBody ch = add_standing_character(w);
    float before[3];
    ASSERT_TRUE(eng_physics_body_position(w, ch, before));

    const float he[3] = {0.5F, 1.0F, 0.5F};
    const float start[3] = {-4.0F, 1.0F, 0.0F};
    const EngBody hb = eng_physics_add_box(w, he, start, ENG_LAYER_HITBOX);
    ASSERT_NE(hb, ENG_BODY_INVALID);
    bool detected = false;
    float x = start[0];
    for (int i = 0; i < 60; i++) { /* de x = -4 à x = +6 : traverse le personnage de part en part */
        x += 10.0F * DT;
        const float target[3] = {x, 1.0F, 0.0F};
        ASSERT_TRUE(eng_physics_move_kinematic(w, hb, target, DT));
        ASSERT_TRUE(eng_physics_step(w, DT));
        EngBody hit[4];
        const uint32_t n = eng_physics_overlaps(w, hb, hit, 4);
        for (uint32_t k = 0; k < n && k < 4; k++) {
            detected = detected || hit[k] == ch;
        }
    }
    float after[3];
    float vel[3];
    ASSERT_TRUE(eng_physics_body_position(w, ch, after));
    ASSERT_TRUE(eng_physics_body_velocity(w, ch, vel));
    ASSERT_NEAR(after[0], before[0], 1e-4F);
    ASSERT_NEAR(after[1], before[1], 1e-4F);
    ASSERT_NEAR(after[2], before[2], 1e-4F);
    ASSERT_NEAR(vel[0], 0.0F, 1e-3F);
    ASSERT_TRUE(detected);
    eng_physics_destroy(w);
}

/* Contrôle du test précédent : le même corps, mais DYNAMIC (donc solide), pousse le personnage. */
UTEST(physics_layers, control_a_dynamic_body_does_push_the_character)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    add_floor(w);
    const EngBody ch = add_standing_character(w);
    const float p[3] = {-4.0F, 0.5F, 0.0F};
    const EngBody ball = eng_physics_add_sphere(w, 0.5F, p, ENG_LAYER_DYNAMIC);
    const float v[3] = {10.0F, 0.0F, 0.0F};
    ASSERT_TRUE(eng_physics_set_body_velocity(w, ball, v));
    for (int i = 0; i < 60; i++) {
        eng_physics_step(w, DT);
    }
    float after[3];
    ASSERT_TRUE(eng_physics_body_position(w, ch, after));
    ASSERT_GT(after[0], 0.5F); /* poussé loin de sa place */
    eng_physics_destroy(w);
}

UTEST(physics_layers, trigger_does_not_block_a_falling_body)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float he[3] = {2.0F, 0.5F, 2.0F};
    const float tp[3] = {0.0F, 5.0F, 0.0F};
    const EngBody tr = eng_physics_add_box(w, he, tp, ENG_LAYER_TRIGGER);
    const float bp[3] = {0.0F, 9.0F, 0.0F};
    const EngBody ball = eng_physics_add_sphere(w, 0.4F, bp, ENG_LAYER_DYNAMIC);
    bool inside = false;
    for (int i = 0; i < 180; i++) {
        eng_physics_step(w, DT);
        EngBody hit[2];
        const uint32_t n = eng_physics_overlaps(w, tr, hit, 2);
        inside = inside || (n == 1 && hit[0] == ball);
    }
    float pos[3];
    ASSERT_TRUE(eng_physics_body_position(w, ball, pos));
    ASSERT_LT(pos[1], 0.0F); /* a traversé la zone, sans freinage ni sol */
    ASSERT_TRUE(inside);     /* et la zone l'a vu passer */
    eng_physics_destroy(w);
}

UTEST(physics_layers, character_keeps_rotation_locked_dynamic_does_not)
{
    NEED_PHYSICS();
    for (int pass = 0; pass < 2; pass++) {
        const EngPhysicsLayer layer = pass == 0 ? ENG_LAYER_CHARACTER : ENG_LAYER_DYNAMIC;
        EngPhysics *w = eng_physics_create(NULL);
        add_floor(w);
        const float he[3] = {0.25F, 1.0F, 0.25F}; /* poteau de 2 m : bascule facilement */
        const float bp[3] = {0.0F, 1.0F, 0.0F};
        const EngBody box = eng_physics_add_box(w, he, bp, layer);
        const float sp[3] = {-3.0F, 1.8F, 0.0F}; /* frappe le haut du poteau : fort couple */
        const EngBody ball = eng_physics_add_sphere(w, 0.3F, sp, ENG_LAYER_DYNAMIC);
        const float v[3] = {15.0F, 0.0F, 0.0F};
        eng_physics_set_body_velocity(w, ball, v);
        float min_w = 1.0F;
        for (int i = 0; i < 40; i++) {
            eng_physics_step(w, DT);
            float q[4];
            ASSERT_TRUE(eng_physics_body_rotation(w, box, q));
            min_w = fminf(min_w, fabsf(q[3]));
        }
        if (pass == 0) {
            ASSERT_NEAR(min_w, 1.0F, 1e-5F); /* toujours droit */
        } else {
            ASSERT_LT(min_w, 0.9F); /* le poteau ordinaire bascule (plus de 25 degrés) */
        }
        eng_physics_destroy(w);
    }
}

/* Une scène où tout se recouvre à l'origine : le filtrage de eng_physics_overlaps suit la matrice.
 */
UTEST(physics_layers, overlaps_follow_the_matrix)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float he[3] = {1.0F, 1.0F, 1.0F};
    const float o[3] = {0.0F, 0.0F, 0.0F};
    const EngBody st = eng_physics_add_box(w, he, o, ENG_LAYER_STATIC);
    const EngBody dy = eng_physics_add_sphere(w, 0.5F, o, ENG_LAYER_DYNAMIC);
    const EngBody ch = eng_physics_add_capsule(w, 0.4F, 0.3F, o, ENG_LAYER_CHARACTER);
    const EngBody hb = eng_physics_add_box(w, he, o, ENG_LAYER_HITBOX);
    const EngBody hb2 = eng_physics_add_box(w, he, o, ENG_LAYER_HITBOX);
    const EngBody tr = eng_physics_add_box(w, he, o, ENG_LAYER_TRIGGER);
    ASSERT_NE(st, ENG_BODY_INVALID);
    ASSERT_NE(tr, ENG_BODY_INVALID);
    ASSERT_NE(hb2, ENG_BODY_INVALID);

    /* Une hitbox voit le dynamique et le personnage, ni le décor, ni les autres capteurs, ni
     * elle-même */
    EngBody out[8];
    for (int i = 0; i < 8; i++) {
        out[i] = 0xABCDEF01U; /* sentinelle : rien ne doit être écrit au-delà du nombre de corps */
    }
    uint32_t n = eng_physics_overlaps(w, hb, out, 8);
    ASSERT_EQ(n, 2U);
    for (int i = 2; i < 8; i++) {
        ASSERT_EQ(out[i], 0xABCDEF01U);
    }
    ASSERT_EQ(out[0], dy < ch ? dy : ch); /* triés par identifiant */
    ASSERT_EQ(out[1], dy < ch ? ch : dy);
    /* Un trigger : pareil */
    ASSERT_EQ(eng_physics_overlaps(w, tr, out, 8), 2U);
    /* Le décor voit le dynamique et le personnage (couples solides), ni les capteurs ni lui-même */
    n = eng_physics_overlaps(w, st, out, 8);
    ASSERT_EQ(n, 2U);
    /* Le dynamique voit le décor, le personnage, les deux hitbox et le trigger (pas lui-même) */
    n = eng_physics_overlaps(w, dy, out, 8);
    ASSERT_EQ(n, 5U);
    /* Comptage seul et troncature */
    ASSERT_EQ(eng_physics_overlaps(w, dy, NULL, 0), 5U);
    EngBody two[2] = {ENG_BODY_INVALID, ENG_BODY_INVALID};
    ASSERT_EQ(eng_physics_overlaps(w, dy, two, 2), 5U); /* total, même si 2 seulement sont écrits */
    ASSERT_LT(two[0], two[1]);
    ASSERT_NE(two[0], ENG_BODY_INVALID);
    eng_physics_destroy(w);
}

/* Identifiants dont l'emplacement est hors du tableau du monde (petit monde : 4 corps) ou libre. */
UTEST(physics_layers, ids_beyond_the_world_capacity_are_unknown)
{
    NEED_PHYSICS();
    EngPhysicsDesc d = {.max_bodies = 4};
    EngPhysics *w = eng_physics_create(&d);
    const float p[3] = {0, 0, 0};
    ASSERT_NE(eng_physics_add_sphere(w, 0.5F, p, ENG_LAYER_STATIC), ENG_BODY_INVALID);
    for (uint32_t id = 4U; id < 12U; id++) { /* emplacements 4 à 11, tous hors tableau */
        ASSERT_EQ((int)eng_physics_body_layer(w, id), (int)ENG_LAYER_COUNT);
        float out[3];
        ASSERT_FALSE(eng_physics_body_position(w, id, out));
    }
    for (uint32_t id = 1U; id < 4U; id++) { /* emplacements libres du tableau */
        ASSERT_EQ((int)eng_physics_body_layer(w, id), (int)ENG_LAYER_COUNT);
    }
    eng_physics_destroy(w);
}

UTEST(physics_layers, no_overlap_when_apart_and_unknown_body)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float he[3] = {0.5F, 0.5F, 0.5F};
    const float a[3] = {0, 0, 0};
    const float b[3] = {3, 0, 0};
    const EngBody hb = eng_physics_add_box(w, he, a, ENG_LAYER_HITBOX);
    const EngBody dy = eng_physics_add_sphere(w, 0.5F, b, ENG_LAYER_DYNAMIC);
    ASSERT_NE(dy, ENG_BODY_INVALID);
    EngBody out[2];
    ASSERT_EQ(eng_physics_overlaps(w, hb, out, 2), 0U);
    ASSERT_EQ(eng_physics_overlaps(w, 4242U, out, 2), 0U);
    ASSERT_EQ(eng_physics_overlaps(w, ENG_BODY_INVALID, out, 2), 0U);
    ASSERT_EQ(eng_physics_overlaps(NULL, hb, out, 2), 0U);
    eng_physics_destroy(w);
}

UTEST(physics_layers, overlap_count_is_capped)
{
    NEED_PHYSICS();
    EngPhysicsDesc d = {.max_bodies = 400};
    EngPhysics *w = eng_physics_create(&d);
    for (int i = 0; i < 300; i++) {
        const float p[3] = {(float)(i % 20), 0.0F, floorf((float)i / 20.0F)};
        ASSERT_NE(eng_physics_add_sphere(w, 0.3F, p, ENG_LAYER_DYNAMIC), ENG_BODY_INVALID);
    }
    const float he[3] = {30.0F, 1.0F, 30.0F};
    const float o[3] = {10.0F, 0.0F, 7.0F};
    const EngBody hb = eng_physics_add_box(w, he, o, ENG_LAYER_HITBOX);
    EngBody out[ENG_PHYSICS_MAX_OVERLAPS];
    ASSERT_EQ(eng_physics_overlaps(w, hb, out, ENG_PHYSICS_MAX_OVERLAPS), ENG_PHYSICS_MAX_OVERLAPS);
    for (uint32_t i = 1; i < ENG_PHYSICS_MAX_OVERLAPS; i++) {
        ASSERT_LT(out[i - 1], out[i]);
    }
    eng_physics_destroy(w);
}

UTEST(physics_layers, move_kinematic_reaches_target_and_validates)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float he[3] = {0.5F, 0.5F, 0.5F};
    const float a[3] = {0, 2, 0};
    const EngBody hb = eng_physics_add_box(w, he, a, ENG_LAYER_HITBOX);
    const float target[3] = {1.5F, 2.0F, -0.5F};
    ASSERT_TRUE(eng_physics_move_kinematic(w, hb, target, DT));
    ASSERT_TRUE(eng_physics_step(w, DT));
    float pos[3];
    ASSERT_TRUE(eng_physics_body_position(w, hb, pos));
    ASSERT_NEAR(pos[0], 1.5F, 1e-3F);
    ASSERT_NEAR(pos[1], 2.0F, 1e-3F); /* pas de gravité : cinématique */
    ASSERT_NEAR(pos[2], -0.5F, 1e-3F);
    /* il garde sa position sans nouvel ordre (pas de dérive) */
    for (int i = 0; i < 30; i++) {
        eng_physics_step(w, DT);
    }
    ASSERT_TRUE(eng_physics_body_position(w, hb, pos));
    ASSERT_NEAR(pos[1], 2.0F, 1e-3F);

    const float nanv[3] = {NAN, 0, 0};
    ASSERT_FALSE(eng_physics_move_kinematic(w, hb, nanv, DT));
    ASSERT_FALSE(eng_physics_move_kinematic(w, hb, target, 0.0F));
    ASSERT_FALSE(eng_physics_move_kinematic(w, hb, target, -DT));
    ASSERT_FALSE(eng_physics_move_kinematic(w, hb, target, NAN));
    ASSERT_FALSE(eng_physics_move_kinematic(w, hb, NULL, DT));
    ASSERT_FALSE(eng_physics_move_kinematic(w, 999U, target, DT));
    ASSERT_FALSE(eng_physics_move_kinematic(NULL, hb, target, DT));
    /* seuls les capteurs sont cinématiques */
    const EngBody st = eng_physics_add_box(w, he, a, ENG_LAYER_STATIC);
    const EngBody dy = eng_physics_add_sphere(w, 0.5F, a, ENG_LAYER_DYNAMIC);
    ASSERT_FALSE(eng_physics_move_kinematic(w, st, target, DT));
    ASSERT_FALSE(eng_physics_move_kinematic(w, dy, target, DT));
    eng_physics_destroy(w);
}

UTEST(physics_layers, body_layer_and_capsule_validation)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float p[3] = {0, 5, 0};
    for (int l = 0; l < 5; l++) {
        const EngBody b = eng_physics_add_sphere(w, 0.5F, p, (EngPhysicsLayer)l);
        ASSERT_NE(b, ENG_BODY_INVALID);
        ASSERT_EQ((int)eng_physics_body_layer(w, b), l);
        eng_physics_remove_body(w, b);
        ASSERT_EQ((int)eng_physics_body_layer(w, b), (int)ENG_LAYER_COUNT); /* retiré : inconnu */
    }
    ASSERT_EQ((int)eng_physics_body_layer(w, 12345U), (int)ENG_LAYER_COUNT);
    ASSERT_EQ((int)eng_physics_body_layer(NULL, 0U), (int)ENG_LAYER_COUNT);
    const float nanp[3] = {NAN, 0, 0};
    ASSERT_EQ(eng_physics_add_capsule(w, 0.0F, 0.3F, p, ENG_LAYER_DYNAMIC), ENG_BODY_INVALID);
    ASSERT_EQ(eng_physics_add_capsule(w, 0.5F, 0.0F, p, ENG_LAYER_DYNAMIC), ENG_BODY_INVALID);
    ASSERT_EQ(eng_physics_add_capsule(w, 0.5F, 0.3F, nanp, ENG_LAYER_DYNAMIC), ENG_BODY_INVALID);
    ASSERT_EQ(eng_physics_add_capsule(w, 0.5F, 0.3F, p, ENG_LAYER_COUNT), ENG_BODY_INVALID);
    ASSERT_EQ(eng_physics_add_capsule(w, NAN, 0.3F, p, ENG_LAYER_DYNAMIC), ENG_BODY_INVALID);
    ASSERT_EQ(eng_physics_add_capsule(NULL, 0.5F, 0.3F, p, ENG_LAYER_DYNAMIC), ENG_BODY_INVALID);
    ASSERT_NE(eng_physics_add_capsule(w, 0.5F, 0.3F, p, ENG_LAYER_CHARACTER), ENG_BODY_INVALID);
    float q[4];
    ASSERT_FALSE(eng_physics_body_rotation(w, 777U, q));
    eng_physics_destroy(w);
}

/* Un capteur est vraiment inerte : une sphère posée sur lui tombe à travers. */
UTEST(physics_layers, hitbox_is_not_a_floor)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float he[3] = {5.0F, 0.5F, 5.0F};
    const float hp[3] = {0, 0, 0};
    eng_physics_add_box(w, he, hp, ENG_LAYER_HITBOX);
    const float bp[3] = {0, 2, 0};
    const EngBody ball = eng_physics_add_sphere(w, 0.5F, bp, ENG_LAYER_DYNAMIC);
    for (int i = 0; i < 120; i++) {
        eng_physics_step(w, DT);
    }
    float pos[3];
    eng_physics_body_position(w, ball, pos);
    ASSERT_LT(pos[1], -5.0F);
    eng_physics_destroy(w);
}
