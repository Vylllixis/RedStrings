#include "engine/progression.h"
#include "utest.h"

UTEST(progression, levels_follow_the_documented_curve)
{
    EXPECT_EQ(eng_prog_need(1U), 100U);
    EXPECT_EQ(eng_prog_need(2U), 150U);
    EXPECT_EQ(eng_prog_need(19U), 1000U);
    EXPECT_EQ(eng_prog_need(20U), 0U);
    EXPECT_EQ(eng_prog_need(0U), 0U);
    EngProgress p;
    eng_prog_init(&p);
    EXPECT_EQ(eng_prog_add_xp(&p, 99U), 0U);
    EXPECT_EQ(p.level, 1U);
    EXPECT_EQ(eng_prog_add_xp(&p, 1U), 1U); /* 100 pile : niveau 2 */
    EXPECT_EQ(p.level, 2U);
    EXPECT_EQ(p.xp, 0U);
    EXPECT_EQ(eng_prog_add_xp(&p, 150U + 200U + 25U), 2U); /* deux niveaux d'un coup */
    EXPECT_EQ(p.level, 4U);
    EXPECT_EQ(p.xp, 25U);
    EXPECT_EQ(p.total_xp, 475U);
    EXPECT_TRUE(eng_prog_valid(&p));
}

UTEST(progression, the_max_level_caps_and_stats_scale)
{
    EngProgress p;
    eng_prog_init(&p);
    EngProgressStats s = eng_prog_stats(&p);
    EXPECT_EQ(s.hp_max, 100.0F);
    EXPECT_EQ(s.stamina_max, 100.0F);
    EXPECT_EQ(s.damage_scale, 1.0F);
    EXPECT_EQ(eng_prog_add_xp(&p, 1000000U), ENG_PROG_MAX_LEVEL - 1U);
    EXPECT_EQ(p.level, ENG_PROG_MAX_LEVEL);
    EXPECT_EQ(p.xp, 0U);
    EXPECT_EQ(eng_prog_add_xp(&p, 500U), 0U);
    EXPECT_EQ(p.total_xp, 1000500U);
    s = eng_prog_stats(&p);
    EXPECT_EQ(s.hp_max, 290.0F);
    EXPECT_EQ(s.stamina_max, 195.0F);
    EXPECT_NEAR(s.damage_scale, 1.95F, 1e-6F);
    p.total_xp = UINT32_MAX - 3U;
    (void)eng_prog_add_xp(&p, 10U);
    EXPECT_EQ(p.total_xp, UINT32_MAX); /* saturé, pas de débordement */
}

UTEST(progression, kill_rewards_and_invalid_states)
{
    EXPECT_EQ(eng_prog_kill_xp(60.0F), 30U);
    EXPECT_EQ(eng_prog_kill_xp(5.0F), 10U);
    EXPECT_EQ(eng_prog_kill_xp(16.0F), 10U); /* 8 arrondi au plancher de 10 */
    EXPECT_EQ(eng_prog_kill_xp(21.0F), 11U); /* 10,5 arrondi à 11 */
    EXPECT_EQ(eng_prog_kill_xp(-1.0F), 10U);
    EXPECT_EQ(eng_prog_kill_xp(1e30F), 100000U);
    EngProgress bad = {0U, 0U, 0U};
    EXPECT_FALSE(eng_prog_valid(&bad));
    EXPECT_EQ(eng_prog_add_xp(&bad, 500U), 0U); /* état abîmé : rien n'est appliqué */
    bad = (EngProgress){3U, 200U, 1000U};       /* xp >= need(3) = 200 */
    EXPECT_FALSE(eng_prog_valid(&bad));
    bad = (EngProgress){3U, 50U, 10U}; /* total < xp */
    EXPECT_FALSE(eng_prog_valid(&bad));
    bad = (EngProgress){ENG_PROG_MAX_LEVEL, 5U, 10U};
    EXPECT_FALSE(eng_prog_valid(&bad));
    EXPECT_EQ(eng_prog_stats(&bad).hp_max, 100.0F); /* invalide : stats du niveau 1 */
    EXPECT_FALSE(eng_prog_valid(NULL));
}
