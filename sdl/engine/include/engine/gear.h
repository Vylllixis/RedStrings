#ifndef ENG_GEAR_H
#define ENG_GEAR_H

/* Butin des boss de la grande carte (demande de Théo, 2026-11-20) : chaque boss donne une arme, une
 * armure et un bouclier. Un seul « palier » d'équipement, de 0 (départ) à 4 (butin du dragon) : le
 * palier le plus haut obtenu vaut pour les trois pièces. Il multiplie les dégâts infligés (arme),
 * réduit les dégâts reçus (armure) et ce que la garde laisse passer (bouclier). Rien n'est
 * verrouillé : on peut battre les boss dans le désordre, le palier ne redescend jamais. */

#include <stdbool.h>
#include <stdint.h>

#define ENG_GEAR_TIERS 5U /* paliers 0 à 4 */

typedef enum EngGearSlot {
    ENG_GEAR_WEAPON = 0,
    ENG_GEAR_ARMOR = 1,
    ENG_GEAR_SHIELD = 2,
    ENG_GEAR_SLOTS
} EngGearSlot;

/* Palier borné à [0, ENG_GEAR_TIERS - 1]. */
uint32_t eng_gear_clamp(uint32_t tier);
/* Dégâts infligés x : 1 ; 1,35 ; 1,8 ; 2,4 ; 3,2. */
float eng_gear_damage(uint32_t tier);
/* Dégâts reçus x : 1 ; 0,8 ; 0,65 ; 0,5 ; 0,4. */
float eng_gear_armor(uint32_t tier);
/* Part des dégâts qui traverse la garde : 0,2 ; 0,15 ; 0,1 ; 0,06 ; 0,03. */
float eng_gear_block(uint32_t tier);
/* Nom de la pièce (« Épée du chevalier », « Armure du givre »...) ; jamais NULL. */
const char *eng_gear_name(uint32_t tier, EngGearSlot slot);
/* Nom court du palier (« de départ », « du chevalier », « du givre », « des sables », « du
 * dragon »). */
const char *eng_gear_tier_name(uint32_t tier);
/* Donne le palier `reward` : *tier devient le plus haut des deux. true s'il a monté. */
bool eng_gear_give(uint32_t *tier, uint32_t reward);

#endif
