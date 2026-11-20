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

UTEST(eng_creature, the_dragon_bites_forward_and_breathes_fire_up_to_nine_metres)
{
    EngCreaturePart rest[ENG_CREATURE_MAX_PARTS];
    EngCreaturePart bite[ENG_CREATURE_MAX_PARTS];
    EngCreaturePart fire[ENG_CREATURE_MAX_PARTS];
    const uint32_t n = eng_creature_dragon(0.0F, 0.0F, 0.0F, rest, ENG_CREATURE_MAX_PARTS);
    ASSERT_GE(n, 25U);
    ASSERT_LE(n, ENG_CREATURE_MAX_PARTS - 3U); /* la place des flammes */
    EXPECT_TRUE(all_valid(rest, n));
    EXPECT_GT(top_of(rest, n), 3.3F); /* la tête à plus de 3,3 m */
    float span = 0.0F;
    for (uint32_t i = 0; i < n; i++) {
        span = fmaxf(span, fabsf(rest[i].pos[0]) + rest[i].half[0]);
    }
    EXPECT_GT(2.0F * span, 8.0F); /* envergure */
    ASSERT_EQ(eng_creature_dragon(0.0F, 1.0F, 0.0F, bite, ENG_CREATURE_MAX_PARTS), n);
    EXPECT_LT(front_of(bite, n), front_of(rest, n) - 0.3F); /* la tête avance pour mordre */
    EXPECT_LT(top_of(bite, n) - top_of(rest, n), 0.01F);
    float head_drop = 0.0F; /* ... et s'abat : la boîte la plus en avant descend de 2 m */
    for (uint32_t i = 0; i < n; i++) {
        if (rest[i].pos[2] < -3.5F && rest[i].color == (uint8_t)ENG_CREATURE_RED) {
            head_drop = fmaxf(head_drop, rest[i].pos[1] - bite[i].pos[1]);
        }
    }
    EXPECT_GT(head_drop, 2.0F);
    const uint32_t nf = eng_creature_dragon(0.0F, 0.0F, 1.0F, fire, ENG_CREATURE_MAX_PARTS);
    EXPECT_EQ(nf, n + 3U); /* trois boîtes de flammes */
    EXPECT_TRUE(all_valid(fire, nf));
    EXPECT_LT(front_of(fire, nf), -12.0F); /* 9 m devant la gueule (à 4 m du centre) */
    bool hot = false;
    for (uint32_t i = n; i < nf; i++) {
        hot = hot || fire[i].color == (uint8_t)ENG_CREATURE_ORANGE ||
              fire[i].color == (uint8_t)ENG_CREATURE_YELLOW;
    }
    EXPECT_TRUE(hot);
    /* les ailes battent : une boîte d'aile change de hauteur avec le temps */
    EngCreaturePart later[ENG_CREATURE_MAX_PARTS];
    ASSERT_EQ(eng_creature_dragon(0.7F, 0.0F, 0.0F, later, ENG_CREATURE_MAX_PARTS), n);
    float wing = 0.0F;
    for (uint32_t i = 0; i < n; i++) {
        if (rest[i].color == (uint8_t)ENG_CREATURE_DARK) {
            wing = fmaxf(wing, fabsf(later[i].pos[1] - rest[i].pos[1]));
        }
    }
    EXPECT_GT(wing, 0.2F);
    EXPECT_EQ(eng_creature_dragon(0.0F, 0.0F, 1.0F, fire, 10U), 10U); /* liste trop courte */
}
