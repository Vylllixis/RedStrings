/* Vol du dragon (engine/dragon_flight.h) : décollage, ronde autour de l'antre, passages de feu sur
 * un point figé, retour et atterrissage ; virages limités, altitude, chute. */
#include <math.h>
#include <string.h>

#include "engine/dragon_flight.h"
#include "utest.h"

#define LEAD 125.0F /* le feu touche le sol 125 m devant lui (140 m dans le jeu) */
#define PASSES 2U   /* passages demandés dans ces essais (le dragon blessé du jeu en fait deux) */

typedef struct Track {
    float max_turn;     /* plus grand changement de cap en un tick (rad) */
    float max_step;     /* plus grand déplacement en un tick (m) */
    float max_altitude; /* plus haute altitude */
    uint32_t flame_ticks;
    uint32_t landings;
    uint32_t ticks;
} Track;

static float wrap_pi(float a)
{
    while (a > 3.14159265F) {
        a -= 6.2831853F;
    }
    while (a < -3.14159265F) {
        a += 6.2831853F;
    }
    return a;
}

static EngDragonFlightInput input(float tx, float tz, bool has_target)
{
    EngDragonFlightInput in;
    memset(&in, 0, sizeof in);
    in.target[0] = tx;
    in.target[1] = tz;
    in.fire_lead = LEAD;
    in.passes = PASSES;
    in.has_target = has_target;
    in.can_take_off = true;
    return in;
}

/* avance d'un tick en mesurant ; au sol, la simulation le laisse où il est */
static bool tick(EngDragonFlight *d, EngDragonFlightInput *in, Track *t)
{
    const float before[2] = {d->pos[0], d->pos[1]};
    const float yaw = d->yaw;
    in->ground[0] = d->pos[0];
    in->ground[1] = d->pos[1];
    in->ground_yaw = d->yaw;
    const bool air = eng_dragon_flight_step(d, in);
    if (t != NULL) {
        const float turn = fabsf(wrap_pi(d->yaw - yaw));
        const float step = hypotf(d->pos[0] - before[0], d->pos[1] - before[1]);
        t->max_turn = turn > t->max_turn ? turn : t->max_turn;
        t->max_step = step > t->max_step ? step : t->max_step;
        t->max_altitude = d->altitude > t->max_altitude ? d->altitude : t->max_altitude;
        t->flame_ticks += d->flame ? 1U : 0U;
        t->landings += d->landed ? 1U : 0U;
        t->ticks++;
    }
    return air;
}

/* avance jusqu'à la phase `phase` (au plus `max` ticks) ; false si elle n'arrive pas */
static bool until(EngDragonFlight *d, EngDragonFlightInput *in, Track *t, EngDragonPhase phase,
                  uint32_t max)
{
    for (uint32_t i = 0; i < max; i++) {
        (void)tick(d, in, t);
        if (d->phase == phase) {
            return true;
        }
    }
    return false;
}

UTEST(dragon_flight, it_rests_then_takes_off_and_circles_its_lair_without_a_target)
{
    EngDragonFlight d;
    const float start[2] = {5.0F, -3.0F};
    eng_dragon_flight_init(&d, start, 0.4F);
    EXPECT_EQ(d.phase, ENG_DRAGON_GROUND);
    EXPECT_FALSE(eng_dragon_flight_airborne(&d));
    EngDragonFlightInput in = input(0.0F, 0.0F, false);
    Track t = {0};
    for (uint32_t i = 0; i + 1U < ENG_DRAGON_REST_TICKS; i++) { /* il se repose 20 s */
        ASSERT_FALSE(tick(&d, &in, &t));
    }
    EXPECT_NEAR(d.altitude, 0.0F, 1e-6F);
    EXPECT_NEAR(d.pos[0], 5.0F, 1e-6F); /* au sol : là où la simulation l'a mis */
    in.can_take_off = false;            /* occupé (attaque, souffle) : il attend */
    for (int i = 0; i < 90; i++) {
        ASSERT_FALSE(tick(&d, &in, &t));
    }
    in.can_take_off = true;
    ASSERT_TRUE(tick(&d, &in, &t));
    EXPECT_EQ(d.phase, ENG_DRAGON_TAKEOFF);
    /* décollage : il monte sans jamais redescendre, droit devant, en 3 s */
    float alt = d.altitude;
    const float yaw = d.yaw;
    for (uint32_t i = 0; i < ENG_DRAGON_TAKEOFF_TICKS; i++) {
        EXPECT_EQ(d.phase, ENG_DRAGON_TAKEOFF);
        ASSERT_TRUE(tick(&d, &in, &t));
        EXPECT_GE(d.altitude, alt);
        alt = d.altitude;
    }
    EXPECT_EQ(d.phase, ENG_DRAGON_PATROL);
    EXPECT_NEAR(d.altitude, ENG_DRAGON_CRUISE_ALT, 0.5F);
    EXPECT_NEAR(d.yaw, yaw, 1e-5F);
    EXPECT_NEAR(d.speed, ENG_DRAGON_SPEED, 1e-4F);
    EXPECT_GT(hypotf(d.pos[0] - 5.0F, d.pos[1] + 3.0F), 50.0F); /* il a avancé en s'élevant */
    /* ronde : après 12 s il tourne à 150 m de l'antre, en croisière, à sa vitesse */
    for (int i = 0; i < 720; i++) {
        ASSERT_TRUE(tick(&d, &in, &t));
    }
    for (int i = 0; i < 600; i++) {
        ASSERT_TRUE(tick(&d, &in, &t));
        EXPECT_NEAR(hypotf(d.pos[0], d.pos[1]), ENG_DRAGON_ORBIT_RADIUS, 30.0F);
        EXPECT_NEAR(d.altitude, ENG_DRAGON_CRUISE_ALT, 0.01F);
    }
    EXPECT_EQ(d.phase, ENG_DRAGON_PATROL);
    EXPECT_FALSE(d.flame);
    EXPECT_GT(fabsf(d.bank), 0.1F); /* il penche dans son virage */
    /* 30 s de ronde, puis il rentre et se pose au centre de l'antre */
    ASSERT_TRUE(until(&d, &in, &t, ENG_DRAGON_RETURN, ENG_DRAGON_PATROL_TICKS));
    ASSERT_TRUE(until(&d, &in, &t, ENG_DRAGON_LANDING, 1800U));
    /* atterrissage : il descend sans jamais remonter ; aux trois quarts il est déjà bas */
    float descent = d.altitude;
    EXPECT_GT(descent, 0.9F * ENG_DRAGON_CRUISE_ALT);
    for (uint32_t i = 0; i < (3U * ENG_DRAGON_LANDING_TICKS) / 4U; i++) {
        (void)tick(&d, &in, &t);
        EXPECT_LE(d.altitude, descent + 1e-4F);
        descent = d.altitude;
    }
    EXPECT_EQ(d.phase, ENG_DRAGON_LANDING);
    EXPECT_LT(d.altitude, 0.25F * ENG_DRAGON_CRUISE_ALT);
    EXPECT_GT(d.altitude, 0.0F);
    ASSERT_TRUE(until(&d, &in, &t, ENG_DRAGON_GROUND, ENG_DRAGON_LANDING_TICKS + 2U));
    EXPECT_TRUE(d.landed);
    EXPECT_LT(hypotf(d.pos[0], d.pos[1]), 3.0F);
    EXPECT_NEAR(d.altitude, 0.0F, 1e-6F);
    EXPECT_NEAR(d.speed, 0.0F, 1e-6F);
    EXPECT_FALSE(tick(&d, &in, &t));
    EXPECT_FALSE(d.landed); /* un seul tick */
    EXPECT_EQ(t.landings, 1U);
    EXPECT_EQ(t.flame_ticks, 0U); /* sans cible, jamais de feu */
    /* jamais de demi-tour sur place ni de bond : virage et pas bornés pendant tout le vol */
    EXPECT_LE(t.max_turn, (2.0F * ENG_DRAGON_TURN_RATE / 60.0F) + 1e-5F);
    EXPECT_LE(t.max_step, (ENG_DRAGON_SPEED / 60.0F) + 2.5F); /* + glissade finale vers l'antre */
    EXPECT_LE(t.max_altitude, ENG_DRAGON_CRUISE_ALT + 0.01F);
}

UTEST(dragon_flight, with_a_target_it_fights_on_the_ground_35_s_before_taking_off)
{
    EngDragonFlight d;
    const float start[2] = {0.0F, 0.0F};
    eng_dragon_flight_init(&d, start, 0.0F);
    EngDragonFlightInput in = input(20.0F, 30.0F, true);
    for (uint32_t i = 0; i + 1U < ENG_DRAGON_GROUND_FIGHT_TICKS; i++) {
        ASSERT_FALSE(tick(&d, &in, NULL)); /* bien après les 20 s de repos : il se bat encore */
    }
    EXPECT_TRUE(tick(&d, &in, NULL));
    EXPECT_EQ(d.phase, ENG_DRAGON_TAKEOFF);
    EXPECT_GT(ENG_DRAGON_GROUND_FIGHT_TICKS, ENG_DRAGON_REST_TICKS);
}

/* Un vol de combat complet : deux passages de feu, puis retour et atterrissage. */
UTEST(dragon_flight, a_fire_pass_aims_at_a_frozen_point_and_burns_a_strip_through_it)
{
    EngDragonFlight d;
    const float start[2] = {0.0F, 0.0F};
    eng_dragon_flight_init(&d, start, 0.0F);
    EngDragonFlightInput in = input(30.0F, -20.0F, true);
    Track t = {0};
    ASSERT_TRUE(until(&d, &in, &t, ENG_DRAGON_TAKEOFF, ENG_DRAGON_GROUND_FIGHT_TICKS + 2U));
    ASSERT_TRUE(until(&d, &in, &t, ENG_DRAGON_STRAFE, 600U));
    for (uint32_t pass = 0; pass < PASSES; pass++) {
        /* approche : pas de feu avant d'avoir figé son axe */
        uint32_t approach = 0;
        while (!d.aim_locked && approach < ENG_DRAGON_STRAFE_TIMEOUT) {
            (void)tick(&d, &in, &t);
            ASSERT_EQ(d.phase, ENG_DRAGON_STRAFE);
            EXPECT_FALSE(d.flame && !d.aim_locked);
            approach++;
        }
        ASSERT_TRUE(d.aim_locked);
        EXPECT_NEAR(d.aim[0], 30.0F, 1e-4F);
        EXPECT_NEAR(d.aim[1], -20.0F, 1e-4F);
        const float gap0 = hypotf(d.aim[0] - d.pos[0], d.aim[1] - d.pos[1]);
        /* son feu atteindra le point visé dans 2 s au moins : le temps de s'écarter */
        EXPECT_GE(gap0, LEAD + (2.0F * ENG_DRAGON_SPEED) - 1.0F);
        EXPECT_LE(gap0, LEAD + 201.0F);
        /* la cible s'écarte de 40 m : il ne la suit plus, son axe est figé */
        in.target[0] = 30.0F + 40.0F;
        float yaw = d.yaw;
        const float yaw_locked = d.yaw;
        uint32_t fire = 0;
        float nearest = 1e9F;    /* plus petite distance du feu au point visé */
        float far_before = 0.0F; /* distance du feu au point visé quand il s'allume */
        float lowest = 1e9F;
        while (d.phase == ENG_DRAGON_STRAFE && fire < 2000U) {
            (void)tick(&d, &in, &t);
            if (d.flame) {
                float p[2];
                eng_dragon_flight_fire_point(&d, LEAD, p);
                const float dist = hypotf(p[0] - 30.0F, p[1] + 20.0F);
                if (fire == 0U) {
                    far_before = dist;
                }
                nearest = dist < nearest ? dist : nearest;
                lowest = d.altitude < lowest ? d.altitude : lowest;
                EXPECT_LE(dist, ENG_DRAGON_FIRE_SPAN + 1.0F);
                EXPECT_NEAR(wrap_pi(d.yaw - yaw), 0.0F, 1e-6F); /* feu allumé : il file droit */
                fire++;
            }
            yaw = d.yaw;
        }
        EXPECT_NEAR(wrap_pi(d.yaw - yaw_locked), 0.0F, 0.04F); /* axe figé : à 2 degrés près */
        EXPECT_EQ(d.phase, ENG_DRAGON_PATROL);
        EXPECT_EQ(d.passes, pass + 1U);
        EXPECT_FALSE(d.flame);
        EXPECT_LT(nearest, 6.0F);                           /* le feu passe SUR le point visé */
        EXPECT_GT(far_before, ENG_DRAGON_FIRE_SPAN - 2.0F); /* allumé 60 m avant */
        /* 120 m de feu à 42 m/s : 2,9 s */
        EXPECT_NEAR((float)fire, 2.0F * ENG_DRAGON_FIRE_SPAN / ENG_DRAGON_SPEED * 60.0F, 4.0F);
        EXPECT_LT(lowest, ENG_DRAGON_CRUISE_ALT - 5.0F); /* il est descendu pour souffler */
        EXPECT_GE(lowest, ENG_DRAGON_STRAFE_ALT - 0.01F);
        in.target[0] = 30.0F;
        if (pass + 1U < PASSES) {
            ASSERT_TRUE(until(&d, &in, &t, ENG_DRAGON_STRAFE, ENG_DRAGON_PATROL_MIN_TICKS + 2U));
        }
    }
    /* après ses passages : il rentre et se pose, même si la cible est toujours là */
    ASSERT_TRUE(until(&d, &in, &t, ENG_DRAGON_RETURN, 4U));
    ASSERT_TRUE(until(&d, &in, &t, ENG_DRAGON_GROUND, 3000U));
    EXPECT_TRUE(d.landed);
    EXPECT_LT(hypotf(d.pos[0], d.pos[1]), 3.0F);
    EXPECT_EQ(t.landings, 1U);
    EXPECT_LE(t.max_turn, (2.0F * ENG_DRAGON_TURN_RATE / 60.0F) + 1e-5F);
    EXPECT_LE(t.max_altitude, ENG_DRAGON_CRUISE_ALT + 0.01F);
    EXPECT_LT(t.ticks, ENG_DRAGON_GROUND_FIGHT_TICKS + 5400U); /* un vol de combat : < 90 s */
    /* au sol, il recommence : 35 s de combat avant le prochain vol */
    EXPECT_EQ(d.ticks, 0U);
    for (uint32_t i = 0; i + 1U < ENG_DRAGON_GROUND_FIGHT_TICKS; i++) {
        ASSERT_FALSE(tick(&d, &in, NULL));
    }
    EXPECT_TRUE(tick(&d, &in, NULL));
    EXPECT_EQ(d.passes, 0U);
}

/* Un passage de feu par vol si l'on ne demande rien ; autant qu'on en demande, trois au plus. */
UTEST(dragon_flight, one_fire_pass_per_flight_unless_more_are_asked_for)
{
    static const uint32_t asked[4] = {0U, 1U, 2U, 9U};
    static const uint32_t expected[4] = {ENG_DRAGON_PASSES, 1U, 2U, ENG_DRAGON_PASSES_MAX};
    uint32_t single_flight_ticks = 0;
    for (size_t k = 0; k < 4U; k++) {
        EngDragonFlight d;
        const float start[2] = {0.0F, 0.0F};
        eng_dragon_flight_init(&d, start, 0.0F);
        EngDragonFlightInput in = input(60.0F, -30.0F, true);
        in.passes = asked[k];
        Track t = {0};
        ASSERT_TRUE(until(&d, &in, &t, ENG_DRAGON_TAKEOFF, ENG_DRAGON_GROUND_FIGHT_TICKS + 2U));
        t = (Track){0};
        uint32_t passes = 0;
        uint32_t guard = 0;
        while (d.phase != ENG_DRAGON_GROUND && guard++ < 12000U) {
            (void)tick(&d, &in, &t);
            passes = d.passes > passes ? d.passes : passes;
        }
        ASSERT_EQ(d.phase, ENG_DRAGON_GROUND);
        EXPECT_EQ(passes, expected[k]);
        /* 2,9 s de feu par passage */
        EXPECT_NEAR((float)t.flame_ticks, (float)expected[k] * 171.0F, 6.0F * (float)expected[k]);
        if (k == 0U) {
            single_flight_ticks = t.ticks;
        }
    }
    EXPECT_EQ((uint32_t)ENG_DRAGON_PASSES, 1U);
    EXPECT_LT(single_flight_ticks, 2700U); /* un vol à un passage : moins de 45 s */
}

/* Cible trop proche, droit devant lui, quand il commence son passage : son feu, qui tombe 125 m
 * devant lui, l'atteindrait en moins de 2 s. Il ne fige pas son axe : il passe au-dessus d'elle,
 * prend de l'élan, fait demi-tour, et revient de plus loin. */
UTEST(dragon_flight, too_close_to_its_target_it_takes_a_run_up_before_its_pass)
{
    EngDragonFlight d;
    const float start[2] = {0.0F, 0.0F};
    eng_dragon_flight_init(&d, start, 0.0F); /* cap 0 : il regarde -Z */
    d.phase = ENG_DRAGON_STRAFE;             /* en vol, en croisière, passage qui commence */
    d.altitude = ENG_DRAGON_CRUISE_ALT;
    d.speed = ENG_DRAGON_SPEED;
    EngDragonFlightInput in = input(0.0F, -(LEAD + 50.0F), true); /* 175 m droit devant */
    in.home[0] = 300.0F;
    bool extended = false;
    uint32_t ticks = 0;
    float nearest_before_lock = 1e9F;
    while (!d.aim_locked && ticks < ENG_DRAGON_STRAFE_TIMEOUT) {
        (void)eng_dragon_flight_step(&d, &in);
        extended = extended || d.extending;
        const float dist = hypotf(d.pos[0] - in.target[0], d.pos[1] - in.target[1]);
        nearest_before_lock = dist < nearest_before_lock ? dist : nearest_before_lock;
        EXPECT_FALSE(d.flame);
        ticks++;
    }
    ASSERT_TRUE(d.aim_locked);
    EXPECT_TRUE(extended);
    EXPECT_LT(nearest_before_lock, 20.0F); /* il est d'abord passé au-dessus d'elle */
    EXPECT_GT(ticks, 400U);                /* élan et demi-tour : plus de 6 s */
    const float gap = hypotf(d.aim[0] - d.pos[0], d.aim[1] - d.pos[1]);
    EXPECT_GE(gap, LEAD + (2.0F * ENG_DRAGON_SPEED) - 1.0F); /* 2 s de feu avant le point visé */
    EXPECT_LE(gap, LEAD + 201.0F);
}

/* Cible loin devant lui : il ne fige son axe que dans les dernières secondes (à moins de 200 m
 * de feu à parcourir), pas dès qu'il l'aperçoit ; d'ici là il suit ses déplacements. */
UTEST(dragon_flight, far_from_its_target_it_only_freezes_its_aim_in_the_last_seconds)
{
    EngDragonFlight d;
    const float start[2] = {0.0F, 0.0F};
    eng_dragon_flight_init(&d, start, 0.0F); /* cap 0 : il regarde -Z */
    d.phase = ENG_DRAGON_STRAFE;
    d.altitude = ENG_DRAGON_CRUISE_ALT;
    d.speed = ENG_DRAGON_SPEED;
    EngDragonFlightInput in = input(0.0F, -700.0F, true); /* 700 m droit devant */
    in.home[0] = 300.0F;
    (void)eng_dragon_flight_step(&d, &in);
    EXPECT_FALSE(d.aim_locked); /* dans l'axe, mais bien trop loin */
    in.target[0] = 40.0F;       /* la cible se déplace de 40 m : il la suit */
    uint32_t ticks = 0;
    while (!d.aim_locked && ticks < ENG_DRAGON_STRAFE_TIMEOUT) {
        (void)eng_dragon_flight_step(&d, &in);
        EXPECT_FALSE(d.flame);
        ticks++;
    }
    ASSERT_TRUE(d.aim_locked);
    EXPECT_NEAR(d.aim[0], 40.0F, 1e-3F); /* là où elle est quand il fige, pas où il l'a vue */
    EXPECT_NEAR(d.aim[1], -700.0F, 1e-3F);
    const float gap = hypotf(d.aim[0] - d.pos[0], d.aim[1] - d.pos[1]);
    EXPECT_LE(gap, LEAD + 200.5F); /* au plus 200 m de feu avant le point visé : 4,8 s */
    EXPECT_GE(gap, LEAD + 150.0F); /* figé dès qu'il est assez près : il était déjà dans l'axe */
    EXPECT_GT(ticks, 300U);        /* il a d'abord parcouru plus de 200 m */
}

UTEST(dragon_flight, wherever_the_target_stands_the_fire_reaches_it)
{
    /* cible sous lui, loin de l'antre, dans chaque direction : le passage aboutit toujours */
    static const float targets[][2] = {{0.0F, 0.0F},     {150.0F, 0.0F},  {-150.0F, 10.0F},
                                       {0.0F, 220.0F},   {90.0F, -90.0F}, {-200.0F, -200.0F},
                                       {10.0F, -140.0F}, {260.0F, 30.0F}};
    for (size_t k = 0; k < sizeof targets / sizeof targets[0]; k++) {
        EngDragonFlight d;
        const float start[2] = {0.0F, 0.0F};
        eng_dragon_flight_init(&d, start, 0.7F * (float)k);
        EngDragonFlightInput in = input(targets[k][0], targets[k][1], true);
        Track t = {0};
        ASSERT_TRUE(until(&d, &in, &t, ENG_DRAGON_TAKEOFF, ENG_DRAGON_GROUND_FIGHT_TICKS + 2U));
        float nearest = 1e9F;
        uint32_t guard = 0;
        while (d.phase != ENG_DRAGON_GROUND && guard++ < 9000U) {
            (void)tick(&d, &in, &t);
            if (d.flame) {
                float p[2];
                eng_dragon_flight_fire_point(&d, LEAD, p);
                const float dist = hypotf(p[0] - targets[k][0], p[1] - targets[k][1]);
                nearest = dist < nearest ? dist : nearest;
            }
        }
        EXPECT_EQ(d.phase, ENG_DRAGON_GROUND);
        EXPECT_EQ(t.landings, 1U);
        EXPECT_LT(nearest, 6.0F);
        EXPECT_GT(t.flame_ticks, 300U); /* deux passages de 2,9 s */
        EXPECT_LT(t.flame_ticks, 360U);
        EXPECT_LE(t.max_turn, (2.0F * ENG_DRAGON_TURN_RATE / 60.0F) + 1e-5F);
    }
}

UTEST(dragon_flight, a_lost_target_sends_it_back_to_its_round_and_a_dead_dragon_falls)
{
    EngDragonFlight d;
    const float start[2] = {0.0F, 0.0F};
    eng_dragon_flight_init(&d, start, 0.0F);
    EngDragonFlightInput in = input(40.0F, 40.0F, true);
    ASSERT_TRUE(until(&d, &in, NULL, ENG_DRAGON_STRAFE, ENG_DRAGON_GROUND_FIGHT_TICKS + 900U));
    in.has_target = false; /* la cible sort de son domaine avant qu'il ait figé son axe */
    (void)tick(&d, &in, NULL);
    EXPECT_EQ(d.phase, ENG_DRAGON_PATROL);
    EXPECT_FALSE(d.flame);
    EXPECT_EQ(d.passes, 0U);
    in.has_target = true;
    in.target[0] = NAN; /* cible absurde : ignorée, comme s'il n'y en avait pas */
    for (uint32_t i = 0; i < ENG_DRAGON_PATROL_MIN_TICKS + 10U; i++) {
        (void)tick(&d, &in, NULL);
    }
    EXPECT_EQ(d.phase, ENG_DRAGON_PATROL);
    EXPECT_TRUE(isfinite(d.pos[0]) && isfinite(d.pos[1]) && isfinite(d.yaw));
    /* il meurt en vol : il tombe, sans feu, et touche le sol en moins de 2 s */
    const float alt = d.altitude;
    EXPECT_GT(alt, 50.0F);
    uint32_t fall = 0;
    bool hit = false;
    while (!hit && fall < 600U) {
        const float before = d.altitude;
        hit = eng_dragon_flight_fall(&d);
        EXPECT_LT(d.altitude, before);
        EXPECT_FALSE(d.flame);
        fall++;
    }
    EXPECT_TRUE(hit);
    EXPECT_LT(fall, 120U);
    EXPECT_EQ(d.phase, ENG_DRAGON_GROUND);
    EXPECT_NEAR(d.altitude, 0.0F, 1e-6F);
    EXPECT_FALSE(eng_dragon_flight_fall(&d)); /* déjà au sol */
}

/* champ par champ : la structure a des octets de remplissage, memcmp ne dirait rien de sûr */
static bool same_flight(const EngDragonFlight *a, const EngDragonFlight *b)
{
    return a->phase == b->phase && a->ticks == b->ticks && a->passes == b->passes &&
           a->pos[0] == b->pos[0] && a->pos[1] == b->pos[1] && a->yaw == b->yaw &&
           a->altitude == b->altitude && a->speed == b->speed && a->bank == b->bank &&
           a->aim[0] == b->aim[0] && a->aim[1] == b->aim[1] && a->from_alt == b->from_alt &&
           a->aim_locked == b->aim_locked && a->extending == b->extending && a->flame == b->flame &&
           a->landed == b->landed;
}

UTEST(dragon_flight, the_same_inputs_give_the_same_flight_and_bad_arguments_are_ignored)
{
    EngDragonFlight a;
    EngDragonFlight b;
    const float start[2] = {12.0F, -7.0F};
    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    eng_dragon_flight_init(&a, start, 1.0F);
    eng_dragon_flight_init(&b, start, 1.0F);
    EngDragonFlightInput in = input(-60.0F, 80.0F, true);
    for (int i = 0; i < 3000; i++) {
        in.ground[0] = a.pos[0];
        in.ground[1] = a.pos[1];
        in.ground_yaw = a.yaw;
        (void)eng_dragon_flight_step(&a, &in);
        (void)eng_dragon_flight_step(&b, &in);
    }
    EXPECT_TRUE(same_flight(&a, &b));
    EXPECT_TRUE(eng_dragon_flight_airborne(&a)); /* 50 s : il a décollé (35 s de combat au sol) */
    /* l'origine du monde bouge : position et point visé suivent, le reste non */
    const EngDragonFlight before = a;
    eng_dragon_flight_shift(&a, 512.0F, -256.0F);
    EXPECT_NEAR(a.pos[0], before.pos[0] + 512.0F, 1e-3F);
    EXPECT_NEAR(a.pos[1], before.pos[1] - 256.0F, 1e-3F);
    EXPECT_NEAR(a.aim[0], before.aim[0] + 512.0F, 1e-3F);
    EXPECT_NEAR(a.aim[1], before.aim[1] - 256.0F, 1e-3F);
    EXPECT_NEAR(a.yaw, before.yaw, 1e-9F);
    EXPECT_NEAR(a.altitude, before.altitude, 1e-9F);
    eng_dragon_flight_shift(&a, NAN, 1.0F);
    EXPECT_NEAR(a.pos[1], before.pos[1] - 256.0F, 1e-3F);
    /* point de feu : `lead` mètres devant lui */
    EngDragonFlight f;
    const float origin[2] = {0.0F, 0.0F};
    eng_dragon_flight_init(&f, origin, 0.0F); /* cap 0 : regarde -Z */
    float p[2] = {9.0F, 9.0F};
    eng_dragon_flight_fire_point(&f, 100.0F, p);
    EXPECT_NEAR(p[0], 0.0F, 1e-4F);
    EXPECT_NEAR(p[1], -100.0F, 1e-4F);
    /* arguments absents ou absurdes */
    const float nan_pos[2] = {NAN, 1.0F};
    eng_dragon_flight_init(&f, nan_pos, INFINITY);
    EXPECT_NEAR(f.pos[0], 0.0F, 1e-9F);
    EXPECT_NEAR(f.yaw, 0.0F, 1e-9F);
    eng_dragon_flight_init(NULL, origin, 0.0F);
    EXPECT_FALSE(eng_dragon_flight_step(NULL, &in));
    EXPECT_FALSE(eng_dragon_flight_step(&f, NULL));
    EXPECT_FALSE(eng_dragon_flight_airborne(NULL));
    EXPECT_FALSE(eng_dragon_flight_fall(NULL));
    eng_dragon_flight_shift(NULL, 1.0F, 1.0F);
    eng_dragon_flight_fire_point(NULL, 1.0F, p);
    eng_dragon_flight_fire_point(&f, 1.0F, NULL);
    EngDragonFlightInput bad = input(0.0F, 0.0F, false);
    bad.home[0] = NAN; /* antre absurde : il ne bouge pas */
    for (uint32_t i = 0; i < ENG_DRAGON_REST_TICKS + 10U; i++) {
        EXPECT_FALSE(eng_dragon_flight_step(&f, &bad));
    }
    EXPECT_EQ(f.phase, ENG_DRAGON_GROUND);
    EXPECT_STREQ(eng_dragon_phase_name(ENG_DRAGON_STRAFE), "passage de feu");
    EXPECT_STREQ(eng_dragon_phase_name((EngDragonPhase)99), "?");
}
