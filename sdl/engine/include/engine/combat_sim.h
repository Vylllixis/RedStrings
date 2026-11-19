#ifndef ENG_COMBAT_SIM_H
#define ENG_COMBAT_SIM_H

#include <stdbool.h>
#include <stdint.h>

#include "engine/ai_world.h"
#include "engine/attack_data.h"
#include "engine/combat_hit.h"
#include "engine/fighter.h"
#include "engine/hurtbox.h"

/* Simulation de combat déterministe (M5-17, base du replay M5-18).
 *
 * Réunit N combattants (≤ 8) sur un plan (x, z) : une entrée par combattant et par tick, aucun
 * autre état caché. Modèle de corps volontairement simple (pas de squelette) pour être testable :
 *   - hurtboxes : tête, torse, jambes (capsules fixes dans le repère du corps) ;
 *   - arme : les `hitbox` de l'attaque, interprétées dans le repère du CORPS (+Z = avant, Y = haut,
 *     origine à 1,1 m du sol) — dans le jeu final ce repère est celui de l'os (voir hurtbox.h).
 * Ordre d'un tick : (1) gel d'impact : si actif, seuls les buffers d'entrée sont alimentés ;
 * (2) tous les combattants avancent d'un tick par indice croissant ; (3) les touches sont calculées
 * pour TOUS les attaquants sur l'état résultant, puis appliquées par attaquant croissant (deux
 * combattants peuvent donc se toucher au même tick : trade). Aucune horloge, aucun aléa. */

#define ENG_SIM_MAX 8U
#define ENG_SIM_EVENTS 32U
#define ENG_SIM_WALK_SPEED 4.0F /* m/s */

typedef struct EngSimBody {
    float pos[2];
    float yaw; /* 0 = regarde -Z ; avant = (-sin yaw, -cos yaw) */
    float prev_pos[2];
    float prev_yaw;
} EngSimBody;

typedef struct EngSimEvent {
    uint32_t tick;
    uint32_t attacker;
    uint32_t defender;
    EngHitKind kind;
    EngHitZone zone;
    EngReactDir dir;
    float dealt;
    bool killed;
    uint32_t hitstop;
    float point[3];
} EngSimEvent;

/* Hurtboxes pilotées par un squelette animé (M5-05) : si `count` > 0, elles remplacent le corps
 * simplifié (3 capsules) de ce combattant. Le simulateur ne connaît pas le squelette : l'appelant
 * échantillonne la pose et appelle `eng_sim_set_rig` avant chaque `eng_sim_step`. */
typedef struct EngSimRig {
    uint32_t count;
    EngCapsule cur[ENG_HURTBOX_MAX];
    EngCapsule prev[ENG_HURTBOX_MAX];
    EngHitZone zones[ENG_HURTBOX_MAX];
} EngSimRig;

typedef struct EngCombatSim {
    const EngAttackSet *attacks;
    uint32_t count;
    uint32_t tick;
    uint32_t hitstop; /* ticks de gel restants */
    EngFighter fighters[ENG_SIM_MAX];
    EngSimBody bodies[ENG_SIM_MAX];
    EngSimEvent events[ENG_SIM_EVENTS]; /* touches du DERNIER tick */
    uint32_t event_count;
    EngSimRig rig[ENG_SIM_MAX];
    float speed_scale[ENG_SIM_MAX]; /* multiplie la marche (1 par défaut) */
    float radius[ENG_SIM_MAX];      /* rayon de collision corps-corps (0 : aucune séparation) */
    const EngAiWorld *world;        /* obstacles statiques (NULL : aucun) */
    uint32_t world_ignore;          /* bit i : le combattant i n'est pas repoussé par `world` */
    uint32_t total_hits; /* touches réelles (dégâts, garde, parade) depuis le début */
} EngCombatSim;

/* `count` combattants (1 à ENG_SIM_MAX), tous à l'origine, paramètres par défaut. false sinon. */
bool eng_sim_init(EngCombatSim *s, const EngAttackSet *attacks, uint32_t count);
/* Capsules du tick à venir (monde). Les précédentes deviennent les capsules « prev » du balayage.
 * `count` = 0 revient au corps simplifié. Retourne false si les arguments sont invalides. */
bool eng_sim_set_rig(EngCombatSim *s, uint32_t i, const EngCapsule *caps, const EngHitZone *zones,
                     uint32_t count);

bool eng_sim_place(EngCombatSim *s, uint32_t i, float x, float z, float yaw);

/* Avance d'un tick. `inputs` : `count` entrées. Les entrées d'un combattant mort sont ignorées. */
void eng_sim_step(EngCombatSim *s, const EngFighterInput *inputs);

/* Empreinte de tout l'état (combattants, corps, gel, tick). */
uint64_t eng_sim_hash(const EngCombatSim *s);

/* Capsules monde de la cible `i` (3 : tête, torse, jambes) au pose courante / précédente. */
#define ENG_SIM_HURTBOXES 3U
void eng_sim_hurtboxes(const EngSimBody *b, bool previous, EngCapsule out[ENG_SIM_HURTBOXES],
                       EngHitZone zones[ENG_SIM_HURTBOXES]);

/* Capsules d'arme de l'attaque `atk` posée sur le corps `b` (pose courante ou précédente). */
uint32_t eng_sim_weapons(const EngSimBody *b, bool previous, const EngAttack *atk,
                         EngCapsule out[ENG_ATTACK_MAX_HITBOXES]);

#endif
