#ifndef ENG_MIXER_H
#define ENG_MIXER_H

#include <stdbool.h>
#include <stdint.h>

/* Mélangeur logiciel stéréo (M5-19) : N voix qui lisent des tampons mono, positionnées par un
 * panoramique à puissance constante. Aucune allocation, aucun SDL : il est déterministe et se teste
 * en comparant des échantillons. Le périphérique audio (audio_device.h) l'appelle depuis le thread
 * audio ; l'hôte doit alors verrouiller le flux autour de `eng_mixer_play`.
 *
 * Un son démarre à une image PRÉCISE grâce à `delay_frames` : c'est ainsi qu'un impact est calé sur
 * son tick (voir combat_audio.h). Les tampons référencés doivent vivre jusqu'à la fin des voix. */

#define ENG_MIXER_VOICES 16U

/* Bus (M8-01) : chaque voix appartient à un bus dont le volume s'applique PENDANT la lecture
 * (baisser la musique dans le menu agit tout de suite). L'ordre est celui des clés d'options
 * (volume.sfx, volume.music, volume.ambience, volume.ui). */
typedef enum EngAudioBus {
    ENG_BUS_SFX,
    ENG_BUS_MUSIC,
    ENG_BUS_AMBIENCE,
    ENG_BUS_UI,
    ENG_BUS_COUNT
} EngAudioBus;

typedef struct EngVoice {
    const float *data;
    uint32_t length;
    uint32_t pos;
    uint32_t delay; /* images à attendre avant de commencer */
    float gain_l;
    float gain_r;
    uint32_t serial; /* ordre de création, pour voler la plus ancienne */
    EngAudioBus bus;
    bool active;
} EngVoice;

typedef struct EngMixer {
    uint32_t sample_rate;
    float master;
    float bus_volume[ENG_BUS_COUNT]; /* dans [0 ; 1], 1 au départ */
    EngVoice voices[ENG_MIXER_VOICES];
    uint32_t serial;
    uint64_t frames_mixed; /* images produites depuis le début */
    uint32_t clipped;      /* échantillons écrêtés depuis le début */
} EngMixer;

bool eng_mixer_init(EngMixer *m, uint32_t sample_rate);

/* Lance une voix. `gain` dans [0 ; 4], `pan` dans [-1 ; 1] (-1 gauche, +1 droite). Retourne
 * l'indice de voix, ou -1 si les arguments sont invalides. Si toutes les voix sont prises, la plus
 * ancienne est remplacée. */
int32_t eng_mixer_play(EngMixer *m, const float *data, uint32_t length, float gain, float pan,
                       uint32_t delay_frames);
/* Comme eng_mixer_play (qui joue sur ENG_BUS_SFX), sur le bus donné ; -1 si le bus est invalide. */
int32_t eng_mixer_play_bus(EngMixer *m, EngAudioBus bus, const float *data, uint32_t length,
                           float gain, float pan, uint32_t delay_frames);
/* Volume d'un bus dans [0 ; 1] ; false (rien ne change) si le bus ou la valeur sont invalides. */
bool eng_mixer_set_bus(EngMixer *m, EngAudioBus bus, float volume);

/* Son 3D simplifié (M8-01), dans le plan (x, z) : auditeur en `listener`, regard de lacet `yaw`
 * (0 = vers -Z, π/2 = vers -X), source en `source`. Gain : 1 jusqu'à `ref_dist`, puis
 * ref_dist / d (loi en 1/d), fondu à 0 sur les derniers 10 % avant `max_dist`, 0 au-delà ; une
 * source derrière l'auditeur est atténuée de 25 % au plus (indice avant/arrière). Pan : sinus de
 * l'angle de la source (+1 à droite, -1 à gauche, 0 devant ou derrière). Paramètres invalides :
 * gain 0, pan 0. */
void eng_audio_spatialize(const float listener[2], float yaw, const float source[2], float ref_dist,
                          float max_dist, float *gain, float *pan);

/* Produit `frames` images stéréo entrelacées (2 × frames flottants) dans `out`. Somme des voix,
 * volume maître, écrêtage dans [-1 ; 1]. */
void eng_mixer_mix(EngMixer *m, float *out, uint32_t frames);

uint32_t eng_mixer_active(const EngMixer *m);
void eng_mixer_stop_all(EngMixer *m);

#endif
