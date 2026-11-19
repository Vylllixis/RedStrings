#include "engine/game_feel.h"

#include <math.h>
#include <string.h>

void eng_feel_params_default(EngFeelParams *p)
{
    if (p == NULL) {
        return;
    }
    p->hitstop_scale = 1.0F;
    p->shake_light = 0.01F;
    p->shake_heavy = 0.03F;
    p->shake_block = 0.008F;
    p->shake_parry = 0.02F;
    p->shake_ticks = 12;
    p->parry_slowmo_scale = 0.3F;
    p->parry_slowmo_ticks = 12;
    p->fov_kick_heavy = 3.0F;
}

static bool in_range(float v, float lo, float hi)
{
    return isfinite(v) && v >= lo && v <= hi;
}

bool eng_feel_params_valid(const EngFeelParams *p)
{
    return p != NULL && in_range(p->hitstop_scale, 0.0F, 3.0F) &&
           in_range(p->shake_light, 0.0F, 0.2F) && in_range(p->shake_heavy, 0.0F, 0.2F) &&
           in_range(p->shake_block, 0.0F, 0.2F) && in_range(p->shake_parry, 0.0F, 0.2F) &&
           p->shake_ticks >= 1 && p->shake_ticks <= 60 &&
           in_range(p->parry_slowmo_scale, 0.05F, 1.0F) && p->parry_slowmo_ticks >= 0 &&
           p->parry_slowmo_ticks <= 60 && in_range(p->fov_kick_heavy, 0.0F, 15.0F);
}

void eng_feel_init(EngFeel *f, uint32_t player)
{
    if (f == NULL) {
        return;
    }
    memset(f, 0, sizeof *f);
    eng_feel_params_default(&f->params);
    f->player = player;
}

bool eng_feel_register_tweaks(EngFeel *f, EngTweaks *t)
{
    if (f == NULL || t == NULL) {
        return false;
    }
    EngFeelParams *p = &f->params;
    return eng_tweaks_add_float(t, "feel.hitstop_scale", &p->hitstop_scale, 0.0F, 3.0F) &&
           eng_tweaks_add_float(t, "feel.shake_light", &p->shake_light, 0.0F, 0.2F) &&
           eng_tweaks_add_float(t, "feel.shake_heavy", &p->shake_heavy, 0.0F, 0.2F) &&
           eng_tweaks_add_float(t, "feel.shake_block", &p->shake_block, 0.0F, 0.2F) &&
           eng_tweaks_add_float(t, "feel.shake_parry", &p->shake_parry, 0.0F, 0.2F) &&
           eng_tweaks_add_int(t, "feel.shake_ticks", &p->shake_ticks, 1, 60) &&
           eng_tweaks_add_float(t, "feel.parry_slowmo_scale", &p->parry_slowmo_scale, 0.05F,
                                1.0F) &&
           eng_tweaks_add_int(t, "feel.parry_slowmo_ticks", &p->parry_slowmo_ticks, 0, 60) &&
           eng_tweaks_add_float(t, "feel.fov_kick_heavy", &p->fov_kick_heavy, 0.0F, 15.0F);
}

uint32_t eng_feel_hitstop(const EngFeel *f, uint32_t base)
{
    if (f == NULL || !isfinite(f->params.hitstop_scale) || f->params.hitstop_scale < 0.0F) {
        return base;
    }
    const float v = roundf((float)base * f->params.hitstop_scale);
    if (v <= 0.0F) {
        return 0U;
    }
    return v >= (float)ENG_ATTACK_MAX_HITSTOP ? ENG_ATTACK_MAX_HITSTOP : (uint32_t)v;
}

static void start_shake(EngFeel *f, float amp, float fov)
{
    if (amp > f->shake_amp || f->shake_left == 0U) {
        f->shake_amp = amp;
    }
    if (fov > f->fov_amp || f->shake_left == 0U) {
        f->fov_amp = fov;
    }
    f->shake_total = (uint32_t)f->params.shake_ticks;
    f->shake_left = f->shake_total;
}

void eng_feel_on_hit(EngFeel *f, const EngSimEvent *e)
{
    if (f == NULL || e == NULL || (e->attacker != f->player && e->defender != f->player)) {
        return;
    }
    const bool heavy = e->hitstop >= ENG_FEEL_HEAVY_HITSTOP;
    switch (e->kind) {
    case ENG_HITKIND_DAMAGE:
        start_shake(f, heavy ? f->params.shake_heavy : f->params.shake_light,
                    heavy ? f->params.fov_kick_heavy : 0.0F);
        break;
    case ENG_HITKIND_BLOCKED:
        start_shake(f, f->params.shake_block, 0.0F);
        break;
    case ENG_HITKIND_PARRIED:
        start_shake(f, f->params.shake_parry, 0.0F);
        f->slow_left = (uint32_t)f->params.parry_slowmo_ticks;
        break;
    default:
        break;
    }
}

/* bruit déterministe dans [-1 ; 1] : fonction pure du compteur et de l'axe */
static float noise(uint32_t n, uint32_t axis)
{
    uint32_t h = n * 2654435761U ^ (axis + 1U) * 40503U;
    h ^= h >> 15;
    h *= 2246822519U;
    h ^= h >> 13;
    h *= 3266489917U;
    h ^= h >> 16;
    return (float)(h & 0xFFFFU) / 32767.5F - 1.0F;
}

void eng_feel_tick(EngFeel *f, float offset[3], float *time_scale, float *fov_kick_deg)
{
    float off[3] = {0.0F, 0.0F, 0.0F};
    float scale = 1.0F;
    float fov = 0.0F;
    if (f != NULL) {
        f->noise_counter++;
        if (f->shake_left > 0U) {
            const float k = (float)f->shake_left / (float)f->shake_total;
            for (uint32_t a = 0; a < 3U; a++) {
                off[a] = noise(f->noise_counter, a) * f->shake_amp * k * k;
            }
            fov = f->fov_amp * k;
            f->shake_left--;
            if (f->shake_left == 0U) {
                f->shake_amp = 0.0F;
                f->fov_amp = 0.0F;
            }
        }
        if (f->slow_left > 0U) {
            scale = f->params.parry_slowmo_scale;
            f->slow_left--;
        }
    }
    if (offset != NULL) {
        memcpy(offset, off, sizeof off);
    }
    if (time_scale != NULL) {
        *time_scale = scale;
    }
    if (fov_kick_deg != NULL) {
        *fov_kick_deg = fov;
    }
}
