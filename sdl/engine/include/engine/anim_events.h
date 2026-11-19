#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/files.h"

/* Événements d'animation (M5-04) : des marqueurs posés à un instant d'un clip (hitbox_on/off, son,
 * effet, fenêtre d'annulation). Ils vivent dans un fichier TEXTE à côté du clip (le format binaire
 * .anim ne change pas), relu à chaud comme les autres données :
 *
 *   # light1.events
 *   event light1 0.133 hitbox_on
 *   event light1 0.200 sound   swoosh_light
 *   event light1 0.200 hitbox_off
 *   event light1 0.300 cancel_open
 *   event light1 0.433 cancel_close
 *   event heavy  0.350 effect  trail_heavy
 *
 * `event CLIP TEMPS TYPE [ARGUMENT]` : TEMPS en secondes depuis le début du clip, dans [0 ; durée].
 * ARGUMENT obligatoire pour `sound` et `effect`, interdit pour les autres types.
 * Tout ou rien : la moindre erreur refuse le fichier entier (« ligne N : message »).
 *
 * Déclenchement : eng_animevents_collect donne les événements d'un pas d'animation [t0 ; t1). Les
 * pas consécutifs se recollent sans trou ni doublon : un événement est déclenché EXACTEMENT UNE
 * FOIS par passage, y compris à travers le rebouclage d'un clip en boucle. */

#define ENG_ANIMEVENT_MAX_NAME 32U
#define ENG_ANIMEVENT_MAX 128U

typedef enum EngAnimEventType {
    ENG_ANIMEVENT_HITBOX_ON = 0,
    ENG_ANIMEVENT_HITBOX_OFF,
    ENG_ANIMEVENT_SOUND,
    ENG_ANIMEVENT_EFFECT,
    ENG_ANIMEVENT_CANCEL_OPEN,
    ENG_ANIMEVENT_CANCEL_CLOSE,
    ENG_ANIMEVENT_TYPE_COUNT
} EngAnimEventType;

typedef struct EngAnimEvent {
    char clip[ENG_ANIMEVENT_MAX_NAME];
    float time; /* secondes */
    EngAnimEventType type;
    char arg[ENG_ANIMEVENT_MAX_NAME]; /* nom du son / de l'effet, sinon vide */
} EngAnimEvent;

typedef struct EngAnimEventSet {
    uint32_t count;
    EngAnimEvent
        events[ENG_ANIMEVENT_MAX]; /* triés par (clip, temps), ordre du fichier à égalité */
} EngAnimEventSet;

const char *eng_animevent_type_name(EngAnimEventType t);

/* true : `out` complet. false : `out` mis à zéro, `err` reçoit « ligne N : message ». */
bool eng_animevents_parse(const char *text, EngAnimEventSet *out, char *err, size_t err_capacity);

/* Indices (dans set->events) des événements du clip `clip` déclenchés par le pas qui amène le
 * temps de clip de `t0` à `t1` (secondes, dépliés : avec `loop`, t1 peut dépasser `duration` de
 * plusieurs cycles ; le temps d'un événement est replié sur chaque cycle traversé).
 *
 *  - intervalle [t0 ; t1[ : un événement à l'instant t0 est déclenché, pas celui à t1 (il le sera
 * au pas suivant, qui commence à t1) ;
 *  - sans `loop`, t1 est borné à `duration` ; un pas qui atteint la fin déclenche aussi les
 *    événements placés exactement à `duration`, une seule fois ;
 *  - t1 <= t0 : rien. Premier pas d'un clip : passer t0 = 0. Un pas de plus de 1024 cycles : rien.
 * Écrit au plus `max` indices dans `out` (ordre chronologique) et retourne leur nombre RÉEL
 * (peut dépasser `max`). */
uint32_t eng_animevents_collect(const EngAnimEventSet *set, const char *clip, float duration,
                                float t0, float t1, bool loop, uint32_t *out, uint32_t max);

/* ── Fichier surveillé (rechargement à chaud) ── */

typedef struct EngAnimEventFile {
    char path[512];
    EngFileWatch watch;
    EngAnimEventSet set;
    bool valid;
    uint32_t version;
    char error[256];
} EngAnimEventFile;

bool eng_animeventfile_open(EngAnimEventFile *file, EngArena *scratch, const char *path);
bool eng_animeventfile_poll(EngAnimEventFile *file, EngArena *scratch, double now_seconds);
