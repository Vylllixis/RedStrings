#ifndef ENG_BOSS_SPECIAL_H
#define ENG_BOSS_SPECIAL_H

/* Boss spéciaux de la grande carte (demande de Théo, 2026-11-20) : logique pure, sans rendu.
 *
 *  - Le cavalier (Chevalier noir, plaine) : il arrive à cheval. Le cheval porte les premiers 40 %
 *    des PV du boss ; quand ils sont partis, cinématique de 4 s (le cheval s'effondre, le chevalier
 *    tombe puis se relève : personne ne peut frapper), puis combat à pied avec les 60 % restants.
 *  - Le dragon (volcan) : en plus de son corps à corps, il crache des flammes : il charge (la
 *    gueule s'ouvre : c'est le télégraphe), brûle un cône devant lui, puis récupère. */

#include <stdbool.h>
#include <stdint.h>

typedef enum EngBossKind {
    ENG_BOSSKIND_NONE = 0, /* boss ordinaire (ou pas de boss) */
    ENG_BOSSKIND_RIDER,    /* le cavalier : cheval, cinématique, à pied */
    ENG_BOSSKIND_DRAGON    /* le dragon : griffes, morsure, flammes */
} EngBossKind;

/* ── Le cavalier ── */
#define ENG_RIDER_HORSE_SHARE 0.4F /* part des PV du boss portée par le cheval */
#define ENG_RIDER_FALL_TICKS 240U  /* durée de la cinématique (4 s à 60 Hz) */
#define ENG_RIDER_SPEED 1.5F       /* à cheval : multiplie la vitesse de marche */

typedef enum EngRiderPhase {
    ENG_RIDER_MOUNTED = 0, /* à cheval */
    ENG_RIDER_FALLING,     /* cinématique : le cheval tombe, le chevalier se relève */
    ENG_RIDER_ON_FOOT      /* à pied */
} EngRiderPhase;

typedef struct EngRider {
    EngRiderPhase phase;
    uint32_t ticks; /* ticks passés dans la phase FALLING */
} EngRider;

void eng_rider_init(EngRider *r);

/* Un pas (60 Hz). `hp` : PV du boss après les coups du pas ; ils sont bornés pour qu'un coup ne
 * dépasse pas la mort du cheval (et tenus pendant la cinématique). Renvoie true au pas où le cheval
 * meurt. Arguments invalides (NULL, max_hp non fini ou <= 0) : false, rien ne change. */
bool eng_rider_step(EngRider *r, float *hp, float max_hp);

/* La cinématique est en cours : ni le joueur ni le boss n'agissent ni ne prennent de coup. */
bool eng_rider_cinematic(const EngRider *r);
/* Effondrement du cheval : 0 debout, 1 couché (atteint en 1,5 s de cinématique). */
float eng_rider_horse_fall(const EngRider *r);
/* Le chevalier est à terre (entre 0,5 s et 2,5 s de cinématique) : avant de se relever. */
bool eng_rider_down(const EngRider *r);
/* PV du cheval, de 0 à 1, pour la barre de vie. */
float eng_rider_horse_hp(const EngRider *r, float hp, float max_hp);

/* ── Le souffle du dragon ── */
#define ENG_FLAME_CHARGE_TICKS 70U    /* la gueule s'ouvre : 1,2 s pour s'écarter */
#define ENG_FLAME_BURN_TICKS 110U     /* le cône brûle */
#define ENG_FLAME_COOLDOWN_TICKS 300U /* 5 s avant le prochain souffle */
#define ENG_FLAME_HIT_EVERY 12U       /* une brûlure tous les 12 ticks : 9 au plus par souffle */

typedef enum EngFlameState {
    ENG_FLAME_IDLE = 0,
    ENG_FLAME_CHARGE,
    ENG_FLAME_BURN,
    ENG_FLAME_COOLDOWN
} EngFlameState;

typedef struct EngFlame {
    EngFlameState state;
    uint32_t ticks; /* ticks passés dans l'état */
} EngFlame;

void eng_flame_init(EngFlame *f);
/* Un pas. `can_start` : le dragon est libre et sa cible à portée de souffle (un souffle ne commence
 * que depuis IDLE). Renvoie true aux pas où le cône brûle ce qu'il touche. */
bool eng_flame_step(EngFlame *f, bool can_start);
/* Le dragon charge ou crache : il ne bouge plus et ne mord pas. */
bool eng_flame_busy(const EngFlame *f);
/* Pour le dessin : charge de 0 à 1 (gueule qui s'ouvre), jet de 0 à 1 (longueur des flammes). */
float eng_flame_charge(const EngFlame *f);
float eng_flame_jet(const EngFlame *f);
/* La cible est-elle dans le cône ? `src` et `target` en (x, z), `yaw` le lacet du dragon (0 regarde
 * -Z), entre `near_m` et `far_m` de distance, à moins de `half_angle` radians de l'axe. Valeurs non
 * finies : false. */
bool eng_flame_reaches(const float src[2], float yaw, const float target[2], float near_m,
                       float far_m, float half_angle);

#endif
