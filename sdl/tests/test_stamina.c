/* Sprint et endurance (M4-09), module pur. Valeurs attendues calculées à la main : drain 20/s,
 * régénération 25/s après 1 s, reprise à 25 points, pas de 1/60 s. */
#include <math.h>

#include "engine/stamina.h"
#include "utest.h"

#define DT (1.0F / 60.0F)

static EngStamina make(void)
{
    EngStamina s;
    eng_stamina_init(&s);
    return s;
}

static bool step(EngStamina *s, bool want, bool moving)
{
    bool run = false;
    (void)eng_stamina_update(s, want, moving, DT, &run);
    return run;
}

UTEST(stamina, starts_full_and_sprints_while_it_lasts)
{
    EngStamina s = make();
    EXPECT_EQ(s.value, ENG_STAMINA_MAX);
    EXPECT_EQ(eng_stamina_fraction(&s), 1.0F);
    for (int i = 0; i < 60; i++) {
        EXPECT_TRUE(step(&s, true, true));
    }
    EXPECT_NEAR(s.value, 80.0F, 1e-3F); /* 1 s de course : -20 */
    EXPECT_NEAR(eng_stamina_fraction(&s), 0.8F, 1e-4F);
}

UTEST(stamina, no_sprint_without_intent_or_movement_and_no_drain)
{
    EngStamina s = make();
    EXPECT_FALSE(step(&s, false, true));
    EXPECT_FALSE(step(&s, true, false)); /* touche pressée à l'arrêt : on ne court pas */
    EXPECT_EQ(s.value, ENG_STAMINA_MAX);
}

UTEST(stamina, runs_out_after_five_seconds_then_stops_even_if_the_key_is_held)
{
    EngStamina s = make();
    int steps = 0;
    while (step(&s, true, true) && steps < 1000) {
        steps++;
    }
    EXPECT_NEAR((float)steps * DT, 5.0F, 0.05F);
    EXPECT_EQ(s.value, 0.0F);
    EXPECT_TRUE(s.exhausted);
    EXPECT_FALSE(step(&s, true, true)); /* touche tenue : toujours épuisé */
}

UTEST(stamina, regeneration_waits_for_the_delay_then_recovers_and_resumes_at_the_threshold)
{
    EngStamina s = make();
    while (step(&s, true, true)) {
    }
    /* 1 s de délai : rien ne revient */
    for (int i = 0; i < 59; i++) {
        step(&s, false, false);
    }
    EXPECT_EQ(s.value, 0.0F);
    /* puis 25 points/s : 25 points au bout d'une seconde de plus */
    int steps = 0;
    while (s.exhausted && steps < 600) {
        EXPECT_FALSE(step(&s, true, true)); /* on insiste : refusé tant qu'épuisé */
        steps++;
    }
    EXPECT_NEAR((float)steps * DT, 1.0F, 0.1F);
    EXPECT_GE(s.value, 25.0F - 1e-3F);
    EXPECT_FALSE(s.exhausted);
    EXPECT_TRUE(step(&s, true, true)); /* la course reprend */
}

UTEST(stamina, regeneration_is_capped_at_full_and_restarts_the_delay_after_each_sprint)
{
    EngStamina s = make();
    for (int i = 0; i < 30; i++) {
        step(&s, true, true);
    }
    const float after_run = s.value;
    for (int i = 0; i < 30; i++) { /* 0,5 s de repos : dans le délai de 1 s */
        step(&s, false, false);
    }
    EXPECT_EQ(s.value, after_run);
    step(&s, true, true); /* un pas de course remet le compteur de délai à 1 s */
    const float v = s.value;
    for (int i = 0; i < 50; i++) {
        step(&s, false, false);
    }
    EXPECT_EQ(s.value, v);
    for (int i = 0; i < 600; i++) {
        step(&s, false, false);
    }
    EXPECT_EQ(s.value, ENG_STAMINA_MAX);
}

/* Pas de 1/16 s (exact en flottant) : la réserve tombe pile à 0, puis remonte pile à 25. */
UTEST(stamina, exact_boundaries_exhaust_at_zero_and_resume_at_the_threshold)
{
    EngStamina s = make();
    EngStaminaParams p = s.params;
    p.drain = 64.0F; /* 4 points par pas */
    p.regen = 16.0F; /* 1 point par pas */
    ASSERT_TRUE(eng_stamina_set_params(&s, &p));
    bool run = false;
    for (int i = 0; i < 25; i++) {
        ASSERT_TRUE(eng_stamina_update(&s, true, true, 0.0625F, &run));
        EXPECT_TRUE(run);
    }
    EXPECT_EQ(s.value, 0.0F);
    EXPECT_TRUE(s.exhausted);      /* pile 0 suffit */
    for (int i = 0; i < 16; i++) { /* délai de 1 s */
        ASSERT_TRUE(eng_stamina_update(&s, false, false, 0.0625F, &run));
    }
    for (int i = 0; i < 24; i++) { /* 24 points */
        ASSERT_TRUE(eng_stamina_update(&s, false, false, 0.0625F, &run));
        EXPECT_TRUE(s.exhausted);
    }
    EXPECT_EQ(s.value, 24.0F);
    ASSERT_TRUE(eng_stamina_update(&s, false, false, 0.0625F, &run));
    EXPECT_EQ(s.value, 25.0F);
    EXPECT_FALSE(s.exhausted); /* pile le seuil : on reprend */
}

UTEST(stamina, restore_and_out_of_range_values_are_handled)
{
    EngStamina s = make();
    while (step(&s, true, true)) {
    }
    eng_stamina_restore(&s);
    EXPECT_EQ(s.value, ENG_STAMINA_MAX);
    EXPECT_FALSE(s.exhausted);
    EXPECT_TRUE(step(&s, true, true));
    s.value = 500.0F;                     /* modifié à la main : ramené dans les bornes */
    EXPECT_FALSE(step(&s, false, false)); /* pas de course demandée */
    EXPECT_EQ(s.value, ENG_STAMINA_MAX);
    s.value = -5.0F;
    EXPECT_FALSE(step(&s, true, true));
    s.value = NAN;
    EXPECT_FALSE(step(&s, true, true));
    EXPECT_EQ(s.value, 0.0F);
    EXPECT_EQ(eng_stamina_fraction(&s), 0.0F);
}

UTEST(stamina, raising_the_value_by_hand_lifts_exhaustion)
{
    EngStamina s = make();
    while (step(&s, true, true)) {
    }
    s.value = 50.0F; /* curseur du panneau de debug */
    step(&s, false, false);
    EXPECT_FALSE(s.exhausted);
    EXPECT_TRUE(step(&s, true, true));
}

UTEST(stamina, params_and_arguments_are_validated)
{
    EngStamina s = make();
    EngStaminaParams p = s.params;
    p.drain = 50.0F;
    EXPECT_TRUE(eng_stamina_set_params(&s, &p));
    EXPECT_EQ(s.params.drain, 50.0F);
    p.regen_delay = 0.0F; /* sans délai : permis */
    p.resume_at = 0.0F;   /* bornes incluses */
    EXPECT_TRUE(eng_stamina_set_params(&s, &p));
    p.resume_at = ENG_STAMINA_MAX;
    EXPECT_TRUE(eng_stamina_set_params(&s, &p));
    const EngStaminaParams good = s.params;
    EngStaminaParams q = good;
    q.drain = 0.0F;
    EXPECT_FALSE(eng_stamina_set_params(&s, &q));
    q = good;
    q.regen = -1.0F;
    EXPECT_FALSE(eng_stamina_set_params(&s, &q));
    q = good;
    q.regen = 0.0F;
    EXPECT_FALSE(eng_stamina_set_params(&s, &q));
    q = good;
    q.regen_delay = -0.1F;
    EXPECT_FALSE(eng_stamina_set_params(&s, &q));
    q = good;
    q.resume_at = 100.1F;
    EXPECT_FALSE(eng_stamina_set_params(&s, &q));
    q = good;
    q.resume_at = -1.0F;
    EXPECT_FALSE(eng_stamina_set_params(&s, &q));
    q = good;
    q.drain = NAN;
    EXPECT_FALSE(eng_stamina_set_params(&s, &q));
    q = good;
    q.regen_delay = INFINITY;
    EXPECT_FALSE(eng_stamina_set_params(&s, &q));
    EXPECT_FALSE(eng_stamina_set_params(NULL, &good));
    EXPECT_FALSE(eng_stamina_set_params(&s, NULL));
    EXPECT_EQ(s.params.drain, 50.0F);

    bool run;
    EXPECT_FALSE(eng_stamina_update(NULL, true, true, DT, &run));
    EXPECT_FALSE(eng_stamina_update(&s, true, true, 0.0F, &run));
    EXPECT_FALSE(eng_stamina_update(&s, true, true, -DT, &run));
    EXPECT_FALSE(eng_stamina_update(&s, true, true, 0.11F, &run));
    EXPECT_FALSE(eng_stamina_update(&s, true, true, NAN, &run));
    EXPECT_EQ(s.value, ENG_STAMINA_MAX);
    EXPECT_TRUE(eng_stamina_update(&s, true, true, 0.1F, NULL)); /* dt maximal, sortie NULL */
}

UTEST(stamina, same_drain_at_30_and_144_fps)
{
    EngStamina a = make();
    EngStamina b = make();
    bool run;
    for (int i = 0; i < 30; i++) {
        EXPECT_TRUE(eng_stamina_update(&a, true, true, 1.0F / 30.0F, &run));
    }
    for (int i = 0; i < 144; i++) {
        EXPECT_TRUE(eng_stamina_update(&b, true, true, 1.0F / 144.0F, &run));
    }
    EXPECT_NEAR(a.value, 80.0F, 1e-2F);
    EXPECT_NEAR(b.value, 80.0F, 1e-2F);
}

/* Critère : l'endurance est affichée dans l'UI de debug, via le registre de réglages en direct. */
UTEST(stamina, is_exposed_in_the_debug_panel_through_the_tweak_registry)
{
    EngStamina s = make();
    EngTweaks t;
    eng_tweaks_init(&t);
    EXPECT_TRUE(eng_stamina_register_tweaks(&s, &t));
    const EngTweak *tw = eng_tweaks_find(&t, "endurance");
    ASSERT_TRUE(tw != NULL);
    EXPECT_EQ(tw->type, ENG_TWEAK_FLOAT);
    EXPECT_EQ(tw->max, ENG_STAMINA_MAX);
    for (int i = 0; i < 60; i++) {
        step(&s, true, true);
    }
    float shown = 0.0F;
    EXPECT_TRUE(eng_tweaks_get_float(&t, "endurance", &shown));
    EXPECT_NEAR(shown, 80.0F, 1e-3F); /* le panneau lit la valeur vivante */
    EXPECT_TRUE(eng_tweaks_set_float(&t, "endurance", 500.0F)); /* borné */
    EXPECT_EQ(s.value, ENG_STAMINA_MAX);
    EXPECT_FALSE(eng_stamina_register_tweaks(&s, &t)); /* nom déjà pris */
    EXPECT_FALSE(eng_stamina_register_tweaks(NULL, &t));
    EXPECT_FALSE(eng_stamina_register_tweaks(&s, NULL));
    eng_tweaks_clear(&t);
}
