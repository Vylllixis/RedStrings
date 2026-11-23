/* Debug draw, côté CPU (M1-11) : accumulation des lignes, capacité, formes. Les valeurs attendues
 * sont calculées à la main (géométrie élémentaire) ; aucune dépendance au GPU. */
#include <math.h>
#include <string.h>

#include "engine/debug_lines.h"
#include "utest.h"

#define EPS 1e-4F

typedef struct Fixture {
    EngArena arena;
    EngDebugLines lines;
} Fixture;

static bool open_lines(Fixture *f, uint32_t max_lines)
{
    return eng_arena_init(&f->arena, "debug-lines-test", (size_t)1 << 20) &&
           eng_debug_lines_init(&f->lines, &f->arena, max_lines);
}

static void close_lines(Fixture *f)
{
    eng_arena_shutdown(&f->arena);
}

#define OPEN(n)                                                                                    \
    Fixture f;                                                                                     \
    ASSERT_TRUE(open_lines(&f, (n)))

static const float *start_of(const EngDebugVertex *list, uint32_t line)
{
    return list[(size_t)line * 2U].position;
}

static const float *end_of(const EngDebugVertex *list, uint32_t line)
{
    return list[(size_t)line * 2U + 1U].position;
}

static float dist(const float *a, const float *b)
{
    return sqrtf((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) +
                 (a[2] - b[2]) * (a[2] - b[2]));
}

static bool same_color(EngDebugColor a, EngDebugColor b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

/* Égalité EXACTE de deux points (les coordonnées copiées ou calculées à l'identique). */
static bool same_point(const float *a, const float *b)
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

static const vec3 ZERO = {0.0F, 0.0F, 0.0F};

UTEST(eng_debug_lines, a_vertex_is_16_bytes_with_the_position_first)
{
    /* le pipeline lit position (offset 0) et couleur (offset 12) : on fige cette disposition */
    EXPECT_EQ(sizeof(EngDebugVertex), (size_t)16);
    EXPECT_EQ(offsetof(EngDebugVertex, position), (size_t)0);
    EXPECT_EQ(offsetof(EngDebugVertex, color), (size_t)12);
    EXPECT_EQ(sizeof(EngDebugColor), (size_t)4);
}

UTEST(eng_debug_lines, init_validates_the_capacity_and_the_arena)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "dl", 4096));
    EngDebugLines lines;
    EXPECT_FALSE(eng_debug_lines_init(&lines, &arena, 0));
    EXPECT_FALSE(eng_debug_lines_init(&lines, &arena, ENG_DEBUG_MAX_LINES + 1U));
    EXPECT_EQ(arena.used, (size_t)0);
    /* 100 lignes × 2 listes × 32 octets = 6400 > 4096 : refusé, et l'arène est restituée */
    EXPECT_FALSE(eng_debug_lines_init(&lines, &arena, 100));
    EXPECT_EQ(arena.used, (size_t)0);
    EXPECT_EQ(lines.capacity, 0U);
    EXPECT_TRUE(eng_debug_lines_init(&lines, &arena, 64));
    EXPECT_EQ(lines.capacity, 64U);
    EXPECT_EQ(eng_debug_lines_count(&lines), 0U);
    eng_arena_shutdown(&arena);
}

UTEST(eng_debug_lines, a_line_stores_both_ends_and_its_color_in_the_current_list)
{
    OPEN(8);
    const vec3 a = {1.0F, 2.0F, 3.0F};
    const vec3 b = {-4.0F, 5.0F, -6.0F};
    EXPECT_TRUE(eng_debug_line(&f.lines, a, b, ENG_DEBUG_RED));
    ASSERT_EQ(eng_debug_lines_depth_count(&f.lines), 1U);
    EXPECT_EQ(eng_debug_lines_overlay_count(&f.lines), 0U);
    EXPECT_TRUE(same_point(start_of(f.lines.depth, 0), a));
    EXPECT_TRUE(same_point(end_of(f.lines.depth, 0), b));
    EXPECT_TRUE(same_color(f.lines.depth[0].color, ENG_DEBUG_RED));
    EXPECT_TRUE(same_color(f.lines.depth[1].color, ENG_DEBUG_RED));

    eng_debug_lines_set_overlay(&f.lines, true);
    EXPECT_TRUE(eng_debug_line(&f.lines, b, a, ENG_DEBUG_BLUE));
    EXPECT_EQ(eng_debug_lines_depth_count(&f.lines), 1U);
    ASSERT_EQ(eng_debug_lines_overlay_count(&f.lines), 1U);
    EXPECT_TRUE(same_point(start_of(f.lines.overlay, 0), b));
    EXPECT_TRUE(same_color(f.lines.overlay[1].color, ENG_DEBUG_BLUE));
    EXPECT_EQ(eng_debug_lines_count(&f.lines), 2U);

    eng_debug_lines_set_overlay(&f.lines, false);
    EXPECT_TRUE(eng_debug_line(&f.lines, a, b, ENG_DEBUG_GREEN));
    EXPECT_EQ(eng_debug_lines_depth_count(&f.lines), 2U);
    close_lines(&f);
}

UTEST(eng_debug_lines, non_finite_input_is_rejected_and_adds_nothing)
{
    OPEN(64);
    const float nan = NAN;
    const float inf = INFINITY;
    const vec3 ok = {1.0F, 1.0F, 1.0F};
    const vec3 bad_nan = {0.0F, nan, 0.0F};
    const vec3 bad_inf = {0.0F, 0.0F, -inf};
    EXPECT_FALSE(eng_debug_line(&f.lines, bad_nan, ok, ENG_DEBUG_RED));
    EXPECT_FALSE(eng_debug_line(&f.lines, ok, bad_inf, ENG_DEBUG_RED));
    EXPECT_FALSE(eng_debug_aabb(&f.lines, bad_nan, ok, ENG_DEBUG_RED));
    EXPECT_FALSE(eng_debug_aabb(&f.lines, ok, bad_inf, ENG_DEBUG_RED));
    EXPECT_FALSE(eng_debug_sphere(&f.lines, bad_nan, 1.0F, ENG_DEBUG_RED, 8));
    EXPECT_FALSE(eng_debug_sphere(&f.lines, ok, nan, ENG_DEBUG_RED, 8));
    EXPECT_FALSE(eng_debug_sphere(&f.lines, ok, inf, ENG_DEBUG_RED, 8));
    EXPECT_FALSE(eng_debug_circle(&f.lines, ok, bad_nan, ok, ENG_DEBUG_RED, 8));
    EXPECT_FALSE(eng_debug_circle(&f.lines, ok, ok, bad_inf, ENG_DEBUG_RED, 8));
    EXPECT_FALSE(eng_debug_arrow(&f.lines, bad_nan, ok, ENG_DEBUG_RED));
    EXPECT_FALSE(eng_debug_arrow(&f.lines, ok, bad_inf, ENG_DEBUG_RED));
    EXPECT_FALSE(eng_debug_grid(&f.lines, bad_nan, 2, 1.0F, ENG_DEBUG_RED, ENG_DEBUG_RED));
    mat4 m;
    glm_mat4_identity(m);
    m[3][1] = nan;
    EXPECT_FALSE(eng_debug_axes(&f.lines, m, 1.0F));
    EXPECT_FALSE(eng_debug_box(&f.lines, m, ok, ENG_DEBUG_RED));
    EXPECT_FALSE(eng_debug_frustum(&f.lines, m, ENG_DEBUG_RED));
    glm_mat4_identity(m);
    m[3][3] = nan; /* dernier coefficient */
    EXPECT_FALSE(eng_debug_axes(&f.lines, m, 1.0F));
    glm_mat4_identity(m);
    EXPECT_FALSE(eng_debug_axes(&f.lines, m, nan));
    EXPECT_EQ(f.lines.rejected, 17U);
    EXPECT_EQ(f.lines.dropped, 0U);
    EXPECT_EQ(eng_debug_lines_count(&f.lines), 0U);
    close_lines(&f);
}

UTEST(eng_debug_lines, a_full_list_drops_lines_and_the_other_list_is_independent)
{
    OPEN(4);
    for (int i = 0; i < 4; i++) {
        EXPECT_TRUE(eng_debug_line(&f.lines, ZERO, (float[3]){1, 0, 0}, ENG_DEBUG_RED));
    }
    EXPECT_EQ(eng_debug_lines_depth_count(&f.lines), 4U);
    EXPECT_FALSE(eng_debug_line(&f.lines, ZERO, (float[3]){2, 0, 0}, ENG_DEBUG_RED));
    EXPECT_EQ(f.lines.dropped, 1U);
    EXPECT_EQ(eng_debug_lines_depth_count(&f.lines), 4U);
    /* la liste superposée a sa propre capacité */
    eng_debug_lines_set_overlay(&f.lines, true);
    EXPECT_TRUE(eng_debug_line(&f.lines, ZERO, (float[3]){3, 0, 0}, ENG_DEBUG_RED));
    EXPECT_EQ(eng_debug_lines_overlay_count(&f.lines), 1U);
    /* la dernière ligne acceptée est bien la 4e, intacte */
    EXPECT_NEAR(end_of(f.lines.depth, 3)[0], 1.0F, EPS);
    close_lines(&f);
}

UTEST(eng_debug_lines, a_shape_is_added_whole_or_not_at_all)
{
    OPEN(20);
    for (int i = 0; i < 9; i++) { /* reste 11 places */
        EXPECT_TRUE(eng_debug_line(&f.lines, ZERO, (float[3]){1, 0, 0}, ENG_DEBUG_RED));
    }
    EXPECT_FALSE(eng_debug_aabb(&f.lines, ZERO, (float[3]){1, 1, 1}, ENG_DEBUG_RED)); /* 12 > 11 */
    EXPECT_EQ(eng_debug_lines_depth_count(&f.lines), 9U); /* pas de boîte à moitié dessinée */
    EXPECT_EQ(f.lines.dropped, 12U);
    EXPECT_FALSE(eng_debug_sphere(&f.lines, ZERO, 1.0F, ENG_DEBUG_RED, 4)); /* 12 > 11 */
    EXPECT_EQ(eng_debug_lines_depth_count(&f.lines), 9U);
    EXPECT_EQ(f.lines.dropped, 24U);
    /* exactement ce qui reste : accepté (3 cercles de 3 segments = 9 lignes, puis 2 libres) */
    EXPECT_TRUE(eng_debug_sphere(&f.lines, ZERO, 1.0F, ENG_DEBUG_RED, 3));
    EXPECT_EQ(eng_debug_lines_depth_count(&f.lines), 18U);
    EXPECT_TRUE(eng_debug_line(&f.lines, ZERO, (float[3]){1, 0, 0}, ENG_DEBUG_RED));
    EXPECT_TRUE(eng_debug_line(&f.lines, ZERO, (float[3]){1, 0, 0}, ENG_DEBUG_RED));
    EXPECT_EQ(eng_debug_lines_depth_count(&f.lines), 20U); /* liste exactement pleine */
    EXPECT_FALSE(eng_debug_line(&f.lines, ZERO, (float[3]){1, 0, 0}, ENG_DEBUG_RED));
    close_lines(&f);
}

UTEST(eng_debug_lines, clear_empties_the_lists_and_the_counters_but_keeps_the_mode)
{
    OPEN(2);
    eng_debug_lines_set_overlay(&f.lines, true);
    for (int i = 0; i < 3; i++) {
        (void)eng_debug_line(&f.lines, ZERO, (float[3]){1, 0, 0}, ENG_DEBUG_RED); /* 1 perdue */
    }
    (void)eng_debug_line(&f.lines, (float[3]){NAN, 0, 0}, ZERO, ENG_DEBUG_RED);
    EXPECT_EQ(f.lines.dropped, 1U);
    EXPECT_EQ(f.lines.rejected, 1U);
    eng_debug_lines_set_overlay(&f.lines, false);
    EXPECT_TRUE(eng_debug_line(&f.lines, ZERO, (float[3]){1, 0, 0}, ENG_DEBUG_RED));
    eng_debug_lines_set_overlay(&f.lines, true);
    eng_debug_lines_clear(&f.lines);
    EXPECT_EQ(eng_debug_lines_count(&f.lines), 0U);
    EXPECT_EQ(f.lines.dropped, 0U);
    EXPECT_EQ(f.lines.rejected, 0U);
    EXPECT_TRUE(f.lines.overlay_mode);
    EXPECT_EQ(f.lines.capacity, 2U);
    EXPECT_TRUE(eng_debug_line(&f.lines, ZERO, (float[3]){1, 0, 0}, ENG_DEBUG_RED));
    EXPECT_EQ(eng_debug_lines_overlay_count(&f.lines), 1U);
    EXPECT_EQ(eng_debug_lines_depth_count(&f.lines), 0U);
    close_lines(&f);
}

UTEST(eng_debug_lines, shapes_never_allocate_after_init)
{
    OPEN(4096);
    const size_t used = f.arena.used;
    mat4 m;
    glm_mat4_identity(m);
    for (int frame = 0; frame < 3; frame++) {
        eng_debug_lines_clear(&f.lines);
        (void)eng_debug_aabb(&f.lines, ZERO, (float[3]){1, 1, 1}, ENG_DEBUG_RED);
        (void)eng_debug_sphere(&f.lines, ZERO, 1.0F, ENG_DEBUG_RED, 0);
        (void)eng_debug_grid(&f.lines, ZERO, 20, 1.0F, ENG_DEBUG_GRAY, ENG_DEBUG_WHITE);
        (void)eng_debug_axes(&f.lines, m, 1.0F);
    }
    EXPECT_EQ(f.arena.used, used);
    close_lines(&f);
}

/* ── Boîtes ── */

/* Chaque ligne de la liste joint deux coins de la boîte [min, max] et suit un seul axe. Chaque coin
 * est l'extrémité de 3 arêtes exactement. */
static bool box_edges_ok(const EngDebugVertex *list, uint32_t first_line, const vec3 min,
                         const vec3 max)
{
    uint32_t corner_uses[8] = {0};
    for (uint32_t i = 0; i < 12; i++) {
        const float *ends[2] = {start_of(list, first_line + i), end_of(list, first_line + i)};
        uint32_t index[2];
        for (int e = 0; e < 2; e++) {
            index[e] = 0;
            for (int axis = 0; axis < 3; axis++) {
                const bool high = fabsf(ends[e][axis] - max[axis]) < EPS;
                const bool low = fabsf(ends[e][axis] - min[axis]) < EPS;
                if (!high && !low) {
                    return false;
                }
                index[e] |= (high && !low ? 1U : 0U) << axis;
            }
            corner_uses[index[e]]++;
        }
        const uint32_t diff = index[0] ^ index[1];
        if (diff != 1U && diff != 2U && diff != 4U) { /* un seul axe change */
            return false;
        }
    }
    for (int c = 0; c < 8; c++) {
        if (corner_uses[c] != 3U) {
            return false;
        }
    }
    return true;
}

UTEST(eng_debug_lines, an_aabb_is_twelve_edges_between_its_eight_corners)
{
    OPEN(32);
    const vec3 min = {-1.0F, 2.0F, -3.0F};
    const vec3 max = {4.0F, 5.0F, 6.0F};
    EXPECT_TRUE(eng_debug_aabb(&f.lines, min, max, ENG_DEBUG_YELLOW));
    ASSERT_EQ(eng_debug_lines_depth_count(&f.lines), 12U);
    EXPECT_TRUE(box_edges_ok(f.lines.depth, 0, min, max));
    for (uint32_t i = 0; i < 24; i++) {
        EXPECT_TRUE(same_color(f.lines.depth[i].color, ENG_DEBUG_YELLOW));
    }
    close_lines(&f);
}

UTEST(eng_debug_lines, an_aabb_given_in_the_wrong_order_is_the_same_box)
{
    OPEN(32);
    EXPECT_TRUE(
        eng_debug_aabb(&f.lines, (float[3]){4, 5, 6}, (float[3]){-1, 2, -3}, ENG_DEBUG_YELLOW));
    ASSERT_EQ(eng_debug_lines_depth_count(&f.lines), 12U);
    EXPECT_TRUE(box_edges_ok(f.lines.depth, 0, (float[3]){-1, 2, -3}, (float[3]){4, 5, 6}));
    close_lines(&f);
}

UTEST(eng_debug_lines, a_box_with_identity_is_centered_on_the_origin)
{
    OPEN(32);
    mat4 m;
    glm_mat4_identity(m);
    EXPECT_TRUE(eng_debug_box(&f.lines, m, (float[3]){1.0F, 2.0F, 3.0F}, ENG_DEBUG_CYAN));
    ASSERT_EQ(eng_debug_lines_depth_count(&f.lines), 12U);
    EXPECT_TRUE(box_edges_ok(f.lines.depth, 0, (float[3]){-1, -2, -3}, (float[3]){1, 2, 3}));
    close_lines(&f);
}

UTEST(eng_debug_lines, a_box_follows_translation_rotation_and_scale)
{
    OPEN(64);
    /* rotation de 90° autour de Y (X -> -Z, Z -> X), puis translation (10, 0, 0) */
    mat4 m;
    glm_mat4_identity(m);
    glm_translate(m, (float[3]){10.0F, 0.0F, 0.0F});
    glm_rotate(m, GLM_PI_2f, (float[3]){0.0F, 1.0F, 0.0F});
    EXPECT_TRUE(eng_debug_box(&f.lines, m, (float[3]){1.0F, 2.0F, 3.0F}, ENG_DEBUG_CYAN));
    /* la demi-largeur X (1) devient -Z, la demi-largeur Z (3) devient X : boîte monde
     * x in [7, 13], y in [-2, 2], z in [-1, 1] */
    EXPECT_TRUE(box_edges_ok(f.lines.depth, 0, (float[3]){7, -2, -1}, (float[3]){13, 2, 1}));
    /* échelle (2, 1, 1) composée avec half (1, 1, 1) : x in [-2, 2] */
    glm_mat4_identity(m);
    glm_scale(m, (float[3]){2.0F, 1.0F, 1.0F});
    EXPECT_TRUE(eng_debug_box(&f.lines, m, (float[3]){1.0F, 1.0F, 1.0F}, ENG_DEBUG_CYAN));
    EXPECT_TRUE(box_edges_ok(f.lines.depth, 12, (float[3]){-2, -1, -1}, (float[3]){2, 1, 1}));
    close_lines(&f);
}

UTEST(eng_debug_lines, a_box_with_a_negative_half_extent_is_rejected)
{
    OPEN(32);
    mat4 m;
    glm_mat4_identity(m);
    EXPECT_FALSE(eng_debug_box(&f.lines, m, (float[3]){1.0F, -0.5F, 1.0F}, ENG_DEBUG_CYAN));
    EXPECT_FALSE(eng_debug_box(&f.lines, m, (float[3]){-1.0F, 1.0F, 1.0F}, ENG_DEBUG_CYAN));
    EXPECT_FALSE(eng_debug_box(&f.lines, m, (float[3]){1.0F, 1.0F, -1.0F}, ENG_DEBUG_CYAN));
    EXPECT_EQ(f.lines.rejected, 3U);
    EXPECT_TRUE(eng_debug_box(&f.lines, m, (float[3]){0.0F, 0.0F, 0.0F}, ENG_DEBUG_CYAN));
    EXPECT_EQ(eng_debug_lines_depth_count(&f.lines), 12U);
    close_lines(&f);
}

/* ── Cercles et sphères ── */

UTEST(eng_debug_lines, a_circle_is_a_closed_loop_at_the_given_radius_in_its_plane)
{
    OPEN(64);
    const vec3 center = {1.0F, 2.0F, 3.0F};
    EXPECT_TRUE(eng_debug_circle(&f.lines, center, (float[3]){2, 0, 0}, (float[3]){0, 0, 2},
                                 ENG_DEBUG_GREEN, 12));
    ASSERT_EQ(eng_debug_lines_depth_count(&f.lines), 12U);
    for (uint32_t i = 0; i < 12; i++) {
        EXPECT_NEAR(dist(start_of(f.lines.depth, i), center), 2.0F, EPS);
        EXPECT_NEAR(dist(end_of(f.lines.depth, i), center), 2.0F, EPS);
        EXPECT_NEAR(start_of(f.lines.depth, i)[1], 2.0F, EPS); /* plan y = 2 */
        if (i > 0) {                                           /* continuité */
            EXPECT_TRUE(same_point(start_of(f.lines.depth, i), end_of(f.lines.depth, i - 1)));
        }
    }
    /* fermé exactement : le dernier point est le premier */
    EXPECT_TRUE(same_point(end_of(f.lines.depth, 11), start_of(f.lines.depth, 0)));
    /* premier point : center + u ; deuxième : angle 30° */
    EXPECT_NEAR(start_of(f.lines.depth, 0)[0], 3.0F, EPS);
    EXPECT_NEAR(end_of(f.lines.depth, 0)[0], 1.0F + 2.0F * cosf(GLM_PIf / 6.0F), EPS);
    EXPECT_NEAR(end_of(f.lines.depth, 0)[2], 3.0F + 2.0F * sinf(GLM_PIf / 6.0F), EPS);
    /* le point d'angle 90° est center + v */
    EXPECT_NEAR(end_of(f.lines.depth, 2)[0], 1.0F, EPS);
    EXPECT_NEAR(end_of(f.lines.depth, 2)[2], 5.0F, EPS);
    close_lines(&f);
}

UTEST(eng_debug_lines, circle_segments_default_and_are_clamped)
{
    OPEN(1024);
    const vec3 u = {1, 0, 0};
    const vec3 v = {0, 1, 0};
    EXPECT_TRUE(eng_debug_circle(&f.lines, ZERO, u, v, ENG_DEBUG_RED, 0));
    EXPECT_EQ(eng_debug_lines_depth_count(&f.lines), ENG_DEBUG_CIRCLE_SEGMENTS);
    eng_debug_lines_clear(&f.lines);
    EXPECT_TRUE(eng_debug_circle(&f.lines, ZERO, u, v, ENG_DEBUG_RED, 1));
    EXPECT_EQ(eng_debug_lines_depth_count(&f.lines), ENG_DEBUG_MIN_SEGMENTS);
    eng_debug_lines_clear(&f.lines);
    EXPECT_TRUE(eng_debug_circle(&f.lines, ZERO, u, v, ENG_DEBUG_RED, 2));
    EXPECT_EQ(eng_debug_lines_depth_count(&f.lines), ENG_DEBUG_MIN_SEGMENTS);
    eng_debug_lines_clear(&f.lines);
    EXPECT_TRUE(eng_debug_circle(&f.lines, ZERO, u, v, ENG_DEBUG_RED, 3));
    EXPECT_EQ(eng_debug_lines_depth_count(&f.lines), 3U);
    eng_debug_lines_clear(&f.lines);
    EXPECT_TRUE(eng_debug_circle(&f.lines, ZERO, u, v, ENG_DEBUG_RED, 256));
    EXPECT_EQ(eng_debug_lines_depth_count(&f.lines), 256U);
    eng_debug_lines_clear(&f.lines);
    EXPECT_TRUE(eng_debug_circle(&f.lines, ZERO, u, v, ENG_DEBUG_RED, 100000));
    EXPECT_EQ(eng_debug_lines_depth_count(&f.lines), ENG_DEBUG_MAX_SEGMENTS);
    close_lines(&f);
}

UTEST(eng_debug_lines, a_sphere_is_three_perpendicular_circles_of_its_radius)
{
    OPEN(64);
    const vec3 center = {0.0F, 1.0F, 0.0F};
    EXPECT_TRUE(eng_debug_sphere(&f.lines, center, 2.5F, ENG_DEBUG_MAGENTA, 8));
    ASSERT_EQ(eng_debug_lines_depth_count(&f.lines), 24U);
    for (uint32_t i = 0; i < 24; i++) {
        EXPECT_NEAR(dist(start_of(f.lines.depth, i), center), 2.5F, EPS);
        EXPECT_NEAR(dist(end_of(f.lines.depth, i), center), 2.5F, EPS);
    }
    /* chaque cercle reste dans un plan passant par le centre : XY (z = 0), YZ (x = 0), XZ (y = 1)
     */
    const int fixed_axis[3] = {2, 0, 1};
    const float fixed_value[3] = {0.0F, 0.0F, 1.0F};
    for (uint32_t circle = 0; circle < 3; circle++) {
        for (uint32_t i = 0; i < 8; i++) {
            EXPECT_NEAR(start_of(f.lines.depth, circle * 8U + i)[fixed_axis[circle]],
                        fixed_value[circle], EPS);
        }
    }
    close_lines(&f);
}

UTEST(eng_debug_lines, a_sphere_of_radius_zero_is_allowed_and_a_negative_one_is_not)
{
    OPEN(64);
    EXPECT_TRUE(eng_debug_sphere(&f.lines, ZERO, 0.0F, ENG_DEBUG_RED, 4));
    EXPECT_EQ(eng_debug_lines_depth_count(&f.lines), 12U);
    EXPECT_FALSE(eng_debug_sphere(&f.lines, ZERO, -0.001F, ENG_DEBUG_RED, 4));
    EXPECT_EQ(f.lines.rejected, 1U);
    EXPECT_EQ(eng_debug_lines_depth_count(&f.lines), 12U);
    close_lines(&f);
}

/* ── Repère, grille, flèche, frustum ── */

UTEST(eng_debug_lines, axes_are_red_green_blue_from_the_origin_of_the_transform)
{
    OPEN(8);
    mat4 m;
    glm_mat4_identity(m);
    glm_translate(m, (float[3]){1.0F, 2.0F, 3.0F});
    glm_rotate(m, GLM_PI_2f, (float[3]){0.0F, 0.0F, 1.0F}); /* X -> Y, Y -> -X */
    EXPECT_TRUE(eng_debug_axes(&f.lines, m, 2.0F));
    ASSERT_EQ(eng_debug_lines_depth_count(&f.lines), 3U);
    for (uint32_t i = 0; i < 3; i++) {
        EXPECT_NEAR(start_of(f.lines.depth, i)[0], 1.0F, EPS);
        EXPECT_NEAR(start_of(f.lines.depth, i)[1], 2.0F, EPS);
        EXPECT_NEAR(start_of(f.lines.depth, i)[2], 3.0F, EPS);
    }
    EXPECT_TRUE(same_color(f.lines.depth[0].color, ENG_DEBUG_RED));
    EXPECT_TRUE(same_color(f.lines.depth[2].color, ENG_DEBUG_GREEN));
    EXPECT_TRUE(same_color(f.lines.depth[4].color, ENG_DEBUG_BLUE));
    EXPECT_NEAR(end_of(f.lines.depth, 0)[0], 1.0F, EPS); /* X monde -> +Y : (1, 4, 3) */
    EXPECT_NEAR(end_of(f.lines.depth, 0)[1], 4.0F, EPS);
    EXPECT_NEAR(end_of(f.lines.depth, 1)[0], -1.0F, EPS); /* Y -> -X : (-1, 2, 3) */
    EXPECT_NEAR(end_of(f.lines.depth, 1)[1], 2.0F, EPS);
    EXPECT_NEAR(end_of(f.lines.depth, 2)[2], 5.0F, EPS); /* Z reste Z : (1, 2, 5) */
    close_lines(&f);
}

UTEST(eng_debug_lines, a_grid_has_lines_both_ways_with_major_lines_every_ten_cells)
{
    OPEN(128);
    const vec3 center = {5.0F, 1.5F, -2.0F};
    EXPECT_TRUE(eng_debug_grid(&f.lines, center, 12, 0.5F, ENG_DEBUG_GRAY, ENG_DEBUG_WHITE));
    ASSERT_EQ(eng_debug_lines_depth_count(&f.lines), 50U); /* 2 × (2 × 12 + 1) */
    uint32_t major = 0;
    for (uint32_t i = 0; i < 50; i++) {
        EXPECT_NEAR(start_of(f.lines.depth, i)[1], 1.5F, EPS); /* plan horizontal */
        EXPECT_NEAR(end_of(f.lines.depth, i)[1], 1.5F, EPS);
        major += same_color(f.lines.depth[(size_t)i * 2U].color, ENG_DEBUG_WHITE) ? 1U : 0U;
        /* chaque ligne est parallèle à X ou à Z, de longueur 2 × 12 × 0,5 = 12 */
        EXPECT_NEAR(dist(start_of(f.lines.depth, i), end_of(f.lines.depth, i)), 12.0F, EPS);
    }
    EXPECT_EQ(major, 6U); /* i = -10, 0, 10 dans chaque sens */
    /* première paire (i = -12) : ligne parallèle à X à z = -2 - 6, puis parallèle à Z à x = 5 - 6
     */
    EXPECT_NEAR(start_of(f.lines.depth, 0)[0], -1.0F, EPS);
    EXPECT_NEAR(start_of(f.lines.depth, 0)[2], -8.0F, EPS);
    EXPECT_NEAR(end_of(f.lines.depth, 0)[0], 11.0F, EPS);
    EXPECT_NEAR(start_of(f.lines.depth, 1)[0], -1.0F, EPS);
    EXPECT_NEAR(start_of(f.lines.depth, 1)[2], -8.0F, EPS);
    EXPECT_NEAR(end_of(f.lines.depth, 1)[2], 4.0F, EPS);
    /* la ligne du milieu (i = 0, indices 24 et 25) passe par le centre et est majeure */
    EXPECT_NEAR(start_of(f.lines.depth, 24)[2], -2.0F, EPS);
    EXPECT_TRUE(same_color(f.lines.depth[48].color, ENG_DEBUG_WHITE));
    EXPECT_TRUE(same_color(f.lines.depth[50].color, ENG_DEBUG_WHITE));
    EXPECT_TRUE(same_color(f.lines.depth[2].color, ENG_DEBUG_GRAY)); /* i = -12 : mineure */
    close_lines(&f);
}

UTEST(eng_debug_lines, grid_parameters_are_validated)
{
    OPEN(4096);
    EXPECT_FALSE(eng_debug_grid(&f.lines, ZERO, 4, 0.0F, ENG_DEBUG_GRAY, ENG_DEBUG_WHITE));
    EXPECT_FALSE(eng_debug_grid(&f.lines, ZERO, 4, -1.0F, ENG_DEBUG_GRAY, ENG_DEBUG_WHITE));
    EXPECT_FALSE(eng_debug_grid(&f.lines, ZERO, 4, NAN, ENG_DEBUG_GRAY, ENG_DEBUG_WHITE));
    EXPECT_FALSE(eng_debug_grid(&f.lines, ZERO, ENG_DEBUG_MAX_GRID_CELLS + 1U, 1.0F, ENG_DEBUG_GRAY,
                                ENG_DEBUG_WHITE));
    EXPECT_EQ(f.lines.rejected, 4U);
    EXPECT_EQ(eng_debug_lines_depth_count(&f.lines), 0U);
    /* cells = 0 : une croix, majeure */
    EXPECT_TRUE(eng_debug_grid(&f.lines, ZERO, 0, 1.0F, ENG_DEBUG_GRAY, ENG_DEBUG_WHITE));
    EXPECT_EQ(eng_debug_lines_depth_count(&f.lines), 2U);
    EXPECT_TRUE(same_color(f.lines.depth[0].color, ENG_DEBUG_WHITE));
    /* le maximum tient : 2 × (2 × 1024 + 1) = 4098 > 4096 : perdu en entier */
    eng_debug_lines_clear(&f.lines);
    EXPECT_FALSE(eng_debug_grid(&f.lines, ZERO, ENG_DEBUG_MAX_GRID_CELLS, 1.0F, ENG_DEBUG_GRAY,
                                ENG_DEBUG_WHITE));
    EXPECT_EQ(f.lines.dropped, 4098U);
    close_lines(&f);
}

/* Barbes d'une flèche : les 4 lignes après la hampe, toutes issues de la pointe, reculées de 15 %
 * de la longueur, écartées de 7,5 %, et deux à deux opposées. */
static bool arrow_barbs_ok(const EngDebugVertex *list, const vec3 from, const vec3 to)
{
    vec3 dir = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
    const float length = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    for (int a = 0; a < 3; a++) {
        dir[a] /= length;
    }
    const float head = length * 0.15F;
    float perp_sum[3] = {0, 0, 0};
    for (uint32_t i = 1; i <= 4; i++) {
        if (dist(start_of(list, i), to) > EPS) {
            return false;
        }
        float back[3];
        for (int a = 0; a < 3; a++) {
            back[a] = end_of(list, i)[a] - to[a];
        }
        const float along = back[0] * dir[0] + back[1] * dir[1] + back[2] * dir[2];
        float perp[3] = {back[0] - along * dir[0], back[1] - along * dir[1],
                         back[2] - along * dir[2]};
        const float spread = sqrtf(perp[0] * perp[0] + perp[1] * perp[1] + perp[2] * perp[2]);
        if (fabsf(along + head) > EPS || fabsf(spread - head * 0.5F) > EPS) {
            return false;
        }
        for (int a = 0; a < 3; a++) {
            perp_sum[a] += perp[a];
        }
    }
    return fabsf(perp_sum[0]) < EPS && fabsf(perp_sum[1]) < EPS && fabsf(perp_sum[2]) < EPS;
}

UTEST(eng_debug_lines, an_arrow_is_a_line_with_four_barbs_at_the_tip)
{
    OPEN(32);
    const vec3 from = {1.0F, 2.0F, 3.0F};
    const vec3 to = {5.0F, 2.0F, 6.0F}; /* longueur 5 */
    EXPECT_TRUE(eng_debug_arrow(&f.lines, from, to, ENG_DEBUG_GREEN));
    ASSERT_EQ(eng_debug_lines_depth_count(&f.lines), 5U);
    EXPECT_TRUE(same_point(start_of(f.lines.depth, 0), from));
    EXPECT_TRUE(same_point(end_of(f.lines.depth, 0), to));
    EXPECT_TRUE(arrow_barbs_ok(f.lines.depth, from, to));
    close_lines(&f);
}

UTEST(eng_debug_lines, a_vertical_arrow_still_gets_proper_barbs)
{
    OPEN(32);
    const vec3 from = {0.0F, 0.0F, 0.0F};
    EXPECT_TRUE(eng_debug_arrow(&f.lines, from, (float[3]){0, 4, 0}, ENG_DEBUG_GREEN));
    EXPECT_TRUE(arrow_barbs_ok(f.lines.depth, from, (float[3]){0, 4, 0}));
    eng_debug_lines_clear(&f.lines);
    EXPECT_TRUE(eng_debug_arrow(&f.lines, (float[3]){0, 4, 0}, from, ENG_DEBUG_GREEN));
    EXPECT_TRUE(arrow_barbs_ok(f.lines.depth, (float[3]){0, 4, 0}, from));
    close_lines(&f);
}

UTEST(eng_debug_lines, a_zero_length_arrow_is_just_a_line)
{
    OPEN(32);
    EXPECT_TRUE(
        eng_debug_arrow(&f.lines, (float[3]){1, 1, 1}, (float[3]){1, 1, 1}, ENG_DEBUG_GREEN));
    EXPECT_EQ(eng_debug_lines_depth_count(&f.lines), 1U);
    close_lines(&f);
}

UTEST(eng_debug_lines, a_frustum_has_its_near_and_far_corners_where_the_projection_says)
{
    OPEN(32);
    mat4 view;
    mat4 proj;
    mat4 vp;
    eng_look_at((float[3]){0, 0, 0}, (float[3]){0, 0, -1}, (float[3]){0, 1, 0}, view);
    eng_perspective(GLM_PI_2f, 1.0F, 1.0F, 10.0F, proj); /* fov 90°, aspect 1 */
    glm_mat4_mul(proj, view, vp);
    EXPECT_TRUE(eng_debug_frustum(&f.lines, vp, ENG_DEBUG_WHITE));
    ASSERT_EQ(eng_debug_lines_depth_count(&f.lines), 12U);
    /* proche : (±1, ±1, -1) ; loin : (±10, ±10, -10) : tous les coins sont l'un ou l'autre */
    uint32_t near_ends = 0;
    uint32_t far_ends = 0;
    for (uint32_t i = 0; i < 24; i++) {
        const float *p = f.lines.depth[i].position;
        if (fabsf(p[2] + 1.0F) < 1e-3F) {
            near_ends++;
            EXPECT_NEAR(fabsf(p[0]), 1.0F, 1e-3F);
            EXPECT_NEAR(fabsf(p[1]), 1.0F, 1e-3F);
        } else {
            far_ends++;
            EXPECT_NEAR(p[2], -10.0F, 1e-2F);
            EXPECT_NEAR(fabsf(p[0]), 10.0F, 1e-2F);
            EXPECT_NEAR(fabsf(p[1]), 10.0F, 1e-2F);
        }
    }
    EXPECT_EQ(near_ends, 12U); /* 4 coins × 3 arêtes */
    EXPECT_EQ(far_ends, 12U);
    close_lines(&f);
}

UTEST(eng_debug_lines, a_frustum_of_a_singular_matrix_is_rejected)
{
    OPEN(32);
    mat4 zero;
    glm_mat4_zero(zero);
    EXPECT_FALSE(eng_debug_frustum(&f.lines, zero, ENG_DEBUG_WHITE));
    EXPECT_EQ(f.lines.rejected, 1U);
    EXPECT_EQ(eng_debug_lines_depth_count(&f.lines), 0U);
    close_lines(&f);
}
