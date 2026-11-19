#include <math.h>
#include <stdint.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/debug_lines.h"
#include "engine/heightmap.h"
#include "engine/physics.h"
#include "utest.h"

#define DT (1.0F / 60.0F)

#define NEED_PHYSICS()                                                                             \
    if (!eng_physics_available()) {                                                                \
        return;                                                                                    \
    }

#define N 65U

/* Rampe : altitude = 0,3 (64 - x) m, 65 x 65 échantillons de 1 m. */
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

/* Une liste de lignes + son arène. */
typedef struct Fixture {
    EngArena arena;
    EngDebugLines lines;
} Fixture;

static bool fixture_init(Fixture *f, uint32_t max_lines)
{
    return eng_arena_init(&f->arena, "test-physics-debug", (size_t)max_lines * 2U * 32U + 4096U) &&
           eng_debug_lines_init(&f->lines, &f->arena, max_lines);
}

static EngPhysicsDebugDraw all_layers(void)
{
    EngPhysicsDebugDraw o;
    memset(&o, 0, sizeof(o));
    o.layer_mask = ENG_LAYERS_ALL;
    return o;
}

static float dist3(const float a[3], const float b[3])
{
    const float dx = a[0] - b[0];
    const float dy = a[1] - b[1];
    const float dz = a[2] - b[2];
    return sqrtf((dx * dx) + (dy * dy) + (dz * dz));
}

static bool same_color(EngDebugColor a, EngDebugColor b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

/* Les couleurs de couche sont pures : elles ne dépendent pas de Jolt. */
UTEST(physics_debug, layer_colors_are_distinct_and_unknown_is_magenta)
{
    for (int a = 0; a < ENG_LAYER_COUNT; a++) {
        for (int b = a + 1; b < ENG_LAYER_COUNT; b++) {
            ASSERT_FALSE(same_color(eng_physics_layer_color((EngPhysicsLayer)a),
                                    eng_physics_layer_color((EngPhysicsLayer)b)));
        }
        ASSERT_FALSE(same_color(eng_physics_layer_color((EngPhysicsLayer)a), ENG_DEBUG_MAGENTA));
    }
    ASSERT_TRUE(same_color(eng_physics_layer_color(ENG_LAYER_COUNT), ENG_DEBUG_MAGENTA));
    ASSERT_TRUE(same_color(eng_physics_layer_color(ENG_LAYER_STATIC), ENG_DEBUG_GRAY));
    ASSERT_TRUE(same_color(eng_physics_layer_color(ENG_LAYER_DYNAMIC), ENG_DEBUG_GREEN));
    ASSERT_TRUE(same_color(eng_physics_layer_color(ENG_LAYER_CHARACTER), ENG_DEBUG_CYAN));
    ASSERT_TRUE(same_color(eng_physics_layer_color(ENG_LAYER_HITBOX), ENG_DEBUG_RED));
    ASSERT_TRUE(same_color(eng_physics_layer_color(ENG_LAYER_TRIGGER), ENG_DEBUG_YELLOW));
}

UTEST(physics_debug, sphere_is_drawn_at_its_centre_with_its_radius)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float c[3] = {3.0F, 2.0F, -1.0F};
    ASSERT_NE(eng_physics_add_sphere(w, 0.7F, c, ENG_LAYER_DYNAMIC), ENG_BODY_INVALID);
    Fixture f;
    ASSERT_TRUE(fixture_init(&f, 4096));
    EngPhysicsDebugDraw o = all_layers();
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    ASSERT_EQ(eng_debug_lines_count(&f.lines), 3U * ENG_DEBUG_CIRCLE_SEGMENTS);
    for (uint32_t i = 0; i < f.lines.depth_count; i++) {
        ASSERT_NEAR(dist3(f.lines.depth[i].position, c), 0.7F, 1e-4F);
        ASSERT_TRUE(same_color(f.lines.depth[i].color, ENG_DEBUG_GREEN));
    }
    /* le nombre de segments demandé est transmis */
    eng_debug_lines_clear(&f.lines);
    o.segments = 8U;
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    ASSERT_EQ(eng_debug_lines_count(&f.lines), 24U);
    eng_arena_shutdown(&f.arena);
    eng_physics_destroy(w);
}

UTEST(physics_debug, box_is_drawn_with_its_eight_corners)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float half[3] = {1.0F, 2.0F, 3.0F};
    const float c[3] = {10.0F, 5.0F, -4.0F};
    ASSERT_NE(eng_physics_add_box(w, half, c, ENG_LAYER_STATIC), ENG_BODY_INVALID);
    Fixture f;
    ASSERT_TRUE(fixture_init(&f, 256));
    const EngPhysicsDebugDraw o = all_layers();
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    ASSERT_EQ(eng_debug_lines_count(&f.lines), 12U);
    for (uint32_t i = 0; i < f.lines.depth_count; i++) {
        const float *p = f.lines.depth[i].position;
        ASSERT_NEAR(fabsf(p[0] - c[0]), 1.0F, 1e-4F);
        ASSERT_NEAR(fabsf(p[1] - c[1]), 2.0F, 1e-4F);
        ASSERT_NEAR(fabsf(p[2] - c[2]), 3.0F, 1e-4F);
        ASSERT_TRUE(same_color(f.lines.depth[i].color, ENG_DEBUG_GRAY));
    }
    eng_arena_shutdown(&f.arena);
    eng_physics_destroy(w);
}

/* Une boîte posée sur une pente s'incline : les arêtes dessinées suivent la rotation simulée. */
UTEST(physics_debug, tilted_box_follows_the_simulated_rotation)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const EngHeightmap map = make_ramp();
    EngPhysicsShape *terrain = eng_physics_shape_heightfield(w, &map);
    eng_physics_add_static_shape(w, terrain, NULL);
    const float half[3] = {0.5F, 0.3F, 0.4F};
    const float start[3] = {20.0F, 15.0F, 30.0F};
    const EngBody box = eng_physics_add_box(w, half, start, ENG_LAYER_DYNAMIC);
    ASSERT_NE(box, ENG_BODY_INVALID);
    for (int i = 0; i < 120; i++) {
        ASSERT_TRUE(eng_physics_step(w, DT));
    }
    float pos[3];
    float q[4];
    ASSERT_TRUE(eng_physics_body_position(w, box, pos));
    ASSERT_TRUE(eng_physics_body_rotation(w, box, q));
    ASSERT_LT(fabsf(q[3]), 0.9999F); /* la boîte est bien inclinée */

    Fixture f;
    ASSERT_TRUE(fixture_init(&f, 256));
    EngPhysicsDebugDraw o = all_layers();
    o.layer_mask = 1U << ENG_LAYER_DYNAMIC;
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    ASSERT_EQ(eng_debug_lines_count(&f.lines), 12U);
    /* chaque sommet dessiné doit être l'un des 8 coins pos + R * (±hx, ±hy, ±hz) */
    mat4 rot;
    versor quat = {q[0], q[1], q[2], q[3]};
    glm_quat_mat4(quat, rot);
    for (uint32_t i = 0; i < f.lines.depth_count; i++) {
        float best = 1e9F;
        for (int s = 0; s < 8; s++) {
            vec3 local = {(s & 1) != 0 ? half[0] : -half[0], (s & 2) != 0 ? half[1] : -half[1],
                          (s & 4) != 0 ? half[2] : -half[2]};
            vec3 world;
            glm_mat4_mulv3(rot, local, 0.0F, world);
            const float corner[3] = {pos[0] + world[0], pos[1] + world[1], pos[2] + world[2]};
            best = fminf(best, dist3(corner, f.lines.depth[i].position));
        }
        ASSERT_LT(best, 1e-3F);
    }
    eng_physics_shape_destroy(terrain);
    eng_arena_shutdown(&f.arena);
    eng_physics_destroy(w);
}

UTEST(physics_debug, capsule_spans_its_full_height_and_radius)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float c[3] = {1.0F, 4.0F, 2.0F};
    ASSERT_NE(eng_physics_add_capsule(w, 0.6F, 0.3F, c, ENG_LAYER_CHARACTER), ENG_BODY_INVALID);
    Fixture f;
    ASSERT_TRUE(fixture_init(&f, 1024));
    const EngPhysicsDebugDraw o = all_layers();
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    ASSERT_EQ(eng_debug_lines_count(&f.lines), (2U * 3U * ENG_DEBUG_CIRCLE_SEGMENTS) + 4U);
    float ymin = 1e9F;
    float ymax = -1e9F;
    float xmax = 0.0F;
    for (uint32_t i = 0; i < f.lines.depth_count; i++) {
        const float *p = f.lines.depth[i].position;
        ymin = fminf(ymin, p[1]);
        ymax = fmaxf(ymax, p[1]);
        xmax = fmaxf(xmax, fabsf(p[0] - c[0]));
        ASSERT_TRUE(same_color(f.lines.depth[i].color, ENG_DEBUG_CYAN));
    }
    ASSERT_NEAR(ymin, c[1] - 0.9F, 1e-4F); /* demi-cylindre 0,6 + rayon 0,3 */
    ASSERT_NEAR(ymax, c[1] + 0.9F, 1e-4F);
    ASSERT_NEAR(xmax, 0.3F, 1e-4F);
    eng_arena_shutdown(&f.arena);
    eng_physics_destroy(w);
}

UTEST(physics_debug, layer_mask_selects_bodies_and_each_layer_has_its_colour)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    for (int l = 0; l < ENG_LAYER_COUNT; l++) {
        const float p[3] = {(float)l * 5.0F, 3.0F, 0.0F};
        ASSERT_NE(eng_physics_add_sphere(w, 0.5F, p, (EngPhysicsLayer)l), ENG_BODY_INVALID);
    }
    Fixture f;
    ASSERT_TRUE(fixture_init(&f, 4096));
    EngPhysicsDebugDraw o = all_layers();
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), (uint32_t)ENG_LAYER_COUNT);
    /* chaque sommet porte la couleur de la couche du corps le plus proche (x = 5 l) */
    for (uint32_t i = 0; i < f.lines.depth_count; i++) {
        const int l = (int)lroundf(f.lines.depth[i].position[0] / 5.0F);
        ASSERT_TRUE(
            same_color(f.lines.depth[i].color, eng_physics_layer_color((EngPhysicsLayer)l)));
    }
    for (int l = 0; l < ENG_LAYER_COUNT; l++) {
        eng_debug_lines_clear(&f.lines);
        o.layer_mask = 1U << l;
        ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
        ASSERT_EQ(eng_debug_lines_count(&f.lines), 3U * ENG_DEBUG_CIRCLE_SEGMENTS);
        ASSERT_TRUE(
            same_color(f.lines.depth[0].color, eng_physics_layer_color((EngPhysicsLayer)l)));
    }
    eng_debug_lines_clear(&f.lines);
    o.layer_mask = 0U;
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 0U);
    ASSERT_EQ(eng_debug_lines_count(&f.lines), 0U);
    eng_arena_shutdown(&f.arena);
    eng_physics_destroy(w);
}

UTEST(physics_debug, sleeping_bodies_are_dimmed)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float ground_half[3] = {20.0F, 1.0F, 20.0F};
    const float ground[3] = {0.0F, -1.0F, 0.0F};
    ASSERT_NE(eng_physics_add_box(w, ground_half, ground, ENG_LAYER_STATIC), ENG_BODY_INVALID);
    const float p[3] = {0.0F, 0.5F, 0.0F};
    const EngBody ball = eng_physics_add_sphere(w, 0.5F, p, ENG_LAYER_DYNAMIC);
    Fixture f;
    ASSERT_TRUE(fixture_init(&f, 4096));
    EngPhysicsDebugDraw o = all_layers();
    o.layer_mask = 1U << ENG_LAYER_DYNAMIC;
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    ASSERT_TRUE(same_color(f.lines.depth[0].color, ENG_DEBUG_GREEN)); /* éveillée */
    for (int i = 0; i < 600 && eng_physics_body_is_active(w, ball); i++) {
        eng_physics_step(w, DT);
    }
    ASSERT_FALSE(eng_physics_body_is_active(w, ball));
    eng_debug_lines_clear(&f.lines);
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    ASSERT_EQ(f.lines.depth[0].color.r, 40U * 2U / 5U);
    ASSERT_EQ(f.lines.depth[0].color.g, 255U * 2U / 5U);
    ASSERT_EQ(f.lines.depth[0].color.b, 40U * 2U / 5U);
    ASSERT_EQ(f.lines.depth[0].color.a, 255U);
    eng_arena_shutdown(&f.arena);
    eng_physics_destroy(w);
}

UTEST(physics_debug, radius_keeps_only_nearby_bodies)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float near_pos[3] = {0.0F, 0.0F, 0.0F};
    const float far_pos[3] = {100.0F, 0.0F, 0.0F};
    eng_physics_add_sphere(w, 1.0F, near_pos, ENG_LAYER_STATIC);
    eng_physics_add_sphere(w, 1.0F, far_pos, ENG_LAYER_STATIC);
    Fixture f;
    ASSERT_TRUE(fixture_init(&f, 4096));
    EngPhysicsDebugDraw o = all_layers();
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 2U); /* rayon 0 : tout */
    o.radius = 10.0F;
    eng_debug_lines_clear(&f.lines);
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    o.radius = 200.0F;
    eng_debug_lines_clear(&f.lines);
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 2U);
    /* le foyer compte : à 98 m de la sphère lointaine (rayon 1) un rayon de 5 m ne l'atteint pas,
     * à 97,5 m... un rayon de 5 m la touche */
    o.focus[0] = 90.0F;
    o.radius = 5.0F;
    eng_debug_lines_clear(&f.lines);
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 0U);
    o.focus[0] = 94.5F;
    eng_debug_lines_clear(&f.lines);
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    eng_arena_shutdown(&f.arena);
    eng_physics_destroy(w);
}

UTEST(physics_debug, heightfield_window_lies_on_the_simulated_surface)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const EngHeightmap map = make_ramp();
    EngPhysicsShape *terrain = eng_physics_shape_heightfield(w, &map);
    eng_physics_add_static_shape(w, terrain, NULL);
    Fixture f;
    ASSERT_TRUE(fixture_init(&f, 32768));
    EngPhysicsDebugDraw o = all_layers();
    /* sans rayon : la boîte englobante seulement */
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    ASSERT_EQ(eng_debug_lines_count(&f.lines), 12U);
    /* fenêtre de 10 cellules autour de (32, 32) : 11 x 11 échantillons */
    o.focus[0] = 32.0F;
    o.focus[2] = 32.0F;
    o.radius = 5.0F;
    eng_debug_lines_clear(&f.lines);
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    ASSERT_EQ(eng_debug_lines_count(&f.lines), (10U * 11U) + (11U * 10U) + (10U * 10U));
    for (uint32_t i = 0; i < f.lines.depth_count; i++) {
        const float *p = f.lines.depth[i].position;
        ASSERT_GE(p[0], 27.0F - 1e-4F);
        ASSERT_LE(p[0], 37.0F + 1e-4F);
        ASSERT_GE(p[2], 27.0F - 1e-4F);
        ASSERT_LE(p[2], 37.0F + 1e-4F);
        ASSERT_NEAR(p[1], eng_heightmap_sample(&map, p[0], p[2]), 2e-3F);
        ASSERT_TRUE(same_color(f.lines.depth[i].color, ENG_DEBUG_GRAY));
    }
    /* au bord du terrain, la fenêtre est tronquée à l'échantillon 0 : 6 x 6 échantillons */
    o.focus[0] = 0.0F;
    o.focus[2] = 0.0F;
    eng_debug_lines_clear(&f.lines);
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    ASSERT_EQ(eng_debug_lines_count(&f.lines), (5U * 6U) + (6U * 5U) + (5U * 5U));
    /* un très grand rayon est plafonné à 64 cellules de côté (soit tout ce terrain de 64 cellules)
     */
    o.focus[0] = 32.0F;
    o.focus[2] = 32.0F;
    o.radius = 1000.0F;
    eng_debug_lines_clear(&f.lines);
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    ASSERT_EQ(eng_debug_lines_count(&f.lines), (64U * 65U) + (65U * 64U) + (64U * 64U));
    eng_physics_shape_destroy(terrain);
    eng_arena_shutdown(&f.arena);
    eng_physics_destroy(w);
}

/* La diagonale dessinée est celle de Jolt (a-d) : le milieu de chaque diagonale est à la hauteur
 * que Jolt donne au centre de la cellule. */
UTEST(physics_debug, heightfield_diagonal_matches_the_collision_surface)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    static uint16_t bumpy[N * N];
    uint32_t seed = 99U;
    for (size_t i = 0; i < (size_t)N * N; i++) {
        seed = (seed * 1664525U) + 1013904223U;
        bumpy[i] = (uint16_t)(20000U + ((seed >> 16) % 8000U));
    }
    const EngHeightmap map = {
        .size = N, .spacing = 1.0F, .altitude_min = 0.0F, .altitude_max = 20.0F, .samples = bumpy};
    EngPhysicsShape *terrain = eng_physics_shape_heightfield(w, &map);
    eng_physics_add_static_shape(w, terrain, NULL);
    Fixture f;
    ASSERT_TRUE(fixture_init(&f, 4096));
    EngPhysicsDebugDraw o = all_layers();
    o.focus[0] = 30.0F;
    o.focus[2] = 30.0F;
    o.radius = 3.0F;
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    uint32_t diagonals = 0;
    for (uint32_t i = 0; i + 1U < f.lines.depth_count; i += 2U) {
        const float *a = f.lines.depth[i].position;
        const float *b = f.lines.depth[i + 1U].position;
        if (fabsf(b[0] - a[0]) < 0.5F || fabsf(b[2] - a[2]) < 0.5F) {
            continue; /* arête, pas diagonale */
        }
        diagonals++;
        const float mx = (a[0] + b[0]) * 0.5F;
        const float mz = (a[2] + b[2]) * 0.5F;
        const float o3[3] = {mx, 100.0F, mz};
        const float d3[3] = {0.0F, -1.0F, 0.0F};
        EngRayHit hit;
        ASSERT_TRUE(eng_physics_raycast(w, o3, d3, 200.0F, ENG_LAYERS_SOLID, &hit));
        ASSERT_NEAR(hit.point[1], (a[1] + b[1]) * 0.5F, 2e-3F);
    }
    ASSERT_EQ(diagonals, 36U); /* 6 x 6 cellules */
    eng_physics_shape_destroy(terrain);
    eng_arena_shutdown(&f.arena);
    eng_physics_destroy(w);
}

UTEST(physics_debug, rotated_heightfield_is_drawn_in_world_space)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const EngHeightmap map = make_ramp();
    EngPhysicsShape *terrain = eng_physics_shape_heightfield(w, &map);
    /* rotation de 90 degrés autour de Y : (x, z) local -> monde (z, -x), plus un décalage */
    EngPhysicsPose pose = eng_physics_pose_identity();
    pose.rotation[1] = 0.70710678F;
    pose.rotation[3] = 0.70710678F;
    pose.position[0] = 100.0F;
    pose.position[1] = 5.0F;
    ASSERT_NE(eng_physics_add_static_shape(w, terrain, &pose), ENG_BODY_INVALID);
    Fixture f;
    ASSERT_TRUE(fixture_init(&f, 4096));
    EngPhysicsDebugDraw o = all_layers();
    o.focus[0] = 132.0F; /* local (32, 32) */
    o.focus[2] = -32.0F;
    o.radius = 4.0F;
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    ASSERT_EQ(eng_debug_lines_count(&f.lines), (8U * 9U) + (9U * 8U) + (8U * 8U));
    for (uint32_t i = 0; i < f.lines.depth_count; i++) {
        const float *p = f.lines.depth[i].position;
        const float lx = -p[2];
        const float lz = p[0] - 100.0F;
        ASSERT_GE(lx, 28.0F - 1e-3F);
        ASSERT_LE(lx, 36.0F + 1e-3F);
        ASSERT_GE(lz, 28.0F - 1e-3F);
        ASSERT_LE(lz, 36.0F + 1e-3F);
        ASSERT_NEAR(p[1], 5.0F + eng_heightmap_sample(&map, lx, lz), 2e-3F);
    }
    eng_physics_shape_destroy(terrain);
    eng_arena_shutdown(&f.arena);
    eng_physics_destroy(w);
}

UTEST(physics_debug, meshes_are_drawn_as_their_bounding_box)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float quad[12] = {0.0F,  0.0F, 0.0F,  0.0F,  0.0F, 10.0F,
                            10.0F, 0.0F, 10.0F, 10.0F, 0.0F, 0.0F};
    const uint32_t idx[6] = {0U, 1U, 2U, 0U, 2U, 3U};
    EngPhysicsShape *shape = eng_physics_shape_mesh(w, quad, 3U * sizeof(float), 4U, idx, 6U);
    ASSERT_TRUE(shape != NULL);
    EngPhysicsPose pose = eng_physics_pose_identity();
    pose.position[1] = 2.0F;
    ASSERT_NE(eng_physics_add_static_shape(w, shape, &pose), ENG_BODY_INVALID);
    Fixture f;
    ASSERT_TRUE(fixture_init(&f, 256));
    const EngPhysicsDebugDraw o = all_layers();
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    ASSERT_EQ(eng_debug_lines_count(&f.lines), 12U);
    for (uint32_t i = 0; i < f.lines.depth_count; i++) {
        const float *p = f.lines.depth[i].position;
        ASSERT_TRUE(fabsf(p[0]) < 0.1F || fabsf(p[0] - 10.0F) < 0.1F);
        ASSERT_TRUE(fabsf(p[2]) < 0.1F || fabsf(p[2] - 10.0F) < 0.1F);
        ASSERT_NEAR(p[1], 2.0F, 0.1F); /* Jolt élargit un peu les boîtes plates */
    }
    eng_physics_shape_destroy(shape);
    eng_arena_shutdown(&f.arena);
    eng_physics_destroy(w);
}

UTEST(physics_debug, invalid_arguments_draw_nothing)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float p[3] = {0.0F, 0.0F, 0.0F};
    eng_physics_add_sphere(w, 1.0F, p, ENG_LAYER_STATIC);
    Fixture f;
    ASSERT_TRUE(fixture_init(&f, 4096));
    const EngPhysicsDebugDraw ok = all_layers();
    ASSERT_EQ(eng_physics_debug_draw(NULL, &f.lines, &ok), 0U);
    ASSERT_EQ(eng_physics_debug_draw(w, NULL, &ok), 0U);
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, NULL), 0U);
    EngPhysicsDebugDraw bad = ok;
    bad.focus[1] = NAN;
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &bad), 0U);
    bad = ok;
    bad.radius = -1.0F;
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &bad), 0U);
    bad.radius = INFINITY;
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &bad), 0U);
    ASSERT_EQ(eng_debug_lines_count(&f.lines), 0U);
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &ok), 1U);
    eng_arena_shutdown(&f.arena);
    eng_physics_destroy(w);
}

UTEST(physics_debug, a_shape_that_does_not_fit_is_not_drawn_at_all)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    for (int i = 0; i < 3; i++) {
        const float p[3] = {(float)i * 5.0F, 0.0F, 0.0F};
        eng_physics_add_sphere(w, 1.0F, p, ENG_LAYER_STATIC);
    }
    Fixture f;
    ASSERT_TRUE(fixture_init(&f, 3U * ENG_DEBUG_CIRCLE_SEGMENTS)); /* une seule sphère tient */
    const EngPhysicsDebugDraw o = all_layers();
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    ASSERT_EQ(eng_debug_lines_count(&f.lines), 3U * ENG_DEBUG_CIRCLE_SEGMENTS);
    ASSERT_EQ(f.lines.dropped, 6U * ENG_DEBUG_CIRCLE_SEGMENTS);
    eng_arena_shutdown(&f.arena);
    eng_physics_destroy(w);
}

UTEST(physics_debug, removed_bodies_are_no_longer_drawn)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float p[3] = {0.0F, 0.0F, 0.0F};
    const EngBody a = eng_physics_add_sphere(w, 1.0F, p, ENG_LAYER_STATIC);
    const float q[3] = {9.0F, 0.0F, 0.0F};
    eng_physics_add_sphere(w, 1.0F, q, ENG_LAYER_STATIC);
    Fixture f;
    ASSERT_TRUE(fixture_init(&f, 4096));
    const EngPhysicsDebugDraw o = all_layers();
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 2U);
    eng_physics_remove_body(w, a);
    eng_debug_lines_clear(&f.lines);
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    for (uint32_t i = 0; i < f.lines.depth_count; i++) {
        ASSERT_GT(f.lines.depth[i].position[0], 7.0F);
    }
    eng_arena_shutdown(&f.arena);
    eng_physics_destroy(w);
}

/* Terrain incliné (rotation de 30 degrés autour de l'axe (1, 0, 1)) : la hauteur du foyer compte
 * pour retrouver sa position locale. */
UTEST(physics_debug, tilted_heightfield_window_is_centred_on_the_focus)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const EngHeightmap map = make_ramp();
    EngPhysicsShape *terrain = eng_physics_shape_heightfield(w, &map);
    EngPhysicsPose pose = eng_physics_pose_identity();
    const float angle = 0.5235988F; /* 30 degrés */
    const float s2 = sinf(angle * 0.5F) * 0.70710678F;
    pose.rotation[0] = s2;
    pose.rotation[2] = s2;
    pose.rotation[3] = cosf(angle * 0.5F);
    pose.position[0] = 10.0F;
    pose.position[1] = -3.0F;
    pose.position[2] = 7.0F;
    ASSERT_NE(eng_physics_add_static_shape(w, terrain, &pose), ENG_BODY_INVALID);
    mat4 rot;
    versor q = {pose.rotation[0], pose.rotation[1], pose.rotation[2], pose.rotation[3]};
    glm_quat_mat4(q, rot);
    /* le foyer est le point local (20, 6, 40), avec 6 m de hauteur locale */
    vec3 local_focus = {20.0F, 6.0F, 40.0F};
    vec3 world_focus;
    glm_mat4_mulv3(rot, local_focus, 1.0F, world_focus);
    Fixture f;
    ASSERT_TRUE(fixture_init(&f, 4096));
    EngPhysicsDebugDraw o = all_layers();
    o.focus[0] = world_focus[0] + pose.position[0];
    o.focus[1] = world_focus[1] + pose.position[1];
    o.focus[2] = world_focus[2] + pose.position[2];
    o.radius = 3.0F;
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    ASSERT_EQ(eng_debug_lines_count(&f.lines), (6U * 7U) + (7U * 6U) + (6U * 6U));
    mat4 inv;
    glm_mat4_transpose_to(rot, inv);
    for (uint32_t i = 0; i < f.lines.depth_count; i++) {
        const float *p = f.lines.depth[i].position;
        vec3 rel = {p[0] - pose.position[0], p[1] - pose.position[1], p[2] - pose.position[2]};
        vec3 local;
        glm_mat4_mulv3(inv, rel, 0.0F, local);
        ASSERT_GE(local[0], 17.0F - 1e-3F);
        ASSERT_LE(local[0], 23.0F + 1e-3F);
        ASSERT_GE(local[2], 37.0F - 1e-3F);
        ASSERT_LE(local[2], 43.0F + 1e-3F);
    }
    eng_physics_shape_destroy(terrain);
    eng_arena_shutdown(&f.arena);
    eng_physics_destroy(w);
}

/* Terrain au pas de 2 m (33 x 33 échantillons) : la fenêtre se compte en cellules, pas en mètres ;
 * elle est tronquée au dernier échantillon. */
UTEST(physics_debug, heightfield_window_counts_cells_not_metres)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    static uint16_t coarse[33 * 33];
    for (size_t i = 0; i < (size_t)33U * 33U; i++) {
        coarse[i] = 3000U;
    }
    const EngHeightmap map = {.size = 33U,
                              .spacing = 2.0F,
                              .altitude_min = 0.0F,
                              .altitude_max = 10.0F,
                              .samples = coarse};
    EngPhysicsShape *terrain = eng_physics_shape_heightfield(w, &map);
    eng_physics_add_static_shape(w, terrain, NULL);
    Fixture f;
    ASSERT_TRUE(fixture_init(&f, 4096));
    EngPhysicsDebugDraw o = all_layers();
    o.focus[0] = 32.0F; /* échantillon 16 */
    o.focus[2] = 32.0F;
    o.radius = 6.0F; /* 3 cellules */
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    ASSERT_EQ(eng_debug_lines_count(&f.lines), (6U * 7U) + (7U * 6U) + (6U * 6U));
    for (uint32_t i = 0; i < f.lines.depth_count; i++) {
        ASSERT_GE(f.lines.depth[i].position[0], 26.0F - 1e-4F);
        ASSERT_LE(f.lines.depth[i].position[0], 38.0F + 1e-4F);
    }
    /* au coin opposé (64, 64) : la fenêtre s'arrête au dernier échantillon (indice 32) */
    o.focus[0] = 64.0F;
    o.focus[2] = 64.0F;
    eng_debug_lines_clear(&f.lines);
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    ASSERT_EQ(eng_debug_lines_count(&f.lines), (3U * 4U) + (4U * 3U) + (3U * 3U));
    for (uint32_t i = 0; i < f.lines.depth_count; i++) {
        ASSERT_LE(f.lines.depth[i].position[0], 64.0F + 1e-4F);
        ASSERT_LE(f.lines.depth[i].position[2], 64.0F + 1e-4F);
    }
    /* foyer très haut au-dessus du terrain : il reste dessiné (la hauteur est ignorée) */
    o.focus[0] = 32.0F;
    o.focus[1] = 500.0F;
    o.focus[2] = 32.0F;
    eng_debug_lines_clear(&f.lines);
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    eng_physics_shape_destroy(terrain);
    eng_arena_shutdown(&f.arena);
    eng_physics_destroy(w);
}

UTEST(physics_debug, radius_test_is_three_dimensional_and_inclusive)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float half[3] = {1.0F, 1.0F, 1.0F};
    const float high[3] = {0.0F, 100.0F, 0.0F}; /* au-dessus du foyer */
    const float side[3] = {10.0F, 0.0F, 0.0F}; /* boîte dont la face la plus proche est en x = 9 */
    eng_physics_add_box(w, half, high, ENG_LAYER_STATIC);
    eng_physics_add_box(w, half, side, ENG_LAYER_STATIC);
    Fixture f;
    ASSERT_TRUE(fixture_init(&f, 4096));
    EngPhysicsDebugDraw o = all_layers();
    o.radius = 20.0F; /* la boîte du haut est à 99 m : la hauteur compte pour un corps ordinaire */
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    o.radius = 9.0F; /* distance exactement égale au rayon : incluse */
    eng_debug_lines_clear(&f.lines);
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    o.radius = 8.99F;
    eng_debug_lines_clear(&f.lines);
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 0U);
    eng_arena_shutdown(&f.arena);
    eng_physics_destroy(w);
}

UTEST(physics_debug, partially_fitting_capsules_and_terrains_do_not_count_as_drawn)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    const float c[3] = {0.0F, 2.0F, 0.0F};
    eng_physics_add_capsule(w, 0.6F, 0.3F, c, ENG_LAYER_CHARACTER);
    Fixture f;
    /* une capsule demande 196 lignes : avec 100, la première sphère tient, pas la suite */
    ASSERT_TRUE(fixture_init(&f, 100));
    const EngPhysicsDebugDraw o = all_layers();
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 0U);
    ASSERT_GT(f.lines.dropped, 0U);
    /* assez pour les deux sphères mais pas les 4 génératrices : 192 + 3 */
    eng_arena_shutdown(&f.arena);
    ASSERT_TRUE(fixture_init(&f, (2U * 3U * ENG_DEBUG_CIRCLE_SEGMENTS) + 3U));
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 0U);
    eng_arena_shutdown(&f.arena);

    const EngHeightmap map = make_ramp();
    EngPhysicsShape *terrain = eng_physics_shape_heightfield(w, &map);
    eng_physics_add_static_shape(w, terrain, NULL);
    ASSERT_TRUE(fixture_init(&f, 100));
    EngPhysicsDebugDraw t = all_layers();
    t.layer_mask = 1U << ENG_LAYER_STATIC;
    t.focus[0] = 32.0F;
    t.focus[2] = 32.0F;
    t.radius = 5.0F; /* 320 lignes demandées */
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &t), 0U);
    ASSERT_GT(f.lines.dropped, 0U);
    eng_physics_shape_destroy(terrain);
    eng_arena_shutdown(&f.arena);
    eng_physics_destroy(w);
}

/* Plafond de la fenêtre : un très grand rayon sur un terrain de 129 x 129 échantillons (128
 * cellules) ne dessine que 64 cellules de côté. */
UTEST(physics_debug, heightfield_window_is_capped_at_64_cells)
{
    NEED_PHYSICS();
    EngPhysics *w = eng_physics_create(NULL);
    static uint16_t big[129 * 129];
    for (size_t i = 0; i < (size_t)129 * 129; i++) {
        big[i] = 5000U;
    }
    const EngHeightmap map = {
        .size = 129U, .spacing = 1.0F, .altitude_min = 0.0F, .altitude_max = 10.0F, .samples = big};
    EngPhysicsShape *terrain = eng_physics_shape_heightfield(w, &map);
    eng_physics_add_static_shape(w, terrain, NULL);
    Fixture f;
    ASSERT_TRUE(fixture_init(&f, 32768));
    EngPhysicsDebugDraw o = all_layers();
    o.focus[0] = 64.0F;
    o.focus[2] = 64.0F;
    o.radius = 5000.0F;
    ASSERT_EQ(eng_physics_debug_draw(w, &f.lines, &o), 1U);
    ASSERT_EQ(eng_debug_lines_count(&f.lines), (64U * 65U) + (65U * 64U) + (64U * 64U));
    eng_physics_shape_destroy(terrain);
    eng_arena_shutdown(&f.arena);
    eng_physics_destroy(w);
}
