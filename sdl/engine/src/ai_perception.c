#include "engine/ai_perception.h"

#include <math.h>
#include <string.h>

void eng_ai_perception_reset(EngAiPerception *p)
{
    if (p != NULL) {
        memset(p, 0, sizeof *p);
    }
}

void eng_ai_perceive(EngAiPerception *p, const EngAiSenses *s, const float self_pos[2],
                     float self_yaw, const float target_pos[2], bool target_alive,
                     float noise_radius, const EngAiWorld *world)
{
    if (p == NULL || s == NULL || self_pos == NULL || target_pos == NULL) {
        return;
    }
    p->sees = false;
    p->hears = false;
    if (!target_alive || !isfinite(target_pos[0]) || !isfinite(target_pos[1])) {
        eng_ai_perception_reset(p);
        return;
    }
    const float dx = target_pos[0] - self_pos[0];
    const float dz = target_pos[1] - self_pos[1];
    const float dist = sqrtf(dx * dx + dz * dz);
    if (dist <= s->sight_range) {
        bool in_cone = dist < 1e-4F;
        if (!in_cone) {
            const float fx = -sinf(self_yaw);
            const float fz = -cosf(self_yaw);
            const float c = (fx * dx + fz * dz) / dist;
            in_cone = c >= cosf(s->sight_half_angle);
        }
        p->sees = in_cone && eng_aiw_los(world, self_pos, target_pos);
    }
    if (!p->sees && noise_radius > 0.0F && dist <= noise_radius && dist <= s->hearing_range) {
        p->hears = true;
    }
    if (p->sees || p->hears) {
        p->last_known[0] = target_pos[0];
        p->last_known[1] = target_pos[1];
        p->since_sensed = 0;
        p->has_memory = true;
        return;
    }
    if (p->has_memory) {
        p->since_sensed++;
        if (p->since_sensed > s->memory_ticks) {
            p->has_memory = false;
        }
    }
}
