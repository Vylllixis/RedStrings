#include "engine/ai_debug.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

bool eng_aidbg_info(const EngAiGroup *g, const EngCombatSim *sim, uint32_t agent,
                    EngAiDebugInfo *out)
{
    if (g == NULL || sim == NULL || out == NULL || agent >= g->count) {
        return false;
    }
    const EngAiAgent *a = &g->agents[agent];
    const EngFighter *f = &sim->fighters[a->fighter];
    memset(out, 0, sizeof *out);
    out->agent = agent;
    out->fighter = a->fighter;
    (void)snprintf(out->state, sizeof out->state, "%s", eng_ai_state_name(a->state));
    (void)snprintf(out->archetype, sizeof out->archetype, "%s", g->arch->a[a->arch].name);
    out->state_ticks = a->state_ticks;
    out->hp = f->vitals.hp;
    out->max_hp = f->vitals.max_hp;
    out->sees = a->perc.sees;
    out->hears = a->perc.hears;
    out->has_memory = a->perc.has_memory;
    out->last_known[0] = a->perc.last_known[0];
    out->last_known[1] = a->perc.last_known[1];
    out->has_token = eng_tokens_has(&g->tokens, a->fighter);
    out->plan = a->plan;
    out->atk_phase = a->atk_phase;
    memcpy(out->cooldown, a->cooldown, sizeof out->cooldown);
    out->phase = a->phase;
    const float dx = sim->bodies[g->player].pos[0] - sim->bodies[a->fighter].pos[0];
    const float dz = sim->bodies[g->player].pos[1] - sim->bodies[a->fighter].pos[1];
    out->dist_to_player = sqrtf(dx * dx + dz * dz);
    out->path_n = a->path_n;
    out->path_i = a->path_i;
    return true;
}

size_t eng_aidbg_format(const EngAiDebugInfo *i, char *buf, size_t capacity)
{
    if (i == NULL || buf == NULL || capacity == 0U) {
        return 0;
    }
    const int n = snprintf(
        buf, capacity,
        "%s #%u  [%s] %u ticks\nPV %.0f / %.0f  phase %u\nvoit %s  entend %s  mémoire %s\n"
        "jeton %s  plan %d  attaque %u\nrecharges %u %u %u %u\ndistance %.1f m  chemin %u/%u\n",
        i->archetype, i->fighter, i->state, i->state_ticks, (double)i->hp, (double)i->max_hp,
        i->phase, i->sees ? "oui" : "non", i->hears ? "oui" : "non", i->has_memory ? "oui" : "non",
        i->has_token ? "oui" : "non", i->plan, i->atk_phase, i->cooldown[0], i->cooldown[1],
        i->cooldown[2], i->cooldown[3], (double)i->dist_to_player, i->path_i, i->path_n);
    if (n < 0) {
        buf[0] = '\0';
        return 0;
    }
    return (size_t)n >= capacity ? capacity - 1U : (size_t)n;
}

int32_t eng_aidbg_pick(const EngAiGroup *g, const EngCombatSim *sim, const float origin[3],
                       const float dir[3], float max_dist)
{
    if (g == NULL || sim == NULL || origin == NULL || dir == NULL || !(max_dist > 0.0F)) {
        return -1;
    }
    const float end[3] = {origin[0] + dir[0] * max_dist, origin[1] + dir[1] * max_dist,
                          origin[2] + dir[2] * max_dist};
    int32_t best = -1;
    float best_t = 1e30F;
    for (uint32_t i = 0; i < g->count; i++) {
        const uint32_t f = g->agents[i].fighter;
        if (sim->fighters[f].vitals.dead) {
            continue;
        }
        EngCapsule box[ENG_HURTBOX_MAX];
        EngHitZone zones[ENG_HURTBOX_MAX];
        uint32_t nb = ENG_SIM_HURTBOXES;
        if (sim->rig[f].count > 0U) {
            nb = sim->rig[f].count;
            memcpy(box, sim->rig[f].cur, sizeof(EngCapsule) * nb);
        } else {
            eng_sim_hurtboxes(&sim->bodies[f], false, box, zones);
        }
        for (uint32_t k = 0; k < nb; k++) {
            float c1[3];
            float c2[3];
            const float d2 = eng_segseg_dist2(origin, end, box[k].a, box[k].b, c1, c2);
            if (d2 <= box[k].radius * box[k].radius) {
                const float t = sqrtf((c1[0] - origin[0]) * (c1[0] - origin[0]) +
                                      (c1[1] - origin[1]) * (c1[1] - origin[1]) +
                                      (c1[2] - origin[2]) * (c1[2] - origin[2]));
                if (t < best_t) {
                    best_t = t;
                    best = (int32_t)i;
                }
            }
        }
    }
    return best;
}

static EngDebugColor state_color(EngAiState s)
{
    switch (s) {
    case ENG_AI_PATROL:
        return ENG_DEBUG_GREEN;
    case ENG_AI_ALERT:
        return ENG_DEBUG_YELLOW;
    case ENG_AI_CHASE:
        return ENG_DEBUG_MAGENTA;
    case ENG_AI_ENGAGE:
        return ENG_DEBUG_RED;
    case ENG_AI_RETURN:
        return ENG_DEBUG_BLUE;
    case ENG_AI_FLEE:
        return ENG_DEBUG_CYAN;
    default:
        return ENG_DEBUG_GRAY;
    }
}

uint32_t eng_aidbg_draw(const EngAiGroup *g, const EngCombatSim *sim, int32_t selected,
                        EngDebugLines *lines)
{
    if (g == NULL || sim == NULL || lines == NULL) {
        return 0;
    }
    uint32_t n = 0;
    for (uint32_t i = 0; i < g->count; i++) {
        const EngAiAgent *a = &g->agents[i];
        const EngFighter *f = &sim->fighters[a->fighter];
        if (f->vitals.dead) {
            continue;
        }
        const EngArchetype *ar = &g->arch->a[a->arch];
        const float *p = sim->bodies[a->fighter].pos;
        const float yaw = sim->bodies[a->fighter].yaw;
        const bool sel = selected == (int32_t)i;
        const EngDebugColor base = sel ? ENG_DEBUG_WHITE : state_color(a->state);
        /* cône : deux bords + arc */
        const float half = ar->fov_deg * 0.5F * 0.01745329252F;
        const vec3 c = {p[0], 0.05F, p[1]};
        vec3 prev = {0, 0, 0};
        for (int k = 0; k <= 8; k++) {
            const float ang = yaw + half - (2.0F * half) * (float)k / 8.0F;
            const vec3 e = {p[0] - sinf(ang) * ar->sight, 0.05F, p[1] - cosf(ang) * ar->sight};
            if ((k == 0 || k == 8) && eng_debug_line(lines, c, e, base)) {
                n++;
            }
            if (k > 0 && eng_debug_line(lines, prev, e, base)) {
                n++;
            }
            memcpy(prev, e, sizeof prev);
        }
        /* cible */
        if (a->perc.sees || a->perc.has_memory) {
            const float *t = a->perc.sees ? sim->bodies[g->player].pos : a->perc.last_known;
            const vec3 from = {p[0], 1.0F, p[1]};
            const vec3 to = {t[0], 1.0F, t[1]};
            if (eng_debug_line(lines, from, to, a->perc.sees ? ENG_DEBUG_CYAN : ENG_DEBUG_YELLOW)) {
                n++;
            }
        }
        /* jeton */
        if (eng_tokens_has(&g->tokens, a->fighter)) {
            const vec3 top = {p[0], 2.2F, p[1]};
            const vec3 u = {0.3F, 0, 0};
            const vec3 v = {0, 0, 0.3F};
            if (eng_debug_circle(lines, top, u, v, ENG_DEBUG_YELLOW, 12)) {
                n += 12U;
            }
        }
        /* chemin restant (sélection seulement) */
        if (sel && a->path_n > 0U) {
            vec3 from = {p[0], 0.1F, p[1]};
            for (uint32_t k = a->path_i; k < a->path_n; k++) {
                const vec3 to = {a->path[k][0], 0.1F, a->path[k][1]};
                if (eng_debug_line(lines, from, to, ENG_DEBUG_GREEN)) {
                    n++;
                }
                memcpy(from, to, sizeof from);
            }
        }
    }
    return n;
}
