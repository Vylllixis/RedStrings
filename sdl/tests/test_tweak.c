#include <math.h>
#include <stdint.h>

#include "engine/tweak.h"
#include "utest.h"

/* Registre de variables réglables : enregistrement, bornes, lecture/écriture par nom. */

UTEST(eng_tweak, registers_and_sets_a_float_in_the_variable)
{
    EngTweaks t;
    eng_tweaks_init(&t);
    float speed = 2.0F;
    ASSERT_TRUE(eng_tweaks_add_float(&t, "vitesse", &speed, 0.0F, 10.0F));
    EXPECT_EQ(t.count, 1U);
    EXPECT_TRUE(eng_tweaks_set_float(&t, "vitesse", 7.5F));
    EXPECT_EQ(speed, 7.5F); /* la variable elle-même est modifiée */
    float out = 0.0F;
    EXPECT_TRUE(eng_tweaks_get_float(&t, "vitesse", &out));
    EXPECT_EQ(out, 7.5F);
    speed = 3.0F; /* l'autre sens : le code modifie sa variable, le registre le voit */
    EXPECT_TRUE(eng_tweaks_get_float(&t, "vitesse", &out));
    EXPECT_EQ(out, 3.0F);
}

UTEST(eng_tweak, float_is_clamped_to_bounds_inclusive)
{
    EngTweaks t;
    eng_tweaks_init(&t);
    float v = 1.0F;
    ASSERT_TRUE(eng_tweaks_add_float(&t, "v", &v, -2.0F, 4.0F));
    EXPECT_TRUE(eng_tweaks_set_float(&t, "v", 100.0F));
    EXPECT_EQ(v, 4.0F);
    EXPECT_TRUE(eng_tweaks_set_float(&t, "v", -100.0F));
    EXPECT_EQ(v, -2.0F);
    EXPECT_TRUE(eng_tweaks_set_float(&t, "v", 4.0F)); /* les bornes elles-mêmes sont permises */
    EXPECT_EQ(v, 4.0F);
    EXPECT_TRUE(eng_tweaks_set_float(&t, "v", -2.0F));
    EXPECT_EQ(v, -2.0F);
    EXPECT_TRUE(eng_tweaks_set_float(&t, "v", INFINITY)); /* infini : borné, pas refusé */
    EXPECT_EQ(v, 4.0F);
    EXPECT_TRUE(eng_tweaks_set_float(&t, "v", -INFINITY));
    EXPECT_EQ(v, -2.0F);
}

UTEST(eng_tweak, nan_is_refused_and_leaves_the_variable_alone)
{
    EngTweaks t;
    eng_tweaks_init(&t);
    float v = 1.5F;
    ASSERT_TRUE(eng_tweaks_add_float(&t, "v", &v, 0.0F, 4.0F));
    EXPECT_FALSE(eng_tweaks_set_float(&t, "v", NAN));
    EXPECT_EQ(v, 1.5F);
}

UTEST(eng_tweak, int_clamped_and_bool_toggled)
{
    EngTweaks t;
    eng_tweaks_init(&t);
    int32_t n = 3;
    bool b = false;
    ASSERT_TRUE(eng_tweaks_add_int(&t, "n", &n, 1, 8));
    ASSERT_TRUE(eng_tweaks_add_bool(&t, "b", &b));
    EXPECT_TRUE(eng_tweaks_set_int(&t, "n", 99));
    EXPECT_EQ(n, 8);
    EXPECT_TRUE(eng_tweaks_set_int(&t, "n", -5));
    EXPECT_EQ(n, 1);
    EXPECT_TRUE(eng_tweaks_set_int(&t, "n", 5));
    EXPECT_EQ(n, 5);
    EXPECT_TRUE(eng_tweaks_set_bool(&t, "b", true));
    EXPECT_TRUE(b);
    bool out = false;
    int32_t iout = 0;
    EXPECT_TRUE(eng_tweaks_get_bool(&t, "b", &out));
    EXPECT_TRUE(out);
    EXPECT_TRUE(eng_tweaks_get_int(&t, "n", &iout));
    EXPECT_EQ(iout, 5);
}

UTEST(eng_tweak, wrong_type_or_unknown_name_is_refused_without_side_effect)
{
    EngTweaks t;
    eng_tweaks_init(&t);
    float f = 1.0F;
    int32_t n = 2;
    bool b = true;
    ASSERT_TRUE(eng_tweaks_add_float(&t, "f", &f, 0.0F, 5.0F));
    ASSERT_TRUE(eng_tweaks_add_int(&t, "n", &n, 0, 5));
    ASSERT_TRUE(eng_tweaks_add_bool(&t, "b", &b));
    EXPECT_FALSE(eng_tweaks_set_int(&t, "f", 3));
    EXPECT_FALSE(eng_tweaks_set_float(&t, "n", 3.0F));
    EXPECT_FALSE(eng_tweaks_set_bool(&t, "n", false));
    EXPECT_FALSE(eng_tweaks_set_float(&t, "b", 0.0F));
    EXPECT_FALSE(eng_tweaks_set_float(&t, "inconnu", 1.0F));
    EXPECT_FALSE(eng_tweaks_set_float(&t, NULL, 1.0F));
    EXPECT_EQ(f, 1.0F);
    EXPECT_EQ(n, 2);
    EXPECT_TRUE(b);
    float of = 9.0F;
    int32_t on = 9;
    bool ob = false;
    EXPECT_FALSE(eng_tweaks_get_float(&t, "n", &of));
    EXPECT_FALSE(eng_tweaks_get_int(&t, "b", &on));
    EXPECT_FALSE(eng_tweaks_get_bool(&t, "f", &ob));
    EXPECT_FALSE(eng_tweaks_get_float(&t, "zzz", &of));
    EXPECT_EQ(of, 9.0F); /* sorties intactes */
    EXPECT_EQ(on, 9);
    EXPECT_FALSE(ob);
}

UTEST(eng_tweak, registration_is_validated)
{
    EngTweaks t;
    eng_tweaks_init(&t);
    float v = 1.0F;
    float out_of_range = 50.0F;
    int32_t n = 1;
    EXPECT_FALSE(eng_tweaks_add_float(&t, NULL, &v, 0.0F, 2.0F));
    EXPECT_FALSE(eng_tweaks_add_float(&t, "", &v, 0.0F, 2.0F));
    EXPECT_FALSE(eng_tweaks_add_float(&t, "v", NULL, 0.0F, 2.0F));
    EXPECT_FALSE(eng_tweaks_add_float(&t, "v", &v, 3.0F, 2.0F)); /* min > max */
    EXPECT_FALSE(eng_tweaks_add_float(&t, "v", &v, NAN, 2.0F));
    EXPECT_FALSE(eng_tweaks_add_float(&t, "v", &v, 0.0F, INFINITY));
    EXPECT_FALSE(eng_tweaks_add_float(&t, "v", &out_of_range, 0.0F, 2.0F)); /* valeur hors bornes */
    float below = -0.5F;
    EXPECT_FALSE(eng_tweaks_add_float(&t, "v", &below, 0.0F, 2.0F)); /* sous le minimum */
    float on_min = 0.0F;
    float on_max = 2.0F;
    EXPECT_TRUE(eng_tweaks_add_float(&t, "min", &on_min, 0.0F, 2.0F)); /* bornes incluses */
    EXPECT_TRUE(eng_tweaks_add_float(&t, "max", &on_max, 0.0F, 2.0F));
    eng_tweaks_clear(&t);
    EXPECT_FALSE(eng_tweaks_add_int(&t, "n", &n, 5, 2));
    EXPECT_FALSE(eng_tweaks_add_int(&t, "n", &n, 2, 9)); /* valeur 1 < min */
    EXPECT_EQ(t.count, 0U);
    /* bornes égales : permis, la variable est figée */
    float fixed = 2.0F;
    EXPECT_TRUE(eng_tweaks_add_float(&t, "fixe", &fixed, 2.0F, 2.0F));
    EXPECT_TRUE(eng_tweaks_set_float(&t, "fixe", 5.0F));
    EXPECT_EQ(fixed, 2.0F);
}

UTEST(eng_tweak, duplicate_name_refused_and_table_has_a_limit)
{
    EngTweaks t;
    eng_tweaks_init(&t);
    float v = 1.0F;
    ASSERT_TRUE(eng_tweaks_add_float(&t, "a", &v, 0.0F, 2.0F));
    EXPECT_FALSE(eng_tweaks_add_float(&t, "a", &v, 0.0F, 2.0F));
    bool b = false;
    EXPECT_FALSE(eng_tweaks_add_bool(&t, "a", &b)); /* même nom, autre type : refusé aussi */
    EXPECT_EQ(t.count, 1U);

    EngTweaks full;
    eng_tweaks_init(&full);
    static bool flags[ENG_TWEAK_MAX + 1U];
    static char names[ENG_TWEAK_MAX + 1U][8];
    for (uint32_t i = 0; i < ENG_TWEAK_MAX; i++) {
        names[i][0] = 'n';
        names[i][1] = (char)('0' + (i / 10U));
        names[i][2] = (char)('0' + (i % 10U));
        names[i][3] = '\0';
        EXPECT_TRUE(eng_tweaks_add_bool(&full, names[i], &flags[i]));
    }
    EXPECT_EQ(full.count, ENG_TWEAK_MAX);
    EXPECT_FALSE(eng_tweaks_add_bool(&full, "de_trop", &flags[ENG_TWEAK_MAX]));
    EXPECT_EQ(full.count, ENG_TWEAK_MAX);
    /* le dernier est bien retrouvable */
    EXPECT_TRUE(eng_tweaks_find(&full, names[ENG_TWEAK_MAX - 1U]) != NULL);
}

UTEST(eng_tweak, find_returns_the_entry_or_null)
{
    EngTweaks t;
    eng_tweaks_init(&t);
    float v = 1.0F;
    ASSERT_TRUE(eng_tweaks_add_float(&t, "alpha", &v, 0.0F, 2.0F));
    const EngTweak *e = eng_tweaks_find(&t, "alpha");
    ASSERT_TRUE(e != NULL);
    EXPECT_EQ(e->type, ENG_TWEAK_FLOAT);
    EXPECT_EQ(e->min, 0.0F);
    EXPECT_EQ(e->max, 2.0F);
    EXPECT_TRUE(e->value == &v);
    EXPECT_TRUE(eng_tweaks_find(&t, "alph") == NULL); /* préfixe : pas une correspondance */
    EXPECT_TRUE(eng_tweaks_find(&t, "alphaa") == NULL);
    EXPECT_TRUE(eng_tweaks_find(&t, NULL) == NULL);
}

UTEST(eng_tweak, reset_restores_the_value_at_registration)
{
    EngTweaks t;
    eng_tweaks_init(&t);
    float f = 1.5F;
    int32_t n = 4;
    bool b = true;
    ASSERT_TRUE(eng_tweaks_add_float(&t, "f", &f, 0.0F, 5.0F));
    ASSERT_TRUE(eng_tweaks_add_int(&t, "n", &n, 0, 9));
    ASSERT_TRUE(eng_tweaks_add_bool(&t, "b", &b));
    f = 4.0F;
    n = 7;
    b = false;
    EXPECT_TRUE(eng_tweaks_reset(&t, 1));
    EXPECT_EQ(n, 4);
    EXPECT_EQ(f, 4.0F); /* seul l'indice demandé est remis */
    EXPECT_FALSE(b);
    eng_tweaks_reset_all(&t);
    EXPECT_EQ(f, 1.5F);
    EXPECT_EQ(n, 4);
    EXPECT_TRUE(b);
    EXPECT_FALSE(eng_tweaks_reset(&t, 3)); /* indice invalide */
}

UTEST(eng_tweak, clear_empties_the_registry)
{
    EngTweaks t;
    eng_tweaks_init(&t);
    float v = 1.0F;
    ASSERT_TRUE(eng_tweaks_add_float(&t, "v", &v, 0.0F, 2.0F));
    eng_tweaks_clear(&t);
    EXPECT_EQ(t.count, 0U);
    EXPECT_TRUE(eng_tweaks_find(&t, "v") == NULL);
    EXPECT_TRUE(eng_tweaks_add_float(&t, "v", &v, 0.0F, 2.0F)); /* le nom est de nouveau libre */
}
