#include "engine/progression.h"

#include <math.h>
#include <stddef.h>

void eng_prog_init(EngProgress *p)
{
    if (p == NULL) {
        return;
    }
    p->level = 1U;
    p->xp = 0U;
    p->total_xp = 0U;
}

uint32_t eng_prog_need(uint32_t level)
{
    if (level < 1U || level >= ENG_PROG_MAX_LEVEL) {
        return 0U;
    }
    return 100U + 50U * (level - 1U);
}

uint32_t eng_prog_add_xp(EngProgress *p, uint32_t amount)
{
    if (p == NULL || !eng_prog_valid(p)) {
        return 0U;
    }
    p->total_xp = amount > UINT32_MAX - p->total_xp ? UINT32_MAX : p->total_xp + amount;
    uint32_t gained = 0U;
    uint64_t pool = (uint64_t)p->xp + amount;
    while (p->level < ENG_PROG_MAX_LEVEL && pool >= eng_prog_need(p->level)) {
        pool -= eng_prog_need(p->level);
        p->level++;
        gained++;
    }
    p->xp = p->level >= ENG_PROG_MAX_LEVEL ? 0U : (uint32_t)pool;
    return gained;
}

uint32_t eng_prog_kill_xp(float enemy_hp_max)
{
    if (!isfinite(enemy_hp_max) || enemy_hp_max <= 0.0F) {
        return 10U;
    }
    const float half = roundf(enemy_hp_max * 0.5F);
    if (half >= 100000.0F) {
        return 100000U;
    }
    return half < 10.0F ? 10U : (uint32_t)half;
}

EngProgressStats eng_prog_stats(const EngProgress *p)
{
    const uint32_t level = p != NULL && eng_prog_valid(p) ? p->level : 1U;
    const float k = (float)(level - 1U);
    EngProgressStats s;
    s.hp_max = 100.0F + 10.0F * k;
    s.stamina_max = 100.0F + 5.0F * k;
    s.damage_scale = 1.0F + 0.05F * k;
    return s;
}

bool eng_prog_valid(const EngProgress *p)
{
    if (p == NULL || p->level < 1U || p->level > ENG_PROG_MAX_LEVEL || p->total_xp < p->xp) {
        return false;
    }
    return p->level == ENG_PROG_MAX_LEVEL ? p->xp == 0U : p->xp < eng_prog_need(p->level);
}
