/* Géométrie de combat (M5-06) : distances de segments, capsules, balayage sans tunneling. */
#include <math.h>

#include "engine/combat_geom.h"
#include "utest.h"

static EngCapsule cap(float ax, float ay, float az, float bx, float by, float bz, float r)
{
    EngCapsule c = {{ax, ay, az}, {bx, by, bz}, r};
    return c;
}

UTEST(combat_geom, segment_distance_matches_hand_computed_cases)
{
    const float o[3] = {0, 0, 0};
    const float x1[3] = {1, 0, 0};
    const float y0[3] = {0, 1, 0};
    const float y1[3] = {1, 1, 0};
    float c1[3];
    float c2[3];
    /* parallèles à distance 1 */
    EXPECT_NEAR(eng_segseg_dist2(o, x1, y0, y1, c1, c2), 1.0F, 1e-6F);
    /* croisés : segment X et segment Z décalé de 0,5 en y : distance 0,5 -> 0,25 */
    const float z0[3] = {0.5F, 0.5F, -1};
    const float z1[3] = {0.5F, 0.5F, 1};
    EXPECT_NEAR(eng_segseg_dist2(o, x1, z0, z1, c1, c2), 0.25F, 1e-6F);
    EXPECT_NEAR(c1[0], 0.5F, 1e-6F);
    EXPECT_NEAR(c2[2], 0.0F, 1e-6F);
    /* bout à bout : le point le plus proche est une extrémité */
    const float far0[3] = {3, 0, 0};
    const float far1[3] = {4, 0, 0};
    EXPECT_NEAR(eng_segseg_dist2(o, x1, far0, far1, c1, c2), 4.0F, 1e-6F);
    EXPECT_NEAR(c1[0], 1.0F, 1e-6F);
    EXPECT_NEAR(c2[0], 3.0F, 1e-6F);
    /* segments dégénérés : points */
    EXPECT_NEAR(eng_segseg_dist2(o, o, far0, far0, NULL, NULL), 9.0F, 1e-6F);
    EXPECT_NEAR(eng_segseg_dist2(o, o, far0, far1, NULL, NULL), 9.0F, 1e-6F);
    EXPECT_NEAR(eng_segseg_dist2(far0, far1, o, o, NULL, NULL), 9.0F, 1e-6F);
    /* symétrie */
    EXPECT_NEAR(eng_segseg_dist2(o, x1, z0, z1, NULL, NULL),
                eng_segseg_dist2(z0, z1, o, x1, NULL, NULL), 1e-6F);
    /* colinéaires qui se recouvrent : 0 */
    const float m0[3] = {0.5F, 0, 0};
    const float m1[3] = {2, 0, 0};
    EXPECT_NEAR(eng_segseg_dist2(o, x1, m0, m1, NULL, NULL), 0.0F, 1e-6F);
}

UTEST(combat_geom, capsules_overlap_when_axis_distance_is_within_the_radius_sum)
{
    const EngCapsule a = cap(0, 0, 0, 0, 1, 0, 0.1F);
    EngCapsule b = cap(0.15F, 0, 0, 0.15F, 1, 0, 0.1F);
    float p[3];
    EXPECT_TRUE(eng_capsule_overlap(&a, &b, p)); /* 0,15 <= 0,2 */
    EXPECT_NEAR(p[0], 0.075F, 1e-6F);
    b = cap(0.21F, 0, 0, 0.21F, 1, 0, 0.1F);
    EXPECT_FALSE(eng_capsule_overlap(&a, &b, p)); /* 0,21 > 0,2 */
    b = cap(0.2F, 0, 0, 0.2F, 1, 0, 0.1F);
    EXPECT_TRUE(eng_capsule_overlap(&a, &b, NULL)); /* tangentes : touchent */
}

UTEST(combat_geom, invalid_capsules_never_overlap)
{
    const EngCapsule a = cap(0, 0, 0, 0, 1, 0, 0.1F);
    EngCapsule bad = a;
    bad.radius = -0.1F;
    EXPECT_FALSE(eng_capsule_overlap(&a, &bad, NULL));
    bad = a;
    bad.a[1] = NAN;
    EXPECT_FALSE(eng_capsule_overlap(&a, &bad, NULL));
    bad = a;
    bad.b[0] = INFINITY;
    EXPECT_FALSE(eng_capsule_overlap(&bad, &a, NULL));
    bad = a;
    bad.radius = NAN;
    EXPECT_FALSE(eng_capsule_overlap(&a, &bad, NULL));
    EXPECT_FALSE(eng_capsule_overlap(NULL, &a, NULL));
    EXPECT_FALSE(eng_capsule_overlap(&a, NULL, NULL));
    float t;
    EXPECT_FALSE(eng_capsule_sweep(&a, &a, &bad, &a, &t, NULL));
    EXPECT_FALSE(eng_capsule_sweep(NULL, &a, &a, &a, &t, NULL));
    EXPECT_FALSE(eng_capsule_sweep(&a, NULL, &a, &a, &t, NULL));
    EXPECT_FALSE(eng_capsule_sweep(&a, &a, NULL, &a, &t, NULL));
    EXPECT_FALSE(eng_capsule_sweep(&a, &a, &a, NULL, &t, NULL));
}

UTEST(combat_geom, a_fast_weapon_crossing_a_thin_target_is_hit_every_time)
{
    /* arme : sphère de 5 cm qui traverse 3 m en un tick (180 m/s) ; cible : fil de 1 cm à x = 1,7
     */
    const EngCapsule w0 = cap(0, 1, 0, 0, 1.4F, 0, 0.05F);
    const EngCapsule w1 = cap(3, 1, 0, 3, 1.4F, 0, 0.05F);
    const EngCapsule target = cap(1.7F, 0.5F, 0, 1.7F, 1.9F, 0, 0.01F);
    float t = -1.0F;
    float p[3];
    EXPECT_TRUE(eng_capsule_sweep(&w0, &w1, &target, &target, &t, p));
    EXPECT_NEAR(t, 1.7F / 3.0F, 0.03F);
    EXPECT_NEAR(p[0], 1.7F, 0.07F);
    /* et les deux positions de fin de tick, testées SEULES, rateraient la cible : */
    EXPECT_FALSE(eng_capsule_overlap(&w0, &target, NULL));
    EXPECT_FALSE(eng_capsule_overlap(&w1, &target, NULL));
}

static float rnd(uint32_t *seed)
{
    *seed = *seed * 1664525U + 1013904223U;
    return (float)(*seed >> 8) / (float)(1U << 24);
}

UTEST(combat_geom, sweep_agrees_with_a_brute_force_reference_on_random_cases)
{
    /* Référence : 2000 sous-pas. Tout contact PROFOND (pénétration > 3 cm) de la référence doit
     * être trouvé par le balayage ; le balayage ne doit rien inventer (un contact trouvé doit être
     * confirmé à l'instant rendu). Générateur déterministe. */
    uint32_t seed = 12345U;
    int deep = 0;
    int found_deep = 0;
    for (int n = 0; n < 400; n++) {
        float r[19];
        for (int k = 0; k < 19; k++) {
            r[k] = rnd(&seed);
        }
        const EngCapsule w0 = cap(r[0] * 2 - 1, r[1] * 2, r[2] * 2 - 1, r[3] * 2 - 1, r[4] * 2,
                                  r[5] * 2 - 1, 0.04F + r[6] * 0.1F);
        const EngCapsule w1 = cap(r[7] * 4 - 2, r[8] * 2, r[9] * 4 - 2, r[10] * 4 - 2, r[11] * 2,
                                  r[12] * 4 - 2, w0.radius);
        const EngCapsule t = cap(r[13] * 2 - 1, r[14] * 2, r[15] * 2 - 1, r[16] * 2 - 1, r[17] * 2,
                                 r[18] * 2 - 1, 0.03F + r[18] * 0.1F);
        float best_pen = -1.0F;
        for (int i = 0; i <= 2000; i++) {
            const float u = (float)i / 2000.0F;
            EngCapsule w = w0;
            for (int k = 0; k < 3; k++) {
                w.a[k] = w0.a[k] + (w1.a[k] - w0.a[k]) * u;
                w.b[k] = w0.b[k] + (w1.b[k] - w0.b[k]) * u;
            }
            const float d2 = eng_segseg_dist2(w.a, w.b, t.a, t.b, NULL, NULL);
            const float pen = (w.radius + t.radius) - sqrtf(d2);
            if (pen > best_pen) {
                best_pen = pen;
            }
        }
        float tt;
        const bool hit = eng_capsule_sweep(&w0, &w1, &t, &t, &tt, NULL);
        if (best_pen > 0.03F) {
            deep++;
            found_deep += hit ? 1 : 0;
            EXPECT_TRUE(hit);
        }
        if (hit) {
            EXPECT_GE(tt, 0.0F);
            EXPECT_LE(tt, 1.0F);
            EXPECT_GT(best_pen, -0.0001F); /* pas de faux contact */
        }
    }
    EXPECT_GT(deep, 30); /* le jeu d'essai contient de vrais contacts */
    EXPECT_EQ(found_deep, deep);
}

UTEST(combat_geom, a_miss_stays_a_miss_and_a_static_overlap_hits_at_time_zero)
{
    const EngCapsule w0 = cap(0, 1, 0, 0, 1.4F, 0, 0.05F);
    const EngCapsule w1 = cap(3, 1, 0, 3, 1.4F, 0, 0.05F);
    const EngCapsule off = cap(1.5F, 1.2F, 2.0F, 1.5F, 1.9F, 2.0F, 0.05F); /* 2 m à côté */
    float t;
    EXPECT_FALSE(eng_capsule_sweep(&w0, &w1, &off, &off, &t, NULL));
    const EngCapsule on = cap(0.05F, 1, 0, 0.05F, 1.4F, 0, 0.05F);
    EXPECT_TRUE(eng_capsule_sweep(&w0, &w1, &on, &on, &t, NULL));
    EXPECT_EQ(t, 0.0F);
    EXPECT_TRUE(eng_capsule_sweep(&w0, &w0, &on, &on, &t, NULL)); /* arme immobile */
}

UTEST(combat_geom, a_moving_target_is_swept_too)
{
    /* arme immobile, cible qui traverse l'arme en un tick */
    const EngCapsule w = cap(0, 0, 0, 0, 1, 0, 0.05F);
    const EngCapsule t0 = cap(-2, 0, 0, -2, 1, 0, 0.05F);
    const EngCapsule t1 = cap(2, 0, 0, 2, 1, 0, 0.05F);
    float tt;
    EXPECT_TRUE(eng_capsule_sweep(&w, &w, &t0, &t1, &tt, NULL));
    EXPECT_NEAR(tt, 0.5F, 0.06F);
}

UTEST(combat_geom, step_count_is_bounded_and_follows_the_displacement)
{
    const EngCapsule a = cap(0, 0, 0, 0, 1, 0, 0.1F);
    EXPECT_EQ(eng_capsule_sweep_steps(&a, &a, &a, &a), 1U);
    EngCapsule far = cap(10, 0, 0, 10, 1, 0, 0.1F);
    /* 10 m / 0,2 m = 50 pas */
    EXPECT_EQ(eng_capsule_sweep_steps(&a, &far, &a, &a), 50U);
    far = cap(1000, 0, 0, 1000, 1, 0, 0.1F);
    EXPECT_EQ(eng_capsule_sweep_steps(&a, &far, &a, &a), 256U); /* plafond */
    /* rayons nuls : pas minimal de 1 cm */
    const EngCapsule z0 = cap(0, 0, 0, 0, 1, 0, 0.0F);
    const EngCapsule z1 = cap(1, 0, 0, 1, 1, 0, 0.0F);
    EXPECT_EQ(eng_capsule_sweep_steps(&z0, &z1, &z0, &z0), 100U);
}
