#include "engine/boss_special.h"

#include <math.h>
#include <stddef.h>

void eng_rider_init(EngRider *r)
{
    if (r != NULL) {
        r->phase = ENG_RIDER_MOUNTED;
        r->ticks = 0;
    }
}

bool eng_rider_step(EngRider *r, float *hp, float max_hp)
{
    if (r == NULL || hp == NULL || !isfinite(max_hp) || !(max_hp > 0.0F)) {
        return false;
    }
    const float foot_hp = max_hp * (1.0F - ENG_RIDER_HORSE_SHARE);
    if (r->phase == ENG_RIDER_MOUNTED) {
        if (!(*hp >
              foot_hp)) { /* le cheval est mort : le reste du coup ne touche pas le chevalier */
            *hp = foot_hp;
            r->phase = ENG_RIDER_FALLING;
            r->ticks = 0;
            return true;
        }
        return false;
    }
    if (r->phase == ENG_RIDER_FALLING) {
        *hp = foot_hp; /* personne ne frappe pendant la cinématique */
        if (++r->ticks >= ENG_RIDER_FALL_TICKS) {
            r->phase = ENG_RIDER_ON_FOOT;
        }
    }
    return false;
}

bool eng_rider_cinematic(const EngRider *r)
{
    return r != NULL && r->phase == ENG_RIDER_FALLING;
}

float eng_rider_horse_fall(const EngRider *r)
{
    if (r == NULL || r->phase == ENG_RIDER_MOUNTED) {
        return 0.0F;
    }
    if (r->phase == ENG_RIDER_ON_FOOT) {
        return 1.0F;
    }
    const float u = (float)r->ticks / 90.0F;
    return u > 1.0F ? 1.0F : u;
}

bool eng_rider_down(const EngRider *r)
{
    return r != NULL && r->phase == ENG_RIDER_FALLING && r->ticks >= 30U && r->ticks < 150U;
}

float eng_rider_horse_hp(const EngRider *r, float hp, float max_hp)
{
    if (r == NULL || r->phase != ENG_RIDER_MOUNTED || !isfinite(hp) || !isfinite(max_hp) ||
        !(max_hp > 0.0F)) {
        return 0.0F;
    }
    const float horse = max_hp * ENG_RIDER_HORSE_SHARE;
    const float left = (hp - (max_hp - horse)) / horse;
    return left < 0.0F ? 0.0F : (left > 1.0F ? 1.0F : left);
}

void eng_flame_init(EngFlame *f)
{
    if (f != NULL) {
        f->state = ENG_FLAME_IDLE;
        f->ticks = 0;
    }
}

bool eng_flame_step(EngFlame *f, bool can_start)
{
    if (f == NULL) {
        return false;
    }
    switch (f->state) {
    case ENG_FLAME_IDLE:
        if (can_start) {
            f->state = ENG_FLAME_CHARGE;
            f->ticks = 0;
        }
        return false;
    case ENG_FLAME_CHARGE:
        if (++f->ticks >= ENG_FLAME_CHARGE_TICKS) {
            f->state = ENG_FLAME_BURN;
            f->ticks = 0;
        }
        return false;
    case ENG_FLAME_BURN: {
        const bool burns = f->ticks % ENG_FLAME_HIT_EVERY == 0U;
        if (++f->ticks >= ENG_FLAME_BURN_TICKS) {
            f->state = ENG_FLAME_COOLDOWN;
            f->ticks = 0;
        }
        return burns;
    }
    case ENG_FLAME_COOLDOWN:
    default:
        if (++f->ticks >= ENG_FLAME_COOLDOWN_TICKS) {
            f->state = ENG_FLAME_IDLE;
            f->ticks = 0;
        }
        return false;
    }
}

bool eng_flame_busy(const EngFlame *f)
{
    return f != NULL && (f->state == ENG_FLAME_CHARGE || f->state == ENG_FLAME_BURN);
}

float eng_flame_charge(const EngFlame *f)
{
    if (f == NULL || f->state == ENG_FLAME_IDLE || f->state == ENG_FLAME_COOLDOWN) {
        return 0.0F;
    }
    return f->state == ENG_FLAME_BURN ? 1.0F : (float)f->ticks / (float)ENG_FLAME_CHARGE_TICKS;
}

float eng_flame_jet(const EngFlame *f)
{
    if (f == NULL || f->state != ENG_FLAME_BURN) {
        return 0.0F;
    }
    /* le jet s'allonge en 0,25 s et retombe sur les 0,25 dernières secondes */
    const float in = (float)f->ticks / 15.0F;
    const float out = (float)(ENG_FLAME_BURN_TICKS - f->ticks) / 15.0F;
    const float u = in < out ? in : out;
    return u > 1.0F ? 1.0F : u;
}

bool eng_flame_reaches(const float src[2], float yaw, const float target[2], float near_m,
                       float far_m, float half_angle)
{
    if (src == NULL || target == NULL || !isfinite(yaw) || !isfinite(near_m) || !isfinite(far_m) ||
        !isfinite(half_angle)) {
        return false;
    }
    const float dx = target[0] - src[0];
    const float dz = target[1] - src[1];
    const float dist = sqrtf((dx * dx) + (dz * dz));
    if (!isfinite(dist) || dist < near_m || dist > far_m) {
        return false;
    }
    if (dist < 1e-4F) {
        return true;
    }
    /* avant du dragon : (-sin yaw, -cos yaw) */
    const float along = ((dx * -sinf(yaw)) + (dz * -cosf(yaw))) / dist;
    return along >= cosf(half_angle);
}
