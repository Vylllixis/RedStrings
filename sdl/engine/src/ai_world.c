#include "engine/ai_world.h"

#include <math.h>
#include <string.h>

void eng_aiw_init(EngAiWorld *w)
{
    if (w != NULL) {
        memset(w, 0, sizeof *w);
    }
}

bool eng_aiw_add_circle(EngAiWorld *w, float x, float z, float r)
{
    if (w == NULL || w->count >= ENG_AI_MAX_OBSTACLES || !isfinite(x) || !isfinite(z) ||
        !isfinite(r) || r <= 0.0F) {
        return false;
    }
    EngAiObstacle *o = &w->obs[w->count++];
    memset(o, 0, sizeof *o);
    o->c[0] = x;
    o->c[1] = z;
    o->r = r;
    return true;
}

bool eng_aiw_add_box(EngAiWorld *w, float x, float z, float half_x, float half_z)
{
    if (w == NULL || w->count >= ENG_AI_MAX_OBSTACLES || !isfinite(x) || !isfinite(z) ||
        !isfinite(half_x) || !isfinite(half_z) || half_x <= 0.0F || half_z <= 0.0F) {
        return false;
    }
    EngAiObstacle *o = &w->obs[w->count++];
    memset(o, 0, sizeof *o);
    o->box = true;
    o->c[0] = x;
    o->c[1] = z;
    o->half[0] = half_x;
    o->half[1] = half_z;
    return true;
}

static float sdf(const EngAiObstacle *o, const float p[2])
{
    const float dx = p[0] - o->c[0];
    const float dz = p[1] - o->c[1];
    if (!o->box) {
        return sqrtf(dx * dx + dz * dz) - o->r;
    }
    const float qx = fabsf(dx) - o->half[0];
    const float qz = fabsf(dz) - o->half[1];
    const float ox = qx > 0.0F ? qx : 0.0F;
    const float oz = qz > 0.0F ? qz : 0.0F;
    const float inside = (qx > qz ? qx : qz);
    return sqrtf(ox * ox + oz * oz) + (inside < 0.0F ? inside : 0.0F);
}

float eng_aiw_distance(const EngAiWorld *w, const float p[2])
{
    float best = INFINITY;
    if (w == NULL || p == NULL) {
        return best;
    }
    for (uint32_t i = 0; i < w->count; i++) {
        const float d = sdf(&w->obs[i], p);
        if (d < best) {
            best = d;
        }
    }
    return best;
}

static bool seg_hits_circle(const EngAiObstacle *o, const float a[2], const float b[2])
{
    const float dx = b[0] - a[0];
    const float dz = b[1] - a[1];
    const float l2 = dx * dx + dz * dz;
    float t = 0.0F;
    if (l2 > 1e-12F) {
        t = ((o->c[0] - a[0]) * dx + (o->c[1] - a[1]) * dz) / l2;
        t = t < 0.0F ? 0.0F : (t > 1.0F ? 1.0F : t);
    }
    const float px = a[0] + dx * t - o->c[0];
    const float pz = a[1] + dz * t - o->c[1];
    return px * px + pz * pz < o->r * o->r;
}

static bool seg_hits_box(const EngAiObstacle *o, const float a[2], const float b[2])
{
    float t0 = 0.0F;
    float t1 = 1.0F;
    for (int i = 0; i < 2; i++) {
        const float d = b[i] - a[i];
        const float lo = o->c[i] - o->half[i];
        const float hi = o->c[i] + o->half[i];
        if (fabsf(d) < 1e-9F) {
            if (a[i] < lo || a[i] > hi) {
                return false;
            }
            continue;
        }
        float ta = (lo - a[i]) / d;
        float tb = (hi - a[i]) / d;
        if (ta > tb) {
            const float tmp = ta;
            ta = tb;
            tb = tmp;
        }
        t0 = ta > t0 ? ta : t0;
        t1 = tb < t1 ? tb : t1;
        if (t0 > t1) {
            return false;
        }
    }
    return true;
}

bool eng_aiw_los(const EngAiWorld *w, const float a[2], const float b[2])
{
    if (w == NULL || a == NULL || b == NULL) {
        return true;
    }
    for (uint32_t i = 0; i < w->count; i++) {
        const EngAiObstacle *o = &w->obs[i];
        if (o->box ? seg_hits_box(o, a, b) : seg_hits_circle(o, a, b)) {
            return false;
        }
    }
    return true;
}

static void push_one(const EngAiObstacle *o, float p[2], float radius)
{
    const float dx = p[0] - o->c[0];
    const float dz = p[1] - o->c[1];
    if (!o->box) {
        const float d = sqrtf(dx * dx + dz * dz);
        const float need = o->r + radius;
        if (d >= need) {
            return;
        }
        const float nx = d > 1e-6F ? dx / d : 1.0F;
        const float nz = d > 1e-6F ? dz / d : 0.0F;
        p[0] = o->c[0] + nx * need;
        p[1] = o->c[1] + nz * need;
        return;
    }
    const float qx = fabsf(dx) - o->half[0];
    const float qz = fabsf(dz) - o->half[1];
    if (qx > 0.0F || qz > 0.0F) { /* à l'extérieur : point le plus proche de la boîte */
        const float cx =
            o->c[0] + (dx < -o->half[0] ? -o->half[0] : (dx > o->half[0] ? o->half[0] : dx));
        const float cz =
            o->c[1] + (dz < -o->half[1] ? -o->half[1] : (dz > o->half[1] ? o->half[1] : dz));
        const float ex = p[0] - cx;
        const float ez = p[1] - cz;
        const float d = sqrtf(ex * ex + ez * ez);
        if (d >= radius) {
            return;
        }
        p[0] = cx + ex / d * radius;
        p[1] = cz + ez / d * radius;
        return;
    }
    if (qx > qz) { /* à l'intérieur : sortir par la face la plus proche */
        p[0] = o->c[0] + (dx < 0.0F ? -1.0F : 1.0F) * (o->half[0] + radius);
    } else {
        p[1] = o->c[1] + (dz < 0.0F ? -1.0F : 1.0F) * (o->half[1] + radius);
    }
}

void eng_aiw_push_out(const EngAiWorld *w, float p[2], float radius)
{
    if (w == NULL || p == NULL || !isfinite(p[0]) || !isfinite(p[1])) {
        return;
    }
    for (uint32_t pass = 0; pass < 2U; pass++) {
        for (uint32_t i = 0; i < w->count; i++) {
            push_one(&w->obs[i], p, radius);
        }
    }
}
