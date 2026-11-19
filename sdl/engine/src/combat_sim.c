#include "engine/combat_sim.h"

#include <math.h>
#include <string.h>

#define SIM_ORIGIN_Y 1.1F

static void pose(const EngSimBody *b, bool previous, float *x, float *z, float *fx, float *fz,
                 float *rx, float *rz)
{
    const float yaw = previous ? b->prev_yaw : b->yaw;
    *x = previous ? b->prev_pos[0] : b->pos[0];
    *z = previous ? b->prev_pos[1] : b->pos[1];
    *fx = -sinf(yaw);
    *fz = -cosf(yaw);
    *rx = cosf(yaw);
    *rz = -sinf(yaw);
}

static void to_world(const EngSimBody *b, bool previous, const float local[3], float y0,
                     float out[3])
{
    float x = 0.0F;
    float z = 0.0F;
    float fx = 0.0F;
    float fz = 0.0F;
    float rx = 0.0F;
    float rz = 0.0F;
    pose(b, previous, &x, &z, &fx, &fz, &rx, &rz);
    out[0] = x + rx * local[0] + fx * local[2];
    out[1] = y0 + local[1];
    out[2] = z + rz * local[0] + fz * local[2];
}

void eng_sim_hurtboxes(const EngSimBody *b, bool previous, EngCapsule out[ENG_SIM_HURTBOXES],
                       EngHitZone zones[ENG_SIM_HURTBOXES])
{
    /* repère local (x droite, y haut, z avant) : {a, b, rayon} */
    static const float boxes[ENG_SIM_HURTBOXES][7] = {
        {0.0F, 1.50F, 0.0F, 0.0F, 1.75F, 0.0F, 0.12F}, /* tête */
        {0.0F, 0.90F, 0.0F, 0.0F, 1.40F, 0.0F, 0.22F}, /* torse */
        {0.0F, 0.15F, 0.0F, 0.0F, 0.85F, 0.0F, 0.17F}, /* jambes */
    };
    static const EngHitZone zone_of[ENG_SIM_HURTBOXES] = {ENG_ZONE_HEAD, ENG_ZONE_TORSO,
                                                          ENG_ZONE_LIMB};
    for (uint32_t i = 0; i < ENG_SIM_HURTBOXES; i++) {
        const float a[3] = {boxes[i][0], boxes[i][1], boxes[i][2]};
        const float c[3] = {boxes[i][3], boxes[i][4], boxes[i][5]};
        to_world(b, previous, a, 0.0F, out[i].a);
        to_world(b, previous, c, 0.0F, out[i].b);
        out[i].radius = boxes[i][6];
        zones[i] = zone_of[i];
    }
}

uint32_t eng_sim_weapons(const EngSimBody *b, bool previous, const EngAttack *atk,
                         EngCapsule out[ENG_ATTACK_MAX_HITBOXES])
{
    if (b == NULL || atk == NULL || out == NULL) {
        return 0;
    }
    for (uint32_t i = 0; i < atk->hitbox_count; i++) {
        to_world(b, previous, atk->hitboxes[i].a, SIM_ORIGIN_Y, out[i].a);
        to_world(b, previous, atk->hitboxes[i].b, SIM_ORIGIN_Y, out[i].b);
        out[i].radius = atk->hitboxes[i].radius;
    }
    return atk->hitbox_count;
}

bool eng_sim_init(EngCombatSim *s, const EngAttackSet *attacks, uint32_t count)
{
    if (s == NULL || attacks == NULL || count == 0U || count > ENG_SIM_MAX) {
        return false;
    }
    memset(s, 0, sizeof *s);
    s->attacks = attacks;
    s->count = count;
    for (uint32_t i = 0; i < count; i++) {
        eng_fighter_init(&s->fighters[i], NULL);
        s->speed_scale[i] = 1.0F;
    }
    return true;
}

bool eng_sim_place(EngCombatSim *s, uint32_t i, float x, float z, float yaw)
{
    if (s == NULL || i >= s->count || !isfinite(x) || !isfinite(z) || !isfinite(yaw)) {
        return false;
    }
    EngSimBody *b = &s->bodies[i];
    b->pos[0] = x;
    b->pos[1] = z;
    b->yaw = yaw;
    b->prev_pos[0] = x;
    b->prev_pos[1] = z;
    b->prev_yaw = yaw;
    s->fighters[i].yaw = yaw;
    return true;
}

bool eng_sim_set_rig(EngCombatSim *s, uint32_t i, const EngCapsule *caps, const EngHitZone *zones,
                     uint32_t count)
{
    if (s == NULL || i >= s->count || count > ENG_HURTBOX_MAX ||
        (count > 0U && (caps == NULL || zones == NULL))) {
        return false;
    }
    EngSimRig *r = &s->rig[i];
    if (count == 0U) {
        memset(r, 0, sizeof *r);
        return true;
    }
    if (r->count == count) {
        memcpy(r->prev, r->cur, sizeof(EngCapsule) * count);
    } else {
        memcpy(r->prev, caps, sizeof(EngCapsule) * count); /* première pose : pas de balayage */
    }
    memcpy(r->cur, caps, sizeof(EngCapsule) * count);
    memcpy(r->zones, zones, sizeof(EngHitZone) * count);
    r->count = count;
    return true;
}

static void move_body(EngCombatSim *s, uint32_t i, const EngFighterInput *in)
{
    EngFighter *f = &s->fighters[i];
    EngSimBody *b = &s->bodies[i];
    const float len = sqrtf(in->move_dir[0] * in->move_dir[0] + in->move_dir[1] * in->move_dir[1]);
    const bool free_state = f->fsm.state == ENG_CS_MOVE || f->fsm.state == ENG_CS_IDLE;
    if (free_state && in->moving && isfinite(len) && len > 0.5F) {
        const float dx = in->move_dir[0] / len;
        const float dz = in->move_dir[1] / len;
        b->yaw = atan2f(-dx, -dz);
        if (f->fsm.state == ENG_CS_MOVE) {
            b->pos[0] += dx * ENG_SIM_WALK_SPEED * s->speed_scale[i] / 60.0F;
            b->pos[1] += dz * ENG_SIM_WALK_SPEED * s->speed_scale[i] / 60.0F;
        }
    }
    b->pos[0] += f->root_delta[0];
    b->pos[1] += f->root_delta[1];
    f->yaw = b->yaw;
}

static void resolve_bodies(EngCombatSim *s)
{
    for (uint32_t pass = 0; pass < 2U; pass++) {
        for (uint32_t i = 0; i < s->count; i++) {
            for (uint32_t j = i + 1U; j < s->count; j++) {
                const float rr = s->radius[i] + s->radius[j];
                if (rr <= 0.0F || s->fighters[i].vitals.dead || s->fighters[j].vitals.dead) {
                    continue;
                }
                float dx = s->bodies[j].pos[0] - s->bodies[i].pos[0];
                float dz = s->bodies[j].pos[1] - s->bodies[i].pos[1];
                const float d = sqrtf(dx * dx + dz * dz);
                if (d >= rr) {
                    continue;
                }
                if (d < 1e-6F) {
                    dx = 1.0F;
                    dz = 0.0F;
                } else {
                    dx /= d;
                    dz /= d;
                }
                const float push = (rr - d) * 0.5F;
                s->bodies[i].pos[0] -= dx * push;
                s->bodies[i].pos[1] -= dz * push;
                s->bodies[j].pos[0] += dx * push;
                s->bodies[j].pos[1] += dz * push;
            }
        }
    }
    if (s->world != NULL) {
        for (uint32_t i = 0; i < s->count; i++) {
            if ((s->world_ignore >> i & 1U) != 0U) {
                continue;
            }
            eng_aiw_push_out(s->world, s->bodies[i].pos, s->radius[i] > 0.0F ? s->radius[i] : 0.3F);
        }
    }
}

static void push_event(EngCombatSim *s, const EngHit *h, const EngHitOutcome *o)
{
    if (s->event_count >= ENG_SIM_EVENTS) {
        return;
    }
    EngSimEvent *e = &s->events[s->event_count++];
    e->tick = s->tick;
    e->attacker = h->attacker;
    e->defender = h->defender;
    e->kind = o->kind;
    e->zone = h->zone;
    e->dir = o->dir;
    e->dealt = o->dealt;
    e->killed = o->killed;
    e->hitstop = o->hitstop;
    memcpy(e->point, h->point, sizeof e->point);
}

#define SIM_MAX_HITS_PER_ATTACKER 8U

void eng_sim_step(EngCombatSim *s, const EngFighterInput *inputs)
{
    if (s == NULL || inputs == NULL) {
        return;
    }
    s->tick++;
    s->event_count = 0;
    if (s->hitstop > 0U) { /* gel d'impact : seules les entrées sont retenues */
        for (uint32_t i = 0; i < s->count; i++) {
            eng_fighter_buffer_inputs(&s->fighters[i], &inputs[i]);
        }
        s->hitstop--;
        return;
    }
    for (uint32_t i = 0; i < s->count; i++) {
        s->bodies[i].prev_pos[0] = s->bodies[i].pos[0];
        s->bodies[i].prev_pos[1] = s->bodies[i].pos[1];
        s->bodies[i].prev_yaw = s->bodies[i].yaw;
    }
    for (uint32_t i = 0; i < s->count; i++) {
        eng_fighter_tick(&s->fighters[i], s->attacks, &inputs[i]);
        move_body(s, i, &inputs[i]);
    }
    /* phase 1 : toutes les touches, sur l'état résultant de ce tick */
    EngHit hits[ENG_SIM_MAX][SIM_MAX_HITS_PER_ATTACKER];
    memset(hits, 0, sizeof hits);
    uint32_t hit_count[ENG_SIM_MAX] = {0};
    EngCapsule box_prev[ENG_SIM_MAX][ENG_HURTBOX_MAX];
    EngCapsule box_cur[ENG_SIM_MAX][ENG_HURTBOX_MAX];
    EngHitZone zones[ENG_SIM_MAX][ENG_HURTBOX_MAX];
    uint32_t box_n[ENG_SIM_MAX];
    const EngAttack *atk_of[ENG_SIM_MAX] = {NULL};
    bool hittable[ENG_SIM_MAX];
    for (uint32_t i = 0; i < s->count; i++) {
        if (s->rig[i].count > 0U) {
            box_n[i] = s->rig[i].count;
            memcpy(box_prev[i], s->rig[i].prev, sizeof(EngCapsule) * box_n[i]);
            memcpy(box_cur[i], s->rig[i].cur, sizeof(EngCapsule) * box_n[i]);
            memcpy(zones[i], s->rig[i].zones, sizeof(EngHitZone) * box_n[i]);
        } else {
            box_n[i] = ENG_SIM_HURTBOXES;
            eng_sim_hurtboxes(&s->bodies[i], true, box_prev[i], zones[i]);
            eng_sim_hurtboxes(&s->bodies[i], false, box_cur[i], zones[i]);
        }
        hittable[i] = eng_fighter_hittable(&s->fighters[i]);
        hit_count[i] = 0;
        atk_of[i] = NULL;
    }
    for (uint32_t i = 0; i < s->count; i++) {
        EngFighter *a = &s->fighters[i];
        if (!eng_fighter_hitboxes_active(a, s->attacks)) {
            continue;
        }
        const EngAttack *atk = eng_fighter_attack(a, s->attacks);
        atk_of[i] = atk;
        EngCapsule wprev[ENG_ATTACK_MAX_HITBOXES];
        EngCapsule wcur[ENG_ATTACK_MAX_HITBOXES];
        const uint32_t nw = eng_sim_weapons(&s->bodies[i], true, atk, wprev);
        (void)eng_sim_weapons(&s->bodies[i], false, atk, wcur);
        EngHitTarget targets[ENG_SIM_MAX];
        uint32_t nt = 0;
        for (uint32_t j = 0; j < s->count; j++) {
            if (j == i) {
                continue;
            }
            const EngHitTarget t = {j, hittable[j], box_n[j], box_prev[j], box_cur[j], zones[j]};
            targets[nt++] = t;
        }
        hit_count[i] = eng_hit_resolve(i, wprev, wcur, nw, targets, nt, &a->tracker, hits[i],
                                       SIM_MAX_HITS_PER_ATTACKER, NULL);
    }
    /* phase 2 : application */
    for (uint32_t i = 0; i < s->count; i++) {
        const EngAttack *atk = atk_of[i]; /* figée en phase 1 : les touches simultanées comptent */
        for (uint32_t k = 0; k < hit_count[i]; k++) {
            EngFighter *d = &s->fighters[hits[i][k].defender];
            float dx = s->bodies[hits[i][k].defender].pos[0] - s->bodies[i].pos[0];
            float dz = s->bodies[hits[i][k].defender].pos[1] - s->bodies[i].pos[1];
            const float len = sqrtf(dx * dx + dz * dz);
            if (len > 1e-6F) {
                dx /= len;
                dz /= len;
            } else {
                dx = -sinf(s->bodies[i].yaw);
                dz = -cosf(s->bodies[i].yaw);
            }
            const float force[2] = {dx, dz};
            const EngHitOutcome o =
                eng_combat_apply_hit(&s->fighters[i], d, atk, hits[i][k].zone, force);
            if (o.kind == ENG_HITKIND_IGNORED) {
                continue;
            }
            s->total_hits++;
            s->bodies[hits[i][k].defender].pos[0] += o.knockback[0];
            s->bodies[hits[i][k].defender].pos[1] += o.knockback[1];
            if (o.hitstop > s->hitstop) {
                s->hitstop = o.hitstop;
            }
            push_event(s, &hits[i][k], &o);
        }
    }
    resolve_bodies(s);
}

uint64_t eng_sim_hash(const EngCombatSim *s)
{
    uint64_t h = 1469598103934665603ULL;
    if (s == NULL) {
        return h;
    }
    const uint32_t head[3] = {s->tick, s->hitstop, s->total_hits};
    for (uint32_t i = 0; i < 3U; i++) {
        h = (h ^ head[i]) * 1099511628211ULL;
    }
    for (uint32_t i = 0; i < s->count; i++) {
        h = (h ^ (uint32_t)eng_fighter_hash(&s->fighters[i])) * 1099511628211ULL;
        h = (h ^ (uint32_t)(eng_fighter_hash(&s->fighters[i]) >> 32)) * 1099511628211ULL;
        const float v[3] = {s->bodies[i].pos[0], s->bodies[i].pos[1], s->bodies[i].yaw};
        for (uint32_t k = 0; k < 3U; k++) {
            uint32_t u = 0;
            memcpy(&u, &v[k], sizeof u);
            h = (h ^ u) * 1099511628211ULL;
        }
    }
    return h;
}
