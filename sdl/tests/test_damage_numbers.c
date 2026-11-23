/* Chiffres de dégâts (M5-17). */
#include <math.h>
#include <string.h>

#include "engine/damage_numbers.h"
#include "utest.h"

static EngSimEvent ev(EngHitKind k, EngHitZone z, float dealt)
{
    EngSimEvent e;
    memset(&e, 0, sizeof e);
    e.kind = k;
    e.zone = z;
    e.dealt = dealt;
    e.point[0] = 1.0F;
    e.point[1] = 1.2F;
    e.point[2] = -3.0F;
    return e;
}

UTEST(damage_numbers, each_kind_has_its_text)
{
    EngDamageNumbers n;
    eng_dmgnum_init(&n);
    char t[16];
    EngSimEvent e = ev(ENG_HITKIND_DAMAGE, ENG_ZONE_TORSO, 10.0F);
    ASSERT_TRUE(eng_dmgnum_spawn(&n, &e));
    ASSERT_TRUE(eng_dmgnum_text(&n.items[0], t, sizeof t));
    EXPECT_STREQ(t, "10");
    e = ev(ENG_HITKIND_DAMAGE, ENG_ZONE_HEAD, 14.6F);
    ASSERT_TRUE(eng_dmgnum_spawn(&n, &e));
    ASSERT_TRUE(eng_dmgnum_text(&n.items[1], t, sizeof t));
    EXPECT_STREQ(t, "15!");
    EXPECT_EQ(n.items[1].kind, ENG_DMG_HEAD);
    e = ev(ENG_HITKIND_BLOCKED, ENG_ZONE_TORSO, 2.0F);
    ASSERT_TRUE(eng_dmgnum_spawn(&n, &e));
    ASSERT_TRUE(eng_dmgnum_text(&n.items[2], t, sizeof t));
    EXPECT_STREQ(t, "(2)");
    e = ev(ENG_HITKIND_PARRIED, ENG_ZONE_TORSO, 0.0F);
    ASSERT_TRUE(eng_dmgnum_spawn(&n, &e));
    ASSERT_TRUE(eng_dmgnum_text(&n.items[3], t, sizeof t));
    EXPECT_STREQ(t, "PARRY");
    e = ev(ENG_HITKIND_DAMAGE, ENG_ZONE_LIMB, 7.5F * 0.75F);
    ASSERT_TRUE(eng_dmgnum_spawn(&n, &e));
    ASSERT_TRUE(eng_dmgnum_text(&n.items[4], t, sizeof t));
    EXPECT_STREQ(t, "6"); /* 5,625 arrondi */
    EXPECT_EQ(eng_dmgnum_count(&n), 5U);
    EXPECT_EQ(n.spawned, 5U);
}

UTEST(damage_numbers, ignored_hits_and_bad_arguments_create_nothing)
{
    EngDamageNumbers n;
    eng_dmgnum_init(&n);
    const EngSimEvent e = ev(ENG_HITKIND_IGNORED, ENG_ZONE_TORSO, 10.0F);
    EXPECT_FALSE(eng_dmgnum_spawn(&n, &e));
    EXPECT_FALSE(eng_dmgnum_spawn(&n, NULL));
    EXPECT_FALSE(eng_dmgnum_spawn(NULL, &e));
    EXPECT_EQ(eng_dmgnum_count(&n), 0U);
    EXPECT_EQ(eng_dmgnum_count(NULL), 0U);
    char t[4];
    EXPECT_FALSE(eng_dmgnum_text(NULL, t, sizeof t));
    const EngSimEvent ok = ev(ENG_HITKIND_DAMAGE, ENG_ZONE_TORSO, 10.0F);
    ASSERT_TRUE(eng_dmgnum_spawn(&n, &ok));
    EXPECT_FALSE(eng_dmgnum_text(&n.items[0], NULL, 4));
    EXPECT_FALSE(eng_dmgnum_text(&n.items[0], t, 0));
    EXPECT_FALSE(eng_dmgnum_text(&n.items[0], t, 2)); /* « 10 » ne tient pas */
    EXPECT_TRUE(eng_dmgnum_text(&n.items[0], t, 3));
    eng_dmgnum_update(NULL);
    eng_dmgnum_init(NULL);
}

UTEST(damage_numbers, numbers_rise_fade_and_vanish_after_exactly_their_lifetime)
{
    EngDamageNumbers n;
    eng_dmgnum_init(&n);
    const EngSimEvent e = ev(ENG_HITKIND_DAMAGE, ENG_ZONE_TORSO, 10.0F);
    ASSERT_TRUE(eng_dmgnum_spawn(&n, &e));
    EXPECT_NEAR(n.items[0].pos[1], 1.4F, 1e-6F); /* point + 0,2 */
    EXPECT_NEAR(n.items[0].pos[0], 1.0F, 1e-6F);
    EXPECT_NEAR(n.items[0].pos[2], -3.0F, 1e-6F);
    EXPECT_NEAR(eng_dmgnum_alpha(&n.items[0]), 1.0F, 1e-6F);
    float prev = 2.0F;
    for (uint32_t i = 1; i < ENG_DMGNUM_LIFE; i++) {
        eng_dmgnum_update(&n);
        EXPECT_EQ(eng_dmgnum_count(&n), 1U);
        const float a = eng_dmgnum_alpha(&n.items[0]);
        EXPECT_LT(a, prev);
        EXPECT_GT(a, 0.0F);
        prev = a;
    }
    EXPECT_NEAR(n.items[0].pos[1], 1.4F + 59.0F * ENG_DMGNUM_RISE, 1e-4F);
    eng_dmgnum_update(&n); /* 60e tick : disparaît */
    EXPECT_EQ(eng_dmgnum_count(&n), 0U);
    EXPECT_NEAR(eng_dmgnum_alpha(&n.items[0]), 0.0F, 1e-9F);
    EXPECT_NEAR(eng_dmgnum_alpha(NULL), 0.0F, 1e-9F);
}

UTEST(damage_numbers, the_pool_replaces_the_oldest_when_full)
{
    EngDamageNumbers n;
    eng_dmgnum_init(&n);
    for (uint32_t i = 0; i < ENG_DMGNUM_MAX + 5U; i++) {
        const EngSimEvent e = ev(ENG_HITKIND_DAMAGE, ENG_ZONE_TORSO, (float)i);
        ASSERT_TRUE(eng_dmgnum_spawn(&n, &e));
    }
    EXPECT_EQ(eng_dmgnum_count(&n), ENG_DMGNUM_MAX);
    EXPECT_EQ(n.spawned, ENG_DMGNUM_MAX + 5U);
    EXPECT_NEAR(n.items[0].value, 32.0F, 1e-6F); /* a remplacé le n° 0 */
    EXPECT_NEAR(n.items[4].value, 36.0F, 1e-6F);
    EXPECT_NEAR(n.items[5].value, 5.0F, 1e-6F); /* le 5 est toujours là */
}
