#include "engine/ai_camp.h"

#include <math.h>
#include <string.h>

void eng_camp_init(EngCamp *c)
{
    if (c != NULL) {
        memset(c, 0, sizeof *c);
    }
}

bool eng_camp_add(EngCamp *c, uint32_t arch, float x, float z, float yaw, const float (*patrol)[2],
                  uint32_t patrol_n)
{
    if (c == NULL || c->count >= ENG_AI_MAX_AGENTS || patrol_n > ENG_AI_PATROL_MAX ||
        (patrol_n > 0U && patrol == NULL) || !isfinite(x) || !isfinite(z) || !isfinite(yaw)) {
        return false;
    }
    EngCampMember *m = &c->m[c->count++];
    memset(m, 0, sizeof *m);
    m->arch = arch;
    m->pos[0] = x;
    m->pos[1] = z;
    m->yaw = yaw;
    m->patrol_n = patrol_n;
    for (uint32_t i = 0; i < patrol_n; i++) {
        m->patrol[i][0] = patrol[i][0];
        m->patrol[i][1] = patrol[i][1];
    }
    return true;
}

bool eng_camp_spawn(const EngCamp *c, EngAiGroup *g, EngCombatSim *sim, uint32_t first)
{
    if (c == NULL || g == NULL || sim == NULL || first + c->count > sim->count) {
        return false;
    }
    for (uint32_t i = 0; i < c->count; i++) {
        const EngCampMember *m = &c->m[i];
        if (!eng_aig_add(g, sim, first + i, m->arch, m->pos[0], m->pos[1], m->yaw, m->patrol,
                         m->patrol_n)) {
            return false;
        }
    }
    return true;
}

bool eng_camp_rest(const EngCamp *c, EngAiGroup *g, EngCombatSim *sim)
{
    if (c == NULL || g == NULL || sim == NULL || c->count > g->count) {
        return false;
    }
    for (uint32_t i = 0; i < g->count; i++) {
        const EngAiAgent *a = &g->agents[i];
        if (!sim->fighters[a->fighter].vitals.dead &&
            (a->state == ENG_AI_CHASE || a->state == ENG_AI_ENGAGE || a->state == ENG_AI_FLEE)) {
            return false;
        }
    }
    for (uint32_t i = 0; i < c->count; i++) {
        if (!eng_aig_reset_agent(g, sim, i) ||
            !eng_sim_place(sim, g->agents[i].fighter, c->m[i].pos[0], c->m[i].pos[1],
                           c->m[i].yaw)) {
            return false;
        }
    }
    return true;
}

bool eng_camp_cleared(const EngAiGroup *g, const EngCombatSim *sim)
{
    if (g == NULL || sim == NULL || g->count == 0U) {
        return false;
    }
    for (uint32_t i = 0; i < g->count; i++) {
        if (!sim->fighters[g->agents[i].fighter].vitals.dead) {
            return false;
        }
    }
    return true;
}

bool eng_arena_walls(EngAiWorld *w, float cx, float cz, float hx, float hz)
{
    if (w == NULL || !(hx > 1.0F) || !(hz > 1.0F) || w->count + 4U > ENG_AI_MAX_OBSTACLES) {
        return false;
    }
    return eng_aiw_add_box(w, cx, cz - hz - 0.5F, hx + 1.0F, 0.5F) &&
           eng_aiw_add_box(w, cx, cz + hz + 0.5F, hx + 1.0F, 0.5F) &&
           eng_aiw_add_box(w, cx - hx - 0.5F, cz, 0.5F, hz) &&
           eng_aiw_add_box(w, cx + hx + 0.5F, cz, 0.5F, hz);
}
