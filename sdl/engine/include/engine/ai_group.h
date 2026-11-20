#ifndef ENG_AI_GROUP_H
#define ENG_AI_GROUP_H

/* Cerveau des ennemis et coordination de groupe (M6-05 à M6-09).
 *
 * États : Patrol (ronde) → Alert (a vu/entendu : s'arrête, regarde) → Chase (poursuite, chemin
 * A*) → Engage (au contact : attend un jeton, ou tourne sur le cercle d'attente ; avec jeton :
 * choisit une attaque selon distance et recharge, se tourne, attaque) → Return (retour au poste)
 * → Flee (PV bas, si l'archétype fuit). Un boss change de phase sous des seuils de PV.
 *
 * Le cerveau ne touche pas la simulation : il lit son état et ÉCRIT des entrées `EngFighterInput`
 * (les mêmes que celles d'un joueur) ; c'est la simulation de combat qui les applique. Tout est
 * déterministe (aléa xorshift graine par ennemi). */

#include <stdbool.h>
#include <stdint.h>

#include "engine/ai_archetype.h"
#include "engine/ai_perception.h"
#include "engine/ai_tokens.h"
#include "engine/ai_world.h"
#include "engine/combat_sim.h"
#include "engine/navgrid.h"

#define ENG_AI_MAX_AGENTS 7U
#define ENG_AI_PATROL_MAX 4U
#define ENG_AI_PATH_MAX 24U
#define ENG_AI_MAX_ATTACKERS 2U

typedef enum EngAiState {
    ENG_AI_PATROL = 0,
    ENG_AI_ALERT,
    ENG_AI_CHASE,
    ENG_AI_ENGAGE,
    ENG_AI_RETURN,
    ENG_AI_FLEE,
    ENG_AI_DEAD,
    ENG_AI_STATE_COUNT
} EngAiState;

const char *eng_ai_state_name(EngAiState s);

typedef struct EngAiAgent {
    bool active;
    uint32_t fighter; /* indice dans la simulation */
    uint32_t arch;
    uint32_t phase; /* phases de boss déjà franchies */
    EngAiState state;
    uint32_t state_ticks;
    float home[2];
    float patrol[ENG_AI_PATROL_MAX][2];
    uint32_t patrol_n;
    uint32_t patrol_i;
    uint32_t patrol_wait;
    EngAiPerception perc;
    float path[ENG_AI_PATH_MAX][2];
    uint32_t path_n;
    uint32_t path_i;
    uint32_t repath_wait;
    float goal[2];
    uint32_t cooldown[ENG_ARCH_PLANS];
    int32_t plan; /* plan en cours ou -1 */
    uint32_t presses_left;
    uint32_t press_gap;
    uint32_t atk_phase; /* 0 aucune, 1 appui envoyé, 2 attaque en cours */
    uint32_t step_next; /* plan pattern (M6-09) : indice de la prochaine attaque de la suite */
    uint32_t atk_wait;
    uint32_t token_hold;
    uint32_t token_cd;
    uint32_t pause; /* ticks d'immobilité (rugissement de changement de phase) */
    uint32_t lost_ticks;
    uint32_t rng;
    float ring[2];
    float cooldown_scale;
} EngAiAgent;

typedef struct EngAiGroup {
    EngAiAgent agents[ENG_AI_MAX_AGENTS];
    uint32_t count;
    EngAiTokens tokens;
    uint32_t player; /* indice du joueur dans la simulation */
    const EngArchSet *arch;
    const EngAiWorld *world;
    EngNavGrid *nav; /* peut être NULL : déplacement en ligne droite */
} EngAiGroup;

void eng_aig_init(EngAiGroup *g, const EngArchSet *arch, const EngAiWorld *world, EngNavGrid *nav,
                  uint32_t player);

/* Crée l'ennemi `fighter` (≠ joueur) : paramètres de combat, PV, vitesse, rayon, position. Les
 * points de ronde (jusqu'à 4) sont optionnels. */
bool eng_aig_add(EngAiGroup *g, EngCombatSim *sim, uint32_t fighter, uint32_t arch, float x,
                 float z, float yaw, const float (*patrol)[2], uint32_t patrol_n);

/* Remet un ennemi dans son état initial (camp réinitialisé) : PV, position, état, recharges. */
bool eng_aig_reset_agent(EngAiGroup *g, EngCombatSim *sim, uint32_t agent_index);

/* Un tick : écrit dans `inputs[f]` l'entrée de chaque ennemi vivant (les autres cases sont
 * laissées telles quelles). `noise_radius` = bruit fait par le joueur ce tick. */
void eng_aig_tick(EngAiGroup *g, EngCombatSim *sim, float noise_radius, EngFighterInput *inputs);

/* La grille de navigation a changé (fenêtre recentrée, cellules arrivées) : chemins à refaire. */
void eng_aig_invalidate_paths(EngAiGroup *g);

/* Ennemis en train d'attaquer (état Attack du combattant) — doit rester <= 2. */
uint32_t eng_aig_attackers(const EngAiGroup *g, const EngCombatSim *sim);

#endif
