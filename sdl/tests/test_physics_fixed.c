#include <math.h>
#include <stdint.h>
#include <string.h>

#include "engine/heightmap.h"
#include "engine/loop.h"
#include "engine/physics.h"
#include "utest.h"

#define NEED_PHYSICS()                                                                             \
    if (!eng_physics_available()) {                                                                \
        return;                                                                                    \
    }

#define FREQ 1000000000ULL /* ticks du compteur par seconde */
#define N 65U

static uint16_t g_ramp[N * N];

static EngHeightmap make_ramp(void)
{
    for (uint32_t z = 0; z < N; z++) {
        for (uint32_t x = 0; x < N; x++) {
            g_ramp[((size_t)z * N) + x] =
                (uint16_t)lroundf((64.0F - (float)x) * 0.3F / 20.0F * 65535.0F);
        }
    }
    return (EngHeightmap){
        .size = N, .spacing = 1.0F, .altitude_min = 0.0F, .altitude_max = 20.0F, .samples = g_ramp};
}

#define BODIES 6

typedef struct Scene {
    EngPhysics *world;
    EngPhysicsShape *terrain;
    EngBody bodies[BODIES];
} Scene;

/* Une rampe, quatre boîtes et deux sphères lâchées en haut : ça roule, ça tombe, ça se heurte. */
static bool scene_init(Scene *s, int worker_threads)
{
    EngPhysicsDesc d;
    memset(&d, 0, sizeof(d));
    d.worker_threads = worker_threads;
    s->world = eng_physics_create(&d);
    if (s->world == NULL) {
        return false;
    }
    const EngHeightmap map = make_ramp();
    s->terrain = eng_physics_shape_heightfield(s->world, &map);
    eng_physics_add_static_shape(s->world, s->terrain, NULL);
    const float half[3] = {0.5F, 0.3F, 0.4F};
    for (int i = 0; i < 4; i++) {
        const float p[3] = {12.0F + ((float)i * 0.9F), 18.0F + ((float)i * 1.3F),
                            30.0F + ((float)i * 0.7F)};
        s->bodies[i] = eng_physics_add_box(s->world, half, p, ENG_LAYER_DYNAMIC);
    }
    for (int i = 0; i < 2; i++) {
        const float p[3] = {11.0F + ((float)i * 0.5F), 22.0F + ((float)i * 1.1F),
                            31.0F + ((float)i * 0.9F)};
        s->bodies[4 + i] = eng_physics_add_sphere(s->world, 0.45F, p, ENG_LAYER_DYNAMIC);
    }
    return true;
}

static void scene_destroy(Scene *s)
{
    eng_physics_shape_destroy(s->terrain);
    eng_physics_destroy(s->world);
}

/* Fait avancer la scène de `total_ticks` en `frames` images de durée presque égale : la somme est
 * exacte. */
static uint32_t run_even(Scene *s, EngLoop *loop, uint64_t total_ticks, uint64_t frames)
{
    uint32_t steps = 0;
    for (uint64_t i = 0; i < frames; i++) {
        const uint64_t delta = (total_ticks * (i + 1ULL) / frames) - (total_ticks * i / frames);
        steps += eng_physics_advance(s->world, loop, delta);
    }
    return steps;
}

/* Images de durées pseudo-aléatoires entre 1 et 40 ms ; la dernière complète le total. */
static uint32_t run_jitter(Scene *s, EngLoop *loop, uint64_t total_ticks, uint32_t seed)
{
    uint32_t steps = 0;
    uint64_t left = total_ticks;
    while (left > 0) {
        seed = (seed * 1664525U) + 1013904223U;
        uint64_t delta = 1000000ULL + ((uint64_t)(seed >> 8) % 39000000ULL);
        if (delta > left) {
            delta = left;
        }
        steps += eng_physics_advance(s->world, loop, delta);
        left -= delta;
    }
    return steps;
}

typedef struct Snapshot {
    float pos[BODIES][3];
    float rot[BODIES][4];
    float vel[BODIES][3];
} Snapshot;

/* Égalité bit à bit (et pas « == » : -0 et +0, ou NaN, ne comptent pas comme identiques). */
static bool same_bits(const Snapshot *a, const Snapshot *b)
{
    const float *fa = &a->pos[0][0];
    const float *fb = &b->pos[0][0];
    const size_t count = sizeof(Snapshot) / sizeof(float);
    for (size_t i = 0; i < count; i++) {
        uint32_t ua;
        uint32_t ub;
        memcpy(&ua, &fa[i], sizeof(ua));
        memcpy(&ub, &fb[i], sizeof(ub));
        if (ua != ub) {
            return false;
        }
    }
    return true;
}

static void take(const Scene *s, Snapshot *out)
{
    memset(out, 0, sizeof(*out));
    for (int i = 0; i < BODIES; i++) {
        eng_physics_body_position(s->world, s->bodies[i], out->pos[i]);
        eng_physics_body_rotation(s->world, s->bodies[i], out->rot[i]);
        eng_physics_body_velocity(s->world, s->bodies[i], out->vel[i]);
    }
}

#define SECONDS 4U
#define TOTAL_TICKS (4ULL * FREQ)

UTEST(physics_fixed, state_is_bit_identical_at_any_frame_rate)
{
    NEED_PHYSICS();
    Snapshot ref;
    bool have_ref = false;
    const uint64_t frame_counts[] = {120ULL, 240ULL, 576ULL, 960ULL, 68ULL, 4000ULL};
    for (size_t k = 0; k < sizeof(frame_counts) / sizeof(frame_counts[0]); k++) {
        Scene s;
        ASSERT_TRUE(scene_init(&s, 1));
        EngLoop loop;
        ASSERT_TRUE(eng_loop_init(&loop, FREQ, 60U, 0.25));
        const uint32_t steps = run_even(&s, &loop, TOTAL_TICKS, frame_counts[k]);
        ASSERT_EQ(steps, 60U * SECONDS);
        ASSERT_EQ(eng_physics_step_count(s.world), 60U * SECONDS);
        Snapshot snap;
        take(&s, &snap);
        if (!have_ref) {
            ref = snap;
            have_ref = true;
        } else {
            ASSERT_TRUE(same_bits(&snap, &ref));
        }
        scene_destroy(&s);
    }
    /* la référence n'est pas triviale : les corps ont bougé et sont inclinés */
    ASSERT_TRUE(have_ref);
    float moved = 0.0F;
    for (int i = 0; i < BODIES; i++) {
        moved += fabsf(ref.pos[i][0] - 12.0F) + fabsf(ref.vel[i][0]) + fabsf(ref.vel[i][1]);
    }
    ASSERT_GT(moved, 1.0F);
}

UTEST(physics_fixed, irregular_frames_give_the_same_state_as_a_steady_rate)
{
    NEED_PHYSICS();
    Scene a;
    Scene b;
    Scene c;
    ASSERT_TRUE(scene_init(&a, 1));
    ASSERT_TRUE(scene_init(&b, 1));
    ASSERT_TRUE(scene_init(&c, 3)); /* et avec un autre nombre de fils de calcul */
    EngLoop la;
    EngLoop lb;
    EngLoop lc;
    ASSERT_TRUE(eng_loop_init(&la, FREQ, 60U, 0.25));
    ASSERT_TRUE(eng_loop_init(&lb, FREQ, 60U, 0.25));
    ASSERT_TRUE(eng_loop_init(&lc, FREQ, 60U, 0.25));
    ASSERT_EQ(run_even(&a, &la, TOTAL_TICKS, 240U), 240U);
    ASSERT_EQ(run_jitter(&b, &lb, TOTAL_TICKS, 7U), 240U);
    ASSERT_EQ(run_jitter(&c, &lc, TOTAL_TICKS, 12345U), 240U);
    Snapshot sa;
    Snapshot sb;
    Snapshot sc;
    take(&a, &sa);
    take(&b, &sb);
    take(&c, &sc);
    ASSERT_TRUE(same_bits(&sa, &sb));
    ASSERT_TRUE(same_bits(&sa, &sc));
    scene_destroy(&a);
    scene_destroy(&b);
    scene_destroy(&c);
}

/* Contrôle : sans pas fixe (un pas par image, dt = durée de l'image) le résultat dépend du rythme
 * d'affichage. C'est ce que la boucle évite. */
UTEST(physics_fixed, a_variable_step_would_depend_on_the_frame_rate)
{
    NEED_PHYSICS();
    Snapshot snaps[2];
    const uint32_t fps[2] = {30U, 144U};
    for (int k = 0; k < 2; k++) {
        Scene s;
        ASSERT_TRUE(scene_init(&s, 1));
        const uint32_t frames = SECONDS * fps[k];
        for (uint32_t i = 0; i < frames; i++) {
            ASSERT_TRUE(eng_physics_step(s.world, 1.0F / (float)fps[k]));
        }
        take(&s, &snaps[k]);
        scene_destroy(&s);
    }
    ASSERT_FALSE(same_bits(&snaps[0], &snaps[1]));
}

UTEST(physics_fixed, a_long_frame_is_clamped_instead_of_catching_up)
{
    NEED_PHYSICS();
    Scene s;
    ASSERT_TRUE(scene_init(&s, 1));
    EngLoop loop;
    ASSERT_TRUE(eng_loop_init(&loop, FREQ, 60U, 0.25));
    /* une image de 10 s (jeu suspendu dans le débogueur) : au plus 0,25 s, soit 15 pas */
    ASSERT_EQ(eng_physics_advance(s.world, &loop, 10ULL * FREQ), 15U);
    ASSERT_EQ(eng_physics_step_count(s.world), 15U);
    ASSERT_EQ(loop.clamped_frames, 1U);
    scene_destroy(&s);
}

UTEST(physics_fixed, advance_rejects_invalid_arguments_without_consuming_time)
{
    NEED_PHYSICS();
    Scene s;
    ASSERT_TRUE(scene_init(&s, 1));
    EngLoop loop;
    ASSERT_TRUE(eng_loop_init(&loop, FREQ, 60U, 0.25));
    ASSERT_EQ(eng_physics_advance(NULL, &loop, FREQ), 0U);
    ASSERT_EQ(eng_physics_advance(s.world, NULL, FREQ), 0U);
    ASSERT_EQ(loop.tick_count, 0U);
    ASSERT_EQ(loop.accumulator, 0U); /* le temps n'a pas été consommé */
    /* pas de 0,5 s : plus grand que ce que la physique accepte */
    EngLoop slow;
    ASSERT_TRUE(eng_loop_init(&slow, FREQ, 2U, 1.0));
    ASSERT_EQ(eng_physics_advance(s.world, &slow, FREQ), 0U);
    ASSERT_EQ(slow.tick_count, 0U);
    ASSERT_EQ(eng_physics_step_count(s.world), 0U);
    ASSERT_EQ(eng_physics_step_count(NULL), 0U);
    /* un pas à 10 Hz (0,1 s) est la limite acceptée */
    EngLoop edge;
    ASSERT_TRUE(eng_loop_init(&edge, FREQ, 10U, 1.0));
    ASSERT_EQ(eng_physics_advance(s.world, &edge, FREQ / 10U), 1U);
    scene_destroy(&s);
}

static float dist3(const float a[3], const float b[3])
{
    const float dx = a[0] - b[0];
    const float dy = a[1] - b[1];
    const float dz = a[2] - b[2];
    return sqrtf((dx * dx) + (dy * dy) + (dz * dz));
}

UTEST(physics_fixed, interpolation_blends_the_last_two_states)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float start[3] = {1.0F, 50.0F, 2.0F};
    const EngBody ball = eng_physics_add_sphere(w, 0.5F, start, ENG_LAYER_DYNAMIC);
    for (int i = 0; i < 10; i++) {
        ASSERT_TRUE(eng_physics_step(w, 1.0F / 60.0F));
    }
    float before[3];
    ASSERT_TRUE(eng_physics_body_position(w, ball, before));
    ASSERT_TRUE(eng_physics_step(w, 1.0F / 60.0F));
    float after[3];
    ASSERT_TRUE(eng_physics_body_position(w, ball, after));
    ASSERT_GT(before[1] - after[1], 0.01F); /* elle tombe de plus d'un centimètre par pas */
    const double alphas[] = {0.0, 0.25, 0.5, 0.999, 1.0};
    for (size_t i = 0; i < sizeof(alphas) / sizeof(alphas[0]); i++) {
        float p[3];
        ASSERT_TRUE(eng_physics_body_interpolated(w, ball, alphas[i], p, NULL));
        for (int k = 0; k < 3; k++) {
            ASSERT_NEAR(p[k], before[k] + ((after[k] - before[k]) * (float)alphas[i]), 1e-5F);
        }
    }
    float p[3];
    ASSERT_TRUE(eng_physics_body_interpolated(w, ball, 1.0, p, NULL));
    ASSERT_TRUE(p[0] == after[0] && p[1] == after[1] && p[2] == after[2]);
    ASSERT_TRUE(eng_physics_body_interpolated(w, ball, 0.0, p, NULL));
    ASSERT_TRUE(p[0] == before[0] && p[1] == before[1] && p[2] == before[2]);
    /* sorties NULL acceptées ; alpha invalide : refusé, sorties inchangées */
    ASSERT_TRUE(eng_physics_body_interpolated(w, ball, 0.5, NULL, NULL));
    const float sentinel[3] = {-7.0F, -8.0F, -9.0F};
    memcpy(p, sentinel, sizeof(p));
    ASSERT_FALSE(eng_physics_body_interpolated(w, ball, -0.01, p, NULL));
    ASSERT_FALSE(eng_physics_body_interpolated(w, ball, 1.01, p, NULL));
    ASSERT_FALSE(eng_physics_body_interpolated(w, ball, NAN, p, NULL));
    ASSERT_FALSE(eng_physics_body_interpolated(w, ball, INFINITY, p, NULL));
    ASSERT_FALSE(eng_physics_body_interpolated(w, ENG_BODY_INVALID, 0.5, p, NULL));
    ASSERT_FALSE(eng_physics_body_interpolated(NULL, ball, 0.5, p, NULL));
    ASSERT_TRUE(p[0] == sentinel[0] && p[1] == sentinel[1] && p[2] == sentinel[2]);
    eng_physics_destroy(w);
}

UTEST(physics_fixed, interpolation_slerps_rotations)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const EngHeightmap map = make_ramp();
    EngPhysicsShape *terrain = eng_physics_shape_heightfield(w, &map);
    eng_physics_add_static_shape(w, terrain, NULL);
    const float half[3] = {0.5F, 0.3F, 0.4F};
    const float start[3] = {20.0F, 15.0F, 30.0F};
    const EngBody box = eng_physics_add_box(w, half, start, ENG_LAYER_DYNAMIC);
    /* on cherche un pas où la boîte tourne vraiment (l'atterrissage sur la pente) */
    float qb[4];
    float qa[4];
    bool found = false;
    for (int i = 0; i < 240 && !found; i++) {
        ASSERT_TRUE(eng_physics_body_rotation(w, box, qb));
        ASSERT_TRUE(eng_physics_step(w, 1.0F / 60.0F));
        ASSERT_TRUE(eng_physics_body_rotation(w, box, qa));
        const float dot =
            fabsf((qa[0] * qb[0]) + (qa[1] * qb[1]) + (qa[2] * qb[2]) + (qa[3] * qb[3]));
        found = dot < 0.99999F;
    }
    ASSERT_TRUE(found);
    /* alpha = 0 : l'état d'avant le pas ; alpha = 1 : l'état courant */
    float q0[4];
    float q1[4];
    float qh[4];
    ASSERT_TRUE(eng_physics_body_interpolated(w, box, 0.0, NULL, q0));
    for (int k = 0; k < 4; k++) {
        ASSERT_NEAR(q0[k], qb[k], 1e-6F);
    }
    ASSERT_TRUE(eng_physics_body_interpolated(w, box, 1.0, NULL, q1));
    ASSERT_TRUE(eng_physics_body_interpolated(w, box, 0.5, NULL, qh));
    ASSERT_TRUE(eng_physics_body_rotation(w, box, qa));
    for (int k = 0; k < 4; k++) {
        ASSERT_NEAR(q1[k], qa[k], 1e-6F);
    }
    /* le quaternion intermédiaire est unitaire et à égale distance angulaire des deux bouts */
    ASSERT_NEAR(sqrtf((qh[0] * qh[0]) + (qh[1] * qh[1]) + (qh[2] * qh[2]) + (qh[3] * qh[3])), 1.0F,
                1e-5F);
    const float d0 = fabsf((qh[0] * q0[0]) + (qh[1] * q0[1]) + (qh[2] * q0[2]) + (qh[3] * q0[3]));
    const float d1 = fabsf((qh[0] * q1[0]) + (qh[1] * q1[1]) + (qh[2] * q1[2]) + (qh[3] * q1[3]));
    ASSERT_NEAR(d0, d1, 1e-5F);
    ASSERT_LT(d0, 1.0F);
    eng_physics_shape_destroy(terrain);
    eng_physics_destroy(w);
}

UTEST(physics_fixed, new_bodies_and_reused_slots_do_not_blend_with_stale_state)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float a_pos[3] = {0.0F, 100.0F, 0.0F};
    const EngBody a = eng_physics_add_sphere(w, 0.5F, a_pos, ENG_LAYER_DYNAMIC);
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(eng_physics_step(w, 1.0F / 60.0F));
    }
    /* un corps créé après le dernier pas : rien à interpoler, on rend son état courant */
    const float b_pos[3] = {500.0F, 7.0F, -3.0F};
    const EngBody b = eng_physics_add_sphere(w, 0.5F, b_pos, ENG_LAYER_DYNAMIC);
    float p[3];
    ASSERT_TRUE(eng_physics_body_interpolated(w, b, 0.0, p, NULL));
    ASSERT_LT(dist3(p, b_pos), 1e-5F);
    /* un emplacement libéré puis réutilisé n'hérite pas de l'état de l'ancien corps */
    eng_physics_remove_body(w, a);
    const float c_pos[3] = {-300.0F, 40.0F, 9.0F};
    const EngBody c = eng_physics_add_sphere(w, 0.5F, c_pos, ENG_LAYER_DYNAMIC);
    ASSERT_TRUE(eng_physics_body_interpolated(w, c, 0.0, p, NULL));
    ASSERT_LT(dist3(p, c_pos), 1e-5F);
    /* après un pas, l'interpolation de alpha = 0 donne l'état d'avant ce pas */
    ASSERT_TRUE(eng_physics_step(w, 1.0F / 60.0F));
    ASSERT_TRUE(eng_physics_body_interpolated(w, c, 0.0, p, NULL));
    ASSERT_LT(dist3(p, c_pos), 1e-4F);
    ASSERT_FALSE(eng_physics_body_interpolated(w, a, 0.5, p, NULL)); /* retiré */
    eng_physics_destroy(w);
}

UTEST(physics_fixed, static_bodies_are_never_blended)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float half[3] = {1.0F, 1.0F, 1.0F};
    const float pos[3] = {4.0F, 5.0F, 6.0F};
    const EngBody rock = eng_physics_add_box(w, half, pos, ENG_LAYER_STATIC);
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(eng_physics_step(w, 1.0F / 60.0F));
    }
    float p[3];
    float q[4];
    ASSERT_TRUE(eng_physics_body_interpolated(w, rock, 0.5, p, q));
    ASSERT_LT(dist3(p, pos), 1e-6F);
    ASSERT_NEAR(q[3], 1.0F, 1e-6F);
    eng_physics_destroy(w);
}
