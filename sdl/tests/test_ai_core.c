/* Monde d'obstacles, navigation, perception, jetons, archétypes (M6-02 à M6-04, M6-07, M6-08). */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "engine/ai_archetype.h"
#include "engine/ai_perception.h"
#include "engine/ai_tokens.h"
#include "engine/ai_world.h"
#include "engine/navgrid.h"
#include "utest.h"

static EngNavGrid g_nav; /* ~300 Ko : pas sur la pile */

UTEST(ai_world, circle_and_box_distances_and_los)
{
    EngAiWorld w;
    eng_aiw_init(&w);
    ASSERT_TRUE(eng_aiw_add_circle(&w, 5.0F, 0.0F, 1.0F));
    ASSERT_TRUE(eng_aiw_add_box(&w, 0.0F, 5.0F, 2.0F, 0.5F));
    const float p1[2] = {8.0F, 0.0F};
    EXPECT_NEAR(eng_aiw_distance(&w, p1), 2.0F, 1e-5F);
    const float p2[2] = {5.0F, 0.0F};
    EXPECT_LT(eng_aiw_distance(&w, p2), 0.0F);
    const float p3[2] = {0.0F, 7.5F};
    EXPECT_NEAR(eng_aiw_distance(&w, p3), 2.0F, 1e-5F);
    const float a[2] = {0.0F, 0.0F};
    const float b[2] = {10.0F, 0.0F};
    EXPECT_FALSE(eng_aiw_los(&w, a, b)); /* le rocher est au milieu */
    const float c[2] = {10.0F, 3.0F};
    EXPECT_TRUE(eng_aiw_los(&w, a, c));
    const float d[2] = {0.0F, 10.0F};
    EXPECT_FALSE(eng_aiw_los(&w, a, d)); /* le mur */
    const float e[2] = {6.0F, 10.0F};
    EXPECT_TRUE(eng_aiw_los(&w, a, e)); /* passe à côté du mur (x = 3 > 2 à z = 5) */
    EXPECT_TRUE(eng_aiw_los(NULL, a, b));
}

UTEST(ai_world, invalid_adds_are_refused_and_capacity_is_bounded)
{
    EngAiWorld w;
    eng_aiw_init(&w);
    EXPECT_FALSE(eng_aiw_add_circle(&w, NAN, 0.0F, 1.0F));
    EXPECT_FALSE(eng_aiw_add_circle(&w, 0.0F, 0.0F, 0.0F));
    EXPECT_FALSE(eng_aiw_add_box(&w, 0.0F, 0.0F, -1.0F, 1.0F));
    EXPECT_FALSE(eng_aiw_add_circle(NULL, 0.0F, 0.0F, 1.0F));
    for (uint32_t i = 0; i < ENG_AI_MAX_OBSTACLES; i++) {
        EXPECT_TRUE(eng_aiw_add_circle(&w, (float)i * 3.0F, 0.0F, 1.0F));
    }
    EXPECT_FALSE(eng_aiw_add_circle(&w, 999.0F, 0.0F, 1.0F));
    EXPECT_TRUE(isinf(eng_aiw_distance(NULL, (const float[2]){0, 0})));
}

UTEST(ai_world, push_out_leaves_a_disc_outside_every_obstacle)
{
    EngAiWorld w;
    eng_aiw_init(&w);
    ASSERT_TRUE(eng_aiw_add_circle(&w, 0.0F, 0.0F, 1.0F));
    ASSERT_TRUE(eng_aiw_add_box(&w, 10.0F, 0.0F, 1.0F, 1.0F));
    float p[2] = {0.5F, 0.0F};
    eng_aiw_push_out(&w, p, 0.4F);
    EXPECT_NEAR(p[0], 1.4F, 1e-4F);
    float q[2] = {9.5F, 0.2F}; /* à l'intérieur de la boîte : sort par la face la plus proche */
    eng_aiw_push_out(&w, q, 0.4F);
    EXPECT_NEAR(q[0], 8.6F, 1e-4F);
    float r[2] = {10.0F, 1.2F}; /* dehors mais trop près */
    eng_aiw_push_out(&w, r, 0.5F);
    EXPECT_NEAR(r[1], 1.5F, 1e-4F);
    float far_p[2] = {5.0F, 5.0F};
    eng_aiw_push_out(&w, far_p, 0.4F);
    EXPECT_NEAR(far_p[0], 5.0F, 1e-6F);
    float nan_p[2] = {NAN, 0.0F};
    eng_aiw_push_out(&w, nan_p, 0.4F); /* ne plante pas */
}

UTEST(navgrid, a_path_goes_around_a_wall_and_never_crosses_a_blocked_cell)
{
    EngAiWorld w;
    eng_aiw_init(&w);
    ASSERT_TRUE(eng_aiw_add_box(&w, 0.0F, 0.0F, 0.5F, 6.0F)); /* mur de 12 m le long de z */
    const float mn[2] = {-15.0F, -15.0F};
    const float mx[2] = {15.0F, 15.0F};
    ASSERT_TRUE(eng_nav_build(&g_nav, &w, mn, mx, 0.5F, 0.4F));
    const float from[2] = {-5.0F, 0.0F};
    const float to[2] = {5.0F, 0.0F};
    float path[32][2];
    const uint32_t n = eng_nav_path(&g_nav, from, to, path, 32);
    ASSERT_GE(n, 2U);
    EXPECT_NEAR(path[n - 1][0], 5.0F, 1e-4F);
    EXPECT_NEAR(path[n - 1][1], 0.0F, 1e-4F);
    float cur[2] = {from[0], from[1]};
    float len = 0.0F;
    for (uint32_t i = 0; i < n; i++) {
        EXPECT_TRUE(eng_nav_line_free(&g_nav, cur, path[i]));
        EXPECT_TRUE(eng_aiw_los(&w, cur, path[i])); /* et pas de traversée réelle du mur */
        len += hypotf(path[i][0] - cur[0], path[i][1] - cur[1]);
        cur[0] = path[i][0];
        cur[1] = path[i][1];
    }
    EXPECT_GT(len, 10.0F + 4.0F); /* plus long que la ligne droite : il contourne */
    EXPECT_LT(len, 20.0F);        /* mais lissé : pas le chemin en escalier */
}

UTEST(navgrid, straight_path_is_one_point_and_edge_cases_are_handled)
{
    EngAiWorld w;
    eng_aiw_init(&w);
    ASSERT_TRUE(eng_aiw_add_circle(&w, 0.0F, 8.0F, 1.0F));
    const float mn[2] = {-10.0F, -10.0F};
    const float mx[2] = {10.0F, 10.0F};
    ASSERT_TRUE(eng_nav_build(&g_nav, &w, mn, mx, 0.5F, 0.3F));
    float path[8][2];
    const float a[2] = {-4.0F, 0.0F};
    const float b[2] = {4.0F, 0.0F};
    EXPECT_EQ(eng_nav_path(&g_nav, a, b, path, 8), 1U);
    const float out[2] = {50.0F, 0.0F};
    EXPECT_EQ(eng_nav_path(&g_nav, a, out, path, 8), 0U);
    const float inside[2] = {0.0F, 8.0F}; /* but dans le rocher : point praticable le plus proche */
    const uint32_t n = eng_nav_path(&g_nav, a, inside, path, 8);
    ASSERT_GE(n, 1U);
    EXPECT_TRUE(eng_nav_walkable(&g_nav, path[n - 1]));
    EXPECT_EQ(eng_nav_path(&g_nav, a, b, path, 0), 0U);
    EXPECT_EQ(eng_nav_path(NULL, a, b, path, 8), 0U);
    EXPECT_FALSE(eng_nav_build(&g_nav, &w, mx, mn, 0.5F, 0.3F));
    const float huge[2] = {1000.0F, 1000.0F};
    EXPECT_FALSE(eng_nav_build(&g_nav, &w, mn, huge, 0.5F, 0.3F));
    EXPECT_FALSE(eng_nav_build(&g_nav, &w, mn, mx, 0.0F, 0.3F));
}

UTEST(navgrid, a_closed_room_has_no_path_and_max_points_truncates_with_the_goal_last)
{
    EngAiWorld w;
    eng_aiw_init(&w);
    ASSERT_TRUE(eng_aiw_add_box(&w, 0.0F, -3.0F, 3.0F, 0.5F));
    ASSERT_TRUE(eng_aiw_add_box(&w, 0.0F, 3.0F, 3.0F, 0.5F));
    ASSERT_TRUE(eng_aiw_add_box(&w, -3.0F, 0.0F, 0.5F, 3.0F));
    ASSERT_TRUE(eng_aiw_add_box(&w, 3.0F, 0.0F, 0.5F, 3.0F));
    const float mn[2] = {-8.0F, -8.0F};
    const float mx[2] = {8.0F, 8.0F};
    ASSERT_TRUE(eng_nav_build(&g_nav, &w, mn, mx, 0.5F, 0.3F));
    float path[4][2];
    const float in[2] = {0.0F, 0.0F};
    const float out[2] = {6.0F, 0.0F};
    EXPECT_EQ(eng_nav_path(&g_nav, in, out, path, 4), 0U);
    /* un labyrinthe en S : 1 seul point autorisé => le dernier est le but */
    EngAiWorld w2;
    eng_aiw_init(&w2);
    ASSERT_TRUE(eng_aiw_add_box(&w2, -4.0F, 2.0F, 4.0F, 0.4F));
    ASSERT_TRUE(eng_aiw_add_box(&w2, 4.0F, -2.0F, 4.0F, 0.4F));
    ASSERT_TRUE(eng_nav_build(&g_nav, &w2, mn, mx, 0.5F, 0.3F));
    const float s[2] = {-6.0F, 6.0F};
    const float g[2] = {6.0F, -6.0F};
    float one[1][2];
    EXPECT_EQ(eng_nav_path(&g_nav, s, g, one, 1), 1U);
    EXPECT_NEAR(one[0][0], 6.0F, 1e-4F);
    EXPECT_NEAR(one[0][1], -6.0F, 1e-4F);
}

UTEST(ai_perception, vision_needs_range_cone_and_a_clear_line)
{
    EngAiWorld w;
    eng_aiw_init(&w);
    ASSERT_TRUE(eng_aiw_add_circle(&w, 0.0F, -6.0F, 1.0F));
    const EngAiSenses s = {12.0F, 0.9599F /* 55° */, 8.0F, 120U};
    EngAiPerception p;
    eng_ai_perception_reset(&p);
    const float self[2] = {0.0F, 0.0F};
    const float front[2] = {3.0F, -5.0F}; /* devant (-Z), hors du rocher */
    eng_ai_perceive(&p, &s, self, 0.0F, front, true, 0.0F, &w);
    EXPECT_TRUE(p.sees);
    EXPECT_TRUE(p.has_memory);
    const float behind_rock[2] = {0.0F, -10.0F};
    eng_ai_perceive(&p, &s, self, 0.0F, behind_rock, true, 0.0F, &w);
    EXPECT_FALSE(p.sees); /* « ne voit pas à travers un rocher » */
    const float back[2] = {0.0F, 5.0F};
    eng_ai_perceive(&p, &s, self, 0.0F, back, true, 0.0F, &w);
    EXPECT_FALSE(p.sees); /* dans le dos */
    const float far_t[2] = {1.0F, -13.0F};
    eng_ai_perceive(&p, &s, self, 0.0F, far_t, true, 0.0F, &w);
    EXPECT_FALSE(p.sees);
    const float edge_in[2] = {sinf(0.95F) * 5.0F, -cosf(0.95F) * 5.0F}; /* 54,4° */
    eng_ai_perceive(&p, &s, self, 0.0F, edge_in, true, 0.0F, &w);
    EXPECT_TRUE(p.sees);
    const float edge_out[2] = {sinf(0.98F) * 5.0F, -cosf(0.98F) * 5.0F}; /* 56,1° */
    eng_ai_perceive(&p, &s, self, 0.0F, edge_out, true, 0.0F, &w);
    EXPECT_FALSE(p.sees);
}

UTEST(ai_perception, hearing_passes_walls_and_memory_expires)
{
    EngAiWorld w;
    eng_aiw_init(&w);
    ASSERT_TRUE(eng_aiw_add_box(&w, 0.0F, -3.0F, 5.0F, 0.5F));
    const EngAiSenses s = {12.0F, 0.9599F, 8.0F, 10U};
    EngAiPerception p;
    eng_ai_perception_reset(&p);
    const float self[2] = {0.0F, 0.0F};
    const float t[2] = {0.0F, -6.0F}; /* derrière le mur */
    eng_ai_perceive(&p, &s, self, 0.0F, t, true, 0.0F, &w);
    EXPECT_FALSE(p.sees);
    EXPECT_FALSE(p.hears); /* silencieux */
    EXPECT_FALSE(p.has_memory);
    eng_ai_perceive(&p, &s, self, 0.0F, t, true, 7.0F, &w); /* bruit de 7 m, cible à 6 m */
    EXPECT_TRUE(p.hears);
    EXPECT_FALSE(p.sees);
    EXPECT_NEAR(p.last_known[1], -6.0F, 1e-5F);
    eng_ai_perceive(&p, &s, self, 0.0F, t, true, 5.0F, &w); /* bruit trop faible */
    EXPECT_FALSE(p.hears);
    EXPECT_TRUE(p.has_memory);
    for (int i = 0; i < 9; i++) {
        eng_ai_perceive(&p, &s, self, 0.0F, t, true, 0.0F, &w);
    }
    EXPECT_TRUE(p.has_memory); /* 10 ticks sans rien : encore connue (> 10 requis) */
    eng_ai_perceive(&p, &s, self, 0.0F, t, true, 0.0F, &w);
    EXPECT_FALSE(p.has_memory);
    eng_ai_perceive(&p, &s, self, 0.0F, t, true, 7.0F, &w);
    EXPECT_TRUE(p.has_memory);
    eng_ai_perceive(&p, &s, self, 0.0F, t, false, 7.0F, &w); /* cible morte : tout est oublié */
    EXPECT_FALSE(p.has_memory);
    EXPECT_FALSE(p.hears);
}

UTEST(ai_tokens, never_more_than_max_and_the_nearest_requesters_win)
{
    EngAiTokens t;
    eng_tokens_init(&t, 2);
    const uint32_t ids[4] = {1, 2, 3, 4};
    const float dist[4] = {5.0F, 3.0F, 4.0F, 1.0F};
    eng_tokens_arbitrate(&t, ids, dist, 4);
    EXPECT_EQ(eng_tokens_in_use(&t), 2U);
    EXPECT_TRUE(eng_tokens_has(&t, 4));
    EXPECT_TRUE(eng_tokens_has(&t, 2));
    EXPECT_FALSE(eng_tokens_has(&t, 1));
    const float dist2[4] = {0.5F, 0.5F, 0.5F, 9.0F}; /* les porteurs gardent leur jeton */
    eng_tokens_arbitrate(&t, ids, dist2, 4);
    EXPECT_TRUE(eng_tokens_has(&t, 4));
    EXPECT_TRUE(eng_tokens_has(&t, 2));
    eng_tokens_release(&t, 4);
    eng_tokens_arbitrate(&t, ids, dist2, 4);
    EXPECT_EQ(eng_tokens_in_use(&t), 2U);
    EXPECT_TRUE(eng_tokens_has(&t, 1)); /* égalité de distance : plus petit identifiant */
    EXPECT_FALSE(eng_tokens_has(&t, 3));
    eng_tokens_init(&t, 99);
    EXPECT_EQ(t.max, ENG_AI_TOKEN_SLOTS);
}

UTEST(ai_tokens, ring_spreads_enemies_evenly_around_the_player_without_duplicates)
{
    const float c[2] = {10.0F, 10.0F};
    const float pos[3][2] = {
        {14.0F, 10.0F}, {14.5F, 10.5F}, {13.0F, 11.0F}}; /* tous du même côté */
    float out[3][2];
    eng_ai_ring(c, pos, 3, 4.0F, out);
    for (int i = 0; i < 3; i++) {
        EXPECT_NEAR(hypotf(out[i][0] - c[0], out[i][1] - c[1]), 4.0F, 1e-3F);
        for (int j = i + 1; j < 3; j++) {
            EXPECT_GT(hypotf(out[i][0] - out[j][0], out[i][1] - out[j][1]),
                      5.0F); /* 4 sqrt3 = 6,9 */
        }
    }
}

static const char ARCH[] = "# camp de test\n"
                           "archetype grunt\n hp 60\n speed 1.0\n light g_slash\n heavy g_smash\n "
                           "plan light 0 1.8 60 3 2\n"
                           " plan heavy 0.5 2.2 150 1 1\nend\n"
                           "archetype boss\n hp 400\n light b1\n heavy b2\n plan light 0 2 60 1 1\n"
                           " phase 0.6 b3 b4 1.2 0.7\n phase 0.3 b5 b6 1.4 0.5\nend\n";

UTEST(ai_archetype, a_text_file_defines_archetypes_with_defaults_and_phases)
{
    EngArchSet set;
    char err[160];
    ASSERT_TRUE(eng_arch_parse(ARCH, &set, err, sizeof err));
    ASSERT_EQ(set.count, 2U);
    EXPECT_STREQ(set.a[0].name, "grunt");
    EXPECT_NEAR(set.a[0].hp, 60.0F, 1e-6F);
    EXPECT_NEAR(set.a[0].sight, 12.0F, 1e-6F); /* défaut */
    EXPECT_EQ(set.a[0].plan_count, 2U);
    EXPECT_TRUE(set.a[0].plans[1].heavy);
    EXPECT_EQ(set.a[0].plans[0].presses, 2U);
    EXPECT_EQ(set.a[1].phase_count, 2U);
    EXPECT_NEAR(set.a[1].phases[1].cooldown_scale, 0.5F, 1e-6F);
    EXPECT_EQ(eng_arch_find(&set, "boss"), 1U);
    EXPECT_EQ(eng_arch_find(&set, "nope"), UINT32_MAX);
}

UTEST(ai_archetype, every_error_is_refused_with_its_line)
{
    /* NOLINTBEGIN(bugprone-suspicious-missing-comma) : chaînes coupées par le formateur */
    static const char *const bad[] = {
        "archetype a\n hp 0\nend\n",
        "archetype a\n hp x\nend\n",
        "archetype a\n bogus 1\nend\n",
        "archetype a\n light l\nend\n", /* heavy et plan manquants */
        "archetype a\n light l\n heavy h\n plan light 2 1 10 1 1\nend\n", /* portée
                                                                             inversée */
        "archetype a\n light l\n heavy h\n plan light 0 1 10 1 9\nend\n", /* trop
                                                                             d'appuis */
        "archetype a\n light l\n heavy h\n plan light 0 1 10 1 1\n",      /* end manquant */
        "hp 10\n",
        "archetype a\n light l\n heavy h\n plan light 0 1 10 1 1\n phase 0.3 a b 1 1\n "
        "phase 0.5 a b 1 1\nend\n", /* PV qui remontent */
        "archetype a\n fov 400\nend\n",
        "archetype a\n light l\n heavy h\n plan light 0 1 10 1 1\nend\narchetype "
        "a\nend\n", /* doublon
                     */
        "",
    };
    /* NOLINTEND(bugprone-suspicious-missing-comma) */
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        EngArchSet set;
        char err[160] = {0};
        EXPECT_FALSE(eng_arch_parse(bad[i], &set, err, sizeof err));
        EXPECT_GT(strlen(err), 0U);
    }
    EXPECT_FALSE(eng_arch_parse(NULL, NULL, NULL, 0));
}

UTEST(ai_archetype, validation_enforces_a_300ms_telegraph_on_every_reachable_attack)
{
    static const char atk_ok[] =
        "attack g_slash\n startup 20\n active 4\n recovery 20\n damage 8\n posture 8\n stamina 5\n"
        " hitstop 3\n cancel_from 6\n next g_slash2\n hitbox hand_r 0 0 0 0 0 0.8 0.12\nend\n"
        "attack g_slash2\n startup 18\n active 4\n recovery 20\n damage 8\n posture 8\n stamina 5\n"
        " hitstop 3\n hitbox hand_r 0 0 0 0 0 0.8 0.12\nend\n"
        "attack g_smash\n startup 30\n active 5\n recovery 30\n damage 20\n posture 20\n stamina "
        "5\n"
        " hitstop 5\n hitbox hand_r 0 0 0 0 0 0.9 0.14\nend\n";
    static const char arch1[] =
        "archetype grunt\n light g_slash\n heavy g_smash\n plan light 0 1.8 60 1 1\nend\n";
    EngAttackSet atk;
    EngArchSet set;
    char err[200];
    ASSERT_TRUE(eng_attackset_parse(atk_ok, &atk, err, sizeof err));
    ASSERT_TRUE(eng_arch_parse(arch1, &set, err, sizeof err));
    EXPECT_TRUE(eng_arch_validate(&set, &atk, err, sizeof err));
    /* la 2e attaque de la chaîne est trop rapide (17 ticks = 283 ms) */
    static const char atk_bad[] =
        "attack g_slash\n startup 20\n active 4\n recovery 20\n damage 8\n posture 8\n stamina 5\n"
        " hitstop 3\n cancel_from 6\n next g_slash2\n hitbox hand_r 0 0 0 0 0 0.8 0.12\nend\n"
        "attack g_slash2\n startup 17\n active 4\n recovery 20\n damage 8\n posture 8\n stamina 5\n"
        " hitstop 3\n hitbox hand_r 0 0 0 0 0 0.8 0.12\nend\n"
        "attack g_smash\n startup 30\n active 5\n recovery 30\n damage 20\n posture 20\n stamina "
        "5\n"
        " hitstop 5\n hitbox hand_r 0 0 0 0 0 0.9 0.14\nend\n";
    ASSERT_TRUE(eng_attackset_parse(atk_bad, &atk, err, sizeof err));
    EXPECT_FALSE(eng_arch_validate(&set, &atk, err, sizeof err));
    EXPECT_NE(strstr(err, "g_slash2"), NULL);
    /* nom inconnu */
    static const char arch2[] =
        "archetype x\n light nope\n heavy g_smash\n plan light 0 1 1 1 1\nend\n";
    ASSERT_TRUE(eng_arch_parse(arch2, &set, err, sizeof err));
    EXPECT_FALSE(eng_arch_validate(&set, &atk, err, sizeof err));
    EXPECT_NE(strstr(err, "inconnue"), NULL);
}

UTEST(ai_perception, offset_self_and_rotated_yaw_give_the_same_answers_as_the_origin_case)
{
    EngAiWorld w;
    eng_aiw_init(&w);
    const EngAiSenses s = {12.0F, 0.9599F, 8.0F, 120U};
    EngAiPerception p;
    eng_ai_perception_reset(&p);
    const float self[2] = {20.0F, -30.0F};
    const float yaw = 1.0F; /* avant = (-sin 1, -cos 1) */
    const float fx = -sinf(yaw);
    const float fz = -cosf(yaw);
    const float ahead[2] = {self[0] + fx * 6.0F, self[1] + fz * 6.0F};
    eng_ai_perceive(&p, &s, self, yaw, ahead, true, 0.0F, &w);
    EXPECT_TRUE(p.sees);
    EXPECT_NEAR(p.last_known[0], ahead[0], 1e-4F);
    const float behind[2] = {self[0] - fx * 6.0F, self[1] - fz * 6.0F};
    eng_ai_perception_reset(&p);
    eng_ai_perceive(&p, &s, self, yaw, behind, true, 0.0F, &w);
    EXPECT_FALSE(p.sees);
    const float too_far[2] = {self[0] + fx * 12.5F, self[1] + fz * 12.5F};
    eng_ai_perceive(&p, &s, self, yaw, too_far, true, 0.0F, &w);
    EXPECT_FALSE(p.sees);
    const float just_in[2] = {self[0] + fx * 11.5F, self[1] + fz * 11.5F};
    eng_ai_perceive(&p, &s, self, yaw, just_in, true, 0.0F, &w);
    EXPECT_TRUE(p.sees);
    /* ouïe : distance calculée depuis `self`, pas depuis l'origine */
    eng_ai_perception_reset(&p);
    const float near_noise[2] = {self[0] - fx * 5.0F, self[1] - fz * 5.0F};
    eng_ai_perceive(&p, &s, self, yaw, near_noise, true, 6.0F, &w);
    EXPECT_TRUE(p.hears);
    eng_ai_perceive(&p, &s, self, yaw, near_noise, true, 4.0F, &w);
    EXPECT_FALSE(p.hears);
    const float over_hearing[2] = {self[0] - fx * 9.0F, self[1] - fz * 9.0F}; /* > 8 m d'écoute */
    eng_ai_perceive(&p, &s, self, yaw, over_hearing, true, 20.0F, &w);
    EXPECT_FALSE(p.hears);
    /* cible quasi confondue : vue (pas de division par zéro) */
    eng_ai_perceive(&p, &s, self, yaw, self, true, 0.0F, &w);
    EXPECT_TRUE(p.sees);
}

UTEST(ai_tokens, ring_slot_zero_keeps_the_direction_of_the_first_enemy)
{
    const float c[2] = {10.0F, -4.0F};
    const float pos[2][2] = {{10.0F, -9.0F}, {16.0F, 2.0F}}; /* le premier est en -Z du centre */
    float out[2][2];
    eng_ai_ring(c, pos, 2, 3.0F, out);
    EXPECT_NEAR(out[0][0], 10.0F, 1e-3F);
    EXPECT_NEAR(out[0][1], -7.0F, 1e-3F); /* 3 m du centre vers l'ennemi 0 */
    const float opp[2] = {c[0] - (out[0][0] - c[0]),
                          c[1] - (out[0][1] - c[1])}; /* 2 ennemis : opposés */
    EXPECT_NEAR(out[1][0], opp[0], 1e-3F);
    EXPECT_NEAR(out[1][1], opp[1], 1e-3F);
    float none[2][2] = {{0}};
    eng_ai_ring(c, pos, 0, 3.0F, none);
    eng_ai_ring(NULL, pos, 2, 3.0F, none);
    EXPECT_NEAR(none[0][0], 0.0F, 1e-9F);
}

UTEST(ai_tokens, release_and_has_ignore_unknown_ids_and_invalid_inputs)
{
    EngAiTokens t;
    eng_tokens_init(&t, 2);
    EXPECT_FALSE(eng_tokens_has(&t, 7));
    EXPECT_FALSE(eng_tokens_has(NULL, 7));
    EXPECT_EQ(eng_tokens_in_use(NULL), 0U);
    eng_tokens_release(&t, 7); /* rien à libérer */
    eng_tokens_release(NULL, 7);
    const uint32_t ids[1] = {7};
    const float d[1] = {1.0F};
    eng_tokens_arbitrate(&t, ids, d, 1);
    EXPECT_TRUE(eng_tokens_has(&t, 7));
    eng_tokens_arbitrate(&t, ids, d, 1); /* idempotent */
    EXPECT_EQ(eng_tokens_in_use(&t), 1U);
    eng_tokens_arbitrate(NULL, ids, d, 1);
    eng_tokens_arbitrate(&t, NULL, d, 1);
    eng_tokens_release(&t, 7);
    EXPECT_EQ(eng_tokens_in_use(&t), 0U);
}

/* Relief (M6-03) : une falaise de 4 m le long de x = 0 (pente 76°), percée d'une rampe douce
 * (15°) entre z = 4 et z = 8 ; partout ailleurs, une pente régulière de 20° (praticable). Le chemin
 * de x = -10 à x = +10 doit passer par la rampe et ne jamais traverser une cellule raide. */
static float cliff_height(void *user, float x, float z)
{
    (void)user;
    const float base = 0.364F * z; /* tan 20° le long de z */
    float wall;
    if (z > 4.0F && z < 8.0F) {
        wall = fminf(4.0F, fmaxf(0.0F, (x + 7.5F) * 0.268F)); /* rampe de 15° de x = -7,5 à +7,5 */
    } else {
        wall = fminf(4.0F, fmaxf(0.0F, (x + 0.5F) * 4.0F)); /* falaise : 4 m sur 1 m */
    }
    return base + wall;
}

UTEST(navgrid, steep_terrain_is_blocked_and_the_path_takes_the_gentle_ramp)
{
    EngAiWorld w;
    eng_aiw_init(&w);
    const float mn[2] = {-16.0F, -16.0F};
    const float mx[2] = {16.0F, 16.0F};
    ASSERT_TRUE(eng_nav_build(&g_nav, &w, mn, mx, 0.5F, 0.4F));
    const uint32_t blocked = eng_nav_block_steep(&g_nav, cliff_height, NULL, 35.0F);
    EXPECT_GT(blocked, 20U); /* la falaise */
    const float on_slope[2] = {-8.0F, -10.0F};
    EXPECT_TRUE(eng_nav_walkable(&g_nav, on_slope)); /* 20° : praticable */
    const float on_cliff[2] = {0.0F, -5.0F};
    EXPECT_FALSE(eng_nav_walkable(&g_nav, on_cliff));
    const float from[2] = {-10.0F, -6.0F};
    const float to[2] = {10.0F, -6.0F};
    float path[64][2];
    const uint32_t n = eng_nav_path(&g_nav, from, to, path, 64);
    ASSERT_GE(n, 2U);
    bool via_ramp = false;
    float cur[2] = {from[0], from[1]};
    for (uint32_t i = 0; i < n; i++) {
        EXPECT_TRUE(eng_nav_line_free(&g_nav, cur, path[i]));
        via_ramp = via_ramp || (path[i][1] > 4.0F && path[i][1] < 8.0F);
        cur[0] = path[i][0];
        cur[1] = path[i][1];
    }
    EXPECT_TRUE(via_ramp);
    EXPECT_EQ(eng_nav_block_steep(&g_nav, NULL, NULL, 35.0F), 0U);
    EXPECT_EQ(eng_nav_block_steep(&g_nav, cliff_height, NULL, 90.0F), 0U);
    /* poste posé sur la falaise : déplacé sur la cellule libre la plus proche (moins d'1 m) */
    float moved[2] = {0.0F, 0.0F};
    ASSERT_TRUE(eng_nav_nearest_walkable(&g_nav, on_cliff, moved));
    EXPECT_TRUE(eng_nav_walkable(&g_nav, moved));
    EXPECT_LT(hypotf(moved[0] - on_cliff[0], moved[1] - on_cliff[1]), 1.0F);
    float same[2] = {0.0F, 0.0F};
    ASSERT_TRUE(eng_nav_nearest_walkable(&g_nav, on_slope, same));
    EXPECT_EQ(same[0], on_slope[0]); /* déjà praticable : inchangé */
    EXPECT_EQ(same[1], on_slope[1]);
    const float outside[2] = {100.0F, 0.0F};
    EXPECT_FALSE(eng_nav_nearest_walkable(&g_nav, outside, same));
    EXPECT_FALSE(eng_nav_nearest_walkable(NULL, on_cliff, same));
}

/* M6-03 : une poche de 3 x 3 cellules entourée de cellules bloquées est retirée (un ennemi y
 * resterait coincé) ; le reste de la grille, une seule grande région, est gardé. */
UTEST(navgrid, small_isolated_pockets_are_blocked_and_the_large_region_is_kept)
{
    EngAiWorld w;
    eng_aiw_init(&w);
    const float mn[2] = {-8.0F, -8.0F};
    const float mx[2] = {8.0F, 8.0F};
    ASSERT_TRUE(eng_nav_build(&g_nav, &w, mn, mx, 0.5F, 0.4F));
    /* anneau bloqué de 5 x 5 autour de la cellule (10, 10) : poche intérieure de 3 x 3 */
    for (uint32_t z = 8; z <= 12; z++) {
        for (uint32_t x = 8; x <= 12; x++) {
            if (x == 8 || x == 12 || z == 8 || z == 12) {
                g_nav.blocked[(z * g_nav.w) + x] = 1U;
            }
        }
    }
    const float in_pocket[2] = {g_nav.origin[0] + (10.5F * g_nav.cell),
                                g_nav.origin[1] + (10.5F * g_nav.cell)};
    const float outside[2] = {4.0F, 4.0F};
    ASSERT_TRUE(eng_nav_walkable(&g_nav, in_pocket));
    EXPECT_EQ(eng_nav_remove_islands(&g_nav, 16U), 9U); /* les 9 cellules de la poche */
    EXPECT_FALSE(eng_nav_walkable(&g_nav, in_pocket));
    EXPECT_TRUE(eng_nav_walkable(&g_nav, outside));
    EXPECT_EQ(eng_nav_remove_islands(&g_nav, 16U), 0U); /* plus rien à retirer */
    EXPECT_EQ(eng_nav_remove_islands(NULL, 16U), 0U);
    EXPECT_EQ(eng_nav_remove_islands(&g_nav, 1U), 0U);
}
