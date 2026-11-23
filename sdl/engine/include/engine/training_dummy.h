#ifndef ENG_TRAINING_DUMMY_H
#define ENG_TRAINING_DUMMY_H

/* Mannequin d'entraînement (M5-17) : un combattant de la simulation piloté par un script simple,
 * qui compte les dégâts reçus (affichés par damage_numbers) et se remet à neuf. */

#include <stdbool.h>
#include <stdint.h>

#include "engine/combat_sim.h"

typedef enum EngDummyMode {
    ENG_DUMMY_STATIC = 0, /* ne fait rien */
    ENG_DUMMY_BLOCK,      /* garde en permanence (la parade se teste en la relâchant/pressant) */
    ENG_DUMMY_ATTACK      /* attaque légère toutes les `period` ticks */
} EngDummyMode;

typedef struct EngTrainingDummy {
    uint32_t fighter;
    EngDummyMode mode;
    uint32_t period;      /* ticks entre deux attaques (mode Attack) */
    uint32_t reset_after; /* ticks sans coup reçu avant de remettre les PV à fond (0 : jamais) */
    uint32_t respawn_after; /* ticks après la mort avant de renaître (0 : jamais) */
    uint32_t timer;
    uint32_t since_hit;
    uint32_t since_death;
    float total_damage; /* dégâts reçus depuis le début */
    float last_damage;
    uint32_t hits_taken;
    float dps_window[2]; /* [0] dégâts sur la fenêtre courante, [1] ticks */
} EngTrainingDummy;

void eng_dummy_init(EngTrainingDummy *d, uint32_t fighter, EngDummyMode mode, uint32_t period);

/* À appeler une fois par tick AVANT `eng_sim_step` : lit les événements du tick précédent
 * (dégâts, remise à neuf) et écrit l'entrée du mannequin dans `in`. */
void eng_dummy_tick(EngTrainingDummy *d, EngCombatSim *sim, EngFighterInput *in);

#endif
