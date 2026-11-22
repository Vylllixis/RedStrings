#include "engine/ai_group.h"

#include <math.h>
#include <string.h>

#define ARRIVE 0.35F
#define REPATH_TICKS 20U
#define ENGAGE_EXTRA 2.0F
#define LOST_TO_CHASE 90U
#define ALERT_TURN_DEG_COS 0.97F

const char *eng_ai_state_name(EngAiState s)
{
    static const char *const names[ENG_AI_STATE_COUNT] = {"Patrol", "Alert", "Chase", "Engage",
                                                          "Return", "Flee",  "Dead"};
    return (unsigned)s < (unsigned)ENG_AI_STATE_COUNT ? names[s] : "?";
}

static uint32_t rnd(EngAiAgent *a)
{
    uint32_t x = a->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    a->rng = x;
    return x;
}

void eng_aig_init(EngAiGroup *g, const EngArchSet *arch, const EngAiWorld *world, EngNavGrid *nav,
                  uint32_t player)
{
    if (g == NULL) {
        return;
    }
    memset(g, 0, sizeof *g);
    g->arch = arch;
    g->world = world;
    g->nav = nav;
    g->player = player;
    eng_tokens_init(&g->tokens, ENG_AI_MAX_ATTACKERS);
}

static void set_state(EngAiAgent *a, EngAiState s)
{
    if (a->state != s) {
        a->state = s;
        a->state_ticks = 0;
        a->path_n = 0;
        a->repath_wait = 0;
    }
}

static bool apply_names(EngCombatSim *sim, uint32_t fighter, const char *light, const char *heavy)
{
    EngFighterParams p = sim->fighters[fighter].params;
    (void)memset(p.light_name, 0, sizeof p.light_name);
    (void)memset(p.heavy_name, 0, sizeof p.heavy_name);
    (void)strncpy(p.light_name, light, sizeof p.light_name - 1U);
    (void)strncpy(p.heavy_name, heavy, sizeof p.heavy_name - 1U);
    return eng_fighter_set_params(&sim->fighters[fighter], &p);
}

bool eng_aig_reset_agent(EngAiGroup *g, EngCombatSim *sim, uint32_t ai)
{
    if (g == NULL || sim == NULL || ai >= g->count) {
        return false;
    }
    EngAiAgent *a = &g->agents[ai];
    const EngArchetype *ar = &g->arch->a[a->arch];
    eng_fighter_init(&sim->fighters[a->fighter], NULL);
    if (!apply_names(sim, a->fighter, ar->light, ar->heavy)) {
        return false;
    }
    sim->fighters[a->fighter].vitals.max_hp = ar->hp;
    sim->fighters[a->fighter].vitals.hp = ar->hp;
    sim->speed_scale[a->fighter] = ar->speed;
    sim->radius[a->fighter] = ar->radius;
    if (!eng_sim_place(sim, a->fighter, a->home[0], a->home[1], 0.0F)) {
        return false;
    }
    eng_tokens_release(&g->tokens, a->fighter);
    eng_ai_perception_reset(&a->perc);
    a->phase = 0;
    a->state = ENG_AI_PATROL;
    a->state_ticks = 0;
    a->patrol_i = 0;
    a->patrol_wait = 0;
    a->path_n = 0;
    a->plan = -1;
    a->presses_left = 0;
    a->atk_phase = 0;
    a->step_next = 0;
    a->token_hold = 0;
    a->token_cd = 0;
    a->pause = 0;
    a->lost_ticks = 0;
    a->cooldown_scale = 1.0F;
    memset(a->cooldown, 0, sizeof a->cooldown);
    return true;
}

bool eng_aig_add(EngAiGroup *g, EngCombatSim *sim, uint32_t fighter, uint32_t arch, float x,
                 float z, float yaw, const float (*patrol)[2], uint32_t patrol_n)
{
    if (g == NULL || sim == NULL || g->arch == NULL || g->count >= ENG_AI_MAX_AGENTS ||
        fighter >= sim->count || fighter == g->player || arch >= g->arch->count ||
        patrol_n > ENG_AI_PATROL_MAX || (patrol_n > 0U && patrol == NULL)) {
        return false;
    }
    for (uint32_t i = 0; i < g->count; i++) {
        if (g->agents[i].fighter == fighter) {
            return false;
        }
    }
    EngAiAgent *a = &g->agents[g->count];
    memset(a, 0, sizeof *a);
    a->active = true;
    a->fighter = fighter;
    a->arch = arch;
    a->home[0] = x;
    a->home[1] = z;
    a->rng = 2463534242U + fighter * 40503U;
    a->patrol_n = patrol_n;
    for (uint32_t i = 0; i < patrol_n; i++) {
        a->patrol[i][0] = patrol[i][0];
        a->patrol[i][1] = patrol[i][1];
    }
    g->count++;
    if (!eng_aig_reset_agent(g, sim, g->count - 1U)) {
        g->count--;
        return false;
    }
    return eng_sim_place(sim, fighter, x, z, yaw);
}

static float dist2(const float a[2], const float b[2])
{
    const float dx = a[0] - b[0];
    const float dz = a[1] - b[1];
    return sqrtf(dx * dx + dz * dz);
}

/* avance vers `goal` ; retourne false si arrivé. `dir` = direction unitaire. */
static bool follow(EngAiGroup *g, EngAiAgent *a, const float pos[2], const float goal[2],
                   float arrive, float dir[2])
{
    if (dist2(pos, goal) <= arrive) {
        return false;
    }
    float target[2] = {goal[0], goal[1]};
    if (g->nav != NULL) {
        const bool moved = dist2(a->goal, goal) > 1.0F;
        if (a->repath_wait > 0U) {
            a->repath_wait--;
        }
        if (a->path_n == 0U || a->path_i >= a->path_n || a->repath_wait == 0U || moved) {
            a->path_n = eng_nav_path(g->nav, pos, goal, a->path, ENG_AI_PATH_MAX);
            a->path_i = 0;
            a->repath_wait = REPATH_TICKS;
            a->goal[0] = goal[0];
            a->goal[1] = goal[1];
        }
        while (a->path_i < a->path_n && dist2(pos, a->path[a->path_i]) <= ARRIVE) {
            a->path_i++;
        }
        if (a->path_i < a->path_n) {
            target[0] = a->path[a->path_i][0];
            target[1] = a->path[a->path_i][1];
        } else if (a->path_n > 0U) {
            return false; /* chemin consommé : arrivé au point praticable le plus proche */
        }
    }
    const float dx = target[0] - pos[0];
    const float dz = target[1] - pos[1];
    const float len = sqrtf(dx * dx + dz * dz);
    if (len < 1e-5F) {
        return false;
    }
    dir[0] = dx / len;
    dir[1] = dz / len;
    return true;
}

static void walk(EngFighterInput *in, const float dir[2])
{
    in->moving = true;
    in->move_dir[0] = dir[0];
    in->move_dir[1] = dir[1];
}

static bool facing(const float yaw, const float pos[2], const float target[2], float cos_min)
{
    const float dx = target[0] - pos[0];
    const float dz = target[1] - pos[1];
    const float len = sqrtf(dx * dx + dz * dz);
    if (len < 1e-5F) {
        return true;
    }
    return (-sinf(yaw) * dx - cosf(yaw) * dz) / len >= cos_min;
}

static void check_phase(EngAiGroup *g, EngAiAgent *a, EngCombatSim *sim_rw)
{
    const EngArchetype *ar = &g->arch->a[a->arch];
    if (a->phase >= ar->phase_count) {
        return;
    }
    const EngFighter *f = &sim_rw->fighters[a->fighter];
    if (f->vitals.hp / f->vitals.max_hp > ar->phases[a->phase].hp_below) {
        return;
    }
    const EngArchPhase *ph = &ar->phases[a->phase];
    if (apply_names(sim_rw, a->fighter, ph->light, ph->heavy)) {
        sim_rw->speed_scale[a->fighter] = ar->speed * ph->speed;
        a->cooldown_scale = ph->cooldown_scale;
        a->phase++;
        a->pause = 45U;
    }
}

void eng_aig_tick(EngAiGroup *g, EngCombatSim *sim, float noise_radius, EngFighterInput *inputs)
{
    if (g == NULL || sim == NULL || inputs == NULL) {
        return;
    }
    EngCombatSim *sim_rw = sim; /* seules les phases de boss écrivent dans la simulation */
    const EngFighter *pf = &sim->fighters[g->player];
    const float *ppos = sim->bodies[g->player].pos;
    const bool player_alive = !pf->vitals.dead;

    /* passe 1 : perception, transitions, demandes de jetons */
    uint32_t want_ids[ENG_AI_MAX_AGENTS];
    float want_dist[ENG_AI_MAX_AGENTS];
    uint32_t wants = 0;
    for (uint32_t i = 0; i < g->count; i++) {
        EngAiAgent *a = &g->agents[i];
        const EngArchetype *ar = &g->arch->a[a->arch];
        const EngFighter *f = &sim->fighters[a->fighter];
        const float *pos = sim->bodies[a->fighter].pos;
        a->state_ticks++;
        for (uint32_t k = 0; k < ENG_ARCH_PLANS; k++) {
            if (a->cooldown[k] > 0U) {
                a->cooldown[k]--;
            }
        }
        if (a->token_cd > 0U) {
            a->token_cd--;
        }
        if (f->vitals.dead) {
            set_state(a, ENG_AI_DEAD);
            eng_tokens_release(&g->tokens, a->fighter);
            continue;
        }
        check_phase(g, a, sim_rw);
        const EngAiSenses senses = {ar->sight, ar->fov_deg * 0.5F * 0.01745329252F, ar->hearing,
                                    ar->memory};
        eng_ai_perceive(&a->perc, &senses, pos, sim->bodies[a->fighter].yaw, ppos, player_alive,
                        noise_radius, g->world);
        const float d = dist2(pos, ppos);
        const bool aware = a->perc.sees || a->perc.hears;
        switch (a->state) {
        case ENG_AI_PATROL:
        case ENG_AI_RETURN:
            if (aware) {
                set_state(a, ENG_AI_ALERT);
            }
            break;
        case ENG_AI_ALERT:
            if (a->state_ticks >= ar->alert) {
                set_state(a, a->perc.has_memory ? ENG_AI_CHASE : ENG_AI_PATROL);
            }
            break;
        case ENG_AI_CHASE:
            if (!a->perc.has_memory) {
                set_state(a, ENG_AI_RETURN);
            } else if (a->perc.sees && d <= ar->standoff + ENGAGE_EXTRA) {
                set_state(a, ENG_AI_ENGAGE);
                a->lost_ticks = 0;
            }
            break;
        case ENG_AI_ENGAGE:
            a->lost_ticks = a->perc.sees ? 0U : a->lost_ticks + 1U;
            if (a->lost_ticks > LOST_TO_CHASE || d > ar->standoff + ENGAGE_EXTRA + 3.0F) {
                if (a->atk_phase == 0U) {
                    set_state(a, a->perc.has_memory ? ENG_AI_CHASE : ENG_AI_RETURN);
                }
            }
            break;
        case ENG_AI_FLEE:
            if (d > ar->sight * 1.2F || a->state_ticks > 360U || !player_alive) {
                set_state(a, ENG_AI_RETURN);
            }
            break;
        default:
            break;
        }
        if (ar->flee_below > 0.0F && a->state != ENG_AI_FLEE && a->atk_phase == 0U &&
            f->vitals.hp / f->vitals.max_hp < ar->flee_below && player_alive) {
            set_state(a, ENG_AI_FLEE);
        }
        /* libération d'un jeton devenu inutile */
        if (eng_tokens_has(&g->tokens, a->fighter)) {
            a->token_hold++;
            if (a->atk_phase == 0U && (a->state != ENG_AI_ENGAGE || a->token_hold > 180U)) {
                eng_tokens_release(&g->tokens, a->fighter);
                a->token_hold = 0;
                a->token_cd = 90U;
            }
        } else {
            a->token_hold = 0;
        }
        if (a->state == ENG_AI_ENGAGE && a->token_cd == 0U &&
            !eng_tokens_has(&g->tokens, a->fighter)) {
            want_ids[wants] = a->fighter;
            want_dist[wants] = d;
            wants++;
        }
    }
    eng_tokens_arbitrate(&g->tokens, want_ids, want_dist, wants);

    /* cercle d'attente des ennemis engagés sans jeton */
    float rpos[ENG_AI_MAX_AGENTS][2];
    float rout[ENG_AI_MAX_AGENTS][2];
    uint32_t ridx[ENG_AI_MAX_AGENTS];
    uint32_t rn = 0;
    for (uint32_t i = 0; i < g->count; i++) {
        const EngAiAgent *a = &g->agents[i];
        if (a->state == ENG_AI_ENGAGE && !eng_tokens_has(&g->tokens, a->fighter)) {
            rpos[rn][0] = sim->bodies[a->fighter].pos[0];
            rpos[rn][1] = sim->bodies[a->fighter].pos[1];
            ridx[rn++] = i;
        }
    }
    if (rn > 0U) {
        eng_ai_ring(ppos, (const float(*)[2])rpos, rn, g->arch->a[g->agents[ridx[0]].arch].standoff,
                    rout);
        for (uint32_t k = 0; k < rn; k++) {
            g->agents[ridx[k]].ring[0] = rout[k][0];
            g->agents[ridx[k]].ring[1] = rout[k][1];
        }
    }

    /* passe 2 : entrées */
    for (uint32_t i = 0; i < g->count; i++) {
        EngAiAgent *a = &g->agents[i];
        const EngArchetype *ar = &g->arch->a[a->arch];
        const EngFighter *f = &sim->fighters[a->fighter];
        const float *pos = sim->bodies[a->fighter].pos;
        EngFighterInput *in = &inputs[a->fighter];
        memset(in, 0, sizeof *in);
        if (f->vitals.dead || a->state == ENG_AI_DEAD) {
            continue;
        }
        const bool in_attack = f->fsm.state == ENG_CS_ATTACK;
        /* suivi de l'attaque en cours */
        if (a->atk_phase == 1U) {
            a->atk_wait++;
            if (in_attack) {
                a->atk_phase = 2;
                a->atk_wait = 0;
            } else if (a->atk_wait > 20U) {
                a->atk_phase = 0;
                a->presses_left = 0;
                a->step_next = 0;
            }
        } else if (a->atk_phase == 2U) {
            a->atk_wait++;
            const uint32_t steps = a->plan >= 0 ? ar->plans[a->plan].step_count : 0U;
            if (!in_attack && a->step_next < steps) {
                /* M6-09 : pattern : l'attaque suivante de la suite, jeton gardé */
                if (ar->plans[a->plan].steps[a->step_next]) {
                    in->heavy = true;
                } else {
                    in->light = true;
                }
                a->step_next++;
                a->atk_phase = 1;
                a->atk_wait = 0;
            } else if (!in_attack) {
                a->atk_phase = 0;
                a->step_next = 0;
                a->presses_left = 0;
                if (eng_tokens_has(&g->tokens, a->fighter)) {
                    eng_tokens_release(&g->tokens, a->fighter);
                    a->token_hold = 0;
                    a->token_cd = 30U;
                }
            } else if (a->presses_left > 0U && a->atk_wait >= 2U) {
                in->light = true;
                a->presses_left--;
                a->atk_wait = 0;
            }
        }
        if (a->atk_phase != 0U) {
            continue; /* engagé dans une attaque : aucune autre action */
        }
        if (a->pause > 0U) {
            a->pause--;
            continue;
        }
        if (f->fsm.state != ENG_CS_IDLE && f->fsm.state != ENG_CS_MOVE) {
            continue;
        }
        float dir[2] = {0.0F, 0.0F};
        const float d = dist2(pos, ppos);
        switch (a->state) {
        case ENG_AI_PATROL:
            if (a->patrol_n > 0U) {
                if (a->patrol_wait > 0U) {
                    a->patrol_wait--;
                } else if (follow(g, a, pos, a->patrol[a->patrol_i], ARRIVE, dir)) {
                    walk(in, dir);
                } else {
                    a->patrol_i = (a->patrol_i + 1U) % a->patrol_n;
                    a->patrol_wait = 60U;
                }
            }
            break;
        case ENG_AI_RETURN:
            if (follow(g, a, pos, a->home, ARRIVE, dir)) {
                walk(in, dir);
            } else {
                set_state(a, ENG_AI_PATROL);
            }
            break;
        case ENG_AI_ALERT:
            if (!facing(sim->bodies[a->fighter].yaw, pos, a->perc.last_known, ALERT_TURN_DEG_COS)) {
                dir[0] = a->perc.last_known[0] - pos[0];
                dir[1] = a->perc.last_known[1] - pos[1];
                const float l = sqrtf(dir[0] * dir[0] + dir[1] * dir[1]);
                if (l > 1e-5F) {
                    dir[0] /= l;
                    dir[1] /= l;
                    walk(in, dir);
                    in->moving = true; /* tourne ; la marche réelle ne démarre qu'à l'état Move */
                }
            }
            break;
        case ENG_AI_CHASE: {
            const float *goal = a->perc.sees ? ppos : a->perc.last_known;
            if (follow(g, a, pos, goal, a->perc.sees ? ARRIVE : 1.0F, dir)) {
                walk(in, dir);
            }
            break;
        }
        case ENG_AI_FLEE: {
            float away[2] = {pos[0] - ppos[0], pos[1] - ppos[1]};
            const float l = sqrtf(away[0] * away[0] + away[1] * away[1]);
            away[0] = l > 1e-5F ? away[0] / l : 1.0F;
            away[1] = l > 1e-5F ? away[1] / l : 0.0F;
            float goal[2] = {pos[0] + away[0] * 8.0F, pos[1] + away[1] * 8.0F};
            if (g->nav != NULL && !eng_nav_walkable(g->nav, goal)) {
                goal[0] = a->home[0];
                goal[1] = a->home[1];
            }
            if (follow(g, a, pos, goal, ARRIVE, dir)) {
                walk(in, dir);
            }
            break;
        }
        case ENG_AI_ENGAGE:
            if (!eng_tokens_has(&g->tokens, a->fighter)) {
                if (follow(g, a, pos, a->ring, 0.6F, dir)) {
                    walk(in, dir);
                }
                break;
            }
            {
                /* choix d'un plan prêt, pondéré, dont la fenêtre contient la distance */
                uint32_t total = 0;
                bool any_ready = false;
                float near_min = 1e9F;
                float far_max = 0.0F;
                for (uint32_t k = 0; k < ar->plan_count; k++) {
                    if (a->cooldown[k] > 0U) {
                        continue;
                    }
                    any_ready = true;
                    if (ar->plans[k].range_min < near_min) {
                        near_min = ar->plans[k].range_min;
                    }
                    if (ar->plans[k].range_max > far_max) {
                        far_max = ar->plans[k].range_max;
                    }
                    if (d >= ar->plans[k].range_min && d <= ar->plans[k].range_max) {
                        total += ar->plans[k].weight;
                    }
                }
                if (!any_ready) {
                    break; /* tout en recharge : garde sa place */
                }
                if (total == 0U) {
                    if (d > far_max * 0.9F) {
                        if (follow(g, a, pos, ppos, ARRIVE, dir)) {
                            walk(in, dir);
                        }
                    } else if (d < near_min) {
                        dir[0] = pos[0] - ppos[0];
                        dir[1] = pos[1] - ppos[1];
                        const float l = sqrtf(dir[0] * dir[0] + dir[1] * dir[1]);
                        if (l > 1e-5F) {
                            dir[0] /= l;
                            dir[1] /= l;
                            walk(in, dir);
                        }
                    }
                    break;
                }
                if (!facing(sim->bodies[a->fighter].yaw, pos, ppos, 0.96F)) {
                    dir[0] = ppos[0] - pos[0];
                    dir[1] = ppos[1] - pos[1];
                    const float l = sqrtf(dir[0] * dir[0] + dir[1] * dir[1]);
                    if (l > 1e-5F) {
                        dir[0] /= l;
                        dir[1] /= l;
                        walk(in, dir);
                    }
                    break;
                }
                uint32_t roll = rnd(a) % total;
                for (uint32_t k = 0; k < ar->plan_count; k++) {
                    if (a->cooldown[k] > 0U || d < ar->plans[k].range_min ||
                        d > ar->plans[k].range_max) {
                        continue;
                    }
                    if (roll < ar->plans[k].weight) {
                        if (ar->plans[k].heavy) {
                            in->heavy = true;
                        } else {
                            in->light = true;
                        }
                        a->plan = (int32_t)k;
                        a->presses_left = ar->plans[k].heavy || ar->plans[k].step_count > 0U
                                              ? 0U
                                              : ar->plans[k].presses - 1U;
                        a->step_next = 1U; /* pattern : la première attaque est partie */
                        a->atk_phase = 1;
                        a->atk_wait = 0;
                        const float scale = a->cooldown_scale > 0.0F ? a->cooldown_scale : 1.0F;
                        a->cooldown[k] = (uint32_t)((float)ar->plans[k].cooldown * scale);
                        break;
                    }
                    roll -= ar->plans[k].weight;
                }
            }
            break;
        default:
            break;
        }
    }
}

uint32_t eng_aig_attackers(const EngAiGroup *g, const EngCombatSim *sim)
{
    uint32_t n = 0;
    if (g == NULL || sim == NULL) {
        return 0;
    }
    for (uint32_t i = 0; i < g->count; i++) {
        const EngFighter *f = &sim->fighters[g->agents[i].fighter];
        if (!f->vitals.dead && f->fsm.state == ENG_CS_ATTACK) {
            n++;
        }
    }
    return n;
}

void eng_aig_invalidate_paths(EngAiGroup *g)
{
    if (g == NULL) {
        return;
    }
    for (uint32_t i = 0; i < g->count; i++) {
        g->agents[i].path_n = 0;
        g->agents[i].path_i = 0;
        g->agents[i].repath_wait = 0;
    }
}
