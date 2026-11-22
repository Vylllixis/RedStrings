#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/combat_geom.h"
#include "engine/hurtbox.h"

/* Détection et résolution des coups (M5-07). Pour UNE attaque active, on balaie les capsules
 * d'arme (position au tick précédent -> au tick courant) contre les hurtboxes des autres
 * combattants (idem) et on rend la liste des touches de ce tick.
 *
 * Garanties :
 *  - une attaque ne touche JAMAIS deux fois la même cible (EngHitTracker : un masque de combattants
 *    déjà touchés, remis à zéro au début de chaque attaque) ;
 *  - ordre de résolution DÉTERMINISTE : les cibles sont traitées par indice de combattant
 * croissant, quel que soit l'ordre du tableau fourni ; pour une cible, la touche retenue est la
 * plus précoce dans le tick, puis (à égalité) la hurtbox d'indice le plus bas, puis l'arme d'indice
 *    le plus bas ;
 *  - une cible non touchable (invulnérable, morte) est sautée SANS être marquée : si elle redevient
 *    touchable avant la fin de l'attaque, elle peut être touchée ;
 *  - un combattant ne se touche pas lui-même.
 * Voir engine/combat_geom.h pour le choix d'un test analytique plutôt que des requêtes Jolt. */

#define ENG_HIT_MAX_FIGHTERS 32U

typedef struct EngHitTracker {
    uint32_t attack_id; /* identifiant de l'instance d'attaque (informatif) */
    uint32_t hit_mask;  /* bit i : le combattant i a déjà été touché par cette attaque */
} EngHitTracker;

typedef struct EngHitTarget {
    uint32_t fighter;        /* indice du combattant, < ENG_HIT_MAX_FIGHTERS */
    bool hittable;           /* false : invulnérable ou mort (voir plus haut) */
    uint32_t count;          /* nombre de hurtboxes */
    const EngCapsule *prev;  /* capsules monde au tick précédent (count éléments) */
    const EngCapsule *cur;   /* au tick courant */
    const EngHitZone *zones; /* zone de chaque hurtbox */
} EngHitTarget;

typedef struct EngHit {
    uint32_t attacker;
    uint32_t defender;
    uint32_t hurtbox; /* indice dans la cible */
    uint32_t weapon;  /* indice de l'arme */
    EngHitZone zone;
    float time;     /* instant dans le tick, 0 à 1 */
    float point[3]; /* point de contact (monde) */
} EngHit;

void eng_hittracker_reset(EngHitTracker *t, uint32_t attack_id);
bool eng_hittracker_has_hit(const EngHitTracker *t, uint32_t fighter);

/* Résout un tick. `weapons_prev` / `weapons_cur` : `weapon_count` capsules d'arme. Retourne le
 * nombre de touches écrites dans `out` (au plus `max`) ; les cibles touchées sont marquées dans
 * `tracker` MÊME si `out` est trop petit pour toutes les contenir (la touche est consommée : le
 * nombre retourné est tronqué à `max`, mais `*total` si non NULL reçoit le nombre réel).
 * Arguments invalides (NULL, indice de combattant hors bornes, doublon d'indice parmi les
 * cibles) : 0 touche, tracker inchangé. */
uint32_t eng_hit_resolve(uint32_t attacker, const EngCapsule *weapons_prev,
                         const EngCapsule *weapons_cur, uint32_t weapon_count,
                         const EngHitTarget *targets, uint32_t target_count, EngHitTracker *tracker,
                         EngHit *out, uint32_t max, uint32_t *total);
