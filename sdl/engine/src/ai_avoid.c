#include "engine/ai_avoid.h"

#include <math.h>
#include <stddef.h>

static bool finite2(const float v[2])
{
    return v != NULL && isfinite(v[0]) && isfinite(v[1]);
}

float eng_avoid_gap(const float pos[2], float radius, const EngAvoidCircle *circles, uint32_t count,
                    uint32_t *nearest)
{
    float best = INFINITY;
    if (!finite2(pos) || circles == NULL || !isfinite(radius)) {
        return best;
    }
    for (uint32_t i = 0; i < count; i++) {
        const float gap =
            hypotf(pos[0] - circles[i].x, pos[1] - circles[i].z) - (circles[i].r + radius);
        if (gap < best) {
            best = gap;
            if (nearest != NULL) {
                *nearest = i;
            }
        }
    }
    return best;
}

/* direction unitaire du centre (cx, cz) vers `pos` ; (1, 0) s'ils sont confondus */
static void away_from(const float pos[2], float cx, float cz, float out[2])
{
    const float dx = pos[0] - cx;
    const float dz = pos[1] - cz;
    const float len = hypotf(dx, dz);
    out[0] = len > 1e-5F ? dx / len : 1.0F;
    out[1] = len > 1e-5F ? dz / len : 0.0F;
}

bool eng_avoid_steer(EngAvoidState *state, const float pos[2], float radius,
                     const EngAvoidCircle *circles, uint32_t count, float dir[2])
{
    if (state == NULL || !finite2(pos) || !finite2(dir) || !isfinite(radius) ||
        (circles == NULL && count > 0U)) {
        return false;
    }
    const float speed = hypotf(dir[0], dir[1]);
    float away[2];
    if (state->fleeing) { /* a touché : droit dehors jusqu'à la marge */
        const float gap =
            hypotf(pos[0] - state->from[0], pos[1] - state->from[1]) - (state->from_r + radius);
        if (gap < ENG_AVOID_MARGIN) {
            away_from(pos, state->from[0], state->from[1], away);
            dir[0] = away[0];
            dir[1] = away[1];
            return true;
        }
        state->fleeing = false;
    }
    if (circles == NULL || count == 0U) {
        return false; /* rien à éviter */
    }
    uint32_t nearest = 0;
    const float gap = eng_avoid_gap(pos, radius, circles, count, &nearest);
    if (!(gap < ENG_AVOID_MARGIN) || nearest >= count) {
        return false;
    }
    const EngAvoidCircle *c = &circles[nearest];
    away_from(pos, c->x, c->z, away);
    if (gap <= 0.0F) { /* contact : il s'éloigne d'au moins la marge */
        state->fleeing = true;
        state->from[0] = c->x;
        state->from[1] = c->z;
        state->from_r = c->r;
        dir[0] = away[0];
        dir[1] = away[1];
        return true;
    }
    const float toward = -((dir[0] * away[0]) + (dir[1] * away[1]));
    if (!(toward > 0.0F) || speed < 1e-5F) {
        return false; /* il s'éloigne déjà, longe l'obstacle, ou ne bouge pas */
    }
    /* plus il est près, plus la part « vers l'obstacle » disparaît, changée en pas de côté */
    const float t = 1.0F - (gap / ENG_AVOID_MARGIN);
    float side[2] = {-away[1], away[0]};
    if ((side[0] * dir[0]) + (side[1] * dir[1]) < 0.0F) { /* du côté où il allait déjà */
        side[0] = -side[0];
        side[1] = -side[1];
    }
    float out[2] = {dir[0] + (t * toward * (away[0] + side[0])),
                    dir[1] + (t * toward * (away[1] + side[1]))};
    const float len = hypotf(out[0], out[1]);
    if (len < 1e-5F) {
        out[0] = side[0] * speed;
        out[1] = side[1] * speed;
    } else {
        out[0] *= speed / len;
        out[1] *= speed / len;
    }
    dir[0] = out[0];
    dir[1] = out[1];
    return true;
}

bool eng_avoid_push_out(float pos[2], float radius, const EngAvoidCircle *circles, uint32_t count)
{
    if (!finite2(pos) || circles == NULL || !isfinite(radius)) {
        return false;
    }
    bool moved = false;
    for (uint32_t pass = 0; pass < 3U; pass++) { /* deux obstacles voisins : quelques passes */
        bool any = false;
        for (uint32_t i = 0; i < count; i++) {
            const float reach = circles[i].r + radius;
            /* 0,1 mm de tolérance : un corps posé pile au contact n'est pas « dedans » */
            if (hypotf(pos[0] - circles[i].x, pos[1] - circles[i].z) < reach - 1e-4F) {
                float away[2];
                away_from(pos, circles[i].x, circles[i].z, away);
                pos[0] = circles[i].x + (away[0] * reach);
                pos[1] = circles[i].z + (away[1] * reach);
                any = true;
            }
        }
        moved = moved || any;
        if (!any) {
            break;
        }
    }
    return moved;
}
