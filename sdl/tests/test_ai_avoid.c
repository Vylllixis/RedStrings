/* Évitement des arbres par les ennemis : marge de 1 m, contournement, fuite au contact. */
#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#include "engine/ai_avoid.h"
#include "utest.h"

static const EngAvoidCircle TREE = {10.0F, 0.0F, 0.4F};

UTEST(ai_avoid, far_from_any_tree_the_direction_is_kept)
{
    EngAvoidState st = {0};
    float dir[2] = {1.0F, 0.0F};
    const float pos[2] = {7.0F, 0.0F}; /* écart : 3 - 0,4 - 0,4 = 2,2 m */
    EXPECT_FALSE(eng_avoid_steer(&st, pos, 0.4F, &TREE, 1U, dir));
    EXPECT_EQ(dir[0], 1.0F);
    EXPECT_EQ(dir[1], 0.0F);
    EXPECT_NEAR(eng_avoid_gap(pos, 0.4F, &TREE, 1U, NULL), 2.2F, 1e-5F);
    EXPECT_TRUE(isinf(eng_avoid_gap(pos, 0.4F, &TREE, 0U, NULL)));
    /* sans obstacle, arguments invalides : rien ne change */
    EXPECT_FALSE(eng_avoid_steer(&st, pos, 0.4F, NULL, 0U, dir));
    EXPECT_FALSE(eng_avoid_steer(NULL, pos, 0.4F, &TREE, 1U, dir));
    float bad[2] = {NAN, 0.0F};
    EXPECT_FALSE(eng_avoid_steer(&st, pos, 0.4F, &TREE, 1U, bad));
    EXPECT_FALSE(eng_avoid_steer(&st, bad, 0.4F, &TREE, 1U, dir));
}

UTEST(ai_avoid, the_closer_to_the_tree_the_less_it_walks_toward_it)
{
    /* il marche droit sur l'arbre : la composante vers l'arbre diminue quand l'écart diminue */
    float previous = 1.0F;
    for (int k = 9; k >= 1; k--) {
        EngAvoidState st = {0};
        const float gap = 0.1F * (float)k;
        const float pos[2] = {10.0F - 0.8F - gap, 0.0F};
        float dir[2] = {1.0F, 0.0F};
        ASSERT_TRUE(eng_avoid_steer(&st, pos, 0.4F, &TREE, 1U, dir));
        EXPECT_NEAR(hypotf(dir[0], dir[1]), 1.0F, 1e-4F); /* même vitesse */
        EXPECT_LT(dir[0], previous);                      /* de moins en moins vers l'arbre */
        EXPECT_GT(fabsf(dir[1]), 0.0F);                   /* un pas de côté : il contourne */
        EXPECT_FALSE(st.fleeing);
        previous = dir[0];
    }
    EXPECT_LT(previous, 0.2F); /* à 10 cm : presque plus rien vers l'arbre */
    /* il s'éloigne ou longe : rien ne change, même tout près */
    EngAvoidState st = {0};
    const float pos[2] = {9.0F, 0.0F};
    float leaving[2] = {-1.0F, 0.0F};
    EXPECT_FALSE(eng_avoid_steer(&st, pos, 0.4F, &TREE, 1U, leaving));
    float along[2] = {0.0F, 1.0F};
    EXPECT_FALSE(eng_avoid_steer(&st, pos, 0.4F, &TREE, 1U, along));
    /* il garde le côté où il allait déjà */
    float left[2] = {0.8F, 0.6F};
    ASSERT_TRUE(eng_avoid_steer(&st, pos, 0.4F, &TREE, 1U, left));
    EXPECT_GT(left[1], 0.6F);
    float right[2] = {0.8F, -0.6F};
    ASSERT_TRUE(eng_avoid_steer(&st, pos, 0.4F, &TREE, 1U, right));
    EXPECT_LT(right[1], -0.6F);
}

UTEST(ai_avoid, touching_a_tree_it_backs_off_at_least_one_metre)
{
    EngAvoidState st = {0};
    float pos[2] = {9.25F, 0.0F}; /* chevauche l'arbre de 5 cm */
    float dir[2] = {1.0F, 0.0F};
    ASSERT_TRUE(eng_avoid_steer(&st, pos, 0.4F, &TREE, 1U, dir));
    EXPECT_TRUE(st.fleeing);
    EXPECT_NEAR(dir[0], -1.0F, 1e-5F); /* droit dehors */
    /* tant qu'il n'est pas revenu à 1 m, il fuit, quoi que veuille son IA */
    int steps = 0;
    while (st.fleeing && steps < 200) {
        float want[2] = {1.0F, 0.0F}; /* l'IA insiste vers l'arbre */
        (void)eng_avoid_steer(&st, pos, 0.4F, &TREE, 1U, want);
        if (!st.fleeing) {
            break;
        }
        EXPECT_NEAR(want[0], -1.0F, 1e-5F);
        pos[0] += want[0] * 0.05F;
        pos[1] += want[1] * 0.05F;
        steps++;
    }
    EXPECT_FALSE(st.fleeing);
    EXPECT_GE(eng_avoid_gap(pos, 0.4F, &TREE, 1U, NULL), 1.0F - 1e-4F);
    /* à l'arrêt contre l'arbre (direction nulle) : il s'éloigne aussi */
    EngAvoidState idle = {0};
    const float stuck[2] = {9.3F, 0.0F};
    float none[2] = {0.0F, 0.0F};
    ASSERT_TRUE(eng_avoid_steer(&idle, stuck, 0.4F, &TREE, 1U, none));
    EXPECT_NEAR(none[0], -1.0F, 1e-5F);
}

UTEST(ai_avoid, an_agent_walking_at_a_tree_goes_around_it_and_reaches_the_other_side)
{
    /* de (0, 0) vers (20, 0), un arbre en plein milieu : il le contourne sans jamais le toucher */
    const EngAvoidCircle trees[2] = {{10.0F, 0.0F, 0.4F}, {10.3F, 1.6F, 0.4F}};
    EngAvoidState st = {0};
    float pos[2] = {0.0F, 0.05F};
    float closest = INFINITY;
    int ticks = 0;
    for (; ticks < 1200 && hypotf(pos[0] - 20.0F, pos[1]) > 0.3F; ticks++) {
        const float to[2] = {20.0F - pos[0], -pos[1]};
        const float len = hypotf(to[0], to[1]);
        float dir[2] = {to[0] / len, to[1] / len};
        (void)eng_avoid_steer(&st, pos, 0.4F, trees, 2U, dir);
        pos[0] += dir[0] * (4.0F / 60.0F); /* 4 m/s */
        pos[1] += dir[1] * (4.0F / 60.0F);
        (void)eng_avoid_push_out(pos, 0.4F, trees, 2U);
        closest = fminf(closest, eng_avoid_gap(pos, 0.4F, trees, 2U, NULL));
    }
    EXPECT_LT(hypotf(pos[0] - 20.0F, pos[1]), 0.31F); /* arrivé */
    EXPECT_LT(ticks, 600);                            /* sans rester bloqué (5 s tout droit) */
    EXPECT_GE(closest, -1e-4F);                       /* jamais dans un tronc */
}

UTEST(ai_avoid, push_out_never_leaves_a_body_inside_a_trunk)
{
    float pos[2] = {10.1F, 0.1F};
    EXPECT_TRUE(eng_avoid_push_out(pos, 0.4F, &TREE, 1U));
    EXPECT_NEAR(hypotf(pos[0] - 10.0F, pos[1]), 0.8F, 1e-4F);
    EXPECT_FALSE(eng_avoid_push_out(pos, 0.4F, &TREE, 1U)); /* déjà dehors */
    float center[2] = {10.0F, 0.0F};                        /* pile au centre : sorti vers +x */
    EXPECT_TRUE(eng_avoid_push_out(center, 0.4F, &TREE, 1U));
    EXPECT_NEAR(center[0], 10.8F, 1e-4F);
    EXPECT_FALSE(eng_avoid_push_out(NULL, 0.4F, &TREE, 1U));
    EXPECT_FALSE(eng_avoid_push_out(pos, 0.4F, NULL, 1U));
}
