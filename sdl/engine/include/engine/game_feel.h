#ifndef ENG_GAME_FEEL_H
#define ENG_GAME_FEEL_H

#include <stdbool.h>
#include <stdint.h>

#include "engine/combat_sim.h"
#include "engine/tweak.h"

/* Sensation de frappe (M5-14) : gel d'impact réglable, secousse de caméra, ralenti de parade,
 * « coup de FOV » sur les coups lourds. Tout est réglable en direct (eng_feel_register_tweaks) :
 * `EngFeel.params` est la seule source de vérité, le registre écrit directement dedans.
 *
 * Module PUR : il ne modifie jamais la simulation. L'hôte lit le résultat de `eng_feel_tick` et
 * l'applique à la caméra / à l'horloge de rendu. La simulation de combat (déterministe) n'est donc
 * pas affectée par les réglages de sensation, sauf le gel d'impact si l'hôte l'applique via
 * `eng_feel_hitstop`.
 *
 * Un coup est « lourd » quand son gel d'impact de données vaut au moins ENG_FEEL_HEAVY_HITSTOP
 * ticks (les lourdes du design en ont 6, les légères 3). */

#define ENG_FEEL_HEAVY_HITSTOP 5U

typedef struct EngFeelParams {
    float hitstop_scale; /* multiplicateur du gel d'impact, 0 à 3 */
    float shake_light;   /* amplitude de secousse (m), 0 à 0,2 */
    float shake_heavy;
    float shake_block;
    float shake_parry;
    int32_t shake_ticks;        /* durée de la secousse, 1 à 60 */
    float parry_slowmo_scale;   /* échelle de temps pendant le ralenti, 0,05 à 1 */
    int32_t parry_slowmo_ticks; /* 0 à 60 */
    float fov_kick_heavy;       /* degrés, 0 à 15 */
} EngFeelParams;

typedef struct EngFeel {
    EngFeelParams params;
    uint32_t player; /* indice du combattant dont on suit la caméra */
    float shake_amp;
    float fov_amp;
    uint32_t shake_left;
    uint32_t shake_total;
    uint32_t slow_left;
    uint32_t noise_counter;
} EngFeel;

void eng_feel_params_default(EngFeelParams *p);
bool eng_feel_params_valid(const EngFeelParams *p);

void eng_feel_init(EngFeel *f, uint32_t player);

/* Enregistre les variables « feel.* » (9). false si le registre refuse l'une d'elles. */
bool eng_feel_register_tweaks(EngFeel *f, EngTweaks *t);

/* Gel d'impact après réglage : arrondi de base × échelle, borné à ENG_ATTACK_MAX_HITSTOP. */
uint32_t eng_feel_hitstop(const EngFeel *f, uint32_t base);

/* Prend en compte une touche. Les événements sans rapport avec le joueur sont ignorés. */
void eng_feel_on_hit(EngFeel *f, const EngSimEvent *e);

/* Un tick de rendu : `offset` = décalage de caméra (m), `time_scale` = échelle de temps (1 hors
 * ralenti), `fov_kick_deg` = ouverture de champ ajoutée. Les trois sont facultatifs (NULL). */
void eng_feel_tick(EngFeel *f, float offset[3], float *time_scale, float *fov_kick_deg);

#endif
