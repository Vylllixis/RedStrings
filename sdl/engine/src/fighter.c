#include "engine/fighter.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include <stdio.h>

#include "engine/assert.h"

void eng_fighter_default_params(EngFighterParams *p)
{
    if (p == NULL) {
        return;
    }
    memset(p, 0, sizeof *p);
    (void)snprintf(p->light_name, sizeof p->light_name, "light1");
    (void)snprintf(p->heavy_name, sizeof p->heavy_name, "heavy");
    p->max_stamina = 100.0F;
    p->stamina_regen = 25.0F / 60.0F;
    p->stamina_delay = 60U;
    p->max_posture = 100.0F;
    p->posture_regen = 20.0F / 60.0F;
    p->posture_delay = 90U;
    p->parry_frames = 8U;
    p->block_multiplier = 0.2F;
    p->dodge_total = 24U;
    p->dodge_inv_from = 3U;
    p->dodge_inv_to = 15U;
    p->dodge_cost = 15.0F;
    p->dodge_distance = 3.0F;
    p->stagger_ticks = 60U;
    p->hitstun_base = 10U;
    p->hitstun_max = 60U;
    p->knockback_per_posture = 0.02F;
}

static bool pos_finite(float v)
{
    return isfinite(v) && v >= 0.0F;
}

static bool params_valid(const EngFighterParams *p)
{
    return p != NULL && p->light_name[0] != '\0' && p->heavy_name[0] != '\0' &&
           memchr(p->light_name, '\0', sizeof p->light_name) != NULL &&
           memchr(p->heavy_name, '\0', sizeof p->heavy_name) != NULL && isfinite(p->max_stamina) &&
           p->max_stamina > 0.0F && pos_finite(p->stamina_regen) && isfinite(p->max_posture) &&
           p->max_posture > 0.0F && pos_finite(p->posture_regen) && p->parry_frames > 0U &&
           isfinite(p->block_multiplier) && p->block_multiplier >= 0.0F &&
           p->block_multiplier <= 1.0F && p->dodge_total > 0U &&
           p->dodge_inv_from < p->dodge_inv_to && p->dodge_inv_to <= p->dodge_total &&
           pos_finite(p->dodge_cost) && pos_finite(p->dodge_distance) && p->stagger_ticks > 0U &&
           p->hitstun_base <= p->hitstun_max && pos_finite(p->knockback_per_posture);
}

bool eng_fighter_set_params(EngFighter *f, const EngFighterParams *p)
{
    if (f == NULL || !params_valid(p)) {
        return false;
    }
    f->params = *p;
    return true;
}

void eng_fighter_init(EngFighter *f, const EngFighterParams *params)
{
    if (f == NULL) {
        return;
    }
    memset(f, 0, sizeof *f);
    if (params != NULL && params_valid(params)) {
        f->params = *params;
    } else {
        eng_fighter_default_params(&f->params);
    }
    eng_fsm_init(&f->fsm);
    eng_vitals_init(&f->vitals, ENG_DAMAGE_DEFAULT_MAX_HP);
    eng_ibuf_init(&f->buffer, ENG_IBUF_DEFAULT_WINDOW);
    eng_hittracker_reset(&f->tracker, 0U);
    f->stamina = f->params.max_stamina;
    f->posture = f->params.max_posture;
    f->attack = ENG_ATTACK_NONE;
}

const EngAttack *eng_fighter_attack(const EngFighter *f, const EngAttackSet *attacks)
{
    if (f == NULL || attacks == NULL || f->fsm.state != ENG_CS_ATTACK ||
        f->attack >= attacks->count) {
        return NULL;
    }
    return &attacks->attacks[f->attack];
}

bool eng_fighter_hitboxes_active(const EngFighter *f, const EngAttackSet *attacks)
{
    const EngAttack *a = eng_fighter_attack(f, attacks);
    return a != NULL && eng_attack_is_active(a, f->attack_tick);
}

bool eng_fighter_hittable(const EngFighter *f)
{
    return f != NULL && eng_vitals_hittable(&f->vitals, f->tick);
}

bool eng_fighter_dodge_invulnerable(const EngFighter *f)
{
    return f != NULL && f->fsm.state == ENG_CS_DODGE && f->state_tick >= f->params.dodge_inv_from &&
           f->state_tick < f->params.dodge_inv_to;
}

/* ── internes ── */

static void spend_stamina(EngFighter *f, float cost)
{
    f->stamina -= cost;
    if (f->stamina < 0.0F) {
        f->stamina = 0.0F;
    }
    f->stamina_wait = f->params.stamina_delay;
}

static void apply_dodge_invuln(EngFighter *f)
{
    if (eng_fighter_dodge_invulnerable(f)) {
        eng_damage_set_invulnerable(&f->vitals, f->tick + 1U);
    }
}

static bool start_attack(EngFighter *f, const EngAttackSet *attacks, uint32_t idx, bool combo)
{
    if (attacks == NULL || idx >= attacks->count) {
        return false;
    }
    const EngAttack *a = &attacks->attacks[idx];
    if (f->stamina < a->stamina) {
        f->events |= ENG_FEV_NO_STAMINA;
        return false;
    }
    if (!eng_fsm_request(&f->fsm, ENG_CS_ATTACK)) {
        return false;
    }
    spend_stamina(f, a->stamina);
    f->attack = idx;
    f->attack_tick = 0U;
    f->attack_serial++;
    eng_hittracker_reset(&f->tracker, f->attack_serial);
    f->events |= ENG_FEV_ATTACK_START | (combo ? ENG_FEV_COMBO : 0U);
    return true;
}

static bool start_dodge(EngFighter *f, const EngFighterInput *in)
{
    if (f->stamina < f->params.dodge_cost) {
        f->events |= ENG_FEV_NO_STAMINA;
        return false;
    }
    if (!eng_fsm_request(&f->fsm, ENG_CS_DODGE)) {
        return false;
    }
    spend_stamina(f, f->params.dodge_cost);
    float dx = in->move_dir[0];
    float dz = in->move_dir[1];
    const float len = sqrtf(dx * dx + dz * dz);
    if (isfinite(len) && len > 0.5F) {
        dx /= len;
        dz /= len;
    } else { /* sans direction : pas en arrière */
        dx = sinf(f->yaw);
        dz = cosf(f->yaw);
    }
    f->dodge_dir[0] = dx;
    f->dodge_dir[1] = dz;
    f->state_tick = 0U;
    f->attack = ENG_ATTACK_NONE;
    f->events |= ENG_FEV_DODGE_START;
    apply_dodge_invuln(f);
    return true;
}

static float smooth(float u)
{
    return u * u * (3.0F - 2.0F * u);
}

static void go_idle(EngFighter *f)
{
    (void)eng_fsm_request(&f->fsm, ENG_CS_IDLE);
    f->attack = ENG_ATTACK_NONE;
}

/* États libres (Idle / Move) : prend l'action la plus prioritaire du buffer. */
static void free_logic(EngFighter *f, const EngAttackSet *attacks, const EngFighterInput *in)
{
    const EngCombatAction act = eng_ibuf_take(&f->buffer, f->tick, ENG_ACT_ALL);
    bool acted = false;
    switch (act) {
    case ENG_ACT_DODGE:
        acted = start_dodge(f, in);
        break;
    case ENG_ACT_BLOCK:
        if (eng_fsm_request(&f->fsm, ENG_CS_PARRY)) {
            f->state_tick = 0U;
            f->events |= ENG_FEV_PARRY_START;
            acted = true;
        }
        break;
    case ENG_ACT_HEAVY:
        acted = attacks != NULL &&
                start_attack(f, attacks, eng_attackset_find(attacks, f->params.heavy_name), false);
        break;
    case ENG_ACT_LIGHT:
        acted = attacks != NULL &&
                start_attack(f, attacks, eng_attackset_find(attacks, f->params.light_name), false);
        break;
    default:
        break;
    }
    if (!acted && (f->fsm.state == ENG_CS_IDLE || f->fsm.state == ENG_CS_MOVE)) {
        (void)eng_fsm_request(&f->fsm, in->moving ? ENG_CS_MOVE : ENG_CS_IDLE);
    }
}

static void regen(EngFighter *f)
{
    const bool busy = f->fsm.state == ENG_CS_ATTACK || f->fsm.state == ENG_CS_DODGE;
    if (f->stamina_wait > 0U) {
        f->stamina_wait--;
    } else if (!busy && f->stamina < f->params.max_stamina) {
        f->stamina += f->params.stamina_regen;
        if (f->stamina > f->params.max_stamina) {
            f->stamina = f->params.max_stamina;
        }
    }
    if (f->posture_wait > 0U) {
        f->posture_wait--;
    } else if (f->fsm.state != ENG_CS_STAGGER && f->posture < f->params.max_posture) {
        f->posture += f->params.posture_regen;
        if (f->posture > f->params.max_posture) {
            f->posture = f->params.max_posture;
        }
    }
}

void eng_fighter_buffer_inputs(EngFighter *f, const EngFighterInput *in)
{
    if (f == NULL || in == NULL) {
        return;
    }
    if (in->light) {
        (void)eng_ibuf_push(&f->buffer, ENG_ACT_LIGHT, f->tick);
    }
    if (in->heavy) {
        (void)eng_ibuf_push(&f->buffer, ENG_ACT_HEAVY, f->tick);
    }
    if (in->dodge) {
        (void)eng_ibuf_push(&f->buffer, ENG_ACT_DODGE, f->tick);
    }
    if (in->block_pressed) {
        (void)eng_ibuf_push(&f->buffer, ENG_ACT_BLOCK, f->tick);
    }
}

void eng_fighter_tick(EngFighter *f, const EngAttackSet *attacks, const EngFighterInput *in)
{
    if (f == NULL || in == NULL) {
        return;
    }
    f->tick++;
    f->events = 0U;
    f->root_delta[0] = 0.0F;
    f->root_delta[1] = 0.0F;
    if (f->fsm.state == ENG_CS_DEAD) {
        return;
    }
    eng_fighter_buffer_inputs(f, in);
    regen(f);

    switch (f->fsm.state) {
    case ENG_CS_ATTACK: {
        f->attack_tick++;
        const EngAttack *a =
            (attacks != NULL && f->attack < attacks->count) ? &attacks->attacks[f->attack] : NULL;
        if (a == NULL || f->attack_tick >= eng_attack_total(a)) {
            f->events |= ENG_FEV_ATTACK_END;
            go_idle(f);
            free_logic(f, attacks, in);
            break;
        }
        if (f->attack_tick == a->startup) {
            f->events |= ENG_FEV_ATTACK_ACTIVE;
        }
        if (eng_attack_can_cancel(a, f->attack_tick)) {
            const uint32_t allowed = ENG_ACT_MASK(ENG_ACT_DODGE) |
                                     (a->next != ENG_ATTACK_NONE ? ENG_ACT_MASK(ENG_ACT_LIGHT) |
                                                                       ENG_ACT_MASK(ENG_ACT_HEAVY)
                                                                 : 0U);
            const EngCombatAction act = eng_ibuf_take(&f->buffer, f->tick, allowed);
            if (act == ENG_ACT_DODGE) {
                f->events |= ENG_FEV_ATTACK_END;
                (void)start_dodge(f, in);
            } else if (act == ENG_ACT_LIGHT) {
                (void)start_attack(f, attacks, a->next, true);
            } else if (act == ENG_ACT_HEAVY) {
                (void)start_attack(f, attacks, eng_attackset_find(attacks, f->params.heavy_name),
                                   true);
            }
        }
        break;
    }
    case ENG_CS_DODGE: {
        f->state_tick++;
        const float t = (float)f->params.dodge_total;
        const float step = f->params.dodge_distance * (smooth((float)f->state_tick / t) -
                                                       smooth((float)(f->state_tick - 1U) / t));
        f->root_delta[0] = f->dodge_dir[0] * step;
        f->root_delta[1] = f->dodge_dir[1] * step;
        apply_dodge_invuln(f);
        if (f->state_tick >= f->params.dodge_total) {
            go_idle(f);
            free_logic(f, attacks, in);
        }
        break;
    }
    case ENG_CS_PARRY:
        f->state_tick++;
        if (f->state_tick >= f->params.parry_frames) {
            if (in->block_held) {
                (void)eng_fsm_request(&f->fsm, ENG_CS_BLOCK);
                f->events |= ENG_FEV_BLOCK_START;
            } else {
                go_idle(f);
                free_logic(f, attacks, in);
            }
        }
        break;
    case ENG_CS_BLOCK:
        if (!in->block_held) {
            go_idle(f);
            free_logic(f, attacks, in);
        } else {
            const EngCombatAction act =
                eng_ibuf_take(&f->buffer, f->tick,
                              ENG_ACT_MASK(ENG_ACT_LIGHT) | ENG_ACT_MASK(ENG_ACT_HEAVY) |
                                  ENG_ACT_MASK(ENG_ACT_DODGE));
            if (act == ENG_ACT_DODGE) {
                (void)start_dodge(f, in);
            } else if (act == ENG_ACT_HEAVY) {
                (void)start_attack(f, attacks, eng_attackset_find(attacks, f->params.heavy_name),
                                   false);
            } else if (act == ENG_ACT_LIGHT) {
                (void)start_attack(f, attacks, eng_attackset_find(attacks, f->params.light_name),
                                   false);
            }
        }
        break;
    case ENG_CS_HITSTUN:
    case ENG_CS_STAGGER:
        if (f->stun_left > 0U) {
            f->stun_left--;
        }
        if (f->stun_left == 0U) {
            if (f->fsm.state == ENG_CS_STAGGER) {
                f->posture = f->params.max_posture;
            }
            go_idle(f);
            f->events |= ENG_FEV_RECOVERED;
        }
        break;
    default: /* IDLE, MOVE */
        free_logic(f, attacks, in);
        break;
    }
    eng_fsm_tick(&f->fsm);
}

/* ── touches ── */

EngReactDir eng_react_dir(float defender_yaw, const float force_dir[2])
{
    const float fx = -sinf(defender_yaw);
    const float fz = -cosf(defender_yaw);
    const float rx = cosf(defender_yaw);
    const float rz = -sinf(defender_yaw);
    const float df = force_dir[0] * fx + force_dir[1] * fz;
    const float dr = force_dir[0] * rx + force_dir[1] * rz;
    if (df <= -0.70710678F) {
        return ENG_REACT_FRONT;
    }
    if (df >= 0.70710678F) {
        return ENG_REACT_BACK;
    }
    return dr > 0.0F ? ENG_REACT_LEFT : ENG_REACT_RIGHT;
}

static void enter_stagger(EngFighter *f)
{
    if (eng_fsm_request(&f->fsm, ENG_CS_STAGGER)) {
        f->stun_left = f->params.stagger_ticks;
        f->attack = ENG_ATTACK_NONE;
        f->events |= ENG_FEV_STAGGER;
    }
}

EngHitOutcome eng_combat_apply_hit(EngFighter *attacker, EngFighter *defender, const EngAttack *atk,
                                   EngHitZone zone, const float force_dir[2])
{
    EngHitOutcome o;
    memset(&o, 0, sizeof o);
    if (attacker == NULL || defender == NULL || atk == NULL || force_dir == NULL) {
        return o;
    }
    if (!eng_fighter_hittable(defender)) {
        return o;
    }
    o.hitstop = atk->hitstop;
    const float scale =
        attacker->params.damage_scale > 0.0F && isfinite(attacker->params.damage_scale)
            ? attacker->params.damage_scale
            : 1.0F;
    const float damage = atk->damage * scale;
    o.dir = eng_react_dir(defender->yaw, force_dir);
    const float klen = sqrtf(force_dir[0] * force_dir[0] + force_dir[1] * force_dir[1]);
    const bool has_dir = isfinite(klen) && klen > 1e-6F;

    if (defender->fsm.state == ENG_CS_PARRY) { /* parade réussie */
        o.kind = ENG_HITKIND_PARRIED;
        defender->events |= ENG_FEV_PARRY_SUCCESS;
        if (attacker->fsm.state != ENG_CS_DEAD) {
            enter_stagger(attacker);
            o.attacker_staggered = attacker->fsm.state == ENG_CS_STAGGER;
        }
        return o;
    }
    if (defender->fsm.state == ENG_CS_BLOCK) { /* garde */
        const EngDamageResult r = eng_damage_apply(
            &defender->vitals, damage * defender->params.block_multiplier, zone, defender->tick);
        if (r.ignored) {
            o.kind = ENG_HITKIND_IGNORED;
            return o;
        }
        o.kind = ENG_HITKIND_BLOCKED;
        o.dealt = r.dealt;
        o.killed = r.killed;
        defender->events |= ENG_FEV_BLOCKED_HIT;
        if (r.killed) {
            (void)eng_fsm_request(&defender->fsm, ENG_CS_DEAD);
            defender->events |= ENG_FEV_DEATH;
            return o;
        }
        o.posture_dealt = atk->posture;
        defender->posture -= atk->posture;
        defender->posture_wait = defender->params.posture_delay;
        if (defender->posture <= 0.0F) {
            defender->posture = 0.0F;
            enter_stagger(defender);
            o.defender_staggered = true;
            o.stun_ticks = defender->params.stagger_ticks;
        }
        return o;
    }
    /* touche franche */
    const EngDamageResult r = eng_damage_apply(&defender->vitals, damage, zone, defender->tick);
    if (r.ignored) {
        return o;
    }
    o.kind = ENG_HITKIND_DAMAGE;
    o.dealt = r.dealt;
    o.killed = r.killed;
    defender->posture_wait = defender->params.posture_delay;
    if (r.killed) {
        (void)eng_fsm_request(&defender->fsm, ENG_CS_DEAD);
        defender->attack = ENG_ATTACK_NONE;
        defender->events |= ENG_FEV_DEATH;
        return o;
    }
    uint32_t stun =
        defender->params.hitstun_base + (uint32_t)(atk->posture < 0.0F ? 0.0F : atk->posture);
    if (stun > defender->params.hitstun_max) {
        stun = defender->params.hitstun_max;
    }
    if (has_dir) {
        const float kb = atk->posture * defender->params.knockback_per_posture;
        o.knockback[0] = force_dir[0] / klen * kb;
        o.knockback[1] = force_dir[1] / klen * kb;
    }
    if (defender->fsm.state == ENG_CS_STAGGER) {
        return o; /* déjà à terre : dégâts seulement */
    }
    if (eng_fsm_request(&defender->fsm, ENG_CS_HITSTUN)) {
        defender->attack = ENG_ATTACK_NONE;
        if (stun > defender->stun_left || defender->fsm.ticks == 0U) {
            defender->stun_left = stun;
        }
        defender->events |= ENG_FEV_HITSTUN;
        o.stun_ticks = stun;
    }
    return o;
}

uint64_t eng_fighter_hash(const EngFighter *f)
{
    uint64_t h = 1469598103934665603ULL;
#define MIX(v)                                                                                     \
    do {                                                                                           \
        uint32_t u_ = 0U;                                                                          \
        memcpy(&u_, &(v), sizeof u_ < sizeof(v) ? sizeof u_ : sizeof(v));                          \
        h = (h ^ u_) * 1099511628211ULL;                                                           \
    } while (0)
    if (f == NULL) {
        return h;
    }
    const uint32_t state = (uint32_t)f->fsm.state;
    MIX(state);
    MIX(f->tick);
    MIX(f->vitals.hp);
    MIX(f->vitals.invulnerable_until);
    MIX(f->stamina);
    MIX(f->posture);
    MIX(f->attack);
    MIX(f->attack_tick);
    MIX(f->state_tick);
    MIX(f->stun_left);
    MIX(f->stamina_wait);
    MIX(f->posture_wait);
    MIX(f->fsm.transitions);
    MIX(f->fsm.refused);
    MIX(f->buffer.count);
#undef MIX
    return h;
}
