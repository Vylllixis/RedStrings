#include "play_boss.h"

#include <math.h>
#include <string.h>

#include "engine/combat_damage.h"

#define PI_F 3.14159265F
#define HORSE_SCALE 1.45F       /* le cheval porte un chevalier de 1,45 fois la taille du joueur */
#define RIDER_RADIUS 1.0F       /* à cheval : rayon de collision (m) */
#define RIDER_SEAT 0.3F         /* le cavalier s'enfonce dans la selle (m, avant échelle) */
#define DRAGON_RADIUS 2.0F      /* collision au centre seulement : on passe sous son ventre */
#define DRAGON_TURN 0.45F       /* rad/s : il pivote lentement (14 s pour un tour) */
#define DRAGON_LEG_RADIUS 4.6F  /* hurtbox d'une patte : une colonne de 9 m de large... */
#define DRAGON_LEG_HEIGHT 16.0F /* ... et 16 m de haut */
#define DRAGON_LEG_BLOCK 5.0F   /* une patte posée repousse le joueur (m) */
#define DRAGON_HEAD_RADIUS 9.5F /* hurtbox de la tête : à portée d'épée quand elle est au sol */
#define DRAGON_STOMP_RADIUS 8.0F /* piétinement : ce qui est à 8 m de ses pattes */
#define DRAGON_BITE_RADIUS                                                                         \
    9.0F /* morsure : ce qui est à 7 m, au sol, de l'endroit où sa tête tombe */
#define DRAGON_SHOCK_RADIUS 34.0F /* onde de choc de l'atterrissage (m autour de lui) */
#define DRAGON_SHOCK_DAMAGE 16.0F /* fois les dégâts de la zone */
#define DRAGON_FLIGHT_BLEND 20.0F /* m d'altitude pour replier les pattes et déployer les ailes */
#define DRAGON_HURT_PASSES 2U     /* sous la moitié de ses PV : deux passages de feu par vol */
#define GROUND_JET 0.82F          /* longueur du jet au sol (fraction de ENG_DRAGON_JET) : 100 m */
#define GROUND_HEAD 0.97F  /* pour souffler au sol, il baisse la tête presque jusqu'à terre */
#define GROUND_LUNGE 0.64F /* ... à 57 m devant lui : sa gueule est alors à 70 m de son centre */
/* souffle au sol : une bande devant lui, de sa gueule (à 68 m) au bout du jet (à 170 m), large de
 * 10 m près de la gueule et de 36 m au bout ; il ne commence que si le joueur est bien dedans */
#define FLAME_NEAR 66.0F
#define FLAME_FAR 172.0F
#define FLAME_HALF_NEAR 5.0F
#define FLAME_HALF_FAR 18.0F
#define FLAME_START_NEAR 74.0F
#define FLAME_START_FAR 160.0F
#define FLAME_START_WIDTH 0.6F /* fraction de la largeur de la bande */
#define FLAME_DAMAGE 8.0F      /* par brûlure, fois les dégâts de la zone */

void play_boss_clear(PlayBoss *b)
{
    if (b != NULL) {
        (void)memset(b, 0, sizeof *b);
    }
}

static bool active(const PlayBoss *b, const EngCombatSim *sim)
{
    return b != NULL && sim != NULL && b->kind != ENG_BOSSKIND_NONE && b->fighter < sim->count;
}

void play_boss_begin(PlayBoss *b, EngBossKind kind, uint32_t fighter, EngCombatSim *sim,
                     float damage_scale)
{
    play_boss_clear(b);
    if (b == NULL || sim == NULL || kind == ENG_BOSSKIND_NONE || fighter >= sim->count) {
        return;
    }
    b->kind = kind;
    b->fighter = fighter;
    eng_rider_init(&b->rider);
    eng_flame_init(&b->flame);
    b->base_speed = sim->speed_scale[fighter];
    b->base_radius = sim->radius[fighter];
    b->flame_damage =
        FLAME_DAMAGE * (isfinite(damage_scale) && damage_scale > 0.0F ? damage_scale : 1.0F);
    if (kind == ENG_BOSSKIND_RIDER) {
        sim->speed_scale[fighter] = b->base_speed * ENG_RIDER_SPEED;
        sim->radius[fighter] = fmaxf(b->base_radius, RIDER_RADIUS);
    } else {
        const EngSimBody *body = &sim->bodies[fighter];
        sim->radius[fighter] = DRAGON_RADIUS;
        sim->world_ignore |= 1U << fighter; /* il enjambe le décor */
        b->home[0] = body->pos[0];
        b->home[1] = body->pos[1];
        b->yaw = body->yaw;
        eng_dragon_flight_init(&b->flight, body->pos, body->yaw);
    }
}

bool play_boss_cinematic(const PlayBoss *b)
{
    return b != NULL && b->kind == ENG_BOSSKIND_RIDER && eng_rider_cinematic(&b->rider);
}

bool play_boss_before_step(PlayBoss *b, EngCombatSim *sim, EngFighterInput *in, uint32_t player)
{
    if (!active(b, sim) || in == NULL || player >= sim->count ||
        sim->fighters[b->fighter].vitals.dead) {
        return false;
    }
    if (play_boss_cinematic(b)) { /* personne n'agit, personne ne prend de coup */
        (void)memset(&in[b->fighter], 0, sizeof in[b->fighter]);
        (void)memset(&in[player], 0, sizeof in[player]);
        eng_damage_set_invulnerable(&sim->fighters[b->fighter].vitals, sim->tick + 2U);
        eng_damage_set_invulnerable(&sim->fighters[player].vitals, sim->tick + 2U);
        return true;
    }
    if (b->kind == ENG_BOSSKIND_DRAGON &&
        (eng_flame_busy(&b->flame) || eng_dragon_flight_airborne(&b->flight))) {
        /* il souffle : il ne bouge plus ; il vole : c'est son vol qui le déplace */
        (void)memset(&in[b->fighter], 0, sizeof in[b->fighter]);
    } else if (b->kind == ENG_BOSSKIND_DRAGON) {
        /* au sol il ne marche pas : il reste au centre de son antre, pivote vers le joueur
         * (dragon_turn) et frappe ; sa taille couvre toute l'arène (l'IA des petits ennemis le
         * ferait reculer vers un « poste d'attente » à 76 m du joueur) */
        in[b->fighter].move_dir[0] = 0.0F;
        in[b->fighter].move_dir[1] = 0.0F;
        in[b->fighter].moving = false;
    }
    return false;
}

/* point du repère du dragon (avant -Z, mètres du modèle) -> monde ; ses pieds sont à `altitude`
 * (0 au sol : le plan de la simulation) */
static void dragon_point(const EngSimBody *body, float altitude, const float p[3], float out[3])
{
    const float c = cosf(body->yaw);
    const float s = sinf(body->yaw);
    const float lx = p[0] * PLAY_DRAGON_SCALE;
    const float lz = p[2] * PLAY_DRAGON_SCALE;
    out[0] = body->pos[0] + (lx * c) + (lz * s);
    out[1] = altitude + (p[1] * PLAY_DRAGON_SCALE);
    out[2] = body->pos[1] - (lx * s) + (lz * c);
}

static EngCapsule capsule(const float a[3], const float b[3], float radius)
{
    EngCapsule cap;
    (void)memcpy(cap.a, a, sizeof cap.a);
    (void)memcpy(cap.b, b, sizeof cap.b);
    cap.radius = radius;
    return cap;
}

typedef enum DragonStrike { STRIKE_NONE = 0, STRIKE_STOMP, STRIKE_BITE } DragonStrike;

/* coup en cours : la lourde est la morsure, la légère le piétinement */
static DragonStrike strike_of(const EngFighter *f, const EngAttackSet *attacks,
                              EngAttackPhase *phase)
{
    *phase = ENG_ATTACK_DONE;
    if (f->vitals.dead || f->fsm.state != ENG_CS_ATTACK) {
        return STRIKE_NONE;
    }
    const EngAttack *atk = eng_fighter_attack(f, attacks);
    if (atk == NULL) {
        return STRIKE_NONE;
    }
    *phase = eng_attack_phase(atk, f->attack_tick);
    return strcmp(atk->name, "dragon_bite") == 0 ? STRIKE_BITE : STRIKE_STOMP;
}

/* La pose du pas à venir. Morsure : la gueule s'ouvre pendant l'anticipation, la tête s'abat au
 * sol pendant la fenêtre active, se relève ensuite. Piétinement : il se cabre pendant
 * l'anticipation et retombe d'un coup. Souffle au sol : il baisse la tête à mesure qu'il charge.
 * En vol : pattes repliées, ailes déployées, et le jet plonge vers le point du sol qu'il brûle. */
static void dragon_pose(PlayBoss *b, const EngCombatSim *sim)
{
    const EngFighter *f = &sim->fighters[b->fighter];
    const EngSimBody *body = &sim->bodies[b->fighter];
    EngAttackPhase phase = ENG_ATTACK_DONE;
    const DragonStrike strike = strike_of(f, sim->attacks, &phase);
    float bite = 0.0F;
    if (strike == STRIKE_BITE) {
        /* après la morsure, la tête reste au sol le temps de la récupération : on peut la frapper
         */
        bite = phase == ENG_ATTACK_STARTUP ? 0.25F : (phase == ENG_ATTACK_ACTIVE ? 1.0F : 0.95F);
    }
    /* où la tête s'abat : pendant qu'il arme sa morsure, là où se tient le joueur (de 30 à 73 m
     * devant son centre) ; le coup parti, il ne corrige plus. Pour souffler : à 57 m. */
    float lunge = b->lunge;
    if (strike == STRIKE_BITE && phase == ENG_ATTACK_STARTUP) {
        lunge = ((b->prey / PLAY_DRAGON_SCALE) - ENG_DRAGON_BITE_NEAR) /
                (ENG_DRAGON_BITE_FAR - ENG_DRAGON_BITE_NEAR);
        lunge = lunge < 0.0F ? 0.0F : (lunge > 1.0F ? 1.0F : lunge);
    }
    if (eng_flame_busy(&b->flame)) {
        bite = GROUND_HEAD * (0.4F + (0.6F * eng_flame_charge(&b->flame)));
        lunge = GROUND_LUNGE;
    }
    if (b->flight.flame) {
        bite = 0.6F;
    }
    b->bite += (bite - b->bite) * 0.4F;
    b->lunge += (lunge - b->lunge) * 0.3F;
    const float rear = strike == STRIKE_STOMP && phase == ENG_ATTACK_STARTUP ? 1.0F : 0.0F;
    b->rear += (rear - b->rear) * (rear > b->rear ? 0.08F : 0.45F);
    const float fly = b->flight.altitude / DRAGON_FLIGHT_BLEND;
    EngDragonPose pose = {.time = b->time,
                          .bite = b->bite,
                          .lunge = b->lunge,
                          .rear = b->rear,
                          .aim = b->pose.aim,
                          .flight = fly > 1.0F ? 1.0F : fly};
    float jet = GROUND_JET * eng_flame_jet(&b->flame);
    if (b->flight.flame) { /* de la gueule au point du sol que son feu brûle, devant lui */
        float mouth[3];
        float at[3];
        eng_creature_dragon_mouth(&pose, mouth);
        dragon_point(body, b->flight.altitude, mouth, at);
        const float ahead = PLAY_DRAGON_FIRE_LEAD + (mouth[2] * PLAY_DRAGON_SCALE);
        const float run = ahead > 1.0F ? ahead : 1.0F;
        pose.aim = atan2f(at[1], run);
        jet = hypotf(at[1], run) / (ENG_DRAGON_JET * PLAY_DRAGON_SCALE);
        jet = jet > 1.0F ? 1.0F : jet;
    } else if (!eng_flame_busy(&b->flame) && b->jet < 0.02F) {
        pose.aim = 0.0F; /* au sol, le jet rase le sol */
    }
    b->jet += (jet - b->jet) * 0.25F;
    pose.flame = b->jet < 0.01F ? 0.0F : b->jet;
    b->pose = pose;
}

void play_boss_rig(PlayBoss *b, EngCombatSim *sim)
{
    if (!active(b, sim) || b->kind != ENG_BOSSKIND_DRAGON) {
        return;
    }
    dragon_pose(b, sim);
    const EngFighter *f = &sim->fighters[b->fighter];
    const EngSimBody *body = &sim->bodies[b->fighter];
    const float altitude = b->flight.altitude;
    /* hurtboxes : ses quatre pattes (des colonnes : ce que l'épée atteint) et sa tête (à portée
     * seulement quand elle est au sol : morsure, souffle) ; en vol, tout est hors d'atteinte */
    EngCapsule caps[ENG_DRAGON_PAWS + 1U];
    EngHitZone zones[ENG_DRAGON_PAWS + 1U];
    float paw[ENG_DRAGON_PAWS][3];
    for (uint32_t i = 0; i < ENG_DRAGON_PAWS; i++) {
        float model[3];
        eng_creature_dragon_paw(&b->pose, i, model);
        dragon_point(body, altitude, model, paw[i]);
        const float low[3] = {paw[i][0], paw[i][1] + 2.0F, paw[i][2]};
        const float high[3] = {paw[i][0], paw[i][1] + DRAGON_LEG_HEIGHT, paw[i][2]};
        caps[i] = capsule(low, high, DRAGON_LEG_RADIUS);
        zones[i] = ENG_ZONE_TORSO;
        paw[i][1] += 2.0F; /* ligne des pattes, pour le piétinement */
    }
    float head[3];
    float at[3];
    float front[3];
    eng_creature_dragon_head(&b->pose, head);
    dragon_point(body, altitude, head, at);
    const float snout[3] = {head[0], head[1] - 0.3F, head[2] - 0.8F};
    dragon_point(body, altitude, snout, front);
    caps[ENG_DRAGON_PAWS] = capsule(at, front, DRAGON_HEAD_RADIUS);
    zones[ENG_DRAGON_PAWS] = ENG_ZONE_HEAD;
    (void)eng_sim_set_rig(sim, b->fighter, caps, zones, ENG_DRAGON_PAWS + 1U);
    /* armes : la morsure suit la tête qui s'abat (de sa nuque à son museau) ; le piétinement
     * frappe autour de ses pattes avant (qui retombent) et arrière */
    EngAttackPhase phase = ENG_ATTACK_DONE;
    EngCapsule weapons[2];
    if (strike_of(f, sim->attacks, &phase) == STRIKE_BITE) {
        weapons[0] = capsule(at, front, DRAGON_BITE_RADIUS);
        (void)eng_sim_set_weapon(sim, b->fighter, weapons, 1U);
        return;
    }
    weapons[0] = capsule(paw[0], paw[1], DRAGON_STOMP_RADIUS);
    weapons[1] = capsule(paw[2], paw[3], DRAGON_STOMP_RADIUS);
    (void)eng_sim_set_weapon(sim, b->fighter, weapons, 2U);
}

static void rider_after(PlayBoss *b, EngCombatSim *sim, PlayBossEvent *out)
{
    EngFighter *f = &sim->fighters[b->fighter];
    const EngSimBody *body = &sim->bodies[b->fighter];
    const float max_hp = f->vitals.max_hp;
    const EngRiderPhase before = b->rider.phase;
    float hp = f->vitals.dead ? 0.0F : f->vitals.hp;
    if (before == ENG_RIDER_ON_FOOT) {
        return;
    }
    if (eng_rider_step(&b->rider, &hp, max_hp)) {
        out->horse_died = true;
        b->horse_pos[0] = body->pos[0];
        b->horse_pos[1] = body->pos[1];
        b->horse_yaw = body->yaw;
        sim->speed_scale[b->fighter] = b->base_speed;
        if (f->vitals.dead) { /* un coup énorme a traversé le cheval : le chevalier se relève */
            const EngFighterParams p = f->params;
            eng_fighter_init(f, &p);
            f->vitals.max_hp = max_hp;
        }
    }
    if (b->rider.phase != ENG_RIDER_MOUNTED) {
        f->vitals.hp = hp;
    }
    if (before == ENG_RIDER_FALLING && b->rider.phase == ENG_RIDER_ON_FOOT) {
        out->dismounted = true;
        sim->radius[b->fighter] = b->base_radius;
    }
    const float dx = body->pos[0] - body->prev_pos[0];
    const float dz = body->pos[1] - body->prev_pos[1];
    const float speed = sqrtf((dx * dx) + (dz * dz)) * 60.0F;
    const float want = speed / (ENG_SIM_WALK_SPEED * 1.2F);
    b->gait += ((want > 1.0F ? 1.0F : want) - b->gait) * 0.15F;
}

static float wrap_angle(float a)
{
    while (a > PI_F) {
        a -= 2.0F * PI_F;
    }
    while (a < -PI_F) {
        a += 2.0F * PI_F;
    }
    return a;
}

/* Flammes ou onde de choc sur le joueur : `damage` avant armure et garde. Rien s'il esquive, s'il
 * est invincible (mode créatif) ou déjà mort. Retourne les PV retirés. */
static float hurt_player(EngCombatSim *sim, uint32_t player, bool invincible, float damage)
{
    EngFighter *pf = &sim->fighters[player];
    if (invincible || pf->vitals.dead || pf->fsm.state == ENG_CS_DODGE) {
        return 0.0F;
    }
    const float armor = isfinite(pf->params.armor_scale) && pf->params.armor_scale > 0.0F
                            ? pf->params.armor_scale
                            : 1.0F;
    const float guard = pf->fsm.state == ENG_CS_BLOCK ? pf->params.block_multiplier : 1.0F;
    const EngDamageResult r =
        eng_damage_apply(&pf->vitals, damage * armor * guard, ENG_ZONE_TORSO, pf->tick);
    if (r.ignored) {
        return 0.0F;
    }
    if (r.killed) { /* brûlure fatale : même fin qu'un coup */
        (void)eng_fsm_request(&pf->fsm, ENG_CS_DEAD);
    }
    return r.dealt;
}

/* Au sol il ne marche pas : il pivote, LENTEMENT (DRAGON_TURN rad/s), vers le joueur qu'il a vu
 * (`track`) : à l'arrêt, et pendant qu'il arme un coup (anticipation) ; son coup part alors dans
 * l'axe. Le coup parti, il ne corrige plus : on l'évite en s'écartant ou en esquivant. */
static void dragon_turn(PlayBoss *b, EngCombatSim *sim, uint32_t player, bool track)
{
    EngSimBody *body = &sim->bodies[b->fighter];
    const EngSimBody *pb = &sim->bodies[player];
    const EngFighter *f = &sim->fighters[b->fighter];
    const float away = hypotf(pb->pos[0] - body->pos[0], pb->pos[1] - body->pos[1]);
    EngAttackPhase phase = ENG_ATTACK_DONE;
    const bool winding =
        strike_of(f, sim->attacks, &phase) != STRIKE_NONE && phase == ENG_ATTACK_STARTUP;
    float want = b->yaw;
    if (track && (f->fsm.state == ENG_CS_IDLE || winding) && away > 1.0F) {
        want = atan2f(-(pb->pos[0] - body->pos[0]), -(pb->pos[1] - body->pos[1]));
    }
    const float max_turn = DRAGON_TURN / 60.0F;
    const float delta = wrap_angle(want - b->yaw);
    b->yaw = wrap_angle(b->yaw +
                        (delta > max_turn ? max_turn : (delta < -max_turn ? -max_turn : delta)));
    body->yaw = b->yaw;
    sim->fighters[b->fighter].yaw = b->yaw;
}

/* Ses pattes posées repoussent le joueur : on tourne autour, on passe entre elles. */
static void dragon_legs(const PlayBoss *b, EngCombatSim *sim, uint32_t player)
{
    const EngSimBody *body = &sim->bodies[b->fighter];
    EngSimBody *pb = &sim->bodies[player];
    for (uint32_t i = 0; i < ENG_DRAGON_PAWS; i++) {
        float model[3];
        float foot[3];
        eng_creature_dragon_paw(&b->pose, i, model);
        if (model[1] > 0.15F) {
            continue; /* patte levée : on passe dessous */
        }
        dragon_point(body, 0.0F, model, foot);
        const float dx = pb->pos[0] - foot[0];
        const float dz = pb->pos[1] - foot[2];
        const float d = hypotf(dx, dz);
        if (d >= DRAGON_LEG_BLOCK) {
            continue;
        }
        const float nx = d > 1e-3F ? dx / d : 1.0F;
        const float nz = d > 1e-3F ? dz / d : 0.0F;
        pb->pos[0] = foot[0] + (nx * DRAGON_LEG_BLOCK);
        pb->pos[1] = foot[2] + (nz * DRAGON_LEG_BLOCK);
    }
}

/* Souffle au sol : il charge (tête baissée) puis brûle une bande devant lui. */
static void dragon_breath(PlayBoss *b, EngCombatSim *sim, uint32_t player, bool engaged,
                          bool invincible, PlayBossEvent *out)
{
    const EngFighter *f = &sim->fighters[b->fighter];
    const EngSimBody *body = &sim->bodies[b->fighter];
    const EngFighter *pf = &sim->fighters[player];
    const EngSimBody *pb = &sim->bodies[player];
    const bool free_now = f->fsm.state == ENG_CS_IDLE || f->fsm.state == ENG_CS_MOVE;
    const bool can_start =
        engaged && free_now && !pf->vitals.dead &&
        eng_flame_strip(body->pos, body->yaw, pb->pos, FLAME_START_NEAR, FLAME_START_FAR,
                        FLAME_HALF_NEAR * FLAME_START_WIDTH, FLAME_HALF_FAR * FLAME_START_WIDTH);
    if (!eng_flame_step(&b->flame, can_start) ||
        !eng_flame_strip(body->pos, body->yaw, pb->pos, FLAME_NEAR, FLAME_FAR, FLAME_HALF_NEAR,
                         FLAME_HALF_FAR)) {
        return; /* pas de brûlure ce pas, ou le joueur est hors de la bande */
    }
    const float dealt = hurt_player(sim, player, invincible, b->flame_damage);
    if (dealt > 0.0F) {
        out->burn_dealt = dealt;
        b->burns++;
    }
}

static void dragon_place(PlayBoss *b, EngCombatSim *sim)
{
    EngSimBody *body = &sim->bodies[b->fighter];
    body->pos[0] = b->flight.pos[0];
    body->pos[1] = b->flight.pos[1];
    body->yaw = b->flight.yaw;
    sim->fighters[b->fighter].yaw = body->yaw;
    b->yaw = body->yaw;
}

/* Vol : décollage quand il est libre, ronde, passages de feu sur le joueur s'il est dans son
 * domaine, atterrissage (onde de choc autour de lui). En l'air, c'est son vol qui le déplace. */
static void dragon_fly(PlayBoss *b, EngCombatSim *sim, uint32_t player, bool invincible,
                       PlayBossEvent *out)
{
    const EngFighter *f = &sim->fighters[b->fighter];
    const EngSimBody *body = &sim->bodies[b->fighter];
    const EngSimBody *pb = &sim->bodies[player];
    const bool was_air = eng_dragon_flight_airborne(&b->flight);
    const bool free_now = f->fsm.state == ENG_CS_IDLE || f->fsm.state == ENG_CS_MOVE;
    const EngDragonFlightInput in = {
        .home = {b->home[0], b->home[1]},
        .target = {pb->pos[0], pb->pos[1]},
        .ground = {body->pos[0], body->pos[1]},
        .ground_yaw = body->yaw,
        .fire_lead = PLAY_DRAGON_FIRE_LEAD,
        /* en pleine forme : un passage de feu par vol ; blessé à
         * plus de la moitié : deux */
        .passes = f->vitals.hp < 0.5F * f->vitals.max_hp ? DRAGON_HURT_PASSES : ENG_DRAGON_PASSES,
        .has_target = play_boss_hunting(b, sim, player),
        .can_take_off = free_now && !eng_flame_busy(&b->flame)};
    const bool air = eng_dragon_flight_step(&b->flight, &in);
    if (air || b->flight.landed) {
        dragon_place(b, sim);
        sim->radius[b->fighter] = air ? 0.0F : DRAGON_RADIUS; /* en l'air, il ne pousse personne */
    }
    if (air && !was_air) {
        out->took_off = true;
        b->flights++;
        eng_flame_init(&b->flame);
    }
    if (b->flight.landed) {
        out->landed = true;
        if (hypotf(pb->pos[0] - body->pos[0], pb->pos[1] - body->pos[1]) <= DRAGON_SHOCK_RADIUS) {
            out->shock_dealt = hurt_player(sim, player, invincible,
                                           DRAGON_SHOCK_DAMAGE * b->flame_damage / FLAME_DAMAGE);
        }
    }
    if (!b->flight.flame) {
        b->fire_ticks = 0;
        return;
    }
    const bool burns = b->fire_ticks % ENG_FLAME_HIT_EVERY == 0U;
    b->fire_ticks++;
    float fire[2];
    eng_dragon_flight_fire_point(&b->flight, PLAY_DRAGON_FIRE_LEAD, fire);
    if (burns && hypotf(pb->pos[0] - fire[0], pb->pos[1] - fire[1]) <= PLAY_DRAGON_FIRE_RADIUS) {
        const float dealt = hurt_player(sim, player, invincible, b->flame_damage);
        if (dealt > 0.0F) {
            out->burn_dealt = dealt;
            b->burns++;
        }
    }
}

static void dragon_after(PlayBoss *b, EngCombatSim *sim, uint32_t player, bool engaged,
                         bool invincible, PlayBossEvent *out)
{
    const EngSimBody *body = &sim->bodies[b->fighter];
    const EngSimBody *pb = &sim->bodies[player];
    b->prey = hypotf(pb->pos[0] - body->pos[0], pb->pos[1] - body->pos[1]);
    if (sim->fighters[b->fighter].vitals.dead) {
        eng_flame_init(&b->flame);
        if (eng_dragon_flight_airborne(&b->flight)) { /* mort en vol : il tombe */
            (void)eng_dragon_flight_fall(&b->flight);
            dragon_place(b, sim);
        }
        return;
    }
    (void)engaged; /* l'IA peut avoir perdu le joueur derrière un pilier : pas lui (44 m de haut) */
    const bool hunting = play_boss_hunting(b, sim, player);
    if (!eng_dragon_flight_airborne(&b->flight)) {
        dragon_turn(b, sim, player, hunting && !eng_flame_busy(&b->flame));
        dragon_legs(b, sim, player);
        dragon_breath(b, sim, player, hunting, invincible, out);
    }
    dragon_fly(b, sim, player, invincible, out);
}

void play_boss_after_step(PlayBoss *b, EngCombatSim *sim, uint32_t player, bool engaged,
                          bool invincible, PlayBossEvent *out)
{
    if (out == NULL) {
        return;
    }
    (void)memset(out, 0, sizeof *out);
    if (!active(b, sim) || player >= sim->count) {
        return;
    }
    b->time += 1.0F / 60.0F;
    if (b->kind == ENG_BOSSKIND_RIDER) {
        rider_after(b, sim, out);
    } else {
        dragon_after(b, sim, player, engaged, invincible, out);
    }
}

bool play_boss_hides_fighter(const PlayBoss *b, uint32_t fighter)
{
    return b != NULL && b->kind == ENG_BOSSKIND_DRAGON && b->fighter == fighter;
}

float play_boss_rider_lift(const PlayBoss *b, uint32_t fighter)
{
    if (b == NULL || b->kind != ENG_BOSSKIND_RIDER || b->fighter != fighter ||
        b->rider.phase == ENG_RIDER_ON_FOOT) {
        return 0.0F;
    }
    const float seat = eng_creature_horse_saddle(eng_rider_horse_fall(&b->rider)) - RIDER_SEAT;
    return seat > 0.0F ? seat * HORSE_SCALE : 0.0F;
}

bool play_boss_rider_down(const PlayBoss *b, uint32_t fighter)
{
    return b != NULL && b->kind == ENG_BOSSKIND_RIDER && b->fighter == fighter &&
           eng_rider_down(&b->rider);
}

bool play_boss_anchor(const PlayBoss *b, const EngCombatSim *sim, float alpha, float xz[2],
                      float *yaw)
{
    if (!active(b, sim) || xz == NULL || yaw == NULL) {
        return false;
    }
    if (b->kind == ENG_BOSSKIND_RIDER && b->rider.phase != ENG_RIDER_MOUNTED) {
        xz[0] = b->horse_pos[0]; /* le cheval reste où il est tombé */
        xz[1] = b->horse_pos[1];
        *yaw = b->horse_yaw;
        return true;
    }
    const EngSimBody *body = &sim->bodies[b->fighter];
    const float t = isfinite(alpha) ? alpha : 1.0F;
    xz[0] = body->prev_pos[0] + ((body->pos[0] - body->prev_pos[0]) * t);
    xz[1] = body->prev_pos[1] + ((body->pos[1] - body->prev_pos[1]) * t);
    *yaw = body->yaw;
    return true;
}

uint32_t play_boss_boxes(const PlayBoss *b, const EngCombatSim *sim, const float feet[3], float yaw,
                         mat4 *places, uint8_t *colors, uint32_t max)
{
    if (!active(b, sim) || feet == NULL || places == NULL || colors == NULL) {
        return 0U;
    }
    EngCreaturePart parts[ENG_CREATURE_MAX_PARTS];
    const uint32_t cap = max < ENG_CREATURE_MAX_PARTS ? max : ENG_CREATURE_MAX_PARTS;
    const bool dead = sim->fighters[b->fighter].vitals.dead;
    float scale = HORSE_SCALE;
    float flatten = 1.0F;
    uint32_t n = 0;
    if (b->kind == ENG_BOSSKIND_RIDER) {
        n = eng_creature_horse(b->time, b->gait, eng_rider_horse_fall(&b->rider), parts, cap);
    } else {
        scale = PLAY_DRAGON_SCALE;
        flatten = dead ? 0.4F : 1.0F; /* mort : affaissé au sol, au repos */
        n = eng_creature_dragon(dead ? NULL : &b->pose, parts, cap);
    }
    mat4 root;
    glm_translate_make(root, (vec3){feet[0], feet[1], feet[2]});
    glm_rotate_y(root, yaw, root);
    if (b->kind == ENG_BOSSKIND_DRAGON && !dead && fabsf(b->flight.bank) > 1e-4F) {
        /* en virage il penche vers l'intérieur, autour de l'axe de son corps (à 1,9 m du modèle) */
        const float pivot = 1.9F * PLAY_DRAGON_SCALE;
        glm_translate(root, (vec3){0.0F, pivot, 0.0F});
        glm_rotate_z(root, -0.6F * b->flight.bank, root);
        glm_translate(root, (vec3){0.0F, -pivot, 0.0F});
    }
    glm_scale(root, (vec3){scale, scale * flatten, scale});
    for (uint32_t i = 0; i < n; i++) {
        const EngCreaturePart *p = &parts[i];
        mat4 m;
        glm_translate_to(root, (vec3){p->pos[0], p->pos[1], p->pos[2]}, m);
        glm_rotate_y(m, p->rot[1], m);
        glm_rotate_x(m, p->rot[0], m);
        glm_rotate_z(m, p->rot[2], m);
        glm_scale(m, (vec3){2.0F * p->half[0], 2.0F * p->half[1], 2.0F * p->half[2]});
        glm_mat4_copy(m, places[i]);
        colors[i] = p->color;
    }
    return n;
}

void play_boss_shift(PlayBoss *b, float dx, float dz)
{
    if (b != NULL && isfinite(dx) && isfinite(dz)) {
        b->horse_pos[0] += dx;
        b->horse_pos[1] += dz;
        b->home[0] += dx;
        b->home[1] += dz;
        eng_dragon_flight_shift(&b->flight, dx, dz);
    }
}

float play_boss_altitude(const PlayBoss *b)
{
    return b != NULL && b->kind == ENG_BOSSKIND_DRAGON ? b->flight.altitude : 0.0F;
}

bool play_boss_airborne(const PlayBoss *b)
{
    return b != NULL && b->kind == ENG_BOSSKIND_DRAGON && eng_dragon_flight_airborne(&b->flight);
}

bool play_boss_hunting(const PlayBoss *b, const EngCombatSim *sim, uint32_t player)
{
    if (!active(b, sim) || b->kind != ENG_BOSSKIND_DRAGON || player >= sim->count ||
        sim->fighters[b->fighter].vitals.dead || sim->fighters[player].vitals.dead) {
        return false;
    }
    const EngSimBody *pb = &sim->bodies[player];
    return hypotf(pb->pos[0] - b->home[0], pb->pos[1] - b->home[1]) <= PLAY_DRAGON_DOMAIN;
}

bool play_boss_diving(const PlayBoss *b)
{
    return b != NULL && b->kind == ENG_BOSSKIND_DRAGON && b->flight.phase == ENG_DRAGON_STRAFE &&
           b->flight.aim_locked;
}

bool play_boss_fire_point(const PlayBoss *b, float out[2])
{
    if (b == NULL || out == NULL || b->kind != ENG_BOSSKIND_DRAGON || !b->flight.flame) {
        return false;
    }
    eng_dragon_flight_fire_point(&b->flight, PLAY_DRAGON_FIRE_LEAD, out);
    return true;
}
