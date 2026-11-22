/* Créatures en blocs : cheval (marche, effondrement) et dragon (gueule, flammes). */
#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#include "engine/creature.h"
#include "utest.h"

static float top_of(const EngCreaturePart *p, uint32_t n)
{
    float top = -1e9F;
    for (uint32_t i = 0; i < n; i++) {
        top = fmaxf(top, p[i].pos[1]);
    }
    return top;
}

static float front_of(const EngCreaturePart *p, uint32_t n)
{
    float front = 1e9F; /* avant = -Z */
    for (uint32_t i = 0; i < n; i++) {
        front = fminf(front, p[i].pos[2] - p[i].half[2]);
    }
    return front;
}

static bool all_valid(const EngCreaturePart *p, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        for (int a = 0; a < 3; a++) {
            if (!isfinite(p[i].pos[a]) || !isfinite(p[i].rot[a]) || !(p[i].half[a] > 0.0F)) {
                return false;
            }
        }
        if (p[i].color >= (uint8_t)ENG_CREATURE_COLORS) {
            return false;
        }
    }
    return true;
}

UTEST(eng_creature, the_horse_stands_gallops_and_lies_down)
{
    EngCreaturePart still[ENG_CREATURE_MAX_PARTS];
    EngCreaturePart run_a[ENG_CREATURE_MAX_PARTS];
    EngCreaturePart run_b[ENG_CREATURE_MAX_PARTS];
    EngCreaturePart down[ENG_CREATURE_MAX_PARTS];
    const uint32_t n = eng_creature_horse(0.0F, 0.0F, 0.0F, still, ENG_CREATURE_MAX_PARTS);
    ASSERT_GE(n, 12U); /* tronc, encolure, tête, crinière, queue, 4 jambes, 4 sabots, selle */
    EXPECT_TRUE(all_valid(still, n));
    EXPECT_GT(top_of(still, n), 2.0F);    /* la tête au-dessus de 2 m */
    EXPECT_LT(front_of(still, n), -1.4F); /* la tête devant (vers -Z) */
    ASSERT_EQ(eng_creature_horse(0.1F, 1.0F, 0.0F, run_a, ENG_CREATURE_MAX_PARTS), n);
    ASSERT_EQ(eng_creature_horse(0.23F, 1.0F, 0.0F, run_b, ENG_CREATURE_MAX_PARTS), n);
    float leg_motion = 0.0F;
    for (uint32_t i = 0; i < n; i++) {
        leg_motion = fmaxf(leg_motion, fabsf(run_a[i].pos[2] - run_b[i].pos[2]));
    }
    EXPECT_GT(leg_motion, 0.1F); /* au galop, les jambes bougent d'un instant à l'autre */
    EngCreaturePart still_later[ENG_CREATURE_MAX_PARTS];
    ASSERT_EQ(eng_creature_horse(5.0F, 0.0F, 0.0F, still_later, ENG_CREATURE_MAX_PARTS), n);
    for (uint32_t i = 0; i < n; i++) { /* à l'arrêt, rien ne bouge */
        EXPECT_NEAR(still_later[i].pos[2], still[i].pos[2], 1e-5F);
    }
    ASSERT_EQ(eng_creature_horse(0.0F, 0.0F, 1.0F, down, ENG_CREATURE_MAX_PARTS), n);
    EXPECT_TRUE(all_valid(down, n));
    EXPECT_LT(top_of(down, n), 1.0F); /* couché : plus rien au-dessus d'un mètre */
    EXPECT_NEAR(eng_creature_horse_saddle(0.0F), 1.62F, 1e-6F);
    EXPECT_NEAR(eng_creature_horse_saddle(1.0F), 0.0F, 1e-6F);
    EXPECT_GT(eng_creature_horse_saddle(0.5F), 0.5F);
    /* liste trop courte ou absente : jamais de débordement */
    EXPECT_EQ(eng_creature_horse(0.0F, 0.0F, 0.0F, still, 5U), 5U);
    EXPECT_EQ(eng_creature_horse(0.0F, 0.0F, 0.0F, NULL, 5U), 0U);
    ASSERT_EQ(eng_creature_horse(NAN, NAN, NAN, still_later, ENG_CREATURE_MAX_PARTS), n);
    EXPECT_TRUE(all_valid(still_later, n)); /* non fini : traité comme 0 */
}

static float span_of(const EngCreaturePart *p, uint32_t n)
{
    float reach = 0.0F;
    for (uint32_t i = 0; i < n; i++) {
        reach = fmaxf(reach, fabsf(p[i].pos[0]) + (p[i].half[0] * fabsf(cosf(p[i].rot[2]))));
    }
    return 2.0F * reach;
}

/* Le modèle est à l'échelle 1 ; le jeu le dessine treize fois plus grand. */
UTEST(eng_creature, the_dragon_bites_down_to_the_ground_rears_and_breathes_a_jet_of_fire)
{
    EngCreaturePart rest[ENG_CREATURE_MAX_PARTS];
    EngCreaturePart other[ENG_CREATURE_MAX_PARTS];
    const EngDragonPose still = {0};
    const uint32_t n = eng_creature_dragon(&still, rest, ENG_CREATURE_MAX_PARTS);
    ASSERT_GE(n, 30U);
    ASSERT_LE(n, ENG_CREATURE_MAX_PARTS - 7U); /* la place des flammes et du bout des ailes */
    EXPECT_TRUE(all_valid(rest, n));
    EXPECT_GT(top_of(rest, n), 3.3F);  /* la tête à plus de 3,3 m */
    EXPECT_GT(span_of(rest, n), 8.0F); /* envergure, ailes à demi repliées */
    float head[3];
    float mouth[3];
    eng_creature_dragon_head(&still, head);
    EXPECT_NEAR(head[1], 3.4F, 1e-4F);
    EXPECT_NEAR(head[2], -3.9F, 1e-4F);
    /* pose absente : au repos */
    ASSERT_EQ(eng_creature_dragon(NULL, other, ENG_CREATURE_MAX_PARTS), n);
    for (uint32_t i = 0; i < n; i++) {
        EXPECT_NEAR(other[i].pos[1], rest[i].pos[1], 1e-6F);
    }
    /* morsure : la tête s'abat JUSQU'AU SOL (à l'échelle du jeu : de 44 m à terre), là où
     * `lunge` le demande : à ses pieds (2,3 m devant son centre) ou au bout de son cou (5,6 m) */
    const EngDragonPose bite = {.bite = 1.0F, .lunge = 1.0F};
    ASSERT_EQ(eng_creature_dragon(&bite, other, ENG_CREATURE_MAX_PARTS), n);
    EXPECT_TRUE(all_valid(other, n));
    eng_creature_dragon_head(&bite, head);
    eng_creature_dragon_mouth(&bite, mouth);
    EXPECT_NEAR(head[1], 0.5F, 1e-4F);
    EXPECT_NEAR(head[2], -ENG_DRAGON_BITE_FAR, 1e-4F);
    EXPECT_NEAR(mouth[1], 0.0F, 1e-4F); /* la gueule touche le sol */
    EXPECT_LT(front_of(other, n), front_of(rest, n) - 0.3F);
    const EngDragonPose bite_near = {.bite = 1.0F, .lunge = 0.0F};
    eng_creature_dragon_head(&bite_near, head);
    EXPECT_NEAR(head[1], 0.5F, 1e-4F);
    EXPECT_NEAR(head[2], -ENG_DRAGON_BITE_NEAR, 1e-4F);
    const EngDragonPose bite_mid = {.bite = 1.0F, .lunge = 0.5F};
    eng_creature_dragon_head(&bite_mid, head);
    EXPECT_NEAR(head[2], -0.5F * (ENG_DRAGON_BITE_NEAR + ENG_DRAGON_BITE_FAR), 1e-4F);
    const EngDragonPose half_open = {.bite = 0.5F, .lunge = 1.0F}; /* à mi-course */
    eng_creature_dragon_head(&half_open, head);
    EXPECT_NEAR(head[2], 0.5F * (-3.9F - ENG_DRAGON_BITE_FAR), 1e-4F);
    eng_creature_dragon_head(&bite, head);
    const EngDragonPose too_much = {.bite = 7.0F}; /* hors bornes : comme 1 */
    float clamped[3];
    eng_creature_dragon_head(&too_much, clamped);
    EXPECT_NEAR(clamped[1], head[1], 1e-6F);
    /* cabré : les pattes avant se lèvent, les pattes arrière restent au sol, la tête monte */
    const EngDragonPose rear = {.rear = 1.0F};
    ASSERT_EQ(eng_creature_dragon(&rear, other, ENG_CREATURE_MAX_PARTS), n);
    float paw[3];
    for (uint32_t i = 0; i < ENG_DRAGON_PAWS; i++) {
        eng_creature_dragon_paw(&rear, i, paw);
        EXPECT_NEAR(paw[1], i < 2U ? 0.9F : 0.0F, 1e-4F);
        EXPECT_NEAR(fabsf(paw[0]), 0.8F, 1e-4F);
        eng_creature_dragon_paw(&still, i, paw);
        EXPECT_NEAR(paw[1], 0.0F, 1e-6F); /* au repos : quatre pattes au sol */
    }
    eng_creature_dragon_head(&rear, head);
    EXPECT_GT(head[1], 3.9F);
    /* souffle au sol : cinq boîtes de feu, les dernières, à l'horizontale devant la gueule */
    const EngDragonPose fire = {.flame = 1.0F};
    const uint32_t nf = eng_creature_dragon(&fire, other, ENG_CREATURE_MAX_PARTS);
    ASSERT_EQ(nf, n + 5U);
    EXPECT_TRUE(all_valid(other, nf));
    eng_creature_dragon_mouth(&fire, mouth);
    EXPECT_LT(front_of(other, nf), mouth[2] - (0.95F * ENG_DRAGON_JET)); /* le jet : 9,5 m */
    bool hot = true;
    for (uint32_t i = n; i < nf; i++) {
        hot = hot && other[i].color != (uint8_t)ENG_CREATURE_DARK &&
              other[i].color != (uint8_t)ENG_CREATURE_BONE;
        EXPECT_NEAR(other[i].pos[1], mouth[1], 1e-4F); /* à l'horizontale */
        if (i > n) {
            EXPECT_LT(other[i].pos[2], other[i - 1U].pos[2]);   /* de plus en plus loin... */
            EXPECT_GT(other[i].half[0], other[i - 1U].half[0]); /* ... et de plus en plus large */
        }
    }
    EXPECT_TRUE(hot);
    EXPECT_EQ(other[n].color, (uint8_t)ENG_CREATURE_YELLOW); /* cœur de la flamme */
    const EngDragonPose half_fire = {.flame = 0.5F}; /* jet à moitié : deux fois plus court */
    EngCreaturePart shorter[ENG_CREATURE_MAX_PARTS];
    ASSERT_EQ(eng_creature_dragon(&half_fire, shorter, ENG_CREATURE_MAX_PARTS), nf);
    EXPECT_NEAR(mouth[2] - shorter[nf - 1U].pos[2], 0.5F * (mouth[2] - other[nf - 1U].pos[2]),
                1e-3F);
    EXPECT_EQ(eng_creature_dragon(&fire, other, 10U), 10U); /* liste trop courte */
    EXPECT_EQ(eng_creature_dragon(&fire, NULL, 10U), 0U);
    /* valeurs non finies : comme 0 */
    const EngDragonPose broken = {NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN};
    ASSERT_EQ(eng_creature_dragon(&broken, other, ENG_CREATURE_MAX_PARTS), n);
    EXPECT_TRUE(all_valid(other, n));
    /* repères : rien n'est écrit sans destination ni pour une patte qui n'existe pas */
    float untouched[3] = {7.0F, 8.0F, 9.0F};
    eng_creature_dragon_paw(&still, ENG_DRAGON_PAWS, untouched);
    EXPECT_NEAR(untouched[0], 7.0F, 1e-9F);
    eng_creature_dragon_paw(&still, 0U, NULL);
    eng_creature_dragon_head(&still, NULL);
    eng_creature_dragon_mouth(&still, NULL);
}

UTEST(eng_creature, the_flying_dragon_tucks_its_legs_spreads_its_wings_and_breathes_downwards)
{
    EngCreaturePart ground[ENG_CREATURE_MAX_PARTS];
    EngCreaturePart air[ENG_CREATURE_MAX_PARTS];
    EngCreaturePart later[ENG_CREATURE_MAX_PARTS];
    const EngDragonPose still = {0};
    const EngDragonPose fly = {.flight = 1.0F};
    const uint32_t n = eng_creature_dragon(&still, ground, ENG_CREATURE_MAX_PARTS);
    const uint32_t nf = eng_creature_dragon(&fly, air, ENG_CREATURE_MAX_PARTS);
    ASSERT_EQ(nf, n + 2U); /* le bout de chaque aile, rentré au sol, se déploie */
    EXPECT_TRUE(all_valid(air, nf));
    EXPECT_GT(span_of(air, nf), 11.5F); /* 12 m d'envergure : 160 m à l'échelle du jeu */
    EXPECT_GT(span_of(air, nf), span_of(ground, n) + 2.0F);
    float paw[3];
    float head[3];
    for (uint32_t i = 0; i < ENG_DRAGON_PAWS; i++) { /* pattes repliées sous le ventre */
        eng_creature_dragon_paw(&fly, i, paw);
        EXPECT_NEAR(paw[1], 0.75F, 1e-4F);
    }
    eng_creature_dragon_head(&fly, head); /* cou tendu : la tête plus basse, plus en avant */
    EXPECT_NEAR(head[1], 2.5F, 1e-4F);
    EXPECT_NEAR(head[2], -4.6F, 1e-4F);
    /* à mi-chemin (décollage) : entre les deux poses */
    const EngDragonPose half = {.flight = 0.5F};
    eng_creature_dragon_paw(&half, 0U, paw);
    EXPECT_NEAR(paw[1], 0.375F, 1e-4F);
    /* en vol, on ne se cabre pas */
    const EngDragonPose rear_fly = {.flight = 1.0F, .rear = 1.0F};
    eng_creature_dragon_paw(&rear_fly, 0U, paw);
    EXPECT_NEAR(paw[1], 0.75F, 1e-4F);
    ASSERT_EQ(eng_creature_dragon(&rear_fly, later, ENG_CREATURE_MAX_PARTS), nf);
    for (uint32_t i = 0; i < nf; i++) { /* aucune boîte ne bouge : ni le corps, ni le cou */
        EXPECT_NEAR(later[i].pos[1], air[i].pos[1], 1e-6F);
        EXPECT_NEAR(later[i].rot[0], air[i].rot[0], 1e-6F);
    }
    /* les ailes battent : le bout de l'aile change de hauteur avec le temps */
    const EngDragonPose fly_later = {.flight = 1.0F, .time = 0.5F};
    ASSERT_EQ(eng_creature_dragon(&fly_later, later, ENG_CREATURE_MAX_PARTS), nf);
    float wing = 0.0F;
    for (uint32_t i = 0; i < nf; i++) {
        if (air[i].color == (uint8_t)ENG_CREATURE_DARK && fabsf(air[i].pos[0]) > 2.0F) {
            wing = fmaxf(wing, fabsf(later[i].pos[1] - air[i].pos[1]));
        }
    }
    EXPECT_GT(wing, 0.8F);
    /* il souffle vers le sol : le jet plonge de `aim` sous l'horizontale */
    const EngDragonPose dive = {.flight = 1.0F, .flame = 1.0F, .aim = 0.8F};
    const uint32_t nd = eng_creature_dragon(&dive, later, ENG_CREATURE_MAX_PARTS);
    ASSERT_EQ(nd, nf + 5U);
    float mouth[3];
    eng_creature_dragon_mouth(&dive, mouth);
    for (uint32_t i = nf; i < nd; i++) {
        EXPECT_NEAR(later[i].rot[0], -0.8F, 1e-5F);
        const float along = hypotf(mouth[1] - later[i].pos[1], mouth[2] - later[i].pos[2]);
        EXPECT_NEAR(mouth[1] - later[i].pos[1], along * sinf(0.8F), 1e-3F); /* sur l'axe du jet */
        EXPECT_NEAR(mouth[2] - later[i].pos[2], along * cosf(0.8F), 1e-3F);
    }
    EXPECT_NEAR(mouth[1] - later[nd - 1U].pos[1], 0.9F * ENG_DRAGON_JET * sinf(0.8F), 1e-3F);
    const EngDragonPose steep = {.flight = 1.0F, .flame = 1.0F, .aim = 9.0F}; /* borné à 1,4 rad */
    ASSERT_EQ(eng_creature_dragon(&steep, later, ENG_CREATURE_MAX_PARTS), nd);
    EXPECT_NEAR(later[nd - 1U].rot[0], -1.4F, 1e-5F);
    /* le pas : les pattes se lèvent en diagonale ; à l'arrêt, toutes au sol */
    const EngDragonPose step = {.stride = 1.0F, .walk = 1.5707963F};
    float lifted[4];
    for (uint32_t i = 0; i < ENG_DRAGON_PAWS; i++) {
        eng_creature_dragon_paw(&step, i, paw);
        lifted[i] = paw[1];
    }
    EXPECT_NEAR(lifted[0], 0.35F, 1e-4F);
    EXPECT_NEAR(lifted[3], 0.35F, 1e-4F);
    EXPECT_NEAR(lifted[1], 0.0F, 1e-4F);
    EXPECT_NEAR(lifted[2], 0.0F, 1e-4F);
    const EngDragonPose no_stride = {.walk = 1.5707963F};
    eng_creature_dragon_paw(&no_stride, 0U, paw);
    EXPECT_NEAR(paw[1], 0.0F, 1e-6F);
}
