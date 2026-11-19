#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/files.h"

/* Données d'attaque (M5-03) : un fichier TEXTE relu à chaud. Toutes les durées sont en TICKS de
 * 60 Hz (voir docs/combat-design.md). Ce module ne contient que les données et leur validation ;
 * l'exécution (états, buffer, combos) est ailleurs.
 *
 *   # attacks.txt
 *   attack light1
 *     startup 8            # ticks avant le premier tick actif (>= 1)
 *     active 4             # ticks où les hitboxes sont actives (>= 1)
 *     recovery 14          # ticks après l'actif (>= 0)
 *     damage 10            # >= 0
 *     posture 8            # dégâts de posture, >= 0 (défaut 0)
 *     stamina 10           # coût, >= 0 (défaut 0)
 *     hitstop 3            # ticks de gel à l'impact, 0 à 30 (défaut 0)
 *     cancel_from 6        # tick DE LA RECOVERY à partir duquel on peut enchaîner/esquiver
 *                          # (0 à recovery ; défaut = recovery : pas d'annulation anticipée)
 *     next light2          # coup suivant du combo (peut être déclaré plus loin dans le fichier)
 *     hitbox hand_r  0 0 0  0 0 0.6  0.12     # os, extrémité A (x y z), extrémité B, rayon (m)
 *   end
 *
 * Une instruction par ligne, `#` commence un commentaire. Au moins une `hitbox` (4 maximum) par
 * attaque. `hitbox` : coordonnées dans le repère de l'os. Toute erreur (mot inconnu, doublon,
 * nombre invalide ou hors bornes, `next` inconnu, attaque sans hitbox...) refuse le fichier ENTIER
 * avec « ligne N : message ». */

#define ENG_ATTACK_MAX_NAME 32U
#define ENG_ATTACK_MAX 32U
#define ENG_ATTACK_MAX_HITBOXES 4U
#define ENG_ATTACK_MAX_TICKS 600U /* durée maximale de chaque phase */
#define ENG_ATTACK_MAX_HITSTOP 30U
#define ENG_ATTACK_NONE UINT32_MAX

typedef struct EngAttackHitbox {
    char joint[ENG_ATTACK_MAX_NAME];
    float a[3];
    float b[3];
    float radius;
} EngAttackHitbox;

typedef struct EngAttack {
    char name[ENG_ATTACK_MAX_NAME];
    uint32_t startup;
    uint32_t active;
    uint32_t recovery;
    float damage;
    float posture;
    float stamina;
    uint32_t hitstop;
    uint32_t cancel_from; /* dans [0 ; recovery] */
    uint32_t next;        /* indice de l'attaque suivante ou ENG_ATTACK_NONE */
    uint32_t hitbox_count;
    EngAttackHitbox hitboxes[ENG_ATTACK_MAX_HITBOXES];
} EngAttack;

typedef struct EngAttackSet {
    uint32_t count;
    EngAttack attacks[ENG_ATTACK_MAX];
} EngAttackSet;

typedef enum EngAttackPhase {
    ENG_ATTACK_STARTUP = 0,
    ENG_ATTACK_ACTIVE,
    ENG_ATTACK_RECOVERY,
    ENG_ATTACK_DONE
} EngAttackPhase;

/* true : `out` complet et cohérent. false : `out` mis à zéro, `err` (si non NULL) reçoit « ligne N
 * : message ». */
bool eng_attackset_parse(const char *text, EngAttackSet *out, char *err, size_t err_capacity);

/* Indice de l'attaque de ce nom ou ENG_ATTACK_NONE. */
uint32_t eng_attackset_find(const EngAttackSet *set, const char *name);

/* Ticks totaux (startup + active + recovery). */
uint32_t eng_attack_total(const EngAttack *a);

/* Phase au tick `tick` (0 = premier tick de l'attaque) : STARTUP [0 ; startup), ACTIVE
 * [startup ; startup+active), RECOVERY jusqu'à total, puis DONE. */
EngAttackPhase eng_attack_phase(const EngAttack *a, uint32_t tick);

/* Les hitboxes sont-elles actives à ce tick ? */
bool eng_attack_is_active(const EngAttack *a, uint32_t tick);

/* Peut-on annuler (enchaîner le coup suivant ou esquiver) à ce tick ? Vrai dans la recovery à
 * partir du tick startup+active+cancel_from, jusqu'à la fin exclue. */
bool eng_attack_can_cancel(const EngAttack *a, uint32_t tick);

/* ── Fichier surveillé (rechargement à chaud) ── */

typedef struct EngAttackFile {
    char path[512];
    EngFileWatch watch;
    EngAttackSet set; /* DERNIÈRE version valide */
    bool valid;
    uint32_t version; /* +1 à chaque version valide chargée */
    char error[256];
} EngAttackFile;

bool eng_attackfile_open(EngAttackFile *file, EngArena *scratch, const char *path);
/* true UNE fois quand une nouvelle version VALIDE vient d'être chargée. Une version invalide est
 * journalisée et l'ancienne reste en place. */
bool eng_attackfile_poll(EngAttackFile *file, EngArena *scratch, double now_seconds);
