#ifndef ENG_DAMAGE_NUMBERS_H
#define ENG_DAMAGE_NUMBERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/combat_sim.h"

/* Chiffres de dégâts flottants (M5-17, mannequin d'entraînement). Module de DONNÉES : la liste
 * des chiffres actifs (texte, position monde, opacité). Le rendu (texte à l'écran) est fait par
 * l'hôte ; ici tout est déterministe et testable. Pool fixe de 32, le plus ancien est remplacé
 * quand il est plein. */

#define ENG_DMGNUM_MAX 32U
#define ENG_DMGNUM_LIFE 60U   /* ticks (1 s) */
#define ENG_DMGNUM_RISE 0.02F /* mètres par tick */

typedef enum EngDmgKind {
    ENG_DMG_HIT = 0, /* touche franche, hors tête */
    ENG_DMG_HEAD,    /* tête : texte « 15! » */
    ENG_DMG_BLOCKED, /* gardée : « (2) » */
    ENG_DMG_PARRIED  /* « PARRY » */
} EngDmgKind;

typedef struct EngDamageNumber {
    bool active;
    EngDmgKind kind;
    float value;
    float pos[3];
    uint32_t age;
} EngDamageNumber;

typedef struct EngDamageNumbers {
    EngDamageNumber items[ENG_DMGNUM_MAX];
    uint32_t next; /* prochain emplacement à (ré)utiliser */
    uint32_t spawned;
} EngDamageNumbers;

void eng_dmgnum_init(EngDamageNumbers *n);

/* Crée le chiffre d'une touche (sans effet pour une touche ignorée). true si créé. */
bool eng_dmgnum_spawn(EngDamageNumbers *n, const EngSimEvent *e);

/* Un tick : vieillit, fait monter, retire ceux qui ont vécu ENG_DMGNUM_LIFE ticks. */
void eng_dmgnum_update(EngDamageNumbers *n);

uint32_t eng_dmgnum_count(const EngDamageNumbers *n);

/* Texte du chiffre (« 15 », « 22! », « (2) », « PARRY »). false si `cap` trop petit. */
bool eng_dmgnum_text(const EngDamageNumber *d, char *out, size_t cap);

/* Opacité 1 → 0 sur la durée de vie (décroissance quadratique). */
float eng_dmgnum_alpha(const EngDamageNumber *d);

#endif
