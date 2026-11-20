#ifndef LAUNCHER_PLAY_BOSS_H
#define LAUNCHER_PLAY_BOSS_H

/* Boss spéciaux de la grande carte, côté jeu (logique pure : engine/boss_special.h, formes :
 * engine/creature.h) :
 *  - le Chevalier noir arrive À CHEVAL : plus rapide, plus large ; le cheval mort, cinématique (le
 *    cheval se couche, le chevalier tombe et se relève), puis combat à pied ;
 *  - le Dragon de lave : corps et gueule en boîtes ; ses hurtboxes et ses coups (griffes, morsure)
 *    suivent ces boîtes ; il crache un cône de flammes.
 * Ce module ne connaît ni le rendu ni l'IA : il agit sur la simulation de combat. */

#include <cglm/cglm.h>
#include <stdbool.h>
#include <stdint.h>

#include "engine/boss_special.h"
#include "engine/combat_sim.h"
#include "engine/creature.h"

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
    float horse_pos[2]; /* où le cheval est tombé (x, z) */
    float horse_yaw;
    uint32_t burns; /* brûlures infligées depuis le début (journal, autotest) */
} PlayBoss;

/* Ce qui s'est passé pendant un pas (play_boss_after_step). */
typedef struct PlayBossEvent {
    bool horse_died;  /* le cheval vient de mourir : la cinématique commence */
    bool dismounted;  /* la cinématique vient de finir : combat à pied */
    float burn_dealt; /* PV retirés au joueur par les flammes ce pas (0 : aucun) */
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
/* Après le pas : phases du cavalier, souffle du dragon (`engaged` : il a vu le joueur ;
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

#endif
