#ifndef LAUNCHER_PLAY_BOSS_H
#define LAUNCHER_PLAY_BOSS_H

/* Boss spéciaux de la grande carte, côté jeu (logique pure : engine/boss_special.h, formes :
 * engine/creature.h) :
 *  - le Chevalier noir arrive À CHEVAL : plus rapide, plus large ; le cheval mort, cinématique (le
 *    cheval se couche, le chevalier tombe et se relève), puis combat à pied ;
 *  - le Dragon de lave, colossal (treize fois le modèle : tête à 44 m, 160 m d'envergure en vol ;
 *    demande de Théo du 2026-11-22 : dix fois plus grand, et il vole). Au sol : on passe sous son
 *    ventre, on frappe ses PATTES (et sa tête quand elle est au sol : après une morsure, pendant
 *    qu'il souffle) ; il reste au centre de son antre, pivote lentement vers le joueur, piétine
 *    ce qui est près de ses pattes, mord loin devant lui, souffle un jet de flammes au ras du
 *    sol. Puis il DÉCOLLE (engine/dragon_flight.h), tourne au-dessus de son antre, fond deux fois
 *    sur le joueur en brûlant une bande de sol, et revient se poser (onde de choc).
 * Ce module ne connaît ni le rendu ni l'IA : il agit sur la simulation de combat. */

#include <cglm/cglm.h>
#include <stdbool.h>
#include <stdint.h>

#include "engine/boss_special.h"
#include "engine/combat_sim.h"
#include "engine/creature.h"
#include "engine/dragon_flight.h"

#define PLAY_DRAGON_SCALE 13.0F       /* le modèle du dragon est dessiné treize fois plus grand */
#define PLAY_DRAGON_DOMAIN 230.0F     /* m autour de l'antre : au-delà, il laisse le joueur */
#define PLAY_DRAGON_FIRE_LEAD 140.0F  /* m : en vol, son feu touche le sol 140 m devant lui */
#define PLAY_DRAGON_FIRE_RADIUS 11.0F /* m autour de ce point : brûlé */

typedef struct PlayBoss {
    EngBossKind kind; /* ENG_BOSSKIND_NONE : rien */
    uint32_t fighter; /* indice du boss dans la simulation */
    EngRider rider;
    EngFlame flame;
    float base_speed; /* vitesse et rayon du combattant hors monture */
    float base_radius;
    float flame_damage; /* dégâts d'une brûlure, avant armure */
    float time;         /* s : animation des créatures */
    float gait;         /* allure du cheval, lissée [0, 1] */
    float bite;         /* gueule du dragon, lissée [0, 1] */
    float lunge;        /* dragon : où sa tête s'abat, de ses pieds (0) au bout de son cou (1) */
    float prey;         /* dragon : distance du joueur à son centre au dernier pas (m) */
    float rear;         /* dragon cabré, lissé [0, 1] */
    float jet;          /* dragon : longueur du jet de flammes, lissée [0, 1] */
    float yaw;          /* dragon : son cap (il pivote lentement) */
    float home[2];      /* dragon : centre de l'antre (x, z) */
    EngDragonFlight flight;
    EngDragonPose pose;  /* dragon : la pose du pas (dessin, hurtboxes et coups en sortent) */
    uint32_t fire_ticks; /* dragon en vol : ticks de feu du passage en cours */
    uint32_t flights;    /* décollages depuis le début (journal, autotest) */
    float horse_pos[2];  /* où le cheval est tombé (x, z) */
    float horse_yaw;
    uint32_t burns; /* brûlures infligées depuis le début (journal, autotest) */
} PlayBoss;

/* Ce qui s'est passé pendant un pas (play_boss_after_step). */
typedef struct PlayBossEvent {
    bool horse_died;   /* le cheval vient de mourir : la cinématique commence */
    bool dismounted;   /* la cinématique vient de finir : combat à pied */
    float burn_dealt;  /* PV retirés au joueur par les flammes ce pas (0 : aucun) */
    float shock_dealt; /* PV retirés par l'onde de choc de l'atterrissage du dragon */
    bool took_off;     /* le dragon vient de décoller */
    bool landed;       /* le dragon vient de se poser */
} PlayBossEvent;

void play_boss_clear(PlayBoss *b);
/* Le boss `fighter` (ré)apparaît : monture ou dragon selon `kind`. `damage_scale` : dégâts de la
 * zone (pour les flammes). Après eng_aig_reset_agent (qui remet vitesse et rayon de l'archétype).
 */
void play_boss_begin(PlayBoss *b, EngBossKind kind, uint32_t fighter, EngCombatSim *sim,
                     float damage_scale);
/* Après l'IA, avant le pas : fige le boss et le joueur pendant la cinématique (true), le dragon
 * pendant son souffle. */
bool play_boss_before_step(PlayBoss *b, EngCombatSim *sim, EngFighterInput *in, uint32_t player);
/* Hurtboxes et armes du dragon pour le pas à venir (rien pour les autres). */
void play_boss_rig(PlayBoss *b, EngCombatSim *sim);
/* Après le pas : phases du cavalier ; dragon : pivot, pattes, souffle, vol (`engaged` : l'IA a vu
 * le joueur, pour le cavalier ; le dragon, lui, traque tout joueur entré dans son domaine ;
 * `invincible` : mode créatif, les flammes ne retirent rien). */
void play_boss_after_step(PlayBoss *b, EngCombatSim *sim, uint32_t player, bool engaged,
                          bool invincible, PlayBossEvent *out);
bool play_boss_cinematic(const PlayBoss *b);
/* Le dragon remplace le personnage : son combattant n'est pas dessiné. */
bool play_boss_hides_fighter(const PlayBoss *b, uint32_t fighter);
/* Hauteur (m) dont le cavalier est soulevé par sa monture ; 0 à pied. */
float play_boss_rider_lift(const PlayBoss *b, uint32_t fighter);
/* Le chevalier est à terre pendant la cinématique (animation de chute). */
bool play_boss_rider_down(const PlayBoss *b, uint32_t fighter);
/* Où dessiner la créature (x, z interpolés, lacet). false : aucune créature. */
bool play_boss_anchor(const PlayBoss *b, const EngCombatSim *sim, float alpha, float xz[2],
                      float *yaw);
/* Boîtes de la créature en repère monde (boîte unité -> monde) et leur couleur. */
uint32_t play_boss_boxes(const PlayBoss *b, const EngCombatSim *sim, const float feet[3], float yaw,
                         mat4 *places, uint8_t *colors, uint32_t max);
/* Recentrage de l'origine du monde. */
void play_boss_shift(PlayBoss *b, float dx, float dz);
/* Dragon : altitude de ses pieds au-dessus du sol de son antre (m ; 0 au sol), et s'il vole. */
float play_boss_altitude(const PlayBoss *b);
bool play_boss_airborne(const PlayBoss *b);
/* Dragon en vol qui souffle : point du sol que son feu brûle (x, z). false sinon. */
bool play_boss_fire_point(const PlayBoss *b, float out[2]);
/* Dragon vivant, joueur vivant dans son domaine (à moins de PLAY_DRAGON_DOMAIN m du centre de
 * l'antre) : il le traque, au sol comme en vol. Sa taille lui fait voir par-dessus tout décor. */
bool play_boss_hunting(const PlayBoss *b, const EngCombatSim *sim, uint32_t player);
/* Dragon en passage de feu, axe figé : il fond sur le point qu'il a visé. */
bool play_boss_diving(const PlayBoss *b);

#endif
