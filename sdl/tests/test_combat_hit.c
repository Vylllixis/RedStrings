/* Détection et résolution des coups (M5-07). */
#include <string.h>

#include "engine/combat_hit.h"
#include "utest.h"

static EngCapsule cap(float x, float y, float z, float len, float r)
{
    EngCapsule c = {{x, y, z}, {x, y + len, z}, r};
    return c;
}

typedef struct Dummy {
    EngCapsule box[2];
    EngHitZone zone[2];
} Dummy;

static Dummy dummy_at(float x)
{
    Dummy d;
    d.box[0] = cap(x, 1.5F, 0, 0.3F, 0.11F); /* tête */
    d.box[1] = cap(x, 0.5F, 0, 0.9F, 0.2F);  /* torse */
    d.zone[0] = ENG_ZONE_HEAD;
    d.zone[1] = ENG_ZONE_TORSO;
    return d;
}

static EngHitTarget target(uint32_t fighter, const Dummy *d, bool hittable)
{
    EngHitTarget t = {fighter, hittable, 2, d->box, d->box, d->zone};
    return t;
}

UTEST(combat_hit, an_attack_never_hits_the_same_target_twice_across_active_ticks)
{
    const Dummy d = dummy_at(1.0F);
    const EngHitTarget t = target(1, &d, true);
    EngHitTracker tr;
    eng_hittracker_reset(&tr, 7);
    const EngCapsule w = cap(0.9F, 0.8F, 0, 0.3F, 0.1F); /* dans le torse, immobile */
    EngHit hits[4];
    uint32_t total = 99;
    int landed = 0;
    for (int tick = 0; tick < 5; tick++) { /* 5 ticks actifs d'affilée */
        landed += (int)eng_hit_resolve(0, &w, &w, 1, &t, 1, &tr, hits, 4, &total);
    }
    EXPECT_EQ(landed, 1);
    EXPECT_TRUE(eng_hittracker_has_hit(&tr, 1));
    EXPECT_FALSE(eng_hittracker_has_hit(&tr, 2));
    /* une nouvelle attaque remet le compteur à zéro */
    eng_hittracker_reset(&tr, 8);
    EXPECT_FALSE(eng_hittracker_has_hit(&tr, 1));
    EXPECT_EQ(eng_hit_resolve(0, &w, &w, 1, &t, 1, &tr, hits, 4, &total), 1U);
    EXPECT_EQ(total, 1U);
}

UTEST(combat_hit, the_hit_reports_zone_contact_point_and_indices)
{
    const Dummy d = dummy_at(1.0F);
    const EngHitTarget t = target(3, &d, true);
    EngHitTracker tr;
    eng_hittracker_reset(&tr, 1);
    const EngCapsule w = cap(0.95F, 1.55F, 0, 0.2F, 0.05F); /* à la tête */
    EngHit h[2];
    ASSERT_EQ(eng_hit_resolve(5, &w, &w, 1, &t, 1, &tr, h, 2, NULL), 1U);
    EXPECT_EQ(h[0].attacker, 5U);
    EXPECT_EQ(h[0].defender, 3U);
    EXPECT_EQ(h[0].hurtbox, 0U);
    EXPECT_EQ(h[0].weapon, 0U);
    EXPECT_EQ(h[0].zone, ENG_ZONE_HEAD);
    EXPECT_NEAR(h[0].point[0], 0.975F, 0.03F);
}

UTEST(combat_hit, resolution_order_is_by_fighter_index_whatever_the_array_order)
{
    const Dummy d1 = dummy_at(1.0F);
    const Dummy d2 = dummy_at(1.0F);
    const Dummy d3 = dummy_at(1.0F);
    const EngHitTarget fwd[3] = {target(1, &d1, true), target(2, &d2, true), target(3, &d3, true)};
    const EngHitTarget rev[3] = {fwd[2], fwd[0], fwd[1]};
    const EngCapsule w = cap(0.9F, 0.8F, 0, 0.3F, 0.1F);
    EngHitTracker tr;
    EngHit a[4];
    EngHit b[4];
    eng_hittracker_reset(&tr, 1);
    ASSERT_EQ(eng_hit_resolve(0, &w, &w, 1, fwd, 3, &tr, a, 4, NULL), 3U);
    eng_hittracker_reset(&tr, 1);
    ASSERT_EQ(eng_hit_resolve(0, &w, &w, 1, rev, 3, &tr, b, 4, NULL), 3U);
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(a[i].defender, (uint32_t)(i + 1));
        EXPECT_EQ(b[i].defender, (uint32_t)(i + 1));
        for (int k = 0; k < 3; k++) {
            EXPECT_TRUE(a[i].point[k] == b[i].point[k]);
        }
    }
}

UTEST(combat_hit, the_earliest_contact_in_the_tick_wins_then_lowest_hurtbox_index)
{
    /* arme qui descend du-dessus de la tête vers le torse en un tick : la tête est touchée avant */
    const Dummy d = dummy_at(1.0F);
    const EngHitTarget t = target(1, &d, true);
    const EngCapsule w0 = cap(1.0F, 2.4F, 0, 0.05F, 0.05F);
    const EngCapsule w1 = cap(1.0F, 0.3F, 0, 0.05F, 0.05F);
    EngHitTracker tr;
    eng_hittracker_reset(&tr, 1);
    EngHit h[1];
    ASSERT_EQ(eng_hit_resolve(0, &w0, &w1, 1, &t, 1, &tr, h, 1, NULL), 1U);
    EXPECT_EQ(h[0].zone, ENG_ZONE_HEAD);
    /* contact simultané (arme immobile touchant les deux) : hurtbox d'indice le plus bas */
    const EngCapsule both = cap(1.0F, 1.3F, 0, 0.3F, 0.1F);
    eng_hittracker_reset(&tr, 2);
    ASSERT_EQ(eng_hit_resolve(0, &both, &both, 1, &t, 1, &tr, h, 1, NULL), 1U);
    EXPECT_EQ(h[0].hurtbox, 0U);
    EXPECT_EQ(h[0].time, 0.0F);
}

UTEST(combat_hit, the_fastest_swing_still_hits_a_thin_target_every_time)
{
    /* 3 m par tick, cible fine : « un coup rapide touche une cible fine à chaque fois » */
    Dummy d;
    d.box[0] = cap(1.7F, 0.5F, 0, 1.4F, 0.01F);
    d.box[1] = d.box[0];
    d.zone[0] = ENG_ZONE_TORSO;
    d.zone[1] = ENG_ZONE_TORSO;
    const EngHitTarget t = target(1, &d, true);
    EngHitTracker tr;
    int hits_n = 0;
    for (int i = 0; i < 50; i++) {
        const float y = 0.6F + 0.01F * (float)i;
        const EngCapsule w0 = cap(0.0F, y, 0, 0.1F, 0.05F);
        const EngCapsule w1 = cap(3.0F, y, 0, 0.1F, 0.05F);
        EngHit h[1];
        eng_hittracker_reset(&tr, (uint32_t)i);
        hits_n += (int)eng_hit_resolve(0, &w0, &w1, 1, &t, 1, &tr, h, 1, NULL);
    }
    EXPECT_EQ(hits_n, 50);
}

UTEST(combat_hit, untouchable_self_and_missed_targets_are_skipped_without_being_marked)
{
    const Dummy d = dummy_at(1.0F);
    const Dummy far = dummy_at(50.0F);
    EngHitTarget ts[3] = {target(0, &d, true), target(1, &d, false), target(2, &far, true)};
    const EngCapsule w = cap(0.9F, 0.8F, 0, 0.3F, 0.1F);
    EngHitTracker tr;
    eng_hittracker_reset(&tr, 1);
    EngHit h[3];
    EXPECT_EQ(eng_hit_resolve(0, &w, &w, 1, ts, 3, &tr, h, 3, NULL), 0U);
    EXPECT_EQ(tr.hit_mask, 0U); /* ni soi-même, ni l'invulnérable, ni le raté */
    /* l'invulnérable redevient touchable avant la fin de l'attaque : il est touché */
    ts[1].hittable = true;
    EXPECT_EQ(eng_hit_resolve(0, &w, &w, 1, ts, 3, &tr, h, 3, NULL), 1U);
    EXPECT_EQ(h[0].defender, 1U);
}

UTEST(combat_hit, a_small_output_buffer_truncates_the_list_but_hits_are_still_consumed)
{
    const Dummy d1 = dummy_at(1.0F);
    const Dummy d2 = dummy_at(1.0F);
    const EngHitTarget ts[2] = {target(1, &d1, true), target(2, &d2, true)};
    const EngCapsule w = cap(0.9F, 0.8F, 0, 0.3F, 0.1F);
    EngHitTracker tr;
    eng_hittracker_reset(&tr, 1);
    EngHit h[1];
    uint32_t total = 0;
    EXPECT_EQ(eng_hit_resolve(0, &w, &w, 1, ts, 2, &tr, h, 1, &total), 1U);
    EXPECT_EQ(total, 2U);
    EXPECT_TRUE(eng_hittracker_has_hit(&tr, 2));
    EXPECT_EQ(eng_hit_resolve(0, &w, &w, 1, ts, 2, &tr, NULL, 0, &total), 0U);
    EXPECT_EQ(total, 0U);
}

UTEST(combat_hit, invalid_arguments_resolve_nothing_and_leave_the_tracker_alone)
{
    const Dummy d = dummy_at(1.0F);
    const EngCapsule w = cap(0.9F, 0.8F, 0, 0.3F, 0.1F);
    EngHitTracker tr;
    eng_hittracker_reset(&tr, 1);
    EngHit h[2];
    EngHitTarget dup[2] = {target(1, &d, true), target(1, &d, true)};
    EXPECT_EQ(eng_hit_resolve(0, &w, &w, 1, dup, 2, &tr, h, 2, NULL), 0U);
    EngHitTarget oob = target(ENG_HIT_MAX_FIGHTERS, &d, true);
    EXPECT_EQ(eng_hit_resolve(0, &w, &w, 1, &oob, 1, &tr, h, 2, NULL), 0U);
    EngHitTarget t1 = target(1, &d, true);
    EXPECT_EQ(eng_hit_resolve(ENG_HIT_MAX_FIGHTERS, &w, &w, 1, &t1, 1, &tr, h, 2, NULL), 0U);
    EXPECT_EQ(eng_hit_resolve(0, NULL, &w, 1, &t1, 1, &tr, h, 2, NULL), 0U);
    EXPECT_EQ(eng_hit_resolve(0, &w, NULL, 1, &t1, 1, &tr, h, 2, NULL), 0U);
    EXPECT_EQ(eng_hit_resolve(0, &w, &w, 1, NULL, 1, &tr, h, 2, NULL), 0U);
    EXPECT_EQ(eng_hit_resolve(0, &w, &w, 1, &t1, 1, NULL, h, 2, NULL), 0U);
    EngHitTarget nozones = t1;
    nozones.zones = NULL;
    EXPECT_EQ(eng_hit_resolve(0, &w, &w, 1, &nozones, 1, &tr, h, 2, NULL), 0U);
    EXPECT_EQ(eng_hit_resolve(0, &w, &w, 0, &t1, 1, &tr, h, 2, NULL), 0U); /* aucune arme */
    EXPECT_EQ(tr.hit_mask, 0U);
    EXPECT_FALSE(eng_hittracker_has_hit(NULL, 0));
    EXPECT_FALSE(eng_hittracker_has_hit(&tr, ENG_HIT_MAX_FIGHTERS));
    eng_hittracker_reset(NULL, 0);
}

UTEST(combat_hit, two_weapons_the_one_touching_first_in_time_is_reported)
{
    const Dummy d = dummy_at(1.0F);
    const EngHitTarget t = target(1, &d, true);
    const EngCapsule wp[2] = {cap(-2.0F, 0.8F, 0, 0.3F, 0.1F), cap(0.0F, 0.8F, 0, 0.3F, 0.1F)};
    const EngCapsule wc[2] = {cap(1.0F, 0.8F, 0, 0.3F, 0.1F), cap(1.0F, 0.8F, 0, 0.3F, 0.1F)};
    EngHitTracker tr;
    eng_hittracker_reset(&tr, 1);
    EngHit h[1];
    ASSERT_EQ(eng_hit_resolve(0, wp, wc, 2, &t, 1, &tr, h, 1, NULL), 1U);
    EXPECT_EQ(h[0].weapon, 1U); /* partie de x = 0 : arrive avant celle partie de x = -2 */
}

UTEST(combat_hit, exactly_the_maximum_number_of_targets_is_resolved_and_one_more_is_refused)
{
    Dummy d[ENG_HIT_MAX_FIGHTERS + 1U];
    EngHitTarget t[ENG_HIT_MAX_FIGHTERS + 1U];
    for (uint32_t i = 0; i < ENG_HIT_MAX_FIGHTERS + 1U; i++) {
        d[i] = dummy_at(1.0F + (float)i * 0.0F);
        t[i] = target(i % ENG_HIT_MAX_FIGHTERS, &d[i], true);
    }
    const EngCapsule w = cap(0.9F, 0.8F, 0, 0.3F, 0.1F);
    EngHitTracker tr;
    eng_hittracker_reset(&tr, 1);
    EngHit hits[ENG_HIT_MAX_FIGHTERS];
    uint32_t total = 0;
    /* l'attaquant (indice 31) est l'une des cibles : il est ignoré, les 31 autres touchées */
    EXPECT_EQ(eng_hit_resolve(31, &w, &w, 1, t, ENG_HIT_MAX_FIGHTERS, &tr, hits,
                              ENG_HIT_MAX_FIGHTERS, &total),
              ENG_HIT_MAX_FIGHTERS - 1U);
    EXPECT_EQ(total, ENG_HIT_MAX_FIGHTERS - 1U);
    EXPECT_EQ(eng_hit_resolve(31, &w, &w, 1, t, ENG_HIT_MAX_FIGHTERS + 1U, &tr, hits,
                              ENG_HIT_MAX_FIGHTERS, &total),
              0U);
    EXPECT_EQ(total, 0U);
}
