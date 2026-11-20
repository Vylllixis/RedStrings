#ifndef ENG_ZONE_DECOR_H
#define ENG_ZONE_DECOR_H

/* Décor des zones de la grande carte (demande de Théo, 2026-11-20 : « une zone stylée de boss »,
 * le château du chevalier, un camp de réveil) : des boîtes posées sur le relief, construites en
 * code comme le cheval et le dragon (engine/creature.h). Logique pure : la liste des boîtes d'une
 * zone, relative à son centre ; le lanceur les pose sur le sol, les dessine et crée les corps
 * physiques des boîtes solides.
 *
 *   boss de la plaine   : le CHÂTEAU du chevalier : cour carrée de 44 m, murs, tours d'angle, porte
 *                         à l'ouest (d'où vient le joueur) et à l'est (vers la montagne), donjon ;
 *   boss de la montagne : un cercle de piliers de glace autour d'un autel ;
 *   boss du désert      : une arène en ruine : colonnes de grès et pans de mur ;
 *   boss du volcan      : des pics de basalte et des brasiers autour de l'antre ;
 *   zones de farm       : un campement (tentes, feu) ou une palissade (zone dure) ;
 *   camp de réveil      : tente, couchage, feu, caisses. */

#include <stdbool.h>
#include <stdint.h>

#include "engine/map_zones.h"

#define ENG_DECOR_MAX 64U
#define ENG_DECOR_CASTLE_HALF 22.0F /* demi-côté de la cour du château (m) */
#define ENG_DECOR_GATE_WIDTH 4.8F   /* largeur de la porte (m) */

typedef enum EngDecorColor {
    ENG_DECOR_STONE = 0, /* murs du château */
    ENG_DECOR_ROOF,      /* toits, ardoise sombre */
    ENG_DECOR_WOOD,      /* palissades, caisses, bûches */
    ENG_DECOR_CLOTH,     /* tentes, bannières */
    ENG_DECOR_ICE,       /* piliers de la montagne */
    ENG_DECOR_SANDSTONE, /* ruines du désert */
    ENG_DECOR_BASALT,    /* pics du volcan */
    ENG_DECOR_FIRE,      /* feux et brasiers (émissif) */
    ENG_DECOR_COLORS
} EngDecorColor;

typedef struct EngDecorBox {
    float x; /* centre, mètres, relatif au centre de la zone (+X : est) */
    float z;
    float half[3]; /* demi-dimensions (x, hauteur, z) */
    float lift;    /* hauteur du bas de la boîte au-dessus du sol */
    float yaw;     /* radians ; toujours 0 pour une boîte solide */
    uint8_t color; /* EngDecorColor */
    bool solid;    /* bloque le joueur et les ennemis (alignée sur les axes) */
} EngDecorBox;

/* Boîtes du décor de `zone` (au plus `max`, au plus ENG_DECOR_MAX). NULL : 0. */
uint32_t eng_zone_decor(const EngMapZone *zone, EngDecorBox *out, uint32_t max);
/* Boîtes du camp de réveil, autour du point de départ. */
uint32_t eng_camp_decor(EngDecorBox *out, uint32_t max);

#endif
