#include "engine/creature.h"

#include <math.h>
#include <stddef.h>

typedef struct Builder {
    EngCreaturePart *out;
    uint32_t max;
    uint32_t count;
} Builder;

static float unit(float v)
{
    if (!isfinite(v) || v < 0.0F) {
        return 0.0F;
    }
    return v > 1.0F ? 1.0F : v;
}

static float finite_or_zero(float v)
{
    return isfinite(v) ? v : 0.0F;
}

/* ajoute une boîte ; ignorée si la liste est pleine */
static void box(Builder *b, EngCreatureColor color, float x, float y, float z, float hx, float hy,
                float hz, float pitch, float yaw, float roll)
{
    if (b->out == NULL || b->count >= b->max) {
        return;
    }
    b->out[b->count++] = (EngCreaturePart){
        .pos = {x, y, z}, .half = {hx, hy, hz}, .rot = {pitch, yaw, roll}, .color = (uint8_t)color};
}

/* couche la créature sur son flanc droit : rotation de `angle` autour de l'axe Z passant par le
 * sol, appliquée au centre et au roulis de chaque boîte */
static void lay_down(Builder *b, uint32_t first, float angle, float sink)
{
    const float c = cosf(angle);
    const float s = sinf(angle);
    for (uint32_t i = first; i < b->count; i++) {
        EngCreaturePart *p = &b->out[i];
        const float x = p->pos[0];
        const float y = p->pos[1];
        p->pos[0] = (x * c) + (y * s);
        p->pos[1] = (-x * s) + (y * c) - sink;
        p->rot[2] -= angle;
    }
}

float eng_creature_horse_saddle(float fall)
{
    return 1.62F * (1.0F - unit(fall)); /* dessus du dos ; à terre quand le cheval est couché */
}

uint32_t eng_creature_horse(float time, float gait, float fall, EngCreaturePart *out, uint32_t max)
{
    Builder b = {out, max, 0};
    const float t = finite_or_zero(time);
    const float g = unit(gait);
    const float f = unit(fall);
    const float alive = 1.0F - f;                /* les jambes ne battent plus à terre */
    const float phase = t * (4.0F + (8.0F * g)); /* pas plus rapides au galop */
    const float swing = 0.65F * g * alive;       /* amplitude des jambes */
    const float bob = 0.05F * g * alive * sinf(2.0F * phase);
    /* tronc, encolure, tête (avant vers -Z) */
    box(&b, ENG_CREATURE_BROWN, 0.0F, 1.22F + bob, 0.0F, 0.34F, 0.36F, 1.0F, 0.0F, 0.0F, 0.0F);
    box(&b, ENG_CREATURE_BROWN, 0.0F, 1.78F + bob, -0.98F, 0.17F, 0.46F, 0.21F, 0.5F, 0.0F, 0.0F);
    box(&b, ENG_CREATURE_BROWN, 0.0F, 2.18F + bob, -1.42F, 0.15F, 0.17F, 0.36F, 0.25F, 0.0F, 0.0F);
    box(&b, ENG_CREATURE_DARK, 0.0F, 1.95F + bob, -0.78F, 0.05F, 0.42F, 0.1F, 0.5F, 0.0F, 0.0F);
    box(&b, ENG_CREATURE_DARK, 0.0F, 2.36F + bob, -1.22F, 0.2F, 0.07F, 0.05F, 0.0F, 0.0F, 0.0F);
    /* queue qui bat */
    box(&b, ENG_CREATURE_DARK, 0.0F, 1.18F + bob, 1.12F, 0.07F, 0.42F, 0.07F,
        -0.35F - (0.2F * g * sinf(phase)), 0.0F, 0.0F);
    /* quatre jambes : diagonales en phase, sabots sombres */
    const float lx[4] = {-0.24F, 0.24F, -0.24F, 0.24F};
    const float lz[4] = {-0.72F, -0.72F, 0.72F, 0.72F};
    for (int i = 0; i < 4; i++) {
        const float a = swing * sinf(phase + ((i == 0 || i == 3) ? 0.0F : 3.1415927F));
        /* la jambe pivote autour de la hanche (y = 0,9) */
        box(&b, ENG_CREATURE_BROWN, lx[i], 0.9F - (0.45F * cosf(a)) + bob,
            lz[i] - (0.45F * sinf(a)), 0.085F, 0.45F, 0.085F, a, 0.0F, 0.0F);
        box(&b, ENG_CREATURE_DARK, lx[i], 0.9F - (0.92F * cosf(a)) + bob + 0.06F,
            lz[i] - (0.92F * sinf(a)), 0.1F, 0.07F, 0.1F, a, 0.0F, 0.0F);
    }
    /* selle */
    box(&b, ENG_CREATURE_DARK, 0.0F, 1.6F + bob, 0.05F, 0.36F, 0.04F, 0.34F, 0.0F, 0.0F, 0.0F);
    if (f > 0.0F) {
        lay_down(&b, 0, 1.45F * f, 0.25F * f);
    }
    return b.count;
}

uint32_t eng_creature_dragon(float time, float bite, float flame, EngCreaturePart *out,
                             uint32_t max)
{
    Builder b = {out, max, 0};
    const float t = finite_or_zero(time);
    const float open = unit(bite);
    const float fire = unit(flame);
    const float breathe = 0.06F * sinf(1.6F * t);
    const float flap = 0.35F + (0.3F * sinf(2.4F * t)); /* roulis des ailes */
    const float sway = 0.25F * sinf(1.1F * t);          /* queue et cou */
    const float lunge = 0.5F * open; /* la tête avance et s'abat quand il mord */
    const float jaw = 0.15F + (0.55F * (open > fire ? open : fire));
    /* corps et ventre */
    box(&b, ENG_CREATURE_RED, 0.0F, 1.9F + breathe, 0.0F, 0.95F, 0.8F, 2.0F, 0.0F, 0.0F, 0.0F);
    box(&b, ENG_CREATURE_ORANGE, 0.0F, 1.25F + breathe, 0.0F, 0.75F, 0.25F, 1.8F, 0.0F, 0.0F, 0.0F);
    /* cou : trois segments qui montent vers l'avant */
    for (int i = 0; i < 3; i++) {
        const float k = (float)i;
        box(&b, ENG_CREATURE_RED, sway * 0.25F * (k + 1.0F),
            2.4F + (0.42F * k) + breathe - (0.6F * open * (k + 1.0F)),
            -2.2F - (0.55F * k) - (lunge * 0.3F * (k + 1.0F)), 0.42F - (0.05F * k), 0.4F, 0.42F,
            0.55F, 0.0F, 0.0F);
    }
    /* tête, mâchoire, yeux, cornes */
    const float hx = sway;
    const float hy = 3.4F + breathe - (2.2F * open);
    const float hz = -3.9F - lunge;
    box(&b, ENG_CREATURE_RED, hx, hy, hz, 0.5F, 0.34F, 0.85F, 0.0F, 0.0F, 0.0F);
    box(&b, ENG_CREATURE_RED, hx, hy - 0.42F - (0.25F * jaw), hz - 0.1F + (0.1F * jaw), 0.42F,
        0.12F, 0.72F, -jaw, 0.0F, 0.0F);
    box(&b, ENG_CREATURE_YELLOW, hx - 0.46F, hy + 0.12F, hz - 0.35F, 0.08F, 0.1F, 0.12F, 0.0F, 0.0F,
        0.0F);
    box(&b, ENG_CREATURE_YELLOW, hx + 0.46F, hy + 0.12F, hz - 0.35F, 0.08F, 0.1F, 0.12F, 0.0F, 0.0F,
        0.0F);
    box(&b, ENG_CREATURE_BONE, hx - 0.3F, hy + 0.55F, hz + 0.55F, 0.08F, 0.35F, 0.08F, -0.6F, 0.0F,
        0.0F);
    box(&b, ENG_CREATURE_BONE, hx + 0.3F, hy + 0.55F, hz + 0.55F, 0.08F, 0.35F, 0.08F, -0.6F, 0.0F,
        0.0F);
    /* queue : quatre segments de plus en plus fins, qui ondulent */
    for (int i = 0; i < 4; i++) {
        const float k = (float)i;
        box(&b, ENG_CREATURE_RED, sway * 0.5F * (k + 1.0F) * sinf((1.3F * t) + k),
            1.7F - (0.28F * k) + breathe, 2.5F + (0.95F * k), 0.5F - (0.1F * k),
            0.42F - (0.08F * k), 0.55F, 0.0F, 0.0F, 0.0F);
    }
    box(&b, ENG_CREATURE_BONE, 0.0F, 0.75F, 6.0F, 0.35F, 0.12F, 0.3F, 0.0F, 0.0F, 0.0F);
    /* ailes : deux segments de chaque côté, qui battent */
    for (int side = -1; side <= 1; side += 2) {
        const float sgn = (float)side;
        box(&b, ENG_CREATURE_DARK, sgn * 1.9F, 3.0F + (0.9F * sinf(flap)), 0.1F, 1.2F, 0.07F, 1.0F,
            0.0F, 0.0F, sgn * flap);
        box(&b, ENG_CREATURE_DARK, sgn * 3.6F, 3.0F + (2.2F * sinf(flap)), 0.4F, 1.0F, 0.06F, 0.8F,
            0.0F, 0.0F, sgn * flap * 1.5F);
    }
    /* quatre pattes et leurs griffes */
    const float px[4] = {-0.8F, 0.8F, -0.8F, 0.8F};
    const float pz[4] = {-1.3F, -1.3F, 1.4F, 1.4F};
    for (int i = 0; i < 4; i++) {
        box(&b, ENG_CREATURE_RED, px[i], 0.7F, pz[i], 0.3F, 0.7F, 0.32F, 0.0F, 0.0F, 0.0F);
        box(&b, ENG_CREATURE_BONE, px[i], 0.08F, pz[i] - 0.35F, 0.28F, 0.08F, 0.2F, 0.0F, 0.0F,
            0.0F);
    }
    /* flammes : trois boîtes devant la gueule, de plus en plus larges, jusqu'à 9 m */
    if (fire > 0.0F) {
        for (int i = 0; i < 3; i++) {
            const float k = (float)i;
            const float reach = fire * (2.2F + (2.6F * k));
            const float flick = 0.1F * sinf((17.0F * t) + (2.0F * k));
            box(&b, i == 0 ? ENG_CREATURE_YELLOW : ENG_CREATURE_ORANGE, hx, hy - 0.5F - (0.25F * k),
                hz - 1.0F - reach, (0.3F + (0.3F * k)) * (1.0F + flick), (0.25F + (0.22F * k)),
                0.9F + (0.5F * k), 0.1F, 0.0F, flick);
        }
    }
    return b.count;
}
