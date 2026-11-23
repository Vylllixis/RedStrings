#ifndef ENG_AUDIO_DEVICE_H
#define ENG_AUDIO_DEVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "engine/mixer.h"

/* Sortie audio SDL3 (M5-19) : un flux stéréo flottant dont le rappel tire ses images du mélangeur.
 * Le rappel tourne dans le thread audio de SDL ; SDL verrouille le flux pendant le rappel, donc
 * l'hôte doit appeler `eng_audio_device_lock` / `unlock` autour de tout accès au mélangeur
 * (eng_mixer_play, lecture de `frames_mixed`...).
 *
 * Ouvrir échoue proprement (false + message de log) s'il n'y a pas de périphérique : le jeu doit
 * alors continuer sans son. On ne peut vérifier ici QUE le comportement du rappel (pilote « dummy »
 * de SDL, sans haut-parleur) ; ce qu'on entend vraiment ne peut être jugé que sur ta machine. */

typedef struct EngAudioDevice EngAudioDevice;

/* Périphérique de lecture par défaut. NULL si échec (pas de périphérique, SDL sans audio...). */
EngAudioDevice *eng_audio_device_open(EngMixer *mixer);
void eng_audio_device_close(EngAudioDevice *d);

void eng_audio_device_lock(EngAudioDevice *d);
void eng_audio_device_unlock(EngAudioDevice *d);

/* Nombre de fois que le rappel a été appelé (lu sous verrou). */
uint32_t eng_audio_device_callbacks(EngAudioDevice *d);

#endif
