#include "engine/stamina.h"

#include <math.h>
#include <stddef.h>

void eng_stamina_init(EngStamina *s)
{
    s->params = (EngStaminaParams){20.0F, 25.0F, 1.0F, 25.0F};
    s->value = ENG_STAMINA_MAX;
    s->delay_left = 0.0F;
    s->exhausted = false;
}

bool eng_stamina_set_params(EngStamina *s, const EngStaminaParams *p)
{
    if (s == NULL || p == NULL || !isfinite(p->drain) || !(p->drain > 0.0F) ||
        !isfinite(p->regen) || !(p->regen > 0.0F) || !isfinite(p->regen_delay) ||
        !(p->regen_delay >= 0.0F) || !isfinite(p->resume_at) || !(p->resume_at >= 0.0F) ||
        p->resume_at > ENG_STAMINA_MAX) {
        return false;
    }
    s->params = *p;
    return true;
}

bool eng_stamina_update(EngStamina *s, bool want_sprint, bool moving, float dt, bool *sprinting)
{
    if (s == NULL || !(dt > 0.0F) || dt > 0.1F) {
        return false;
    }
    /* la variable de debug peut avoir été modifiée à la main : on la ramène dans les bornes */
    if (!isfinite(s->value)) {
        s->value = 0.0F;
    }
    s->value = fminf(fmaxf(s->value, 0.0F), ENG_STAMINA_MAX);

    const bool run = want_sprint && moving && !s->exhausted && s->value > 0.0F;
    if (run) {
        s->value -= s->params.drain * dt;
        s->delay_left = s->params.regen_delay;
        if (s->value <= 0.0F) {
            s->value = 0.0F;
            s->exhausted = true;
        }
    } else if (s->delay_left > 0.0F) {
        s->delay_left = fmaxf(s->delay_left - dt, 0.0F);
    } else {
        s->value = fminf(s->value + (s->params.regen * dt), ENG_STAMINA_MAX);
    }
    if (s->exhausted && s->value >= s->params.resume_at && !run) {
        s->exhausted = false;
    }
    if (sprinting != NULL) {
        *sprinting = run;
    }
    return true;
}

float eng_stamina_fraction(const EngStamina *s)
{
    return fminf(fmaxf(s->value / ENG_STAMINA_MAX, 0.0F), 1.0F);
}

void eng_stamina_restore(EngStamina *s)
{
    s->value = ENG_STAMINA_MAX;
    s->delay_left = 0.0F;
    s->exhausted = false;
}

bool eng_stamina_register_tweaks(EngStamina *s, EngTweaks *tweaks)
{
    return s != NULL && tweaks != NULL &&
           eng_tweaks_add_float(tweaks, "endurance", &s->value, 0.0F, ENG_STAMINA_MAX);
}
