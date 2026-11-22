#include "engine/vfx.h"

#include <math.h>
#include <string.h>

static uint32_t rng_next(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

static float rng_unit(uint32_t *s) /* [0 ; 1) */
{
    return (float)(rng_next(s) >> 8) / 16777216.0F;
}

void eng_particles_init(EngParticles *p, uint32_t seed)
{
    if (p == NULL) {
        return;
    }
    memset(p, 0, sizeof *p);
    p->rng = seed != 0U ? seed : 0x1234567U; /* xorshift ne tolère pas 0 */
}

bool eng_particle_spec_valid(const EngParticleSpec *s)
{
    return s != NULL && s->count >= 1U && s->count <= 64U && isfinite(s->speed_min) &&
           isfinite(s->speed_max) && s->speed_min >= 0.0F && s->speed_max >= s->speed_min &&
           isfinite(s->gravity) && s->life_min >= 1U && s->life_max >= s->life_min &&
           s->life_max <= 600U && isfinite(s->size) && s->size > 0.0F;
}

bool eng_particle_preset(EngVfxPreset preset, EngParticleSpec *out)
{
    static const EngParticleSpec presets[ENG_VFX_PRESET_COUNT] = {
        {12, 2.0F, 5.0F, 9.8F, 10, 18, 0.03F, {255, 230, 60, 255}, {255, 120, 20, 255}},
        {28, 3.0F, 8.0F, 9.8F, 14, 26, 0.05F, {255, 200, 60, 255}, {200, 30, 20, 255}},
        {10, 1.5F, 4.0F, 4.0F, 8, 14, 0.03F, {200, 230, 255, 255}, {90, 140, 255, 255}},
        {16, 4.0F, 7.0F, 0.0F, 6, 10, 0.06F, {255, 255, 255, 255}, {40, 230, 255, 255}},
    };
    if (out == NULL || (unsigned)preset >= (unsigned)ENG_VFX_PRESET_COUNT) {
        return false;
    }
    *out = presets[preset];
    return true;
}

uint32_t eng_particles_burst(EngParticles *p, const EngParticleSpec *spec, const float origin[3])
{
    if (p == NULL || origin == NULL || !eng_particle_spec_valid(spec) || !isfinite(origin[0]) ||
        !isfinite(origin[1]) || !isfinite(origin[2])) {
        return 0;
    }
    uint32_t made = 0;
    for (uint32_t i = 0; i < spec->count; i++) {
        /* on tire toujours les mêmes nombres, place ou pas : le flux reste reproductible */
        const float z = 2.0F * rng_unit(&p->rng) - 1.0F;
        const float a = 6.28318531F * rng_unit(&p->rng);
        const float speed =
            spec->speed_min + (spec->speed_max - spec->speed_min) * rng_unit(&p->rng);
        const uint32_t span = spec->life_max - spec->life_min + 1U;
        const uint32_t life = spec->life_min + rng_next(&p->rng) % span;
        if (p->count >= ENG_PARTICLES_MAX) {
            p->dropped++;
            continue;
        }
        const float r = sqrtf(fmaxf(0.0F, 1.0F - z * z));
        EngParticle *q = &p->items[p->count++];
        q->pos[0] = origin[0];
        q->pos[1] = origin[1];
        q->pos[2] = origin[2];
        q->vel[0] = r * cosf(a) * speed;
        q->vel[1] = z * speed;
        q->vel[2] = r * sinf(a) * speed;
        q->age = 0;
        q->life = life;
        q->size = spec->size;
        q->gravity = spec->gravity;
        q->start = spec->start;
        q->end = spec->end;
        made++;
        p->spawned++;
    }
    return made;
}

void eng_particles_update(EngParticles *p)
{
    if (p == NULL) {
        return;
    }
    uint32_t w = 0;
    for (uint32_t i = 0; i < p->count; i++) {
        EngParticle q = p->items[i];
        q.age++;
        if (q.age >= q.life) {
            continue;
        }
        q.vel[1] -= q.gravity / 60.0F; /* Euler semi-implicite : vitesse puis position */
        q.pos[0] += q.vel[0] / 60.0F;
        q.pos[1] += q.vel[1] / 60.0F;
        q.pos[2] += q.vel[2] / 60.0F;
        p->items[w++] = q;
    }
    p->count = w;
}

EngDebugColor eng_particle_color(const EngParticle *q)
{
    if (q == NULL || q->life == 0U) {
        return ENG_DEBUG_WHITE;
    }
    const float u = fminf((float)q->age / (float)q->life, 1.0F);
    EngDebugColor c;
    c.r = (uint8_t)lroundf((float)q->start.r + ((float)q->end.r - (float)q->start.r) * u);
    c.g = (uint8_t)lroundf((float)q->start.g + ((float)q->end.g - (float)q->start.g) * u);
    c.b = (uint8_t)lroundf((float)q->start.b + ((float)q->end.b - (float)q->start.b) * u);
    c.a = 255;
    return c;
}

float eng_particle_alpha(const EngParticle *q)
{
    if (q == NULL || q->life == 0U || q->age >= q->life) {
        return 0.0F;
    }
    return 1.0F - (float)q->age / (float)q->life;
}

uint32_t eng_vfx_on_event(EngParticles *p, const EngSimEvent *e)
{
    if (p == NULL || e == NULL || e->kind == ENG_HITKIND_IGNORED) {
        return 0;
    }
    EngVfxPreset preset = ENG_VFX_SPARK_LIGHT;
    switch (e->kind) {
    case ENG_HITKIND_BLOCKED:
        preset = ENG_VFX_SPARK_BLOCK;
        break;
    case ENG_HITKIND_PARRIED:
        preset = ENG_VFX_FLASH_PARRY;
        break;
    default:
        preset = e->hitstop >= 5U ? ENG_VFX_SPARK_HEAVY : ENG_VFX_SPARK_LIGHT;
        break;
    }
    EngParticleSpec spec;
    if (!eng_particle_preset(preset, &spec)) {
        return 0;
    }
    return eng_particles_burst(p, &spec, e->point);
}

uint32_t eng_particles_draw_debug(const EngParticles *p, EngDebugLines *lines)
{
    if (p == NULL || lines == NULL) {
        return 0;
    }
    uint32_t n = 0;
    for (uint32_t i = 0; i < p->count; i++) {
        const EngParticle *q = &p->items[i];
        const vec3 a = {q->pos[0], q->pos[1], q->pos[2]};
        const vec3 b = {q->pos[0] - q->vel[0] * 0.02F, q->pos[1] - q->vel[1] * 0.02F,
                        q->pos[2] - q->vel[2] * 0.02F};
        if (eng_debug_line(lines, a, b, eng_particle_color(q))) {
            n++;
        }
    }
    return n;
}

bool eng_trail_init(EngWeaponTrail *t, uint32_t life)
{
    if (t == NULL || life < 1U || life > ENG_TRAIL_MAX) {
        return false;
    }
    memset(t, 0, sizeof *t);
    t->life = life;
    return true;
}

bool eng_trail_push(EngWeaponTrail *t, const float a[3], const float b[3])
{
    if (t == NULL || t->life == 0U || a == NULL || b == NULL) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        if (!isfinite(a[i]) || !isfinite(b[i])) {
            return false;
        }
    }
    if (t->count >= ENG_TRAIL_MAX) { /* plein : on perd le plus ancien */
        memmove(&t->samples[0], &t->samples[1], sizeof(EngTrailSample) * (ENG_TRAIL_MAX - 1U));
        t->count = ENG_TRAIL_MAX - 1U;
    }
    EngTrailSample *s = &t->samples[t->count++];
    memcpy(s->a, a, sizeof s->a);
    memcpy(s->b, b, sizeof s->b);
    s->age = 0;
    return true;
}

void eng_trail_update(EngWeaponTrail *t)
{
    if (t == NULL) {
        return;
    }
    uint32_t w = 0;
    for (uint32_t i = 0; i < t->count; i++) {
        EngTrailSample s = t->samples[i];
        s.age++;
        if (s.age >= t->life) {
            continue;
        }
        t->samples[w++] = s;
    }
    t->count = w;
}

float eng_trail_alpha(const EngWeaponTrail *t, uint32_t i)
{
    if (t == NULL || i >= t->count || t->life == 0U) {
        return 0.0F;
    }
    return 1.0F - (float)t->samples[i].age / (float)t->life;
}

uint32_t eng_trail_build_strip(const EngWeaponTrail *t, float (*out_pos)[3], float *out_alpha,
                               uint32_t max_vertices)
{
    if (t == NULL || out_pos == NULL || out_alpha == NULL) {
        return 0;
    }
    uint32_t v = 0;
    for (uint32_t i = 0; i < t->count && v + 2U <= max_vertices; i++) {
        memcpy(out_pos[v], t->samples[i].a, sizeof out_pos[v]);
        memcpy(out_pos[v + 1U], t->samples[i].b, sizeof out_pos[v + 1U]);
        out_alpha[v] = eng_trail_alpha(t, i);
        out_alpha[v + 1U] = out_alpha[v];
        v += 2U;
    }
    return v;
}

uint32_t eng_trail_draw_debug(const EngWeaponTrail *t, EngDebugLines *lines)
{
    if (t == NULL || lines == NULL) {
        return 0;
    }
    uint32_t n = 0;
    for (uint32_t i = 0; i < t->count; i++) {
        const float al = eng_trail_alpha(t, i);
        const uint8_t g = (uint8_t)lroundf(255.0F * al);
        const vec3 a = {t->samples[i].a[0], t->samples[i].a[1], t->samples[i].a[2]};
        const vec3 b = {t->samples[i].b[0], t->samples[i].b[1], t->samples[i].b[2]};
        if (eng_debug_line(lines, a, b, ENG_DEBUG_RGB(g, g, g))) {
            n++;
        }
        if (i > 0U) {
            const vec3 pa = {t->samples[i - 1U].b[0], t->samples[i - 1U].b[1],
                             t->samples[i - 1U].b[2]};
            if (eng_debug_line(lines, pa, b, ENG_DEBUG_RGB(g, g, g))) {
                n++;
            }
        }
    }
    return n;
}
