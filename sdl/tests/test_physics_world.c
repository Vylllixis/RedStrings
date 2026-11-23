#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "engine/arena.h"
#include "engine/heightmap.h"
#include "engine/model.h"
#include "engine/physics.h"
#include "engine/rock.h"
#include "utest.h"

#define DT (1.0F / 60.0F)

#define NEED_PHYSICS()                                                                             \
    if (!eng_physics_available()) {                                                                \
        return;                                                                                    \
    }

#define RAMP_N 65U

/* Rampe : altitude = 0,3 x (64 - x) mètres (pente de 16,7 degrés, descend vers +x), 65 x 65
 * échantillons de 1 m, plage 0 à 20 m (19,2 m en x = 0). */
static uint16_t g_ramp[RAMP_N * RAMP_N];

static EngHeightmap make_ramp(void)
{
    for (uint32_t z = 0; z < RAMP_N; z++) {
        for (uint32_t x = 0; x < RAMP_N; x++) {
            g_ramp[((size_t)z * RAMP_N) + x] =
                (uint16_t)lroundf((64.0F - (float)x) * 0.3F / 20.0F * 65535.0F);
        }
    }
    return (EngHeightmap){.size = RAMP_N,
                          .spacing = 1.0F,
                          .altitude_min = 0.0F,
                          .altitude_max = 20.0F,
                          .samples = g_ramp};
}

static bool ray_down(EngPhysics *w, float x, float z, EngRayHit *hit)
{
    const float o[3] = {x, 100.0F, z};
    const float d[3] = {0.0F, -1.0F, 0.0F};
    return eng_physics_raycast(w, o, d, 200.0F, ENG_LAYERS_SOLID, hit);
}

UTEST(physics_world, heightfield_matches_the_rendered_surface_on_a_ramp)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const EngHeightmap map = make_ramp();
    EngPhysicsShape *shape = eng_physics_shape_heightfield(w, &map);
    ASSERT_TRUE(shape != NULL);
    const EngBody b = eng_physics_add_static_shape(w, shape, NULL);
    ASSERT_NE(b, ENG_BODY_INVALID);
    ASSERT_EQ((int)eng_physics_body_layer(w, b), (int)ENG_LAYER_STATIC);
    /* Plan : les deux diagonales sont identiques, la comparaison est exacte partout, bords compris.
     */
    for (int i = 0; i <= 32; i++) {
        for (int k = 0; k <= 32; k++) {
            const float x = (float)i * 2.0F; /* 0, 2, ..., 64 : bord inclus */
            const float z = (float)k * 2.0F;
            EngRayHit hit;
            ASSERT_TRUE(ray_down(w, x, z, &hit));
            ASSERT_NEAR(hit.point[1], eng_heightmap_sample(&map, x, z), 2e-3F);
            ASSERT_EQ(hit.body, b);
        }
    }
    /* Positions entre échantillons */
    for (int i = 0; i < 20; i++) {
        const float x = 3.37F + ((float)i * 2.91F);
        const float z = 5.11F + ((float)i * 2.17F);
        EngRayHit hit;
        ASSERT_TRUE(ray_down(w, x, z, &hit));
        ASSERT_NEAR(hit.point[1], eng_heightmap_sample(&map, x, z), 2e-3F);
    }
    eng_physics_shape_destroy(shape); /* les corps déjà créés restent valides */
    EngRayHit hit;
    ASSERT_TRUE(ray_down(w, 10.0F, 10.0F, &hit));
    ASSERT_NEAR(hit.point[1], 0.3F * 54.0F, 2e-3F);
    eng_physics_destroy(w);
}

/* Champ de taille par défaut (1025, impair) non plan : les sommets sont exacts, et le centre d'une
 * cellule vaut (a+d)/2 ou (b+c)/2 selon la diagonale de Jolt (le rendu alterne les deux). */
#define BIG_N 1025U
static uint16_t g_big[BIG_N * BIG_N];

UTEST(physics_world, default_size_heightfield_has_exact_vertices_and_diagonal_centres)
{
    NEED_PHYSICS();
    uint32_t seed = 12345U;
    for (size_t i = 0; i < (size_t)BIG_N * BIG_N; i++) {
        seed = (seed * 1664525U) + 1013904223U;
        g_big[i] = (uint16_t)(20000U + ((seed >> 16) % 4000U));
    }
    const EngHeightmap map = {.size = BIG_N,
                              .spacing = 1.0F,
                              .altitude_min = 0.0F,
                              .altitude_max = 100.0F,
                              .samples = g_big};
    EngPhysics *w = eng_physics_create(NULL);
    EngPhysicsShape *shape = eng_physics_shape_heightfield(w, &map);
    ASSERT_TRUE(shape != NULL);
    eng_physics_add_static_shape(w, shape, NULL);
    const uint32_t cells[4][2] = {{3U, 5U}, {500U, 501U}, {1022U, 1022U}, {777U, 12U}};
    for (int i = 0; i < 4; i++) {
        const float x = (float)cells[i][0];
        const float z = (float)cells[i][1];
        EngRayHit hit;
        ASSERT_TRUE(ray_down(w, x, z, &hit));
        ASSERT_NEAR(hit.point[1], eng_heightmap_sample(&map, x, z), 2e-3F);
        const float a = eng_heightmap_sample(&map, x, z);
        const float b = eng_heightmap_sample(&map, x + 1.0F, z);
        const float c = eng_heightmap_sample(&map, x, z + 1.0F);
        const float d = eng_heightmap_sample(&map, x + 1.0F, z + 1.0F);
        ASSERT_TRUE(ray_down(w, x + 0.5F, z + 0.5F, &hit));
        const float e1 = fabsf(hit.point[1] - ((a + d) * 0.5F));
        const float e2 = fabsf(hit.point[1] - ((b + c) * 0.5F));
        ASSERT_LT(fminf(e1, e2), 2e-3F);
    }
    eng_physics_shape_destroy(shape);
    eng_physics_destroy(w);
}

/* Normale de la rampe : pente -0,3 en x : (0,3, 1, 0) normalisée. */
UTEST(physics_world, ray_normal_and_distance)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const EngHeightmap map = make_ramp();
    EngPhysicsShape *shape = eng_physics_shape_heightfield(w, &map);
    eng_physics_add_static_shape(w, shape, NULL);
    EngRayHit hit;
    ASSERT_TRUE(ray_down(w, 20.0F, 30.0F, &hit));
    const float n = 1.0F / sqrtf(1.09F);
    ASSERT_NEAR(hit.normal[0], 0.3F * n, 2e-3F);
    ASSERT_NEAR(hit.normal[1], n, 2e-3F);
    ASSERT_NEAR(hit.normal[2], 0.0F, 2e-3F);
    ASSERT_NEAR(hit.distance, 100.0F - (0.3F * 44.0F), 2e-3F);
    /* trop court : pas de touche */
    const float o[3] = {20.0F, 100.0F, 30.0F};
    const float d[3] = {0.0F, -1.0F, 0.0F};
    EngRayHit miss;
    ASSERT_FALSE(eng_physics_raycast(w, o, d, 86.0F, ENG_LAYERS_SOLID, &miss));
    ASSERT_TRUE(eng_physics_raycast(w, o, d, 87.0F, ENG_LAYERS_SOLID, &miss));
    /* direction non normalisée : même résultat */
    const float d3[3] = {0.0F, -3.0F, 0.0F};
    ASSERT_TRUE(eng_physics_raycast(w, o, d3, 200.0F, ENG_LAYERS_SOLID, &miss));
    ASSERT_NEAR(miss.distance, 100.0F - (0.3F * 44.0F), 2e-3F);
    /* masque de couches : le terrain (STATIC) n'est pas vu par un masque DYNAMIC */
    ASSERT_FALSE(eng_physics_raycast(w, o, d, 200.0F, 1U << ENG_LAYER_DYNAMIC, &miss));
    ASSERT_TRUE(eng_physics_raycast(w, o, d, 200.0F, 1U << ENG_LAYER_STATIC, &miss));
    ASSERT_FALSE(eng_physics_raycast(w, o, d, 200.0F, 0U, &miss));
    eng_physics_shape_destroy(shape);
    eng_physics_destroy(w);
}

UTEST(physics_world, raycast_rejects_invalid_input)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float o[3] = {0, 5, 0};
    const float d[3] = {0, -1, 0};
    const float zero[3] = {0, 0, 0};
    const float nan3[3] = {NAN, 0, 0};
    EngRayHit hit;
    ASSERT_FALSE(eng_physics_raycast(NULL, o, d, 10.0F, ENG_LAYERS_ALL, &hit));
    ASSERT_FALSE(eng_physics_raycast(w, o, d, 10.0F, ENG_LAYERS_ALL, NULL));
    ASSERT_FALSE(eng_physics_raycast(w, NULL, d, 10.0F, ENG_LAYERS_ALL, &hit));
    ASSERT_FALSE(eng_physics_raycast(w, o, NULL, 10.0F, ENG_LAYERS_ALL, &hit));
    ASSERT_FALSE(eng_physics_raycast(w, nan3, d, 10.0F, ENG_LAYERS_ALL, &hit));
    ASSERT_FALSE(eng_physics_raycast(w, o, nan3, 10.0F, ENG_LAYERS_ALL, &hit));
    ASSERT_FALSE(eng_physics_raycast(w, o, zero, 10.0F, ENG_LAYERS_ALL, &hit));
    ASSERT_FALSE(eng_physics_raycast(w, o, d, 0.0F, ENG_LAYERS_ALL, &hit));
    ASSERT_FALSE(eng_physics_raycast(w, o, d, -1.0F, ENG_LAYERS_ALL, &hit));
    ASSERT_FALSE(eng_physics_raycast(w, o, d, INFINITY, ENG_LAYERS_ALL, &hit));
    ASSERT_FALSE(eng_physics_raycast(w, o, d, 10.0F, ENG_LAYERS_ALL, &hit)); /* monde vide */
    eng_physics_destroy(w);
}

/* Les capteurs ne sont pas vus par un rayon limité aux couches solides, mais le sont avec ALL. */
UTEST(physics_world, rays_respect_the_layer_mask_for_sensors)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float he[3] = {2, 2, 2};
    const float p[3] = {0, 0, 0};
    const EngBody hb = eng_physics_add_box(w, he, p, ENG_LAYER_HITBOX);
    const float o[3] = {0, 10, 0};
    const float d[3] = {0, -1, 0};
    EngRayHit hit;
    ASSERT_FALSE(eng_physics_raycast(w, o, d, 50.0F, ENG_LAYERS_SOLID, &hit));
    ASSERT_TRUE(eng_physics_raycast(w, o, d, 50.0F, ENG_LAYERS_ALL, &hit));
    ASSERT_EQ(hit.body, hb);
    ASSERT_NEAR(hit.distance, 8.0F, 1e-3F);
    eng_physics_destroy(w);
}

/* Critère M4-03 : une sphère roule sur le terrain (pente de 0,3) et heurte un rocher. */
static float roll_for(EngPhysics *w, EngBody ball, int steps, float *max_below_surface,
                      const EngHeightmap *map, const float target[3], float *min_dist)
{
    float closest = 1e9F;
    float worst = 0.0F; /* plus grande pénétration sous la surface (m) */
    for (int i = 0; i < steps; i++) {
        eng_physics_step(w, DT);
        float p[3];
        eng_physics_body_position(w, ball, p);
        /* le centre doit rester à au moins ~ rayon x cos(pente) au-dessus du sol */
        const float dx = p[0] - target[0];
        const float dy = p[1] - target[1];
        const float dz = p[2] - target[2];
        closest = fminf(closest, sqrtf((dx * dx) + (dy * dy) + (dz * dz)));
        const float depth = eng_heightmap_sample(map, p[0], p[2]) + 0.45F - p[1];
        worst = fmaxf(worst, depth);
    }
    *max_below_surface = worst;
    *min_dist = closest;
    float p[3];
    eng_physics_body_position(w, ball, p);
    return p[0];
}

UTEST(physics_world, sphere_rolls_down_the_terrain_and_stops_at_a_rock)
{
    NEED_PHYSICS();
    const EngHeightmap map = make_ramp();
    float d_free = 0.0F;
    float d_blocked = 0.0F;
    float below = 0.0F;
    for (int pass = 0; pass < 2; pass++) {
        EngPhysics *w = eng_physics_create(NULL);
        EngPhysicsShape *terrain = eng_physics_shape_heightfield(w, &map);
        eng_physics_add_static_shape(w, terrain, NULL);
        EngPhysicsShape *rock = NULL;
        EngArena arena;
        ASSERT_TRUE(eng_arena_init(&arena, "test-rock", 1U << 20));
        if (pass == 1) {
            EngRockParams rp = eng_rock_default_params();
            rp.radius = 2.5F;
            EngModel model;
            ASSERT_TRUE(eng_rock_build(&arena, &rp, &model));
            rock = eng_physics_shape_model(w, &model);
            ASSERT_TRUE(rock != NULL);
            /* le rocher est posé sur la rampe à x = 30 (altitude 10,2 m) */
            EngPhysicsPose pose = eng_physics_pose_identity();
            pose.position[0] = 30.0F;
            pose.position[1] = 10.2F;
            pose.position[2] = 32.0F;
            ASSERT_NE(eng_physics_add_static_shape(w, rock, &pose), ENG_BODY_INVALID);
        }
        /* la sphère est lâchée sans vitesse à x = 10 (altitude 16,2 m) : la pente la fait rouler
         * vers +x */
        const float start[3] = {10.0F, 16.2F + 0.5F, 32.0F};
        const EngBody ball = eng_physics_add_sphere(w, 0.45F, start, ENG_LAYER_DYNAMIC);
        ASSERT_NE(ball, ENG_BODY_INVALID);
        float worst = 0.0F;
        const float centre[3] = {30.0F, 10.2F, 32.0F};
        float dist = 0.0F;
        roll_for(w, ball, 420, &worst, &map, centre, &dist);
        below = fmaxf(below, worst);
        if (pass == 0) {
            d_free = dist;
        } else {
            d_blocked = dist;
        }
        eng_physics_shape_destroy(rock);
        eng_physics_shape_destroy(terrain);
        eng_arena_shutdown(&arena);
        eng_physics_destroy(w);
    }
    /* sans rocher la sphère passe au centre du futur rocher (distance < 1 m) ; avec lui (rayon 2,5
     * m) son centre reste à plus de 1,5 m de ce point : elle ne traverse jamais le rocher, elle le
     * contourne ou reste en amont. */
    ASSERT_LT(d_free, 1.0F);
    ASSERT_GT(d_blocked, 1.5F);
    /* jamais enfoncée dans le terrain */
    ASSERT_LT(below, 0.05F);
}

/* ── Validation des paramètres et poses des formes ── */

static EngHeightmap flat_map(uint16_t *samples, uint32_t size)
{
    for (size_t i = 0; i < (size_t)size * size; i++) {
        samples[i] = 1000U;
    }
    return (EngHeightmap){.size = size,
                          .spacing = 1.0F,
                          .altitude_min = 0.0F,
                          .altitude_max = 10.0F,
                          .samples = samples};
}

UTEST(physics_world, heightfield_parameters_are_validated)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    static uint16_t samples[8 * 8];
    EngHeightmap map = flat_map(samples, 8U);
    EngPhysicsShape *ok = eng_physics_shape_heightfield(w, &map);
    ASSERT_TRUE(ok != NULL);
    eng_physics_shape_destroy(ok);
    ASSERT_TRUE(eng_physics_shape_heightfield(NULL, &map) == NULL);
    ASSERT_TRUE(eng_physics_shape_heightfield(w, NULL) == NULL);
    EngHeightmap bad = map;
    bad.samples = NULL;
    ASSERT_TRUE(eng_physics_shape_heightfield(w, &bad) == NULL);
    bad = map;
    bad.size = 3U; /* trop petit */
    ASSERT_TRUE(eng_physics_shape_heightfield(w, &bad) == NULL);
    bad.size = ENG_PHYSICS_MAX_HEIGHTFIELD + 1U; /* trop grand : refusé avant toute lecture */
    ASSERT_TRUE(eng_physics_shape_heightfield(w, &bad) == NULL);
    bad = map;
    bad.spacing = 0.0F;
    ASSERT_TRUE(eng_physics_shape_heightfield(w, &bad) == NULL);
    bad.spacing = NAN;
    ASSERT_TRUE(eng_physics_shape_heightfield(w, &bad) == NULL);
    bad = map;
    bad.altitude_max = bad.altitude_min; /* plage nulle */
    ASSERT_TRUE(eng_physics_shape_heightfield(w, &bad) == NULL);
    bad = map;
    bad.altitude_max = NAN;
    ASSERT_TRUE(eng_physics_shape_heightfield(w, &bad) == NULL);
    bad = map;
    bad.altitude_min = INFINITY;
    ASSERT_TRUE(eng_physics_shape_heightfield(w, &bad) == NULL);
    /* les tailles limites sont acceptées : 4 x 4 */
    static uint16_t small[4 * 4];
    EngHeightmap four = flat_map(small, 4U);
    EngPhysicsShape *s4 = eng_physics_shape_heightfield(w, &four);
    ASSERT_TRUE(s4 != NULL);
    eng_physics_shape_destroy(s4);
    eng_physics_destroy(w);
}

UTEST(physics_world, largest_allowed_heightfield_is_accepted)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const size_t n = ENG_PHYSICS_MAX_HEIGHTFIELD;
    uint16_t *samples = malloc(n * n * sizeof(uint16_t));
    ASSERT_TRUE(samples != NULL);
    const EngHeightmap map = flat_map(samples, ENG_PHYSICS_MAX_HEIGHTFIELD);
    EngPhysicsShape *s = eng_physics_shape_heightfield(w, &map);
    ASSERT_TRUE(s != NULL);
    eng_physics_shape_destroy(s);
    free(samples);
    eng_physics_destroy(w);
}

/* Carré de 10 m dans le plan y = 0, normale +y (enroulement sortant vu du dessus), sommets stockés
 * avec un pas de 16 octets (x, y, z, remplissage). */
static const float QUAD[4][4] = {{0.0F, 0.0F, 0.0F, 0.0F},
                                 {0.0F, 0.0F, 10.0F, 0.0F},
                                 {10.0F, 0.0F, 10.0F, 0.0F},
                                 {10.0F, 0.0F, 0.0F, 0.0F}};
static const uint32_t QUAD_IDX[6] = {0U, 1U, 2U, 0U, 2U, 3U};

UTEST(physics_world, mesh_parameters_are_validated)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const size_t stride = 4U * sizeof(float);
    EngPhysicsShape *ok = eng_physics_shape_mesh(w, &QUAD[0][0], stride, 4U, QUAD_IDX, 6U);
    ASSERT_TRUE(ok != NULL);
    eng_physics_shape_destroy(ok);
    /* minimum : 3 sommets, 3 indices, pas de 12 octets */
    const float tri[9] = {0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 0.0F, 0.0F};
    const uint32_t tri_idx[3] = {0U, 1U, 2U};
    EngPhysicsShape *t = eng_physics_shape_mesh(w, tri, 3U * sizeof(float), 3U, tri_idx, 3U);
    ASSERT_TRUE(t != NULL);
    eng_physics_shape_destroy(t);
    ASSERT_TRUE(eng_physics_shape_mesh(NULL, tri, 12U, 3U, tri_idx, 3U) == NULL);
    ASSERT_TRUE(eng_physics_shape_mesh(w, NULL, 12U, 3U, tri_idx, 3U) == NULL);
    ASSERT_TRUE(eng_physics_shape_mesh(w, tri, 12U, 3U, NULL, 3U) == NULL);
    ASSERT_TRUE(eng_physics_shape_mesh(w, tri, 8U, 3U, tri_idx, 3U) == NULL);  /* pas trop court */
    ASSERT_TRUE(eng_physics_shape_mesh(w, tri, 12U, 2U, tri_idx, 3U) == NULL); /* 2 sommets */
    ASSERT_TRUE(eng_physics_shape_mesh(w, tri, 12U, 3U, tri_idx, 2U) == NULL); /* 2 indices */
    ASSERT_TRUE(eng_physics_shape_mesh(w, &QUAD[0][0], stride, 4U, QUAD_IDX, 5U) == NULL);
    /* indices hors borne : exactement vertex_count est déjà invalide */
    const uint32_t over0[3] = {3U, 1U, 2U};
    const uint32_t over1[3] = {0U, 3U, 2U};
    const uint32_t over2[3] = {0U, 1U, 3U};
    ASSERT_TRUE(eng_physics_shape_mesh(w, tri, 12U, 3U, over0, 3U) == NULL);
    ASSERT_TRUE(eng_physics_shape_mesh(w, tri, 12U, 3U, over1, 3U) == NULL);
    ASSERT_TRUE(eng_physics_shape_mesh(w, tri, 12U, 3U, over2, 3U) == NULL);
    /* sommet non fini */
    const float nan_tri[9] = {0.0F, 0.0F, NAN, 0.0F, 0.0F, 1.0F, 1.0F, 0.0F, 0.0F};
    ASSERT_TRUE(eng_physics_shape_mesh(w, nan_tri, 12U, 3U, tri_idx, 3U) == NULL);
    ASSERT_TRUE(eng_physics_shape_model(w, NULL) == NULL);
    eng_physics_shape_destroy(NULL); /* sans effet */
    eng_physics_destroy(w);
}

UTEST(physics_world, shape_poses_are_applied_and_validated)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    EngPhysicsShape *quad =
        eng_physics_shape_mesh(w, &QUAD[0][0], 4U * sizeof(float), 4U, QUAD_IDX, 6U);
    ASSERT_TRUE(quad != NULL);
    static uint16_t samples[8 * 8];
    const EngHeightmap map = flat_map(samples, 8U);
    EngPhysicsShape *field = eng_physics_shape_heightfield(w, &map);
    ASSERT_TRUE(field != NULL);

    EngPhysicsPose pose = eng_physics_pose_identity();
    ASSERT_NE(eng_physics_add_static_shape(w, quad, NULL), ENG_BODY_INVALID);
    ASSERT_EQ(eng_physics_body_count(w), 1U);
    /* refus : forme ou monde NULL ; sans effet sur le nombre de corps */
    ASSERT_EQ(eng_physics_add_static_shape(w, NULL, &pose), ENG_BODY_INVALID);
    ASSERT_EQ(eng_physics_add_static_shape(NULL, quad, &pose), ENG_BODY_INVALID);
    EngPhysicsPose bad = pose;
    bad.rotation[3] = 2.0F; /* quaternion non unitaire */
    ASSERT_EQ(eng_physics_add_static_shape(w, quad, &bad), ENG_BODY_INVALID);
    bad = pose;
    bad.scale = 0.0F;
    ASSERT_EQ(eng_physics_add_static_shape(w, quad, &bad), ENG_BODY_INVALID);
    bad.scale = -1.0F;
    ASSERT_EQ(eng_physics_add_static_shape(w, quad, &bad), ENG_BODY_INVALID);
    bad.scale = INFINITY;
    ASSERT_EQ(eng_physics_add_static_shape(w, quad, &bad), ENG_BODY_INVALID);
    bad = pose;
    bad.position[1] = NAN;
    ASSERT_EQ(eng_physics_add_static_shape(w, quad, &bad), ENG_BODY_INVALID);
    bad = pose;
    bad.rotation[0] = NAN;
    ASSERT_EQ(eng_physics_add_static_shape(w, quad, &bad), ENG_BODY_INVALID);
    bad = pose;
    bad.scale = 2.0F; /* une échelle n'est pas permise pour un champ de hauteurs */
    ASSERT_EQ(eng_physics_add_static_shape(w, field, &bad), ENG_BODY_INVALID);
    ASSERT_EQ(eng_physics_body_count(w), 1U);

    /* échelle 2 sur un maillage : le carré couvre alors 0..20 */
    EngRayHit hit;
    ASSERT_FALSE(ray_down(w, 15.0F, 15.0F, &hit));
    pose.scale = 2.0F;
    pose.position[1] = 3.0F;
    ASSERT_NE(eng_physics_add_static_shape(w, quad, &pose), ENG_BODY_INVALID);
    ASSERT_TRUE(ray_down(w, 15.0F, 15.0F, &hit));
    ASSERT_NEAR(hit.point[1], 3.0F, 1e-3F);
    ASSERT_NEAR(hit.normal[1], 1.0F, 1e-3F);

    /* rotation (0,5 ; 0,5 ; 0,5 ; 0,5) : x -> y, y -> z, z -> x. Le carré (x, 0, z) devient
     * (z, x, 0) : dans le plan z = 0, normale +z. */
    pose = eng_physics_pose_identity();
    pose.rotation[0] = 0.5F;
    pose.rotation[1] = 0.5F;
    pose.rotation[2] = 0.5F;
    pose.rotation[3] = 0.5F;
    pose.position[0] = 100.0F;
    ASSERT_NE(eng_physics_add_static_shape(w, quad, &pose), ENG_BODY_INVALID);
    const float o[3] = {105.0F, 5.0F, 10.0F};
    const float d[3] = {0.0F, 0.0F, -1.0F};
    ASSERT_TRUE(eng_physics_raycast(w, o, d, 50.0F, ENG_LAYERS_SOLID, &hit));
    ASSERT_NEAR(hit.distance, 10.0F, 1e-3F);
    ASSERT_NEAR(hit.normal[2], 1.0F, 1e-3F);
    eng_physics_shape_destroy(field);
    eng_physics_shape_destroy(quad);
    eng_physics_destroy(w);
}

UTEST(physics_world, oblique_ray_returns_exact_distance_and_point)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const EngHeightmap map = make_ramp();
    EngPhysicsShape *shape = eng_physics_shape_heightfield(w, &map);
    eng_physics_add_static_shape(w, shape, NULL);
    /* de (10, 50, 32) vers (1, -1, 0), direction non normalisée : 50 - t = 0,3 (54 - t) avec
     * t = 48,2857 ; la distance vaut t racine de 2 */
    const float o[3] = {10.0F, 50.0F, 32.0F};
    const float d[3] = {1.0F, -1.0F, 0.0F};
    EngRayHit hit;
    ASSERT_TRUE(eng_physics_raycast(w, o, d, 100.0F, ENG_LAYERS_SOLID, &hit));
    const float t = 33.8F / 0.7F;
    ASSERT_NEAR(hit.point[0], 10.0F + t, 5e-3F);
    ASSERT_NEAR(hit.point[1], 50.0F - t, 5e-3F);
    ASSERT_NEAR(hit.point[2], 32.0F, 1e-3F);
    ASSERT_NEAR(hit.distance, t * sqrtf(2.0F), 5e-3F);
    /* longueur maximale trop courte de 1 cm : pas de touche */
    ASSERT_FALSE(eng_physics_raycast(w, o, d, (t * sqrtf(2.0F)) - 0.01F, ENG_LAYERS_SOLID, &hit));
    /* longueurs et directions invalides */
    ASSERT_FALSE(eng_physics_raycast(w, o, d, 0.0F, ENG_LAYERS_SOLID, &hit));
    ASSERT_FALSE(eng_physics_raycast(w, o, d, -5.0F, ENG_LAYERS_SOLID, &hit));
    ASSERT_FALSE(eng_physics_raycast(w, o, d, INFINITY, ENG_LAYERS_SOLID, &hit));
    ASSERT_FALSE(eng_physics_raycast(w, o, d, NAN, ENG_LAYERS_SOLID, &hit));
    eng_physics_shape_destroy(shape);
    eng_physics_destroy(w);
}

UTEST(physics_world, heightfield_honours_a_non_zero_minimum_altitude)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    static uint16_t samples[8 * 8];
    EngHeightmap map = flat_map(samples, 8U);
    map.altitude_min = 5.0F; /* échantillon 1000 : 5 + 1000 / 65535 x 10 */
    map.altitude_max = 15.0F;
    EngPhysicsShape *shape = eng_physics_shape_heightfield(w, &map);
    ASSERT_TRUE(shape != NULL);
    eng_physics_add_static_shape(w, shape, NULL);
    EngRayHit hit;
    ASSERT_TRUE(ray_down(w, 3.0F, 3.0F, &hit));
    ASSERT_NEAR(hit.point[1], 5.0F + (1000.0F / 65535.0F * 10.0F), 2e-3F);
    ASSERT_NEAR(hit.point[1], eng_heightmap_sample(&map, 3.0F, 3.0F), 2e-3F);
    eng_physics_shape_destroy(shape);
    eng_physics_destroy(w);
}

UTEST(physics_world, ray_with_a_z_component_returns_the_right_point)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const EngHeightmap map = make_ramp();
    EngPhysicsShape *shape = eng_physics_shape_heightfield(w, &map);
    eng_physics_add_static_shape(w, shape, NULL);
    /* de (20, 50, 10) vers (0, -1, 1) : la pente ne dépend que de x, donc y = 13,2 en t = 36,8 */
    const float o[3] = {20.0F, 50.0F, 10.0F};
    const float d[3] = {0.0F, -1.0F, 1.0F};
    EngRayHit hit;
    ASSERT_TRUE(eng_physics_raycast(w, o, d, 100.0F, ENG_LAYERS_SOLID, &hit));
    ASSERT_NEAR(hit.point[0], 20.0F, 1e-3F);
    ASSERT_NEAR(hit.point[1], 13.2F, 5e-3F);
    ASSERT_NEAR(hit.point[2], 46.8F, 5e-3F);
    ASSERT_NEAR(hit.distance, 36.8F * sqrtf(2.0F), 5e-3F);
    eng_physics_shape_destroy(shape);
    eng_physics_destroy(w);
}

/* Écrase la pile sous l'appelant : un pointeur resté sur une variable locale d'une fonction déjà
 * terminée y lirait des octets 0xAA. */
static void scribble_stack(void)
{
    volatile unsigned char junk[16384];
    for (size_t i = 0; i < sizeof(junk); i++) {
        junk[i] = 0xAAU;
    }
}

UTEST(physics_world, raycast_does_not_depend_on_stack_left_by_world_creation)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const EngHeightmap map = make_ramp();
    EngPhysicsShape *shape = eng_physics_shape_heightfield(w, &map);
    eng_physics_add_static_shape(w, shape, NULL);
    scribble_stack();
    EngRayHit hit;
    ASSERT_TRUE(ray_down(w, 10.0F, 10.0F, &hit));
    ASSERT_NEAR(hit.point[1], 0.3F * 54.0F, 2e-3F);
    eng_physics_shape_destroy(shape);
    eng_physics_destroy(w);
}

/* Jolt complète un champ de 65 échantillons (non multiple de son bloc) : les échantillons ajoutés
 * n'ont pas de collision, donc aucune surface au-delà du bord du terrain affiché. */
UTEST(physics_world, there_is_no_collision_surface_beyond_the_terrain_edge)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const EngHeightmap map = make_ramp();
    EngPhysicsShape *shape = eng_physics_shape_heightfield(w, &map);
    eng_physics_add_static_shape(w, shape, NULL);
    EngRayHit hit;
    ASSERT_TRUE(ray_down(w, 63.5F, 30.0F, &hit));
    ASSERT_NEAR(hit.point[1], 0.15F, 2e-3F);
    ASSERT_FALSE(ray_down(w, 64.5F, 30.0F, &hit));
    ASSERT_FALSE(ray_down(w, 66.5F, 30.0F, &hit));
    ASSERT_FALSE(ray_down(w, 30.0F, 64.5F, &hit));
    ASSERT_FALSE(ray_down(w, -0.5F, 30.0F, &hit));
    eng_physics_shape_destroy(shape);
    eng_physics_destroy(w);
}
