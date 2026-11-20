#ifndef ENG_AMBIENCE_H
#define ENG_AMBIENCE_H

/* Ambiance sonore et musique (M8-01), synthétisées au chargement (aucun fichier son) puis rejouées
 * en boucle SANS TROU sur les bus du mixeur :
 *   - vent (bus AMBIENCE, toujours) : bruit filtré, souffle lent ;
 *   - oiseaux (AMBIENCE, le jour) : gazouillis épars ; grillons (AMBIENCE, la nuit) : trilles ;
 *   - musique (bus MUSIC) : nappe de quatre accords (la mineur, fa, do, sol), 32 s.
 * Chaque couche est une suite de lectures du même tampon : la suivante est lancée avec un retard
 * qui la fait commencer exactement à la fin de la précédente (compté en images du mixeur). Le
 * volume jour / nuit d'une couche est pris au début de chaque boucle (quelques secondes). */

#include <stdbool.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/mixer.h"

typedef enum EngAmbienceLayer {
    ENG_AMB_WIND,
    ENG_AMB_BIRDS,
    ENG_AMB_CRICKETS,
    ENG_AMB_MUSIC,
    ENG_AMB_LAYER_COUNT
} EngAmbienceLayer;

typedef struct EngAmbience {
    uint32_t rate;
    float *data[ENG_AMB_LAYER_COUNT];
    uint32_t length[ENG_AMB_LAYER_COUNT];     /* images (mono) */
    uint64_t next_start[ENG_AMB_LAYER_COUNT]; /* image du mixeur où commence la boucle suivante */
    uint32_t loops[ENG_AMB_LAYER_COUNT];      /* boucles lancées (mesure, tests) */
    float last_gain[ENG_AMB_LAYER_COUNT];
    bool started;
    bool ready;
} EngAmbience;

/* Synthétise les quatre couches dans `arena` (~13 Mo à 48 kHz). false si la mémoire manque. */
bool eng_ambience_init(EngAmbience *a, EngArena *arena, uint32_t sample_rate);

/* Gain d'une couche pour un poids de nuit [0 ; 1] (0 : plein jour). */
float eng_ambience_gain(EngAmbienceLayer layer, float night);

/* À appeler souvent (chaque image), mixeur VERROUILLÉ (eng_audio_device_lock) : lance les boucles
 * qui doivent commencer dans les `lookahead` prochaines images du mixeur. */
void eng_ambience_update(EngAmbience *a, EngMixer *m, float night, uint32_t lookahead);

#endif
