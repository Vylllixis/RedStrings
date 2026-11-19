/* Système de dégâts (M5-08). */
#include <math.h>

#include "engine/combat_damage.h"
#include "utest.h"

UTEST(combat_damage, zone_multipliers_follow_the_design)
{
    EXPECT_EQ(eng_zone_multiplier(ENG_ZONE_HEAD), 1.5F);
    EXPECT_EQ(eng_zone_multiplier(ENG_ZONE_TORSO), 1.0F);
    EXPECT_EQ(eng_zone_multiplier(ENG_ZONE_LIMB), 0.75F);
    EXPECT_EQ(eng_zone_multiplier((EngHitZone)ENG_ZONE_COUNT), 1.0F);
}

UTEST(combat_damage, a_hit_removes_base_times_zone_and_starts_the_invulnerability)
{
    EngVitals v;
    eng_vitals_init(&v, 0.0F); /* défaut : 100 */
    EXPECT_EQ(v.hp, 100.0F);
    EXPECT_EQ(v.max_hp, 100.0F);
    EXPECT_EQ(v.died_tick, ENG_DAMAGE_NEVER);
    const EngDamageResult r = eng_damage_apply(&v, 10.0F, ENG_ZONE_HEAD, 100);
    EXPECT_FALSE(r.ignored);
    EXPECT_FALSE(r.killed);
    EXPECT_EQ(r.dealt, 15.0F);
    EXPECT_EQ(v.hp, 85.0F);
    EXPECT_EQ(v.invulnerable_until, 106U);
    EXPECT_FALSE(eng_vitals_hittable(&v, 105));
    EXPECT_TRUE(eng_vitals_hittable(&v, 106));
    /* pendant l'invincibilité : ignoré */
    const EngDamageResult ig = eng_damage_apply(&v, 50.0F, ENG_ZONE_TORSO, 105);
    EXPECT_TRUE(ig.ignored);
    EXPECT_EQ(ig.dealt, 0.0F);
    EXPECT_EQ(v.hp, 85.0F);
    /* à 106 : touché de nouveau (membre : 10 x 0,75) */
    EXPECT_EQ(eng_damage_apply(&v, 10.0F, ENG_ZONE_LIMB, 106).dealt, 7.5F);
    EXPECT_EQ(v.hp, 77.5F);
}

UTEST(combat_damage, the_fatal_hit_kills_at_exactly_its_tick_and_nothing_is_taken_after)
{
    EngVitals v;
    eng_vitals_init(&v, 20.0F);
    EXPECT_FALSE(eng_damage_apply(&v, 10.0F, ENG_ZONE_TORSO, 10).killed); /* 10 PV restent */
    const EngDamageResult r = eng_damage_apply(&v, 40.0F, ENG_ZONE_TORSO, 16);
    EXPECT_TRUE(r.killed);
    EXPECT_EQ(r.dealt, 10.0F); /* borné aux PV restants */
    EXPECT_EQ(v.hp, 0.0F);
    EXPECT_TRUE(v.dead);
    EXPECT_EQ(v.died_tick, 16U); /* « mort au bon tick » */
    EXPECT_FALSE(eng_vitals_hittable(&v, 1000));
    const EngDamageResult after = eng_damage_apply(&v, 40.0F, ENG_ZONE_TORSO, 1000);
    EXPECT_TRUE(after.ignored);
    EXPECT_FALSE(after.killed);
    EXPECT_EQ(v.died_tick, 16U); /* inchangé */
}

UTEST(combat_damage, an_exact_lethal_amount_kills_and_zero_damage_does_not)
{
    EngVitals v;
    eng_vitals_init(&v, 15.0F);
    EXPECT_FALSE(eng_damage_apply(&v, 0.0F, ENG_ZONE_HEAD, 1).killed);
    EXPECT_EQ(v.hp, 15.0F);
    EXPECT_TRUE(eng_damage_apply(&v, 10.0F, ENG_ZONE_HEAD, 50).killed); /* 10 x 1,5 = 15 */
    EXPECT_EQ(v.died_tick, 50U);
}

UTEST(combat_damage, a_dodge_extends_invulnerability_and_never_shortens_it)
{
    EngVitals v;
    eng_vitals_init(&v, 100.0F);
    eng_damage_set_invulnerable(&v, 20);
    EXPECT_FALSE(eng_vitals_hittable(&v, 19));
    EXPECT_TRUE(eng_vitals_hittable(&v, 20));
    eng_damage_set_invulnerable(&v, 10); /* plus court : sans effet */
    EXPECT_EQ(v.invulnerable_until, 20U);
    EXPECT_TRUE(eng_damage_apply(&v, 10.0F, ENG_ZONE_TORSO, 15).ignored);
    EXPECT_EQ(v.hp, 100.0F);
}

UTEST(combat_damage, healing_is_capped_and_ignored_for_the_dead_and_bad_values)
{
    EngVitals v;
    eng_vitals_init(&v, 100.0F);
    (void)eng_damage_apply(&v, 30.0F, ENG_ZONE_TORSO, 0);
    eng_vitals_heal(&v, 10.0F);
    EXPECT_EQ(v.hp, 80.0F);
    eng_vitals_heal(&v, 1000.0F);
    EXPECT_EQ(v.hp, 100.0F);
    eng_vitals_heal(&v, -5.0F);
    eng_vitals_heal(&v, NAN);
    EXPECT_EQ(v.hp, 100.0F);
    (void)eng_damage_apply(&v, 1000.0F, ENG_ZONE_TORSO, 50);
    eng_vitals_heal(&v, 50.0F);
    EXPECT_EQ(v.hp, 0.0F);
    EXPECT_TRUE(v.dead);
}

UTEST(combat_damage, invalid_arguments_are_ignored)
{
    EngVitals v;
    eng_vitals_init(&v, 100.0F);
    EXPECT_TRUE(eng_damage_apply(&v, -1.0F, ENG_ZONE_TORSO, 0).ignored);
    EXPECT_TRUE(eng_damage_apply(&v, NAN, ENG_ZONE_TORSO, 0).ignored);
    EXPECT_TRUE(eng_damage_apply(&v, INFINITY, ENG_ZONE_TORSO, 0).ignored);
    EXPECT_TRUE(eng_damage_apply(NULL, 5.0F, ENG_ZONE_TORSO, 0).ignored);
    EXPECT_EQ(v.hp, 100.0F);
    eng_vitals_init(&v, NAN);
    EXPECT_EQ(v.max_hp, 100.0F);
    eng_vitals_init(&v, -5.0F);
    EXPECT_EQ(v.max_hp, 100.0F);
    eng_vitals_init(NULL, 1.0F);
    EXPECT_FALSE(eng_vitals_hittable(NULL, 0));
    eng_damage_set_invulnerable(NULL, 5);
    eng_vitals_heal(NULL, 5.0F);
}

UTEST(combat_damage, a_zero_damage_hit_is_not_ignored_and_triggers_iframes)
{
    EngVitals v;
    eng_vitals_init(&v, 100.0F);
    const EngDamageResult r = eng_damage_apply(&v, 0.0F, ENG_ZONE_TORSO, 10U);
    EXPECT_FALSE(r.ignored);
    EXPECT_NEAR(r.dealt, 0.0F, 1e-9F);
    EXPECT_NEAR(v.hp, 100.0F, 1e-6F);
    EXPECT_FALSE(eng_vitals_hittable(&v, 10U));
}
