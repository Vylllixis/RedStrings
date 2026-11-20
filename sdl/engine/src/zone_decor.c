#include "engine/zone_decor.h"

#include <math.h>
#include <stddef.h>

typedef struct Builder {
    EngDecorBox *out;
    uint32_t max;
    uint32_t count;
} Builder;

static void add(Builder *b, EngDecorColor color, bool solid, float x, float z, float hx, float hy,
                float hz, float lift, float yaw)
{
    if (b->out == NULL || b->count >= b->max || b->count >= ENG_DECOR_MAX) {
        return;
    }
    b->out[b->count++] = (EngDecorBox){.x = x,
                                       .z = z,
                                       .half = {hx, hy, hz},
                                       .lift = lift,
                                       .yaw = solid ? 0.0F : yaw,
                                       .color = (uint8_t)color,
                                       .solid = solid};
}

/* Le château : murs en 9 segments par côté (le relief passe dessous), une porte au milieu du mur
 * ouest (l'entrée, avec ses deux tours) et une au milieu du mur est (la sortie vers la montagne),
 * tours aux angles, donjon dans l'angle nord-est. */
static void castle(Builder *b)
{
    const float half = ENG_DECOR_CASTLE_HALF;
    const float gate = ENG_DECOR_GATE_WIDTH;
    const float thick = 0.6F;
    const float wall_h = 2.5F;                          /* demi-hauteur : murs de 5 m */
    const float side = ((2.0F * half) - gate) * 0.125F; /* 8 segments + la porte au centre */
    for (int i = 0; i < 9; i++) {
        /* centres le long d'un côté : 4 segments, la porte (i = 4), 4 segments */
        const float len = i == 4 ? gate : side;
        const float before =
            i < 4 ? (float)i * side
                  : (4.0F * side) + (i > 4 ? gate : 0.0F) + ((float)(i > 4 ? i - 5 : 0) * side);
        const float at = -half + before + (0.5F * len);
        add(b, ENG_DECOR_STONE, true, at, -half, 0.5F * len, wall_h, thick, 0.0F, 0.0F); /* nord */
        add(b, ENG_DECOR_STONE, true, at, half, 0.5F * len, wall_h, thick, 0.0F, 0.0F);  /* sud */
        if (i != 4) { /* ouest et est : les portes restent ouvertes (on entre, on ressort) */
            add(b, ENG_DECOR_STONE, true, -half, at, thick, wall_h, 0.5F * len, 0.0F, 0.0F);
            add(b, ENG_DECOR_STONE, true, half, at, thick, wall_h, 0.5F * len, 0.0F, 0.0F);
        }
    }
    for (int i = 0; i < 4; i++) { /* tours d'angle et leur toit */
        const float x = (i & 1) != 0 ? half : -half;
        const float z = (i & 2) != 0 ? half : -half;
        add(b, ENG_DECOR_STONE, true, x, z, 2.2F, 4.0F, 2.2F, 0.0F, 0.0F);
        add(b, ENG_DECOR_ROOF, false, x, z, 2.6F, 0.5F, 2.6F, 8.0F, 0.0F);
    }
    for (int i = -1; i <= 1; i += 2) { /* tours de la porte, bannières */
        const float z = (float)i * ((0.5F * gate) + 1.3F);
        add(b, ENG_DECOR_STONE, true, -half, z, 1.3F, 3.5F, 1.3F, 0.0F, 0.0F);
        add(b, ENG_DECOR_ROOF, false, -half, z, 1.6F, 0.4F, 1.6F, 7.0F, 0.0F);
        add(b, ENG_DECOR_CLOTH, false, -half - 1.4F, z, 0.05F, 1.4F, 0.6F, 3.4F, 0.0F);
    }
    /* donjon dans l'angle nord-est de la cour */
    const float keep_x = half - 6.0F;
    const float keep_z = -(half - 7.0F);
    add(b, ENG_DECOR_STONE, true, keep_x, keep_z, 3.5F, 5.5F, 4.5F, 0.0F, 0.0F);
    add(b, ENG_DECOR_ROOF, false, keep_x, keep_z, 4.0F, 0.6F, 5.0F, 11.0F, 0.0F);
    add(b, ENG_DECOR_CLOTH, false, keep_x - 3.6F, keep_z, 0.05F, 2.0F, 0.9F, 5.0F, 0.0F);
}

/* `count` piliers en cercle de rayon `radius`, de hauteurs variées (déterministes). */
static void ring(Builder *b, EngDecorColor color, uint32_t count, float radius, float half_side,
                 float height_min, float height_step)
{
    for (uint32_t i = 0; i < count; i++) {
        const float a = 6.2831853F * ((float)i + 0.5F) / (float)count;
        const float h = height_min + (height_step * (float)((i * 7U) % 4U));
        add(b, color, true, radius * cosf(a), radius * sinf(a), half_side, 0.5F * h, half_side,
            0.0F, 0.0F);
    }
}

static void campfire(Builder *b, float x, float z, float size)
{
    add(b, ENG_DECOR_FIRE, false, x, z, 0.3F * size, 0.3F * size, 0.3F * size, 0.1F, 0.6F);
    add(b, ENG_DECOR_WOOD, false, x, z, 0.7F * size, 0.1F, 0.12F, 0.0F, 0.4F);
    add(b, ENG_DECOR_WOOD, false, x, z, 0.7F * size, 0.1F, 0.12F, 0.0F, 2.0F);
}

/* une tente : un corps de toile (solide) et son faîte sombre */
static void tent(Builder *b, float x, float z)
{
    add(b, ENG_DECOR_CLOTH, true, x, z, 1.5F, 0.9F, 1.2F, 0.0F, 0.0F);
    add(b, ENG_DECOR_ROOF, false, x, z, 1.7F, 0.12F, 0.25F, 1.8F, 0.0F);
}

uint32_t eng_zone_decor(const EngMapZone *zone, EngDecorBox *out, uint32_t max)
{
    Builder b = {out, max, 0};
    if (zone == NULL || out == NULL) {
        return 0U;
    }
    if (zone->kind == ENG_MAPZONE_BOSS) {
        switch (zone->biome) {
        case ENG_BIOME_PLAIN:
            castle(&b);
            break;
        case ENG_BIOME_SNOW: /* piliers de glace, autel sombre au centre */
            ring(&b, ENG_DECOR_ICE, 10U, 18.0F, 1.0F, 5.0F, 1.5F);
            add(&b, ENG_DECOR_ROOF, false, 0.0F, 0.0F, 2.5F, 0.15F, 2.5F, 0.0F, 0.0F);
            break;
        case ENG_BIOME_DESERT: /* colonnes de grès, quatre pans de mur bas */
            ring(&b, ENG_DECOR_SANDSTONE, 12U, 17.0F, 0.9F, 2.0F, 1.6F);
            for (int i = 0; i < 4; i++) {
                const float x = (i & 1) != 0 ? 21.0F : -21.0F;
                const float z = (i & 2) != 0 ? 9.0F : -9.0F;
                add(&b, ENG_DECOR_SANDSTONE, true, x, z, 0.7F, 0.9F, 3.5F, 0.0F, 0.0F);
            }
            break;
        case ENG_BIOME_VOLCANO: /* pics de basalte, brasiers sur leurs socles */
        default:
            ring(&b, ENG_DECOR_BASALT, 8U, 24.0F, 1.6F, 6.0F, 1.2F);
            for (int i = 0; i < 4; i++) {
                const float a = 0.7853982F + (1.5707963F * (float)i);
                const float x = 15.0F * cosf(a);
                const float z = 15.0F * sinf(a);
                add(&b, ENG_DECOR_BASALT, true, x, z, 0.6F, 0.7F, 0.6F, 0.0F, 0.0F);
                add(&b, ENG_DECOR_FIRE, false, x, z, 0.45F, 0.35F, 0.45F, 1.4F, 0.5F);
            }
            break;
        }
    } else if (zone->kind == ENG_MAPZONE_FARM_HARD) { /* palissade en arc, grand feu */
        for (int i = 0; i < 9; i++) {
            const float a = 2.2F + (0.24F * (float)i); /* arc du côté est */
            add(&b, ENG_DECOR_WOOD, true, 13.0F * cosf(a + 3.1415927F),
                13.0F * sinf(a + 3.1415927F), 0.35F, 1.4F + (0.2F * (float)(i % 2)), 0.35F, 0.0F,
                0.0F);
        }
        campfire(&b, 9.0F, 0.0F, 1.6F);
    } else { /* campement : trois tentes autour d'un feu, à l'écart des ennemis */
        tent(&b, 10.0F, -6.0F);
        tent(&b, 12.0F, 2.0F);
        tent(&b, 8.0F, 8.0F);
        campfire(&b, 8.0F, 0.5F, 1.0F);
    }
    return b.count;
}

uint32_t eng_camp_decor(EngDecorBox *out, uint32_t max)
{
    Builder b = {out, max, 0};
    if (out == NULL) {
        return 0U;
    }
    tent(&b, -4.0F, -3.0F);
    add(&b, ENG_DECOR_CLOTH, false, -1.2F, -2.2F, 0.45F, 0.06F, 1.0F, 0.0F, 0.3F); /* couchage */
    campfire(&b, 0.5F, -3.5F, 1.0F);
    add(&b, ENG_DECOR_WOOD, true, -3.0F, 2.5F, 0.5F, 0.5F, 0.5F, 0.0F, 0.0F); /* caisses */
    add(&b, ENG_DECOR_WOOD, true, -4.1F, 2.8F, 0.4F, 0.4F, 0.4F, 0.0F, 0.0F);
    add(&b, ENG_DECOR_WOOD, false, -3.4F, 2.6F, 0.35F, 0.35F, 0.35F, 1.0F, 0.5F);
    return b.count;
}
