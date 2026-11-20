#ifndef ENG_CREATURE_H
#define ENG_CREATURE_H

/* Créatures en blocs (demande de Théo, 2026-11-20, question Q13) : le cheval du Chevalier noir et
 * le dragon final sont construits en code, faute de modèles CC0 utilisables : une liste de boîtes
 * (centre, demi-dimensions, rotation, couleur) dans le repère de la créature, animée par des
 * fonctions pures du temps. Le lanceur les dessine avec une boîte unité par couleur.
 *
 * Repère de la créature : pieds à y = 0, avant vers -Z (comme les personnages : lacet 0 regarde
 * -Z), +X à droite, mètres. */

#include <stdint.h>

#define ENG_CREATURE_MAX_PARTS 40U

typedef enum EngCreatureColor {
    ENG_CREATURE_BROWN = 0, /* robe du cheval */
    ENG_CREATURE_DARK,      /* crinière, sabots, ailes, cendre */
    ENG_CREATURE_RED,       /* écailles du dragon */
    ENG_CREATURE_ORANGE,    /* ventre, flammes */
    ENG_CREATURE_BONE,      /* cornes, griffes, dents */
    ENG_CREATURE_YELLOW,    /* yeux, cœur de la flamme */
    ENG_CREATURE_COLORS
} EngCreatureColor;

typedef struct EngCreaturePart {
    float pos[3];  /* centre */
    float half[3]; /* demi-dimensions (x, y, z) */
    float
        rot[3]; /* rotations en radians : tangage (X), lacet (Y), roulis (Z), appliquées Y, X, Z */
    uint8_t color; /* EngCreatureColor */
} EngCreaturePart;

/* Cheval (2,4 m de long, dos à 1,5 m) : `time` en secondes, `gait` [0, 1] l'allure (0 arrêté, 1 au
 * galop : les jambes battent), `fall` [0, 1] l'effondrement (1 : couché sur le flanc). Renvoie le
 * nombre de boîtes écrites (au plus `max`). Arguments non finis : traités comme 0. */
uint32_t eng_creature_horse(float time, float gait, float fall, EngCreaturePart *out, uint32_t max);

/* Hauteur de la selle au-dessus du sol (m) pour le même `fall` : où poser le cavalier. */
float eng_creature_horse_saddle(float fall);

/* Dragon (corps de 4 m, 8 m d'envergure, tête à 3,4 m) : `time` en secondes (ailes, queue, cou),
 * `bite` [0, 1] la gueule qui s'ouvre et le cou qui frappe, `flame` [0, 1] le jet de flammes devant
 * la gueule (jusqu'à 9 m). Renvoie le nombre de boîtes écrites (au plus `max`). */
uint32_t eng_creature_dragon(float time, float bite, float flame, EngCreaturePart *out,
                             uint32_t max);

#endif
