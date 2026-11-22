/* Débogueur d'animation (M4-16) : lecteur (pause, pas, vitesse, boucle), réglages ramenés dans leur
 * plage, dessin (os, repos, axes, trajectoire). Chaîne de 3 articulations et clip de 1 s faits à la
 * main : l'articulation 1 tourne de 0 à 90° autour de Z. */
#include <math.h>
#include <string.h>

#include "engine/anim_debug.h"
#include "utest.h"

#define H 0.70710678F

static EngJoint g_joints[3] = {
    {-1, {0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}, "r"},
    {0, {0, 1, 0}, {0, 0, 0, 1}, {1, 1, 1}, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -1, 0, 1}, "a"},
    {1, {0, 1, 0}, {0, 0, 0, 1}, {1, 1, 1}, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -2, 0, 1}, "b"},
};
static EngAnimChannel g_channels[1] = {{1, ENG_ANIM_ROTATION, 2, 0}};
static float g_times[2] = {0.0F, 1.0F};
static float g_values[8] = {0, 0, 0, 1, 0, 0, H, H};
static uint32_t g_first[1] = {0};

static EngSkeleton skeleton(void)
{
    return (EngSkeleton){.joint_count = 3, .joints = g_joints};
}

static EngAnimClip clip(void)
{
    return (EngAnimClip){.duration = 1.0F,
                         .channel_count = 1,
                         .channels = g_channels,
                         .key_total = 2,
                         .times = g_times,
                         .values = g_values,
                         .value_first = g_first,
                         .skeleton_path = "p.skel"};
}

typedef struct Fx {
    EngArena arena;
    EngSkeleton sk;
    EngAnimClip clip;
    EngAnimDebug dbg;
} Fx;

static bool fx_init(Fx *f)
{
    f->sk = skeleton();
    f->clip = clip();
    return eng_arena_init(&f->arena, "anim-debug", (size_t)1 << 16) &&
           eng_anim_debug_init(&f->dbg, &f->arena, &f->sk, &f->clip);
}

UTEST(anim_debug, init_sets_the_defaults_and_evaluates_the_first_frame)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EXPECT_NEAR(f.dbg.time, 0.0F, 1e-7F);
    EXPECT_NEAR(f.dbg.speed, 1.0F, 1e-7F);
    EXPECT_TRUE(f.dbg.loop);
    EXPECT_FALSE(f.dbg.paused);
    EXPECT_EQ(f.dbg.selected, -1);
    EXPECT_NEAR(f.dbg.world[2][3][1], 2.0F, 1e-6F); /* repos : (0, 2, 0) */
    EXPECT_EQ(eng_anim_debug_frame(&f.dbg), 0U);
    EXPECT_EQ(eng_anim_debug_frame_count(&f.dbg), 31U); /* 1 s à 30 Hz : images 0 à 30 */
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_debug, init_refuses_bad_arguments_and_a_clip_for_another_skeleton)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngAnimDebug d;
    EXPECT_FALSE(eng_anim_debug_init(NULL, &f.arena, &f.sk, &f.clip));
    EXPECT_FALSE(eng_anim_debug_init(&d, NULL, &f.sk, &f.clip));
    EXPECT_FALSE(eng_anim_debug_init(&d, &f.arena, NULL, &f.clip));
    EXPECT_FALSE(eng_anim_debug_init(&d, &f.arena, &f.sk, NULL));
    EngAnimChannel far[1] = {
        {3, ENG_ANIM_ROTATION, 2, 0}}; /* le squelette a les articulations 0-2 */
    EngAnimClip other = f.clip;
    other.channels = far;
    EXPECT_FALSE(eng_anim_debug_init(&d, &f.arena, &f.sk, &other));
    EXPECT_TRUE(d.world == NULL);
    EngAnimClip bad = f.clip;
    bad.duration = NAN;
    EXPECT_FALSE(eng_anim_debug_init(&d, &f.arena, &f.sk, &bad));
    bad.duration = -1.0F;
    EXPECT_FALSE(eng_anim_debug_init(&d, &f.arena, &f.sk, &bad));
    /* mémoire insuffisante : l'arène est restituée */
    EngArena tiny;
    ASSERT_TRUE(eng_arena_init(&tiny, "tiny", 128));
    EXPECT_FALSE(eng_anim_debug_init(&d, &tiny, &f.sk, &f.clip));
    EXPECT_EQ(tiny.used, 0U);
    eng_arena_shutdown(&tiny);
    EXPECT_FALSE(eng_anim_debug_update(NULL, 0.1F));
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_debug, time_advances_by_dt_times_speed_and_pause_stops_it)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    ASSERT_TRUE(eng_anim_debug_update(&f.dbg, 0.25F));
    EXPECT_NEAR(f.dbg.time, 0.25F, 1e-6F);
    f.dbg.speed = 2.0F;
    ASSERT_TRUE(eng_anim_debug_update(&f.dbg, 0.125F));
    EXPECT_NEAR(f.dbg.time, 0.5F, 1e-6F);
    f.dbg.paused = true;
    ASSERT_TRUE(eng_anim_debug_update(&f.dbg, 10.0F));
    EXPECT_NEAR(f.dbg.time, 0.5F, 1e-7F);
    f.dbg.paused = false;
    ASSERT_TRUE(eng_anim_debug_update(&f.dbg, -1.0F)); /* dt négatif : ignoré */
    EXPECT_NEAR(f.dbg.time, 0.5F, 1e-7F);
    ASSERT_TRUE(eng_anim_debug_update(&f.dbg, NAN));
    EXPECT_NEAR(f.dbg.time, 0.5F, 1e-7F);
    f.dbg.speed = 0.0F;
    ASSERT_TRUE(eng_anim_debug_update(&f.dbg, 1.0F));
    EXPECT_NEAR(f.dbg.time, 0.5F, 1e-7F);
    /* la pose suit l'instant : à 0,5 s, 45° : articulation 2 en (-0,7071, 1,7071, 0) */
    EXPECT_NEAR(f.dbg.world[2][3][0], -H, 1e-5F);
    EXPECT_NEAR(f.dbg.world[2][3][1], 1.0F + H, 1e-5F);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_debug, looping_wraps_in_both_directions_and_without_a_loop_it_stops_and_pauses)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    f.dbg.time = 0.9F;
    ASSERT_TRUE(eng_anim_debug_update(&f.dbg, 0.3F));
    EXPECT_NEAR(f.dbg.time, 0.2F, 1e-5F);
    EXPECT_FALSE(f.dbg.paused);
    f.dbg.speed = -1.0F;
    ASSERT_TRUE(eng_anim_debug_update(&f.dbg, 0.5F));
    EXPECT_NEAR(f.dbg.time, 0.7F, 1e-5F); /* en arrière : 0,2 - 0,5 = -0,3 -> 0,7 */

    f.dbg.loop = false;
    f.dbg.speed = 1.0F;
    f.dbg.time = 0.9F;
    ASSERT_TRUE(eng_anim_debug_update(&f.dbg, 0.3F));
    EXPECT_NEAR(f.dbg.time, 1.0F, 1e-7F);
    EXPECT_TRUE(f.dbg.paused);
    f.dbg.paused = false;
    f.dbg.speed = -1.0F;
    f.dbg.time = 0.1F;
    ASSERT_TRUE(eng_anim_debug_update(&f.dbg, 0.3F));
    EXPECT_NEAR(f.dbg.time, 0.0F, 1e-7F);
    EXPECT_TRUE(f.dbg.paused);
    /* en pause à la fin, relancer la lecture sans boucle : le temps reste à la butée et repause */
    f.dbg.paused = false;
    f.dbg.speed = 1.0F;
    f.dbg.time = 1.0F;
    ASSERT_TRUE(eng_anim_debug_update(&f.dbg, 0.1F));
    EXPECT_NEAR(f.dbg.time, 1.0F, 1e-7F);
    EXPECT_TRUE(f.dbg.paused);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_debug, stepping_moves_by_one_frame_pauses_and_respects_the_loop)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    ASSERT_TRUE(eng_anim_debug_step(&f.dbg, 3));
    EXPECT_TRUE(f.dbg.paused);
    EXPECT_NEAR(f.dbg.time, 0.1F, 1e-6F);
    EXPECT_EQ(eng_anim_debug_frame(&f.dbg), 3U);
    ASSERT_TRUE(eng_anim_debug_step(&f.dbg, -1));
    EXPECT_NEAR(f.dbg.time, 2.0F / 30.0F, 1e-6F);
    EXPECT_EQ(eng_anim_debug_frame(&f.dbg), 2U);
    ASSERT_TRUE(eng_anim_debug_step(&f.dbg, -3)); /* boucle : repasse par la fin */
    EXPECT_NEAR(f.dbg.time, 1.0F - (1.0F / 30.0F), 1e-5F);
    f.dbg.loop = false;
    ASSERT_TRUE(eng_anim_debug_step(&f.dbg, 100));
    EXPECT_NEAR(f.dbg.time, 1.0F, 1e-7F);
    ASSERT_TRUE(eng_anim_debug_step(&f.dbg, -100));
    EXPECT_NEAR(f.dbg.time, 0.0F, 1e-7F);
    f.dbg.frame_rate = 10.0F; /* la cadence règle la taille du pas */
    ASSERT_TRUE(eng_anim_debug_step(&f.dbg, 1));
    EXPECT_NEAR(f.dbg.time, 0.1F, 1e-6F);
    EXPECT_EQ(eng_anim_debug_frame_count(&f.dbg), 11U);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_debug, seek_clamps_and_refuses_a_non_finite_time)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    ASSERT_TRUE(eng_anim_debug_seek(&f.dbg, 0.75F));
    EXPECT_NEAR(f.dbg.time, 0.75F, 1e-7F);
    ASSERT_TRUE(eng_anim_debug_seek(&f.dbg, 5.0F));
    EXPECT_NEAR(f.dbg.time, 1.0F, 1e-7F);
    ASSERT_TRUE(eng_anim_debug_seek(&f.dbg, -5.0F));
    EXPECT_NEAR(f.dbg.time, 0.0F, 1e-7F);
    ASSERT_TRUE(eng_anim_debug_seek(&f.dbg, 0.25F));
    EXPECT_FALSE(eng_anim_debug_seek(&f.dbg, NAN));
    EXPECT_FALSE(eng_anim_debug_seek(&f.dbg, INFINITY));
    EXPECT_NEAR(f.dbg.time, 0.25F, 1e-7F); /* inchangé */
    EXPECT_FALSE(eng_anim_debug_seek(NULL, 0.0F));
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_debug, absurd_settings_are_brought_back_into_range)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    f.dbg.time = NAN;
    f.dbg.speed = 100.0F;
    f.dbg.frame_rate = 0.0F;
    f.dbg.selected = 99;
    ASSERT_TRUE(eng_anim_debug_update(&f.dbg, 0.0F));
    EXPECT_NEAR(f.dbg.time, 0.0F, 1e-7F);
    EXPECT_NEAR(f.dbg.speed, ENG_ANIM_DEBUG_MAX_SPEED, 1e-7F);
    EXPECT_NEAR(f.dbg.frame_rate, ENG_ANIM_DEBUG_DEFAULT_RATE, 1e-7F);
    EXPECT_EQ(f.dbg.selected, 2); /* la dernière articulation */
    f.dbg.selected = -7;
    f.dbg.speed = -100.0F;
    f.dbg.time = 50.0F;
    f.dbg.frame_rate = NAN;
    ASSERT_TRUE(eng_anim_debug_update(&f.dbg, 0.0F));
    EXPECT_EQ(f.dbg.selected, -1);
    EXPECT_NEAR(f.dbg.speed, -ENG_ANIM_DEBUG_MAX_SPEED, 1e-7F);
    EXPECT_NEAR(f.dbg.time, 1.0F, 1e-7F);
    EXPECT_NEAR(f.dbg.frame_rate, ENG_ANIM_DEBUG_DEFAULT_RATE, 1e-7F);
    f.dbg.speed = NAN;
    ASSERT_TRUE(eng_anim_debug_update(&f.dbg, 0.0F));
    EXPECT_NEAR(f.dbg.speed, 1.0F, 1e-7F);
    eng_arena_shutdown(&f.arena);
}

static bool lines_ready(EngDebugLines *lines, EngArena *arena)
{
    return eng_debug_lines_init(lines, arena, 1024);
}

UTEST(anim_debug, drawing_shows_bones_then_rest_axes_and_the_trail_of_the_followed_joint)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngArena big;
    ASSERT_TRUE(eng_arena_init(&big, "lines", (size_t)1 << 20));
    EngDebugLines lines;
    ASSERT_TRUE(lines_ready(&lines, &big));
    mat4 root;
    glm_mat4_identity(root);
    /* 3 articulations : 2 os, et chaque articulation porte une croix de 3 lignes */
    EXPECT_EQ(eng_anim_debug_draw(&f.dbg, root, &lines), 2U);
    EXPECT_EQ(eng_debug_lines_overlay_count(&lines), 2U + (3U * 3U));
    EXPECT_EQ(eng_debug_lines_depth_count(&lines), 0U); /* tout est superposé */
    EXPECT_FALSE(lines.overlay_mode);                   /* le mode de la liste est rétabli */
    const uint32_t base = eng_debug_lines_count(&lines);

    eng_debug_lines_clear(&lines);
    f.dbg.show_rest = true;
    (void)eng_anim_debug_draw(&f.dbg, root, &lines);
    EXPECT_EQ(eng_debug_lines_count(&lines), 2U * base); /* le repos double le dessin */

    eng_debug_lines_clear(&lines);
    f.dbg.show_rest = false;
    f.dbg.selected = 2;
    f.dbg.show_trail = false;
    (void)eng_anim_debug_draw(&f.dbg, root, &lines);
    EXPECT_EQ(eng_debug_lines_count(&lines),
              base + 1U + 3U); /* os jaune + axes, pas de trajectoire */

    eng_debug_lines_clear(&lines);
    f.dbg.show_trail = true;
    (void)eng_anim_debug_draw(&f.dbg, root, &lines);
    EXPECT_EQ(eng_debug_lines_count(&lines), base + 1U + 3U + ENG_ANIM_DEBUG_TRAIL_SEGMENTS);
    /* la trajectoire est magenta, commence au repos (0, 2, 0) et finit en (-1, 1, 0) (90°) */
    const EngDebugVertex *v = lines.overlay;
    const uint32_t first = (base + 1U + 3U) * 2U; /* premier sommet de la trajectoire */
    EXPECT_NEAR(v[first].position[0], 0.0F, 1e-5F);
    EXPECT_NEAR(v[first].position[1], 2.0F, 1e-5F);
    EXPECT_EQ(v[first].color.r, ENG_DEBUG_MAGENTA.r);
    const uint32_t last = lines.overlay_count - 1U;
    EXPECT_NEAR(v[last].position[0], -1.0F, 1e-5F);
    EXPECT_NEAR(v[last].position[1], 1.0F, 1e-5F);
    /* l'os suivi est jaune : son segment va de l'articulation 1 (0, 1, 0) à l'articulation 2 */
    bool yellow = false;
    for (uint32_t i = 0; i < lines.overlay_count; i++) {
        yellow =
            yellow || (v[i].color.r == ENG_DEBUG_YELLOW.r && v[i].color.g == ENG_DEBUG_YELLOW.g &&
                       v[i].color.b == ENG_DEBUG_YELLOW.b);
    }
    EXPECT_TRUE(yellow);

    /* placement du personnage : la racine déplace tout */
    eng_debug_lines_clear(&lines);
    glm_translate(root, (vec3){10.0F, 0.0F, 0.0F});
    f.dbg.selected = -1;
    (void)eng_anim_debug_draw(&f.dbg, root, &lines);
    EXPECT_NEAR(lines.overlay[0].position[0], 10.0F + 0.02F, 1.0F); /* croix à l'articulation 0 */
    for (uint32_t i = 0; i < lines.overlay_count; i++) {
        EXPECT_GT(lines.overlay[i].position[0], 9.0F);
    }
    /* arguments invalides */
    EXPECT_EQ(eng_anim_debug_draw(NULL, root, &lines), 0U);
    EXPECT_EQ(eng_anim_debug_draw(&f.dbg, NULL, &lines), 0U);
    EXPECT_EQ(eng_anim_debug_draw(&f.dbg, root, NULL), 0U);
    eng_arena_shutdown(&big);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_debug, the_list_overlay_mode_is_restored_even_when_it_was_on)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngArena big;
    ASSERT_TRUE(eng_arena_init(&big, "lines", (size_t)1 << 20));
    EngDebugLines lines;
    ASSERT_TRUE(lines_ready(&lines, &big));
    eng_debug_lines_set_overlay(&lines, true);
    mat4 root;
    glm_mat4_identity(root);
    (void)eng_anim_debug_draw(&f.dbg, root, &lines);
    EXPECT_TRUE(lines.overlay_mode);
    eng_arena_shutdown(&big);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_debug, the_status_line_says_where_we_are)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    ASSERT_TRUE(eng_anim_debug_seek(&f.dbg, 0.5F));
    f.dbg.selected = 1;
    f.dbg.paused = true;
    f.dbg.speed = 0.5F;
    f.dbg.loop = false;
    ASSERT_TRUE(eng_anim_debug_update(&f.dbg, 0.0F));
    char text[160];
    const size_t n = eng_anim_debug_describe(&f.dbg, text, sizeof text);
    EXPECT_EQ(n, strlen(text));
    EXPECT_NE(strstr(text, "t 0.50 / 1.00 s"), NULL);
    EXPECT_NE(strstr(text, "image 15 / 31"), NULL);
    EXPECT_NE(strstr(text, "x0.50"), NULL);
    EXPECT_NE(strstr(text, "pause"), NULL);
    EXPECT_NE(strstr(text, "sans boucle"), NULL);
    EXPECT_NE(strstr(text, "suivi : 1 a"), NULL);
    f.dbg.selected = -1;
    f.dbg.paused = false;
    f.dbg.loop = true;
    (void)eng_anim_debug_describe(&f.dbg, text, sizeof text);
    EXPECT_NE(strstr(text, "lecture"), NULL);
    EXPECT_NE(strstr(text, "| boucle"), NULL);
    EXPECT_NE(strstr(text, "suivi : aucun"), NULL);
    char tiny[8];
    EXPECT_EQ(eng_anim_debug_describe(&f.dbg, tiny, sizeof tiny), sizeof tiny - 1U);
    EXPECT_EQ(strlen(tiny), sizeof tiny - 1U);
    EXPECT_EQ(eng_anim_debug_describe(&f.dbg, NULL, 10), (size_t)0);
    EXPECT_EQ(eng_anim_debug_describe(&f.dbg, text, 0), (size_t)0);
    EXPECT_EQ(eng_anim_debug_describe(NULL, text, sizeof text), (size_t)0);
    EXPECT_EQ(text[0], '\0');
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_debug, nothing_is_allocated_after_init)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    const size_t used = f.arena.used;
    for (int i = 0; i < 100; i++) {
        (void)eng_anim_debug_update(&f.dbg, 0.016F);
        (void)eng_anim_debug_step(&f.dbg, 1);
        f.dbg.paused = false;
    }
    EXPECT_EQ(f.arena.used, used);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_debug, following_the_root_draws_axes_only_and_a_zero_length_clip_has_no_trail)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngArena big;
    ASSERT_TRUE(eng_arena_init(&big, "lines", (size_t)1 << 20));
    EngDebugLines lines;
    ASSERT_TRUE(lines_ready(&lines, &big));
    mat4 root;
    glm_mat4_identity(root);
    (void)eng_anim_debug_draw(&f.dbg, root, &lines);
    const uint32_t base = eng_debug_lines_count(&lines);
    f.dbg.show_trail = false;
    /* articulation 0 : pas d'os parent, donc seulement les 3 axes en plus */
    eng_debug_lines_clear(&lines);
    f.dbg.selected = 0;
    (void)eng_anim_debug_draw(&f.dbg, root, &lines);
    EXPECT_EQ(eng_debug_lines_count(&lines), base + 3U);
    /* articulation 1 : os jaune + 3 axes */
    eng_debug_lines_clear(&lines);
    f.dbg.selected = 1;
    (void)eng_anim_debug_draw(&f.dbg, root, &lines);
    EXPECT_EQ(eng_debug_lines_count(&lines), base + 1U + 3U);
    /* le point de la trajectoire à mi-clip est la pose à 0,5 s (45 degrés) */
    eng_debug_lines_clear(&lines);
    f.dbg.selected = 2;
    f.dbg.show_trail = true;
    (void)eng_anim_debug_draw(&f.dbg, root, &lines);
    const uint32_t trail0 = (base + 1U + 3U) * 2U;
    const EngDebugVertex *mid =
        &lines.overlay[trail0 + ((ENG_ANIM_DEBUG_TRAIL_SEGMENTS / 2U) * 2U)];
    EXPECT_NEAR(mid->position[0], -H, 1e-4F);
    EXPECT_NEAR(mid->position[1], 1.0F + H, 1e-4F);

    /* clip de durée nulle : accepté, aucune trajectoire */
    EngAnimDebug d;
    EngAnimClip zero = f.clip;
    zero.duration = 0.0F;
    ASSERT_TRUE(eng_anim_debug_init(&d, &f.arena, &f.sk, &zero));
    d.selected = 2;
    d.show_trail = true;
    eng_debug_lines_clear(&lines);
    (void)eng_anim_debug_draw(&d, root, &lines);
    EXPECT_EQ(eng_debug_lines_count(&lines), base + 1U + 3U);
    EXPECT_TRUE(eng_anim_debug_update(&d, 1.0F));
    EXPECT_NEAR(d.time, 0.0F, 1e-7F);
    /* canaux annoncés mais absents : refus */
    EngAnimClip broken = f.clip;
    broken.channels = NULL;
    EXPECT_FALSE(eng_anim_debug_init(&d, &f.arena, &f.sk, &broken));
    eng_arena_shutdown(&big);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_debug, the_status_line_is_cut_exactly_at_the_buffer_size)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    char full[160];
    const size_t n = eng_anim_debug_describe(&f.dbg, full, sizeof full);
    ASSERT_GT(n, (size_t)10);
    char exact[160];
    EXPECT_EQ(eng_anim_debug_describe(&f.dbg, exact, n), n - 1U); /* il manque la place du zéro */
    EXPECT_EQ(strlen(exact), n - 1U);
    EXPECT_EQ(eng_anim_debug_describe(&f.dbg, exact, n + 1U), n);
    eng_arena_shutdown(&f.arena);
}
