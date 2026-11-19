#include "engine/combat_geom.h"

#include <math.h>
#include <stddef.h>

#define SWEEP_MAX_STEPS 256U
#define MIN_STEP 0.01F /* m : pas minimal quand les rayons sont nuls */

static float dot3(const float a[3], const float b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Ericson, Real-Time Collision Detection, §5.1.9. */
float eng_segseg_dist2(const float p1[3], const float q1[3], const float p2[3], const float q2[3],
                       float c1[3], float c2[3])
{
    float d1[3] = {q1[0] - p1[0], q1[1] - p1[1], q1[2] - p1[2]};
    float d2[3] = {q2[0] - p2[0], q2[1] - p2[1], q2[2] - p2[2]};
    const float r[3] = {p1[0] - p2[0], p1[1] - p2[1], p1[2] - p2[2]};
    const float a = dot3(d1, d1);
    const float e = dot3(d2, d2);
    const float f = dot3(d2, r);
    const float eps = 1e-12F;
    float s;
    float t;
    if (a <= eps && e <= eps) {
        s = 0.0F;
        t = 0.0F;
    } else if (a <= eps) {
        s = 0.0F;
        t = clampf(f / e, 0.0F, 1.0F);
    } else {
        const float c = dot3(d1, r);
        if (e <= eps) {
            t = 0.0F;
            s = clampf(-c / a, 0.0F, 1.0F);
        } else {
            const float b = dot3(d1, d2);
            const float denom = a * e - b * b;
            s = (denom > eps * a * e) ? clampf((b * f - c * e) / denom, 0.0F, 1.0F) : 0.0F;
            t = (b * s + f) / e;
            if (t < 0.0F) {
                t = 0.0F;
                s = clampf(-c / a, 0.0F, 1.0F);
            } else if (t > 1.0F) {
                t = 1.0F;
                s = clampf((b - c) / a, 0.0F, 1.0F);
            }
        }
    }
    float x[3];
    float y[3];
    for (int i = 0; i < 3; i++) {
        x[i] = p1[i] + d1[i] * s;
        y[i] = p2[i] + d2[i] * t;
    }
    if (c1 != NULL) {
        for (int i = 0; i < 3; i++) {
            c1[i] = x[i];
        }
    }
    if (c2 != NULL) {
        for (int i = 0; i < 3; i++) {
            c2[i] = y[i];
        }
    }
    const float dx[3] = {x[0] - y[0], x[1] - y[1], x[2] - y[2]};
    return dot3(dx, dx);
}

static bool capsule_ok(const EngCapsule *c)
{
    for (int i = 0; i < 3; i++) {
        if (!isfinite(c->a[i]) || !isfinite(c->b[i])) {
            return false;
        }
    }
    return isfinite(c->radius) && c->radius >= 0.0F;
}

bool eng_capsule_overlap(const EngCapsule *x, const EngCapsule *y, float point[3])
{
    if (x == NULL || y == NULL || !capsule_ok(x) || !capsule_ok(y)) {
        return false;
    }
    float c1[3];
    float c2[3];
    const float d2 = eng_segseg_dist2(x->a, x->b, y->a, y->b, c1, c2);
    const float r = x->radius + y->radius;
    if (d2 > r * r) {
        return false;
    }
    if (point != NULL) {
        for (int i = 0; i < 3; i++) {
            point[i] = 0.5F * (c1[i] + c2[i]);
        }
    }
    return true;
}

static float disp(const float p[3], const float q[3])
{
    const float d[3] = {q[0] - p[0], q[1] - p[1], q[2] - p[2]};
    return sqrtf(dot3(d, d));
}

uint32_t eng_capsule_sweep_steps(const EngCapsule *w0, const EngCapsule *w1, const EngCapsule *t0,
                                 const EngCapsule *t1)
{
    float move = 0.0F; /* borne du déplacement relatif de n'importe quel point de l'axe */
    const float wm = fmaxf(disp(w0->a, w1->a), disp(w0->b, w1->b));
    const float tm = fmaxf(disp(t0->a, t1->a), disp(t0->b, t1->b));
    move = wm + tm;
    float step = w0->radius + t0->radius;
    if (step < MIN_STEP) {
        step = MIN_STEP;
    }
    const float n = ceilf(move / step);
    if (!(n >= 1.0F)) {
        return 1U;
    }
    return (n > (float)SWEEP_MAX_STEPS) ? SWEEP_MAX_STEPS : (uint32_t)n;
}

static void lerp3(const float p[3], const float q[3], float t, float out[3])
{
    for (int i = 0; i < 3; i++) {
        out[i] = p[i] + (q[i] - p[i]) * t;
    }
}

bool eng_capsule_sweep(const EngCapsule *w0, const EngCapsule *w1, const EngCapsule *t0,
                       const EngCapsule *t1, float *time, float point[3])
{
    if (w0 == NULL || w1 == NULL || t0 == NULL || t1 == NULL || !capsule_ok(w0) ||
        !capsule_ok(w1) || !capsule_ok(t0) || !capsule_ok(t1)) {
        return false;
    }
    const uint32_t n = eng_capsule_sweep_steps(w0, w1, t0, t1);
    for (uint32_t i = 0; i <= n; i++) {
        const float u = (float)i / (float)n;
        EngCapsule w = {.radius = w0->radius};
        EngCapsule t = {.radius = t0->radius};
        lerp3(w0->a, w1->a, u, w.a);
        lerp3(w0->b, w1->b, u, w.b);
        lerp3(t0->a, t1->a, u, t.a);
        lerp3(t0->b, t1->b, u, t.b);
        if (eng_capsule_overlap(&w, &t, point)) {
            if (time != NULL) {
                *time = u;
            }
            return true;
        }
    }
    return false;
}
