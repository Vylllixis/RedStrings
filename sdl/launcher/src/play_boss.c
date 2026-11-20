#include "play_boss.h"

#include <math.h>
#include <string.h>

#include "engine/combat_damage.h"

#define HORSE_SCALE 1.45F     /* le cheval porte un chevalier de 1,45 fois la taille du joueur */
#define RIDER_RADIUS 1.0F     /* à cheval : rayon de collision (m) */
#define RIDER_SEAT 0.3F       /* le cavalier s'enfonce dans la selle (m, avant échelle) */
#define DRAGON_SCALE 1.3F     /* tête à 4,4 m, 10 m d'envergure */
#define DRAGON_RADIUS 2.0F    /* rayon de collision du corps (m) */
#define FLAME_START_NEAR 4.0F /* le dragon ne souffle que sur une cible entre 4 et 14 m, devant */
#define FLAME_START_FAR 14.0F
#define FLAME_START_HALF 0.6F
#define FLAME_NEAR 2.5F /* le cône brûle de 2,5 à 16 m, à 24 degrés de l'axe */
#define FLAME_FAR 16.0F
#define FLAME_HALF 0.42F
#define FLAME_DAMAGE 8.0F /* par brûlure, fois les dégâts de la zone */

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
        sim->radius[fighter] = DRAGON_RADIUS;
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
    if (b->kind == ENG_BOSSKIND_DRAGON && eng_flame_busy(&b->flame)) {
        (void)memset(&in[b->fighter], 0, sizeof in[b->fighter]); /* il souffle : il ne bouge plus */
    }
    return false;
}

/* point du repère du dragon (avant -Z, mètres avant échelle) -> monde */
static void dragon_point(const EngSimBody *body, float x, float y, float z, float out[3])
{
    const float c = cosf(body->yaw);
    const float s = sinf(body->yaw);
    const float lx = x * DRAGON_SCALE;
    const float lz = z * DRAGON_SCALE;
    out[0] = body->pos[0] + (lx * c) + (lz * s);
    out[1] = y * DRAGON_SCALE;
    out[2] = body->pos[1] - (lx * s) + (lz * c);
}

static EngCapsule dragon_capsule(const EngSimBody *body, const float a[3], const float b[3],
                                 float radius)
{
    EngCapsule cap;
    dragon_point(body, a[0], a[1], a[2], cap.a);
    dragon_point(body, b[0], b[1], b[2], cap.b);
    cap.radius = radius * DRAGON_SCALE;
    return cap;
}

/* gueule du dragon selon l'attaque en cours : elle s'ouvre pendant l'anticipation, frappe pendant
 * la fenêtre active, se referme ensuite */
static float bite_target(const EngFighter *f, const EngAttackSet *attacks)
{
    if (f->vitals.dead || f->fsm.state != ENG_CS_ATTACK) {
        return 0.0F;
    }
    const EngAttack *atk = eng_fighter_attack(f, attacks);
    if (atk == NULL) {
        return 0.0F;
    }
    const EngAttackPhase ph = eng_attack_phase(atk, f->attack_tick);
    if (ph == ENG_ATTACK_STARTUP) {
        return 0.25F;
    }
    return ph == ENG_ATTACK_ACTIVE ? 1.0F : 0.4F;
}

void play_boss_rig(PlayBoss *b, EngCombatSim *sim)
{
    if (!active(b, sim) || b->kind != ENG_BOSSKIND_DRAGON) {
        return;
    }
    const EngFighter *f = &sim->fighters[b->fighter];
    const EngSimBody *body = &sim->bodies[b->fighter];
    b->bite += (bite_target(f, sim->attacks) - b->bite) * 0.4F;
    const float open = b->bite;
    /* hurtboxes : ventre (bas), tête, pattes avant et arrière (au sol : ce que l'épée atteint) */
    const float head[3] = {0.0F, 3.4F - (2.2F * open), -3.9F - (0.5F * open)};
    EngCapsule caps[4];
    caps[0] =
        dragon_capsule(body, (float[3]){0.0F, 1.5F, -1.8F}, (float[3]){0.0F, 1.5F, 1.8F}, 1.0F);
    caps[1] = dragon_capsule(body, head, (float[3]){head[0], head[1], head[2] - 0.6F}, 0.6F);
    caps[2] =
        dragon_capsule(body, (float[3]){-0.9F, 0.7F, -1.3F}, (float[3]){0.9F, 0.7F, -1.3F}, 0.6F);
    caps[3] =
        dragon_capsule(body, (float[3]){-0.9F, 0.7F, 1.4F}, (float[3]){0.9F, 0.7F, 1.4F}, 0.6F);
    const EngHitZone zones[4] = {ENG_ZONE_TORSO, ENG_ZONE_HEAD, ENG_ZONE_LIMB, ENG_ZONE_LIMB};
    (void)eng_sim_set_rig(sim, b->fighter, caps, zones, 4U);
    /* armes : le cou et la gueule qui s'abattent ; les griffes des pattes avant */
    EngCapsule weapons[2];
    weapons[0] = dragon_capsule(body, (float[3]){0.0F, 2.6F - (0.9F * open), -2.4F},
                                (float[3]){head[0], head[1] - 0.3F, head[2] - 0.8F}, 0.55F);
    weapons[1] = dragon_capsule(body, (float[3]){-1.2F, 0.6F, -1.8F - (0.6F * open)},
                                (float[3]){1.2F, 0.6F, -1.8F - (0.6F * open)}, 0.75F);
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

static void dragon_after(PlayBoss *b, EngCombatSim *sim, uint32_t player, bool engaged,
                         bool invincible, PlayBossEvent *out)
{
    const EngFighter *f = &sim->fighters[b->fighter];
    const EngSimBody *body = &sim->bodies[b->fighter];
    EngFighter *pf = &sim->fighters[player];
    const EngSimBody *pb = &sim->bodies[player];
    if (f->vitals.dead) {
        eng_flame_init(&b->flame);
        return;
    }
    const bool free_now = f->fsm.state == ENG_CS_IDLE || f->fsm.state == ENG_CS_MOVE;
    const bool can_start = engaged && free_now && !pf->vitals.dead &&
                           eng_flame_reaches(body->pos, body->yaw, pb->pos, FLAME_START_NEAR,
                                             FLAME_START_FAR, FLAME_START_HALF);
    if (!eng_flame_step(&b->flame, can_start) ||
        !eng_flame_reaches(body->pos, body->yaw, pb->pos, FLAME_NEAR, FLAME_FAR, FLAME_HALF) ||
        invincible || pf->fsm.state == ENG_CS_DODGE) {
        return; /* pas de brûlure : hors du cône, en esquive, invincible */
    }
    const float armor = isfinite(pf->params.armor_scale) && pf->params.armor_scale > 0.0F
                            ? pf->params.armor_scale
                            : 1.0F;
    const float guard = pf->fsm.state == ENG_CS_BLOCK ? pf->params.block_multiplier : 1.0F;
    const EngDamageResult r =
        eng_damage_apply(&pf->vitals, b->flame_damage * armor * guard, ENG_ZONE_TORSO, pf->tick);
    if (r.ignored) {
        return;
    }
    if (r.killed) { /* brûlure fatale : même fin qu'un coup */
        (void)eng_fsm_request(&pf->fsm, ENG_CS_DEAD);
    }
    out->burn_dealt = r.dealt;
    b->burns++;
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
        scale = DRAGON_SCALE;
        flatten = dead ? 0.4F : 1.0F; /* mort : affaissé au sol */
        n = eng_creature_dragon(dead ? 0.0F : b->time, dead ? 0.0F : b->bite,
                                dead ? 0.0F : eng_flame_jet(&b->flame), parts, cap);
        if (!dead && eng_flame_busy(&b->flame)) { /* la gueule s'ouvre pendant la charge */
            n = eng_creature_dragon(b->time, 0.35F * eng_flame_charge(&b->flame),
                                    eng_flame_jet(&b->flame), parts, cap);
        }
    }
    mat4 root;
    glm_translate_make(root, (vec3){feet[0], feet[1], feet[2]});
    glm_rotate_y(root, yaw, root);
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
    }
}
