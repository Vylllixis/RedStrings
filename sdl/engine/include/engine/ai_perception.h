#ifndef ENG_AI_PERCEPTION_H
#define ENG_AI_PERCEPTION_H

/* Perception d'un ennemi (M6-02) : vue (cône + ligne de vue), ouïe (bruits), mémoire de la
 * dernière position connue. Tout est en ticks entiers à 60 Hz. */

#include <stdbool.h>
#include <stdint.h>

#include "engine/ai_world.h"

typedef struct EngAiSenses {
    float sight_range;      /* m */
    float sight_half_angle; /* rad : moitié de l'ouverture du cône */
    float hearing_range;    /* m : portée maximale d'écoute */
    uint32_t memory_ticks;  /* durée pendant laquelle la dernière position reste « connue » */
} EngAiSenses;

typedef struct EngAiPerception {
    bool sees;  /* la cible est vue CE tick */
    bool hears; /* la cible a fait un bruit audible CE tick */
    bool has_memory;
    float last_known[2];
    uint32_t since_sensed; /* ticks depuis la dernière vue / écoute */
} EngAiPerception;

/* `noise_radius` : rayon du bruit émis par la cible ce tick (0 = silencieuse). L'ouïe passe à
 * travers les obstacles ; la vue non. Une cible morte n'est ni vue ni entendue et efface la
 * mémoire. */
void eng_ai_perceive(EngAiPerception *p, const EngAiSenses *s, const float self_pos[2],
                     float self_yaw, const float target_pos[2], bool target_alive,
                     float noise_radius, const EngAiWorld *world);

void eng_ai_perception_reset(EngAiPerception *p);

#endif
