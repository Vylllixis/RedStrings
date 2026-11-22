#ifndef ENG_COMBAT_REPLAY_H
#define ENG_COMBAT_REPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/combat_sim.h"

/* Rejeu déterministe du combat (M5-18).
 *
 * Un replay = la position de départ + UNE entrée compacte par combattant et par tick. Rejouer
 * ces entrées dans une simulation neuve redonne exactement le même état final (même empreinte
 * `eng_sim_hash`). L'entrée est quantifiée (2 octets) : le jeu doit donc alimenter la simulation
 * avec `eng_input_unpack(eng_input_pack(entrée))`, en direct comme au rejeu, sinon la direction
 * de déplacement diffère de 0,7° au plus entre les deux.
 *
 * Limite honnête : le déterminisme est garanti pour un même binaire sur une même machine (mêmes
 * flottants, mêmes `sinf`/`cosf`). D'une plate-forme ou d'un compilateur à l'autre, les empreintes
 * peuvent différer ; ce n'est pas testé. */

#define ENG_PIN_LIGHT 0x01U
#define ENG_PIN_HEAVY 0x02U
#define ENG_PIN_DODGE 0x04U
#define ENG_PIN_BLOCK_PRESSED 0x08U
#define ENG_PIN_BLOCK_HELD 0x10U
#define ENG_PIN_MOVING 0x20U
#define ENG_PIN_ALL 0x3FU

#define ENG_REPLAY_MAX_TICKS 216000U /* une heure à 60 Hz */

typedef struct EngPackedInput {
    uint8_t flags;
    uint8_t dir; /* 0 : aucune ; 1 à 255 : angle de la direction (x, z) */
} EngPackedInput;

EngPackedInput eng_input_pack(const EngFighterInput *in);
EngFighterInput eng_input_unpack(EngPackedInput p);

typedef struct EngReplaySetup {
    uint32_t count;
    float x[ENG_SIM_MAX];
    float z[ENG_SIM_MAX];
    float yaw[ENG_SIM_MAX];
} EngReplaySetup;

typedef struct EngReplay {
    uint32_t fighters;
    uint32_t capacity;    /* ticks que `data` peut contenir */
    uint32_t ticks;       /* ticks enregistrés */
    EngPackedInput *data; /* fighters × capacity entrées, fournies par l'appelant */
} EngReplay;

bool eng_replay_init(EngReplay *r, EngPackedInput *storage, uint32_t capacity, uint32_t fighters);

/* Ajoute un tick (`fighters` entrées). false si plein ou argument invalide. */
bool eng_replay_record(EngReplay *r, const EngPackedInput *tick_inputs);

/* Entrées du tick `t` (0 = premier) ou NULL. */
const EngPackedInput *eng_replay_tick(const EngReplay *r, uint32_t t);

/* Crée la simulation du replay (positions de départ). false si le setup est invalide. */
bool eng_replay_setup_sim(EngCombatSim *s, const EngAttackSet *attacks,
                          const EngReplaySetup *setup);

/* Rejoue tout. `*final_hash` : empreinte finale. `trace` (optionnel, `trace_cap` éléments) reçoit
 * l'empreinte après chaque tick, pour localiser le premier tick où deux rejeux divergent. */
bool eng_replay_run(const EngReplay *r, const EngAttackSet *attacks, const EngReplaySetup *setup,
                    uint64_t *final_hash, uint64_t *trace, uint32_t trace_cap);

/* Premier indice où les deux traces diffèrent, ou `n` si identiques. */
uint32_t eng_replay_first_divergence(const uint64_t *a, const uint64_t *b, uint32_t n);

/* Format binaire (petit-boutiste, indépendant de la machine) : "RSRP", version 1, combattants,
 * ticks, empreinte finale, position de départ, entrées. Retourne la taille écrite, 0 si `cap` est
 * trop petit ou arguments invalides. */
size_t eng_replay_size(const EngReplay *r);
size_t eng_replay_write(const EngReplay *r, const EngReplaySetup *setup, uint64_t final_hash,
                        uint8_t *out, size_t cap);

/* Lit un replay. Refuse (false, rien d'écrit de façon exploitable) : magic/version inconnus,
 * nombre de combattants hors 1..ENG_SIM_MAX, ticks > ENG_REPLAY_MAX_TICKS ou > `capacity`, taille
 * non exacte (tronqué ou octets en trop), drapeaux inconnus, flottants non finis. */
bool eng_replay_read(const uint8_t *buf, size_t len, EngPackedInput *storage, uint32_t capacity,
                     EngReplay *r, EngReplaySetup *setup, uint64_t *final_hash);

#endif
