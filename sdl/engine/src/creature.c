#include "engine/creature.h"

#include <math.h>
#include <stddef.h>

#include "creature_parts.h"

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
