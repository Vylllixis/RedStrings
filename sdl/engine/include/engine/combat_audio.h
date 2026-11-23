#ifndef ENG_COMBAT_AUDIO_H
#define ENG_COMBAT_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

#include "engine/combat_sim.h"
#include "engine/mixer.h"
#include "engine/sfx.h"

/* Sons du combat (M5-19) : traduit les touches de la simulation en voix du mélangeur.
 *
 * Calage : le tick de simulation N correspond à l'image audio  N × fréquence / 60  (arithmétique
 * entière exacte à 48 kHz : 800 images par tick). `eng_combat_audio_on_event` démarre la voix
 * exactement à cette image si elle n'est pas encore passée (sinon tout de suite) : la latence ne
 * dépend donc plus de la taille des blocs audio. Limite : ce calage est mesuré sur les échantillons
 * du mélangeur ; le retard du matériel (tampon de la carte son) s'y ajoute et n'est pas mesuré. */

typedef struct EngCombatAudio {
    EngMixer *mixer;
    float bank[ENG_SFX_COUNT][ENG_SFX_MAX_SAMPLES];
    uint32_t length[ENG_SFX_COUNT];
} EngCombatAudio;

/* Synthétise la banque de sons à la fréquence du mélangeur. false si `mixer` est invalide ou sa
 * fréquence n'est pas gérée (8000 à 48000 Hz). */
bool eng_combat_audio_init(EngCombatAudio *a, EngMixer *mixer);

/* Image audio du début du tick (0 = début de la simulation). */
uint64_t eng_audio_tick_frame(uint32_t tick, uint32_t sample_rate);

/* Joue le son d'une touche. `listener` : position (x, z) de l'oreille ; `listener_yaw` : son cap
 * (le panoramique dépend de la position du contact par rapport à sa droite). Retourne false si
 * l'événement n'a pas de son (touche ignorée) ou si un argument est invalide. */
bool eng_combat_audio_on_event(EngCombatAudio *a, const EngSimEvent *e, const float listener[2],
                               float listener_yaw);

/* Joue directement un son de la banque. */
bool eng_combat_audio_play(EngCombatAudio *a, EngSfx s, float gain, float pan, uint32_t delay);

#endif
