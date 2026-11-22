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

#define ENG_CREATURE_MAX_PARTS 48U

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

/* Dragon. Le modèle est écrit à l'échelle 1 (corps de 4 m, 9 m d'envergure au sol, tête à 3,4 m) ;
 * le jeu le dessine treize fois plus grand (launcher/src/play_boss.c) : dix fois le dragon d'avant
 * (demande de Théo, 2026-11-22), 52 m de corps, tête à 44 m, 160 m d'envergure en vol.
 *
 * Tous les champs sont bornés : une valeur non finie compte pour 0, hors bornes elle est ramenée
 * dans les bornes. */
typedef struct EngDragonPose {
    float time;   /* s : respiration, queue, cou, battement des ailes */
    float bite;   /* [0, 1] : la gueule s'ouvre, la tête s'abat JUSQU'AU SOL */
    float lunge;  /* [0, 1] : où la tête s'abat : 0 à ses pieds (ENG_DRAGON_BITE_NEAR devant son
                   * centre), 1 au bout de son cou (ENG_DRAGON_BITE_FAR) */
    float rear;   /* [0, 1] : il se cabre, pattes avant levées (elles retombent pour écraser) */
    float flame;  /* [0, 1] : longueur du jet de flammes (1 : ENG_DRAGON_JET) */
    float aim;    /* rad, [0 ; 1,4] : le jet plonge de cet angle sous l'horizontale (vol) */
    float flight; /* [0, 1] : 0 au sol ; 1 en vol : pattes repliées, cou tendu, ailes déployées */
    float walk;   /* rad : phase du pas (les pattes se lèvent tour à tour, en diagonale) */
    float stride; /* [0, 1] : amplitude du pas (0 : à l'arrêt) */
} EngDragonPose;

#define ENG_DRAGON_JET 9.5F /* longueur du jet de flammes à `flame` = 1, repère du modèle */
#define ENG_DRAGON_BITE_NEAR 2.3F /* la tête s'abat de 2,3 m (lunge 0) à 5,6 m (lunge 1) devant */
#define ENG_DRAGON_BITE_FAR 5.6F  /* son centre, repère du modèle */
#define ENG_DRAGON_PAWS                                                                            \
    4U /* 0 avant gauche, 1 avant droite, 2 arrière gauche, 3 arrière droite                     \
        */

/* Écrit les boîtes du dragon dans cette pose (au plus `max`) ; `pose` NULL : au repos. Renvoie
 * leur nombre. Les boîtes du jet de flammes sont les dernières. */
uint32_t eng_creature_dragon(const EngDragonPose *pose, EngCreaturePart *out, uint32_t max);

/* Repères du même modèle, pour que les coups suivent le corps dessiné (repère de la créature) :
 * centre de la tête ; gueule (d'où part le jet) ; dessous du pied `paw` (0 à 3 ; à y = 0 quand il
 * est posé). `out` NULL ou `paw` hors bornes : rien n'est écrit. */
void eng_creature_dragon_head(const EngDragonPose *pose, float out[3]);
void eng_creature_dragon_mouth(const EngDragonPose *pose, float out[3]);
void eng_creature_dragon_paw(const EngDragonPose *pose, uint32_t paw, float out[3]);

#endif
