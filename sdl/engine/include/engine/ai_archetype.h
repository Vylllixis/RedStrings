#ifndef ENG_AI_ARCHETYPE_H
#define ENG_AI_ARCHETYPE_H

/* Archétypes d'ennemis définis par des DONNÉES (M6-08/09) : un fichier texte décrit stats,
 * sens, attaques et phases de boss ; ajouter un archétype ne demande aucune recompilation.
 *
 *   archetype grunt
 *    hp 60                  # points de vie
 *    speed 1.0              # multiplicateur de la marche (4 m/s)
 *    radius 0.4             # rayon de collision
 *    sight 14               # portée de vue (m)
 *    fov 110                # ouverture TOTALE du cône de vue (degrés)
 *    hearing 8              # portée d'écoute (m)
 *    memory 300             # ticks de mémoire de la dernière position
 *    alert 30               # ticks d'alerte avant la poursuite
 *    standoff 4             # rayon d'attente autour du joueur (sans jeton)
 *    flee_below 0           # fraction de PV sous laquelle il fuit (0 : jamais)
 *    light NOM              # attaque légère (jeu d'attaques de combat)
 *    heavy NOM              # attaque lourde
 *    plan light 0 1.8 60 3 1  # genre portée_min portée_max recharge(ticks) poids appuis
 *    phase 0.5 NOM_L NOM_H 1.3 0.6   # boss : sous cette fraction de PV, change d'attaques,
 *                                    #   vitesse (x) et recharges (x)
 *   end
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/attack_data.h"

#define ENG_ARCH_MAX 8U
#define ENG_ARCH_NAME 32U
#define ENG_ARCH_PLANS 4U
#define ENG_ARCH_PHASES 3U
#define ENG_ARCH_MIN_TELEGRAPH 18U /* 300 ms à 60 Hz */

typedef struct EngArchPlan {
    bool heavy;
    float range_min;
    float range_max;
    uint32_t cooldown;
    uint32_t weight;
    uint32_t presses; /* appuis consécutifs (enchaînement léger) */
} EngArchPlan;

typedef struct EngArchPhase {
    float hp_below;
    char light[ENG_ARCH_NAME];
    char heavy[ENG_ARCH_NAME];
    float speed;
    float cooldown_scale;
} EngArchPhase;

typedef struct EngArchetype {
    char name[ENG_ARCH_NAME];
    float hp;
    float speed;
    float radius;
    float sight;
    float fov_deg;
    float hearing;
    uint32_t memory;
    uint32_t alert;
    float standoff;
    float flee_below;
    char light[ENG_ARCH_NAME];
    char heavy[ENG_ARCH_NAME];
    EngArchPlan plans[ENG_ARCH_PLANS];
    uint32_t plan_count;
    EngArchPhase phases[ENG_ARCH_PHASES];
    uint32_t phase_count;
} EngArchetype;

typedef struct EngArchSet {
    EngArchetype a[ENG_ARCH_MAX];
    uint32_t count;
} EngArchSet;

/* Erreurs : message avec numéro de ligne dans `err`. */
bool eng_arch_parse(const char *text, EngArchSet *out, char *err, size_t err_capacity);

uint32_t eng_arch_find(const EngArchSet *set, const char *name); /* UINT32_MAX si absent */

/* Vérifie contre un jeu d'attaques : noms existants et ANTICIPATION >= 300 ms pour chaque
 * attaque atteignable (chaîne `next` de la légère, lourde, et celles de chaque phase). */
bool eng_arch_validate(const EngArchSet *set, const EngAttackSet *attacks, char *err,
                       size_t err_capacity);

#endif
