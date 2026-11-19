#include "engine/combat_debug.h"

#include <string.h>

#include "engine/hurtbox.h"

bool eng_cdbg_init(EngCombatDebugger *d, const EngReplay *replay, const EngAttackSet *attacks,
                   const EngReplaySetup *setup)
{
    if (d == NULL || replay == NULL || attacks == NULL || setup == NULL ||
        replay->fighters != setup->count) {
        return false;
    }
    memset(d, 0, sizeof *d);
    d->replay = replay;
    d->attacks = attacks;
    d->setup = *setup;
    d->show_hurtboxes = true;
    d->show_weapons = true;
    d->show_contacts = true;
    memset(d->state_rows, ' ', sizeof d->state_rows);
    memset(d->hit_rows, ' ', sizeof d->hit_rows);
    return eng_replay_setup_sim(&d->sim, attacks, &d->setup);
}

uint32_t eng_cdbg_tick(const EngCombatDebugger *d)
{
    return d != NULL ? d->sim.tick : 0U;
}

uint32_t eng_cdbg_length(const EngCombatDebugger *d)
{
    return d != NULL && d->replay != NULL ? d->replay->ticks : 0U;
}

char eng_cdbg_state_char(const EngFighter *f, const EngAttackSet *attacks, bool frozen)
{
    if (f == NULL) {
        return '?';
    }
    if (frozen) {
        return '#';
    }
    switch (f->fsm.state) {
    case ENG_CS_IDLE:
        return '.';
    case ENG_CS_MOVE:
        return 'm';
    case ENG_CS_ATTACK: {
        const EngAttack *a = eng_fighter_attack(f, attacks);
        if (a == NULL) {
            return '?';
        }
        if (f->attack_tick < a->startup) {
            return 's';
        }
        return eng_attack_is_active(a, f->attack_tick) ? 'A' : 'r';
    }
    case ENG_CS_DODGE:
        return eng_fighter_dodge_invulnerable(f) ? 'D' : 'd';
    case ENG_CS_BLOCK:
        return 'b';
    case ENG_CS_PARRY:
        return 'P';
    case ENG_CS_HITSTUN:
        return 'h';
    case ENG_CS_STAGGER:
        return 'S';
    case ENG_CS_DEAD:
        return 'X';
    default:
        return '?';
    }
}

bool eng_cdbg_step(EngCombatDebugger *d)
{
    if (d == NULL || d->sim.tick >= d->replay->ticks) {
        return false;
    }
    const EngPackedInput *p = eng_replay_tick(d->replay, d->sim.tick);
    EngFighterInput in[ENG_SIM_MAX];
    for (uint32_t i = 0; i < d->sim.count; i++) {
        in[i] = eng_input_unpack(p[i]);
    }
    const bool frozen = d->sim.hitstop > 0U;
    eng_sim_step(&d->sim, in);
    const uint32_t slot = d->sim.tick % ENG_CDBG_HISTORY;
    for (uint32_t i = 0; i < d->sim.count; i++) {
        d->state_rows[i][slot] = eng_cdbg_state_char(&d->sim.fighters[i], d->attacks, frozen);
        d->hit_rows[i][slot] = ' ';
    }
    for (uint32_t k = 0; k < d->sim.event_count; k++) {
        const EngSimEvent *e = &d->sim.events[k];
        char def = '*';
        if (e->kind == ENG_HITKIND_BLOCKED) {
            def = 'b';
        } else if (e->kind == ENG_HITKIND_PARRIED) {
            def = 'p';
        }
        d->hit_rows[e->defender][slot] = def;
        d->hit_rows[e->attacker][slot] = '>';
    }
    return true;
}

bool eng_cdbg_seek(EngCombatDebugger *d, uint32_t tick)
{
    if (d == NULL || tick > d->replay->ticks) {
        return false;
    }
    if (tick < d->sim.tick) { /* reculer : repartir du début */
        if (!eng_replay_setup_sim(&d->sim, d->attacks, &d->setup)) {
            return false;
        }
        memset(d->state_rows, ' ', sizeof d->state_rows);
        memset(d->hit_rows, ' ', sizeof d->hit_rows);
    }
    while (d->sim.tick < tick) {
        if (!eng_cdbg_step(d)) {
            return false;
        }
    }
    return true;
}

bool eng_cdbg_step_back(EngCombatDebugger *d)
{
    if (d == NULL || d->sim.tick == 0U) {
        return false;
    }
    return eng_cdbg_seek(d, d->sim.tick - 1U);
}

static uint32_t line(const EngCombatDebugger *d, const char rows[][ENG_CDBG_HISTORY],
                     uint32_t fighter, uint32_t window, char *out, size_t cap)
{
    if (d == NULL || out == NULL || cap == 0U || fighter >= d->sim.count) {
        return 0;
    }
    uint32_t n = window > ENG_CDBG_HISTORY ? ENG_CDBG_HISTORY : window;
    if (n > d->sim.tick) {
        n = d->sim.tick;
    }
    if ((size_t)n + 1U > cap) {
        n = (uint32_t)(cap - 1U);
    }
    for (uint32_t i = 0; i < n; i++) {
        const uint32_t tick = d->sim.tick - n + 1U + i;
        out[i] = rows[fighter][tick % ENG_CDBG_HISTORY];
    }
    out[n] = '\0';
    return n;
}

uint32_t eng_cdbg_timeline(const EngCombatDebugger *d, uint32_t fighter, uint32_t window, char *out,
                           size_t cap)
{
    return d != NULL ? line(d, d->state_rows, fighter, window, out, cap) : 0U;
}

uint32_t eng_cdbg_hitline(const EngCombatDebugger *d, uint32_t fighter, uint32_t window, char *out,
                          size_t cap)
{
    return d != NULL ? line(d, d->hit_rows, fighter, window, out, cap) : 0U;
}

uint32_t eng_cdbg_draw(const EngCombatDebugger *d, EngDebugLines *lines)
{
    if (d == NULL || lines == NULL) {
        return 0;
    }
    uint32_t drawn = 0;
    for (uint32_t i = 0; i < d->sim.count; i++) {
        const EngFighter *f = &d->sim.fighters[i];
        if (d->show_hurtboxes) {
            EngCapsule box[ENG_HURTBOX_MAX];
            EngHitZone zones[ENG_HURTBOX_MAX];
            uint32_t nb = ENG_SIM_HURTBOXES;
            if (d->sim.rig[i].count > 0U) {
                nb = d->sim.rig[i].count;
                memcpy(box, d->sim.rig[i].cur, sizeof(EngCapsule) * nb);
                memcpy(zones, d->sim.rig[i].zones, sizeof(EngHitZone) * nb);
            } else {
                eng_sim_hurtboxes(&d->sim.bodies[i], false, box, zones);
            }
            const bool touchable = eng_fighter_hittable(f);
            for (uint32_t k = 0; k < nb; k++) {
                const EngDebugColor col = touchable ? eng_zone_color(zones[k]) : ENG_DEBUG_GRAY;
                if (eng_capsule_debug_draw(lines, &box[k], col)) {
                    drawn++;
                }
            }
        }
        if (d->show_weapons && eng_fighter_hitboxes_active(f, d->attacks)) {
            const EngAttack *atk = eng_fighter_attack(f, d->attacks);
            EngCapsule w[ENG_ATTACK_MAX_HITBOXES];
            const uint32_t nw = eng_sim_weapons(&d->sim.bodies[i], false, atk, w);
            for (uint32_t k = 0; k < nw; k++) {
                if (eng_capsule_debug_draw(lines, &w[k], ENG_DEBUG_RED)) {
                    drawn++;
                }
            }
        }
    }
    if (d->show_contacts) {
        for (uint32_t k = 0; k < d->sim.event_count; k++) {
            const vec3 c = {d->sim.events[k].point[0], d->sim.events[k].point[1],
                            d->sim.events[k].point[2]};
            (void)eng_debug_sphere(lines, c, 0.08F, ENG_DEBUG_WHITE, 12);
        }
    }
    return drawn;
}
