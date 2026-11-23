#include "engine/training_dummy.h"

#include <string.h>

void eng_dummy_init(EngTrainingDummy *d, uint32_t fighter, EngDummyMode mode, uint32_t period)
{
    if (d == NULL) {
        return;
    }
    memset(d, 0, sizeof *d);
    d->fighter = fighter;
    d->mode = mode;
    d->period = period == 0U ? 90U : period;
    d->reset_after = 180U;
    d->respawn_after = 120U;
}

void eng_dummy_tick(EngTrainingDummy *d, EngCombatSim *sim, EngFighterInput *in)
{
    if (d == NULL || sim == NULL || in == NULL || d->fighter >= sim->count) {
        return;
    }
    memset(in, 0, sizeof *in);
    EngFighter *f = &sim->fighters[d->fighter];
    d->since_hit++;
    for (uint32_t k = 0; k < sim->event_count; k++) {
        const EngSimEvent *e = &sim->events[k];
        if (e->defender == d->fighter && e->kind == ENG_HITKIND_DAMAGE) {
            d->total_damage += e->dealt;
            d->last_damage = e->dealt;
            d->hits_taken++;
            d->since_hit = 0;
        }
    }
    if (f->vitals.dead) {
        d->since_death++;
        if (d->respawn_after > 0U && d->since_death >= d->respawn_after) {
            const EngFighterParams p = f->params;
            const float max_hp = f->vitals.max_hp;
            eng_fighter_init(f, &p);
            f->vitals.max_hp = max_hp;
            f->vitals.hp = max_hp;
            d->since_death = 0;
        }
        return;
    }
    if (d->reset_after > 0U && d->since_hit >= d->reset_after && f->vitals.hp < f->vitals.max_hp) {
        f->vitals.hp = f->vitals.max_hp;
    }
    d->timer++;
    switch (d->mode) {
    case ENG_DUMMY_BLOCK:
        in->block_held = true;
        in->block_pressed = (d->timer == 1U);
        break;
    case ENG_DUMMY_ATTACK:
        in->light = (d->timer % d->period) == 0U;
        break;
    default:
        break;
    }
}
