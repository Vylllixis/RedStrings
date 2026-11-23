#ifndef ENG_DRAGON_FLIGHT_H
#define ENG_DRAGON_FLIGHT_H

/* Vol du dragon (demande de Théo, 2026-11-22 : « donne-lui la possibilité de voler »).
 *
 * Logique pure, sans rendu ni simulation : une position (x, z), un cap, une altitude, mis à jour
 * 60 fois par seconde. Le dragon vole comme un planeur lourd : vitesse constante, virages limités
 * (ENG_DRAGON_TURN_RATE), jamais de demi-tour sur place.
 *
 *   AU SOL      il marche et se bat (l'IA ordinaire le déplace) ; il décolle après
 *               ENG_DRAGON_GROUND_FIGHT_TICKS de combat, ou ENG_DRAGON_REST_TICKS sans cible
 *   DÉCOLLAGE   il s'élève en accélérant droit devant lui
 *   RONDE       il tourne autour de son antre, à ENG_DRAGON_ORBIT_RADIUS, en croisière
 *   PASSAGE     il fond sur sa cible : trop près d'elle, il s'en éloigne d'abord (prise d'élan),
 *               puis revient ; à l'approche il FIGE le point visé (la cible peut alors s'écarter
 *               de son axe), descend et souffle tant que son feu, qui touche le sol `fire_lead`
 *               mètres devant lui, est à moins de ENG_DRAGON_FIRE_SPAN du point visé
 *   RETOUR      après ses passages (un par vol ; l'appelant peut en demander plus : le dragon
 *               blessé en fait deux) ou sa ronde sans cible, il revient à l'antre
 *   ATTERRISSAGE il ralentit, descend et se pose au centre de l'antre (`landed` vrai un tick)
 *
 * Avant = (-sin yaw, -cos yaw), comme les corps de la simulation de combat. Tests :
 * tests/test_dragon_flight.c. */

#include <stdbool.h>
#include <stdint.h>

#define ENG_DRAGON_CRUISE_ALT 75.0F         /* m au-dessus du sol de l'antre, en croisière */
#define ENG_DRAGON_STRAFE_ALT 36.0F         /* m pendant un passage de feu */
#define ENG_DRAGON_SPEED 42.0F              /* m/s en vol */
#define ENG_DRAGON_TURN_RATE 0.75F          /* rad/s : rayon de virage de 56 m */
#define ENG_DRAGON_CLIMB_RATE 14.0F         /* m/s : montée et descente en vol */
#define ENG_DRAGON_ORBIT_RADIUS 150.0F      /* m : ronde autour de l'antre */
#define ENG_DRAGON_FIRE_SPAN 60.0F          /* m : il souffle de 60 m avant à 60 m après le but */
#define ENG_DRAGON_TAKEOFF_TICKS 180U       /* 3 s */
#define ENG_DRAGON_LANDING_TICKS 240U       /* 4 s */
#define ENG_DRAGON_GROUND_FIGHT_TICKS 2100U /* 35 s de combat au sol avant de redécoller */
#define ENG_DRAGON_REST_TICKS 1200U         /* sans cible : 20 s au sol entre deux rondes */
#define ENG_DRAGON_PATROL_TICKS 1800U       /* sans cible : 30 s de ronde */
#define ENG_DRAGON_PATROL_MIN_TICKS 150U    /* 2,5 s de ronde au moins avant un passage */
#define ENG_DRAGON_PASSES 1U                /* passages de feu par vol, sauf demande */
#define ENG_DRAGON_PASSES_MAX 3U            /* au plus, quoi qu'on demande */
#define ENG_DRAGON_STRAFE_TIMEOUT 1500U     /* 25 s : un passage qui n'aboutit pas est abandonné */

typedef enum EngDragonPhase {
    ENG_DRAGON_GROUND = 0,
    ENG_DRAGON_TAKEOFF,
    ENG_DRAGON_PATROL,
    ENG_DRAGON_STRAFE,
    ENG_DRAGON_RETURN,
    ENG_DRAGON_LANDING
} EngDragonPhase;

typedef struct EngDragonFlight {
    EngDragonPhase phase;
    uint32_t ticks;  /* ticks passés dans la phase */
    uint32_t passes; /* passages de feu terminés pendant ce vol */
    float pos[2];    /* x, z (m) */
    float yaw;       /* cap (rad) */
    float altitude;  /* m au-dessus du sol de l'antre ; 0 au sol */
    float speed;     /* m/s */
    float bank;      /* roulis d'affichage (rad) : il penche dans ses virages */
    float aim[2];    /* passage : point visé, figé à l'approche */
    float from_alt;  /* atterrissage : altitude au début de la descente */
    bool aim_locked;
    bool extending; /* passage : trop près de sa cible, il prend de l'élan avant de revenir */
    bool flame;     /* il souffle (passage, feu près du point visé) */
    bool landed;    /* vrai pendant le seul tick où il touche le sol */
} EngDragonFlight;

typedef struct EngDragonFlightInput {
    float home[2];     /* centre de l'antre */
    float target[2];   /* sa cible (le joueur) */
    float ground[2];   /* au sol : où la simulation l'a mis (il marche) */
    float ground_yaw;  /* au sol : son cap dans la simulation */
    float fire_lead;   /* m : distance, devant lui, où son feu touche le sol en passage */
    uint32_t passes;   /* passages de feu de ce vol ; 0 : ENG_DRAGON_PASSES ; borné au maximum */
    bool has_target;   /* la cible est dans son domaine, vivante */
    bool can_take_off; /* au sol : libre de décoller (ni attaque ni souffle en cours) */
} EngDragonFlightInput;

/* Au sol, à l'arrêt, en (pos, yaw). Valeurs non finies : (0, 0), cap 0. */
void eng_dragon_flight_init(EngDragonFlight *d, const float pos[2], float yaw);

/* Avance d'un tick (1/60 s). Retourne true s'il est EN L'AIR après ce tick : l'appelant place
 * alors son corps en `pos`, `yaw`, `altitude`. Au sol (false), il suit `in->ground`. Entrée non
 * finie : la cible est ignorée (comme `has_target` faux) ; `d` ou `in` NULL : false. */
bool eng_dragon_flight_step(EngDragonFlight *d, const EngDragonFlightInput *in);

bool eng_dragon_flight_airborne(const EngDragonFlight *d);

/* Il est mort en vol : il tombe (un tick), sans plus rien décider. Retourne true le tick où il
 * touche le sol (il est alors AU SOL, altitude 0, sans flamme). Déjà au sol : false. */
bool eng_dragon_flight_fall(EngDragonFlight *d);

/* L'origine flottante du monde a bougé : position et point visé suivent. */
void eng_dragon_flight_shift(EngDragonFlight *d, float dx, float dz);

/* Point du sol que son feu touche en passage : `lead` mètres devant lui. */
void eng_dragon_flight_fire_point(const EngDragonFlight *d, float lead, float out[2]);

const char *eng_dragon_phase_name(EngDragonPhase phase);

#endif
