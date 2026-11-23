#ifndef ENG_EQUIPMENT_H
#define ENG_EQUIPMENT_H

/* Équipement du joueur (M8-08) : les armes possédées (un masque de bits) et celle qui est en main.
 * Une arme = un jeu d'attaques (noms dans assets/combat/attacks.txt), un modèle tenu et l'option
 * d'un bouclier. Le jeu commence avec l'épée et le bouclier ; l'espadon se gagne (camp vaincu). */

#include <stdbool.h>
#include <stdint.h>

typedef enum EngWeaponId {
    ENG_WEAPON_SWORD = 0,  /* épée et bouclier : enchaînement de 3 coups, lourde = estoc */
    ENG_WEAPON_GREATSWORD, /* espadon à deux mains : coups lents, plus forts, plus longs */
    ENG_WEAPON_COUNT
} EngWeaponId;

typedef struct EngWeaponDef {
    const char *label; /* nom affiché */
    const char *light; /* attaque légère (début de chaîne) */
    const char *heavy; /* attaque lourde */
    bool two_handed;   /* pas de bouclier */
} EngWeaponDef;

typedef struct EngEquipment {
    uint32_t owned;    /* bit i : arme i possédée */
    uint32_t equipped; /* EngWeaponId en main (toujours possédée) */
} EngEquipment;

void eng_equip_init(EngEquipment *e);            /* l'épée et le bouclier, en main */
const EngWeaponDef *eng_weapon_def(uint32_t id); /* NULL hors bornes */
bool eng_equip_owns(const EngEquipment *e, uint32_t id);
/* Donne une arme. true si elle est nouvelle. */
bool eng_equip_give(EngEquipment *e, uint32_t id);
/* Met en main une arme possédée. false (rien ne change) sinon. */
bool eng_equip_select(EngEquipment *e, uint32_t id);
/* Arme possédée suivante (en boucle), mise en main ; renvoie son identifiant. */
uint32_t eng_equip_next(EngEquipment *e);
/* Cohérent : seulement des armes connues, au moins une, celle en main possédée. */
bool eng_equip_valid(const EngEquipment *e);

#endif
