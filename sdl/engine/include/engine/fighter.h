#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/attack_data.h"
#include "engine/combat_damage.h"
#include "engine/combat_hit.h"
#include "engine/combat_state.h"
#include "engine/input_buffer.h"

/* Combattant (M5-10 combos, M5-11 esquive, M5-12 garde et parade, M5-13 réactions). Module pur et
 * déterministe, 60 ticks par seconde, sans allocation : il assemble la machine à états (M5-02), les
 * données d'attaque (M5-03), le buffer d'entrées (M5-09) et les dégâts (M5-08). Il ne connaît ni le
 * monde ni le rendu : la boucle de combat lui fournit les entrées du tick et lui applique les
 * touches calculées ailleurs (combat_hit.h). Règles : docs/combat-design.md.
 *
 * Un tick : eng_fighter_tick(f, attacks, &input). Le compteur f->tick est incrémenté AU DÉBUT du
 * tick (premier tick = 1) : les touches du tick N se résolvent après eng_fighter_tick de tous les
 * combattants, avec f->tick == N.
 *
 *  - Combos : depuis une attaque, dans sa fenêtre d'annulation (attack_data), une pression légère
 *    lance `next` (s'il existe), une lourde lance `heavy_name` (seulement si `next` existe : une
 *    attaque finale ne s'annule que par une esquive), une esquive annule tout. Hors fenêtre, la
 *    pression reste dans le buffer 10 ticks et part à l'ouverture.
 *  - Esquive : durée totale, ticks d'invulnérabilité [inv_from ; inv_to[, coût d'endurance,
 * distance parcourue (root motion simplifié : profil lissé).
 *  - Parade : les `parry_frames` premiers ticks de la garde ; une touche reçue alors n'inflige rien
 *    et met l'attaquant en Stagger. Ensuite : garde (dégâts x 0,2, posture entamée).
 *  - Réactions : hitstun (durée selon la posture du coup) ; posture à 0 -> Stagger ; direction du
 *    coup (avant, arrière, côtés) et recul rendus à l'appelant. */

#define ENG_FIGHTER_NAME_MAX ENG_ATTACK_MAX_NAME

typedef struct EngFighterParams {
    char light_name[ENG_FIGHTER_NAME_MAX]; /* première attaque légère d'une chaîne */
    char heavy_name[ENG_FIGHTER_NAME_MAX]; /* attaque lourde */
    float max_stamina;
    float stamina_regen;    /* points par tick (25/s = 0,41666 par tick) */
    uint32_t stamina_delay; /* ticks sans action avant la régénération (60) */
    float max_posture;
    float posture_regen;         /* points par tick (20/s) */
    uint32_t posture_delay;      /* ticks sans coup reçu avant la régénération (90) */
    uint32_t parry_frames;       /* fenêtre de parade (8) */
    float block_multiplier;      /* dégâts gardés (0,2) */
    uint32_t dodge_total;        /* durée de l'esquive (24) */
    uint32_t dodge_inv_from;     /* premier tick invulnérable (3) */
    uint32_t dodge_inv_to;       /* premier tick NON invulnérable (15) */
    float dodge_cost;            /* endurance (15) */
    float dodge_distance;        /* mètres parcourus sur toute l'esquive (3,0) */
    uint32_t stagger_ticks;      /* durée du Stagger (60) */
    uint32_t hitstun_base;       /* ticks de hitstun = base + posture du coup, bornés (10) */
    uint32_t hitstun_max;        /* (60) */
    float knockback_per_posture; /* m par point de posture (0,02) */
    float damage_scale;          /* dégâts INFLIGÉS x (progression, M8-08) ; 0 : 1 */
    float armor_scale;           /* dégâts REÇUS x (armure, butin des boss) ; 0 : 1 */
} EngFighterParams;

typedef struct EngFighterInput {
    bool light; /* pressée CE tick */
    bool heavy;
    bool dodge;
    bool block_pressed;
    bool block_held;   /* maintenue (y compris le tick de la pression) */
    bool moving;       /* le joueur déplace son personnage (Idle <-> Move) */
    float move_dir[2]; /* direction voulue (x, z) unitaire ou nulle : cap de l'esquive */
} EngFighterInput;

enum {
    ENG_FEV_ATTACK_START = 1U << 0,
    ENG_FEV_ATTACK_ACTIVE = 1U << 1, /* premier tick actif de l'attaque */
    ENG_FEV_ATTACK_END = 1U << 2,
    ENG_FEV_COMBO = 1U << 3, /* enchaînement vers `next` ou la lourde */
    ENG_FEV_DODGE_START = 1U << 4,
    ENG_FEV_PARRY_START = 1U << 5,
    ENG_FEV_BLOCK_START = 1U << 6,
    ENG_FEV_NO_STAMINA = 1U << 7, /* action refusée faute d'endurance */
    ENG_FEV_HITSTUN = 1U << 8,
    ENG_FEV_STAGGER = 1U << 9,
    ENG_FEV_DEATH = 1U << 10,
    ENG_FEV_PARRY_SUCCESS = 1U << 11,
    ENG_FEV_BLOCKED_HIT = 1U << 12,
    ENG_FEV_RECOVERED = 1U << 13 /* sortie de hitstun / stagger */
};

typedef struct EngFighter {
    EngFighterParams params;
    EngCombatFsm fsm;
    EngVitals vitals;
    EngInputBuffer buffer;
    EngHitTracker tracker;
    uint32_t tick; /* tick courant (0 avant le premier) */
    float stamina;
    float posture;
    uint32_t stamina_wait; /* ticks restant avant la régénération d'endurance */
    uint32_t posture_wait;
    uint32_t attack;      /* indice de l'attaque en cours ou ENG_ATTACK_NONE */
    uint32_t attack_tick; /* ticks écoulés depuis le début de l'attaque (0 au tick de départ) */
    uint32_t attack_serial;
    uint32_t state_tick; /* ticks écoulés dans Parry / Dodge */
    uint32_t stun_left;  /* ticks restant en hitstun / stagger */
    float dodge_dir[2];
    float root_delta[2]; /* déplacement imposé ce tick (esquive), mètres */
    float yaw;           /* cap (radians, 0 = -Z), renseigné par l'appelant */
    uint32_t events;     /* ENG_FEV_* du tick courant (remis à 0 au début du tick) */
} EngFighter;

/* Valeurs de docs/combat-design.md. */
void eng_fighter_default_params(EngFighterParams *p);

/* Refusé (false, inchangé) : valeur non finie ou incohérente (inv_from >= inv_to, inv_to >
 * dodge_total, parry_frames 0, max <= 0, multiplicateur hors [0 ; 1], noms vides...). */
bool eng_fighter_set_params(EngFighter *f, const EngFighterParams *p);

/* Combattant neuf : PV pleins, endurance et posture pleines, état Idle. */
void eng_fighter_init(EngFighter *f, const EngFighterParams *params /* NULL : défauts */);

/* Un tick. `attacks` peut être NULL (aucune attaque possible). */
void eng_fighter_tick(EngFighter *f, const EngAttackSet *attacks, const EngFighterInput *in);

/* Met les pressions du tick dans le buffer SANS avancer le combattant (gel d'impact : une entrée
 * n'est jamais perdue). Appelée aussi par eng_fighter_tick. */
void eng_fighter_buffer_inputs(EngFighter *f, const EngFighterInput *in);

/* Attaque en cours (NULL si l'état n'est pas Attack ou si l'indice est hors ensemble). */
const EngAttack *eng_fighter_attack(const EngFighter *f, const EngAttackSet *attacks);

/* Les hitboxes de l'attaque en cours sont-elles actives CE tick ? */
bool eng_fighter_hitboxes_active(const EngFighter *f, const EngAttackSet *attacks);

/* Touchable ce tick ? (vivant, hors invincibilité) */
bool eng_fighter_hittable(const EngFighter *f);

/* Invulnérable à cause de l'esquive CE tick ? */
bool eng_fighter_dodge_invulnerable(const EngFighter *f);

/* ── Touches ── */

typedef enum EngHitKind {
    ENG_HITKIND_IGNORED = 0, /* cible non touchable (invulnérable, morte) */
    ENG_HITKIND_DAMAGE,
    ENG_HITKIND_BLOCKED,
    ENG_HITKIND_PARRIED
} EngHitKind;

typedef enum EngReactDir {
    ENG_REACT_FRONT = 0, /* touché de face : poussé vers l'arrière */
    ENG_REACT_BACK,      /* touché dans le dos */
    ENG_REACT_LEFT,      /* touché du côté gauche */
    ENG_REACT_RIGHT
} EngReactDir;

typedef struct EngHitOutcome {
    EngHitKind kind;
    float dealt;
    float posture_dealt;
    bool killed;
    bool defender_staggered; /* posture brisée */
    bool attacker_staggered; /* parade réussie */
    uint32_t hitstop;        /* ticks de gel pour les deux */
    uint32_t stun_ticks;     /* durée de la réaction imposée au défenseur (0 si aucune) */
    EngReactDir dir;
    float knockback[2]; /* déplacement de recul (x, z), mètres, dans le sens de la force */
} EngHitOutcome;

/* Direction de réaction : `force_dir` = vecteur unitaire (x, z) allant de l'attaquant VERS le
 * défenseur ; `defender_yaw` : cap du défenseur (0 = regarde -Z). Seuils à 45°. */
EngReactDir eng_react_dir(float defender_yaw, const float force_dir[2]);

/* Applique la touche d'`attacker` (attaque `atk`, zone touchée) à `defender`. Met à jour les deux
 * combattants (états, PV, posture, événements). `force_dir` : voir eng_react_dir (non nul, sinon
 * (0,0) et aucun recul). Arguments NULL : IGNORED. */
EngHitOutcome eng_combat_apply_hit(EngFighter *attacker, EngFighter *defender, const EngAttack *atk,
                                   EngHitZone zone, const float force_dir[2]);

/* Empreinte de l'état (pour les tests de déterminisme et les replays, M5-18). FNV-1a 64 bits de
 * l'état, des PV, de l'endurance, de la posture et des compteurs. */
uint64_t eng_fighter_hash(const EngFighter *f);
