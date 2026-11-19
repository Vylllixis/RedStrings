#include "engine/combat_hit.h"

#include <stddef.h>
#include <string.h>

void eng_hittracker_reset(EngHitTracker *t, uint32_t attack_id)
{
    if (t != NULL) {
        t->attack_id = attack_id;
        t->hit_mask = 0U;
    }
}

bool eng_hittracker_has_hit(const EngHitTracker *t, uint32_t fighter)
{
    return t != NULL && fighter < ENG_HIT_MAX_FIGHTERS && (t->hit_mask & (1U << fighter)) != 0U;
}

uint32_t eng_hit_resolve(uint32_t attacker, const EngCapsule *weapons_prev,
                         const EngCapsule *weapons_cur, uint32_t weapon_count,
                         const EngHitTarget *targets, uint32_t target_count, EngHitTracker *tracker,
                         EngHit *out, uint32_t max, uint32_t *total)
{
    if (total != NULL) {
        *total = 0;
    }
    if (tracker == NULL || attacker >= ENG_HIT_MAX_FIGHTERS ||
        (weapon_count > 0 && (weapons_prev == NULL || weapons_cur == NULL)) ||
        (target_count > 0 && targets == NULL) || target_count > ENG_HIT_MAX_FIGHTERS) {
        return 0;
    }
    /* ordre déterministe : indices de cibles triés par combattant croissant (insertion) */
    uint32_t order[ENG_HIT_MAX_FIGHTERS];
    uint32_t seen = 0U;
    for (uint32_t i = 0; i < target_count; i++) {
        const EngHitTarget *t = &targets[i];
        if (t->fighter >= ENG_HIT_MAX_FIGHTERS || (seen & (1U << t->fighter)) != 0U ||
            (t->count > 0 && (t->prev == NULL || t->cur == NULL || t->zones == NULL))) {
            return 0; /* indice invalide ou doublon : rien n'est résolu */
        }
        seen |= 1U << t->fighter;
        uint32_t j = i;
        while (j > 0 && targets[order[j - 1]].fighter > t->fighter) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = i;
    }
    uint32_t written = 0;
    uint32_t real = 0;
    for (uint32_t k = 0; k < target_count; k++) {
        const EngHitTarget *t = &targets[order[k]];
        if (t->fighter == attacker || !t->hittable || (tracker->hit_mask & (1U << t->fighter))) {
            continue;
        }
        bool found = false;
        EngHit best;
        memset(&best, 0, sizeof best);
        for (uint32_t h = 0; h < t->count; h++) {
            for (uint32_t w = 0; w < weapon_count; w++) {
                float time = 0.0F;
                float point[3];
                if (!eng_capsule_sweep(&weapons_prev[w], &weapons_cur[w], &t->prev[h], &t->cur[h],
                                       &time, point)) {
                    continue;
                }
                /* plus précoce ; à égalité la 1re rencontrée (hurtbox puis arme croissantes) */
                if (!found || time < best.time) {
                    found = true;
                    best.attacker = attacker;
                    best.defender = t->fighter;
                    best.hurtbox = h;
                    best.weapon = w;
                    best.zone = t->zones[h];
                    best.time = time;
                    memcpy(best.point, point, sizeof best.point);
                }
            }
        }
        if (!found) {
            continue;
        }
        tracker->hit_mask |= 1U << t->fighter;
        real++;
        if (written < max && out != NULL) {
            out[written++] = best;
        }
    }
    if (total != NULL) {
        *total = real;
    }
    return written;
}
