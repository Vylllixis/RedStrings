#include "engine/damage_numbers.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void eng_dmgnum_init(EngDamageNumbers *n)
{
    if (n != NULL) {
        memset(n, 0, sizeof *n);
    }
}

bool eng_dmgnum_spawn(EngDamageNumbers *n, const EngSimEvent *e)
{
    if (n == NULL || e == NULL || e->kind == ENG_HITKIND_IGNORED) {
        return false;
    }
    EngDmgKind kind = ENG_DMG_HIT;
    switch (e->kind) {
    case ENG_HITKIND_BLOCKED:
        kind = ENG_DMG_BLOCKED;
        break;
    case ENG_HITKIND_PARRIED:
        kind = ENG_DMG_PARRIED;
        break;
    default:
        kind = e->zone == ENG_ZONE_HEAD ? ENG_DMG_HEAD : ENG_DMG_HIT;
        break;
    }
    EngDamageNumber *d = &n->items[n->next];
    n->next = (n->next + 1U) % ENG_DMGNUM_MAX;
    d->active = true;
    d->kind = kind;
    d->value = e->dealt;
    d->pos[0] = e->point[0];
    d->pos[1] = e->point[1] + 0.2F;
    d->pos[2] = e->point[2];
    d->age = 0;
    n->spawned++;
    return true;
}

void eng_dmgnum_update(EngDamageNumbers *n)
{
    if (n == NULL) {
        return;
    }
    for (uint32_t i = 0; i < ENG_DMGNUM_MAX; i++) {
        EngDamageNumber *d = &n->items[i];
        if (!d->active) {
            continue;
        }
        d->age++;
        d->pos[1] += ENG_DMGNUM_RISE;
        if (d->age >= ENG_DMGNUM_LIFE) {
            d->active = false;
        }
    }
}

uint32_t eng_dmgnum_count(const EngDamageNumbers *n)
{
    uint32_t c = 0;
    if (n == NULL) {
        return 0;
    }
    for (uint32_t i = 0; i < ENG_DMGNUM_MAX; i++) {
        c += n->items[i].active ? 1U : 0U;
    }
    return c;
}

bool eng_dmgnum_text(const EngDamageNumber *d, char *out, size_t cap)
{
    if (d == NULL || out == NULL || cap == 0U) {
        return false;
    }
    const long v = lroundf(d->value);
    int w = 0;
    switch (d->kind) {
    case ENG_DMG_HEAD:
        w = snprintf(out, cap, "%ld!", v);
        break;
    case ENG_DMG_BLOCKED:
        w = snprintf(out, cap, "(%ld)", v);
        break;
    case ENG_DMG_PARRIED:
        w = snprintf(out, cap, "PARRY");
        break;
    default:
        w = snprintf(out, cap, "%ld", v);
        break;
    }
    return w >= 0 && (size_t)w < cap;
}

float eng_dmgnum_alpha(const EngDamageNumber *d)
{
    if (d == NULL || !d->active || d->age >= ENG_DMGNUM_LIFE) {
        return 0.0F;
    }
    const float u = (float)d->age / (float)ENG_DMGNUM_LIFE;
    return 1.0F - u * u;
}
