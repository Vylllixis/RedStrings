/* Butin des boss : un palier d'équipement de 0 à 4, plus fort à chaque boss, jamais perdu. */
#include <string.h>

#include "engine/gear.h"
#include "utest.h"

UTEST(eng_gear, each_tier_hits_harder_and_protects_better)
{
    EXPECT_NEAR(eng_gear_damage(0), 1.0F, 0.0F); /* le départ ne change rien */
    EXPECT_NEAR(eng_gear_armor(0), 1.0F, 0.0F);
    EXPECT_NEAR(eng_gear_block(0), 0.2F, 0.0F); /* la garde d'origine */
    for (uint32_t t = 1; t < ENG_GEAR_TIERS; t++) {
        EXPECT_GT(eng_gear_damage(t), eng_gear_damage(t - 1U));
        EXPECT_LT(eng_gear_armor(t), eng_gear_armor(t - 1U));
        EXPECT_LT(eng_gear_block(t), eng_gear_block(t - 1U));
        EXPECT_GT(eng_gear_armor(t), 0.0F);
        EXPECT_GT(eng_gear_block(t), 0.0F);
    }
    EXPECT_NEAR(eng_gear_damage(4), 3.2F, 1e-6F);
    EXPECT_NEAR(eng_gear_damage(99), 3.2F, 1e-6F); /* borné au dernier palier */
    EXPECT_EQ(eng_gear_clamp(99), 4U);
}

UTEST(eng_gear, every_piece_has_a_name_and_tiers_are_named_after_their_boss)
{
    for (uint32_t t = 0; t < ENG_GEAR_TIERS; t++) {
        for (int s = 0; s < ENG_GEAR_SLOTS; s++) {
            EXPECT_GT(strlen(eng_gear_name(t, (EngGearSlot)s)), 5U);
        }
    }
    EXPECT_STREQ(eng_gear_name(1, ENG_GEAR_WEAPON), "Épée du chevalier");
    EXPECT_STREQ(eng_gear_name(4, ENG_GEAR_SHIELD), "Écu du dragon");
    EXPECT_STREQ(eng_gear_tier_name(2), "du givre");
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    EXPECT_STREQ(eng_gear_name(1, (EngGearSlot)7), "?");
}

UTEST(eng_gear, a_reward_only_raises_the_tier)
{
    uint32_t tier = 0;
    EXPECT_TRUE(eng_gear_give(&tier, 2)); /* boss de la montagne d'abord : palier 2 */
    EXPECT_EQ(tier, 2U);
    EXPECT_FALSE(eng_gear_give(&tier, 1)); /* puis celui de la plaine : rien de mieux */
    EXPECT_EQ(tier, 2U);
    EXPECT_FALSE(eng_gear_give(&tier, 2));
    EXPECT_TRUE(eng_gear_give(&tier, 9)); /* borné : palier 4 */
    EXPECT_EQ(tier, 4U);
    EXPECT_FALSE(eng_gear_give(NULL, 3));
}
