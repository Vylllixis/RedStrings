/* Équipement du joueur (M8-08). */
#include <string.h>

#include "engine/equipment.h"
#include "utest.h"

UTEST(equipment, a_new_game_starts_with_the_sword_and_shield_in_hand)
{
    EngEquipment e;
    eng_equip_init(&e);
    EXPECT_TRUE(eng_equip_valid(&e));
    EXPECT_EQ(e.equipped, (uint32_t)ENG_WEAPON_SWORD);
    EXPECT_TRUE(eng_equip_owns(&e, ENG_WEAPON_SWORD));
    EXPECT_FALSE(eng_equip_owns(&e, ENG_WEAPON_GREATSWORD));
    EXPECT_EQ(eng_equip_next(&e), (uint32_t)ENG_WEAPON_SWORD); /* une seule arme : reste en main */
}

UTEST(equipment, the_greatsword_is_given_once_and_selected_only_when_owned)
{
    EngEquipment e;
    eng_equip_init(&e);
    EXPECT_FALSE(eng_equip_select(&e, ENG_WEAPON_GREATSWORD)); /* pas encore possédé */
    EXPECT_EQ(e.equipped, (uint32_t)ENG_WEAPON_SWORD);
    EXPECT_TRUE(eng_equip_give(&e, ENG_WEAPON_GREATSWORD));
    EXPECT_FALSE(eng_equip_give(&e, ENG_WEAPON_GREATSWORD)); /* déjà là */
    EXPECT_TRUE(eng_equip_select(&e, ENG_WEAPON_GREATSWORD));
    EXPECT_EQ(e.equipped, (uint32_t)ENG_WEAPON_GREATSWORD);
    EXPECT_FALSE(eng_equip_give(&e, ENG_WEAPON_COUNT));
    EXPECT_FALSE(eng_equip_select(&e, 99U));
    EXPECT_FALSE(eng_equip_give(NULL, ENG_WEAPON_SWORD));
}

UTEST(equipment, next_cycles_through_owned_weapons_only)
{
    EngEquipment e;
    eng_equip_init(&e);
    (void)eng_equip_give(&e, ENG_WEAPON_GREATSWORD);
    EXPECT_EQ(eng_equip_next(&e), (uint32_t)ENG_WEAPON_GREATSWORD);
    EXPECT_EQ(eng_equip_next(&e), (uint32_t)ENG_WEAPON_SWORD);
    EXPECT_EQ(eng_equip_next(&e), (uint32_t)ENG_WEAPON_GREATSWORD);
}

UTEST(equipment, validity_rejects_unknown_bits_and_an_unowned_weapon_in_hand)
{
    EngEquipment e = {.owned = 1U << ENG_WEAPON_SWORD, .equipped = ENG_WEAPON_GREATSWORD};
    EXPECT_FALSE(eng_equip_valid(&e)); /* en main sans la posséder */
    e = (EngEquipment){.owned = 0U, .equipped = ENG_WEAPON_SWORD};
    EXPECT_FALSE(eng_equip_valid(&e)); /* aucune arme */
    e = (EngEquipment){.owned = (1U << ENG_WEAPON_SWORD) | (1U << 7U), .equipped = 0U};
    EXPECT_FALSE(eng_equip_valid(&e)); /* arme inconnue */
    EXPECT_FALSE(eng_equip_valid(NULL));
}

UTEST(equipment, each_weapon_names_its_attacks_and_the_greatsword_has_no_shield)
{
    const EngWeaponDef *s = eng_weapon_def(ENG_WEAPON_SWORD);
    const EngWeaponDef *g = eng_weapon_def(ENG_WEAPON_GREATSWORD);
    ASSERT_TRUE(s != NULL && g != NULL);
    EXPECT_EQ(strcmp(s->light, "light1"), 0);
    EXPECT_EQ(strcmp(g->light, "gs_light1"), 0);
    EXPECT_EQ(strcmp(g->heavy, "gs_heavy"), 0);
    EXPECT_FALSE(s->two_handed);
    EXPECT_TRUE(g->two_handed);
    EXPECT_TRUE(eng_weapon_def(ENG_WEAPON_COUNT) == NULL);
}
