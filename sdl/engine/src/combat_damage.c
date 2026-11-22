#include "engine/combat_damage.h"

#include <math.h>
#include <stddef.h>

float eng_zone_multiplier(EngHitZone zone)
{
    switch (zone) {
    case ENG_ZONE_HEAD:
        return 1.5F;
    case ENG_ZONE_TORSO:
        return 1.0F;
    case ENG_ZONE_LIMB:
        return 0.75F;
    default:
        return 1.0F;
    }
}

void eng_vitals_init(EngVitals *v, float max_hp)
{
    if (v == NULL) {
        return;
    }
    if (!isfinite(max_hp) || !(max_hp > 0.0F)) {
        max_hp = ENG_DAMAGE_DEFAULT_MAX_HP;
    }
    v->hp = max_hp;
    v->max_hp = max_hp;
    v->invulnerable_until = 0U;
    v->dead = false;
    v->died_tick = ENG_DAMAGE_NEVER;
    v->iframes = ENG_DAMAGE_DEFAULT_IFRAMES;
}

bool eng_vitals_hittable(const EngVitals *v, uint32_t now)
{
    return v != NULL && !v->dead && now >= v->invulnerable_until;
}

EngDamageResult eng_damage_apply(EngVitals *v, float base, EngHitZone zone, uint32_t now)
{
    EngDamageResult r = {0.0F, true, false};
    if (v == NULL || !isfinite(base) || base < 0.0F || !eng_vitals_hittable(v, now)) {
        return r;
    }
    r.ignored = false;
    r.dealt = base * eng_zone_multiplier(zone);
    if (r.dealt > v->hp) {
        r.dealt = v->hp;
    }
    v->hp -= r.dealt;
    if (v->hp <= 0.0F) {
        v->hp = 0.0F;
        v->dead = true;
        v->died_tick = now;
        r.killed = true;
    } else {
        eng_damage_set_invulnerable(v, now + v->iframes);
    }
    return r;
}

void eng_damage_set_invulnerable(EngVitals *v, uint32_t until)
{
    if (v != NULL && until > v->invulnerable_until) {
        v->invulnerable_until = until;
    }
}

void eng_vitals_heal(EngVitals *v, float amount)
{
    if (v == NULL || v->dead || !isfinite(amount) || amount < 0.0F) {
        return;
    }
    v->hp += amount;
    if (v->hp > v->max_hp) {
        v->hp = v->max_hp;
    }
}
