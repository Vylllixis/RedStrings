#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/hurtbox.h"

/* Système de dégâts (M5-08) : points de vie, multiplicateurs par zone, mort, invincibilité
 * temporaire. Module pur, déterministe, en ticks de 60 Hz.
 *
 *   dégâts = base x multiplicateur(zone)        (tête 1,5 ; torse 1,0 ; membres 0,75)
 *
 * Après une touche, le combattant est invincible `iframes` ticks (6 par défaut) : les touches
 * suivantes sont ignorées (dealt = 0, ignored = true). L'invincibilité est PROLONGÉE par l'esquive
 * (eng_damage_set_invulnerable). Un combattant mort (pv = 0) ne reçoit plus rien. `died_tick`
 * enregistre le tick du coup fatal : « mort déclenchée au bon tick ». */

#define ENG_DAMAGE_DEFAULT_MAX_HP 100.0F
#define ENG_DAMAGE_DEFAULT_IFRAMES 6U
#define ENG_DAMAGE_NEVER UINT32_MAX

typedef struct EngVitals {
    float hp;
    float max_hp;
    uint32_t invulnerable_until; /* tick (exclu) jusqu'auquel les touches sont ignorées */
    bool dead;
    uint32_t died_tick; /* ENG_DAMAGE_NEVER tant que vivant */
    uint32_t iframes;   /* invincibilité après une touche, en ticks */
} EngVitals;

typedef struct EngDamageResult {
    float dealt;  /* PV retirés (0 si ignoré) */
    bool ignored; /* touche ignorée : invincibilité ou déjà mort */
    bool killed;  /* ce coup est le coup fatal */
} EngDamageResult;

/* Multiplicateur de la zone ; 1 hors bornes. */
float eng_zone_multiplier(EngHitZone zone);

/* PV pleins, vivant. `max_hp` > 0 fini, sinon la valeur par défaut. */
void eng_vitals_init(EngVitals *v, float max_hp);

/* Applique une touche de dégâts de base `base` (>= 0, fini) au tick `now`. Résultat : voir
 * EngDamageResult. Arguments invalides (NULL, base négative ou non finie) : ignoré. */
EngDamageResult eng_damage_apply(EngVitals *v, float base, EngHitZone zone, uint32_t now);

/* Le combattant est-il touchable au tick `now` ? (vivant et hors invincibilité) */
bool eng_vitals_hittable(const EngVitals *v, uint32_t now);

/* Rend invulnérable jusqu'au tick `until` (exclu) ; ne raccourcit jamais une invincibilité déjà
 * plus longue. */
void eng_damage_set_invulnerable(EngVitals *v, uint32_t until);

/* Soigne de `amount` (>= 0, fini), borné à max_hp ; sans effet sur un mort. */
void eng_vitals_heal(EngVitals *v, float amount);
