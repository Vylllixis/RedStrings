#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/anim_blendspace.h"
#include "engine/anim_clip.h"
#include "engine/arena.h"
#include "engine/combat_state.h"
#include "engine/pose.h"
#include "engine/skeleton.h"

/* Animation d'un acteur de combat (M4) : le JEU dit ce qui se passe (vitesse au sol, au sol ou en
 * l'air, état de la machine de combat et son tick), l'animateur choisit quoi jouer et produit une
 * pose locale, avec un fondu à chaque changement.
 *
 *   état de combat          programme joué
 *   ATTACK                  clip de l'attaque, instant = décalage + (tick + fraction) / 60 x
 * vitesse (le clip est CALÉ sur la frame data : voir attack_data.h, ligne `anim`) DODGE roulade :
 * pose recroquevillée + rotation de la racine de 2π x avancée autour de l'axe (haut x direction) ;
 * rayon = distance / 2π, donc la pose roule sans glisser sur la distance parcourue par le jeu
 * (M4-20) BLOCK / PARRY           garde (boucle) / parade (une fois) HITSTUN / STAGGER       touché
 * / déséquilibre (une fois) DEAD                    mort (une fois, dernière image tenue) IDLE /
 * MOVE             au sol : espace de mélange repos / marche / course réglé par la vitesse ; en
 * l'air : début de saut puis boucle « en l'air » ; atterrissage court
 *
 * Pieds sans glissement (M4-18) : les positions de l'espace de mélange SONT les vitesses au sol de
 * chaque clip (walk_speed, run_speed) ; au-delà de la course, la lecture accélère dans la même
 * proportion que la vitesse. Fondus (M4-17) : à chaque changement de programme, la pose affichée
 * est figée et mélangée vers la nouvelle (courbe eng_crossfade_curve) : aucun saut de pose.
 *
 * Unités : tout est dans l'unité du SQUELETTE (vitesses et rayon divisés par l'échelle de rendu).
 * Rien n'est alloué après eng_actor_anim_init. */

typedef enum EngActorRole {
    ENG_ACTOR_IDLE = 0,
    ENG_ACTOR_WALK,
    ENG_ACTOR_RUN,
    ENG_ACTOR_JUMP_START,
    ENG_ACTOR_JUMP_AIR,
    ENG_ACTOR_JUMP_LAND,
    ENG_ACTOR_ROLL_POSE, /* pose recroquevillée de la roulade (lue à `roll_pose_time`) */
    ENG_ACTOR_BLOCK,
    ENG_ACTOR_PARRY,
    ENG_ACTOR_HIT,
    ENG_ACTOR_STAGGER,
    ENG_ACTOR_DEATH,
    ENG_ACTOR_ROLE_COUNT
} EngActorRole;

typedef struct EngActorSet {
    const EngAnimClip *clips[ENG_ACTOR_ROLE_COUNT]; /* NULL : rôle absent (pose de repos) */
    float walk_speed;     /* vitesse au sol du clip de marche (unité du squelette / s), > 0 */
    float run_speed;      /* idem pour la course, > walk_speed */
    float roll_pose_time; /* instant lu dans ENG_ACTOR_ROLL_POSE, s */
    float roll_radius;    /* rayon de la roulade (unité du squelette) : distance / 2π ; 0 : pas de
                             rotation */
    uint32_t root_joint;  /* articulation tournée par la roulade */
    float fade;           /* durée des fondus, s, dans [0 ; 1] */
    float speed_smooth;   /* constante de temps du lissage de la vitesse qui règle le mélange
                             repos / marche / course, s, dans [0 ; 1] (0 : aucun) : sans lui, un
                             démarrage (0 -> 0,5 m/s en un tick) ferait sauter les jambes */
    float land_time;      /* durée jouée de l'atterrissage, s */
} EngActorSet;

/* Programmes (ce qui est joué) ; exposés pour les tests et le débogage. */
typedef enum EngActorProgram {
    ENG_AP_LOCOMOTION = 0,
    ENG_AP_JUMP_START,
    ENG_AP_JUMP_AIR,
    ENG_AP_JUMP_LAND,
    ENG_AP_ATTACK,
    ENG_AP_ROLL,
    ENG_AP_BLOCK,
    ENG_AP_PARRY,
    ENG_AP_HIT,
    ENG_AP_STAGGER,
    ENG_AP_DEATH,
    ENG_AP_COUNT
} EngActorProgram;

typedef struct EngActorDrive {
    float ground_speed; /* vitesse horizontale, unité du squelette / s */
    bool grounded;
    EngCombatState state;
    uint32_t state_ticks; /* ticks depuis l'entrée dans l'état (pour ATTACK : depuis le début de
                             l'attaque, EngFighter.attack_tick) */
    float tick_fraction;  /* [0 ; 1[ : position du rendu entre deux ticks */
    const EngAnimClip *attack_clip; /* ATTACK : clip de l'attaque (NULL : repos) */
    float attack_speed;             /* ATTACK : vitesse de lecture, > 0 */
    float attack_offset;            /* ATTACK : instant du clip au tick 0, s */
    uint32_t attack_serial;         /* ATTACK : change à chaque nouvelle attaque (combo) */
    float dodge_progress;           /* DODGE : part de la distance parcourue, [0 ; 1] */
    float dodge_dir[2];             /* DODGE : direction (x, z) dans le repère du squelette */
} EngActorDrive;

typedef struct EngActorAnim {
    const EngSkeleton *skeleton;
    const EngActorSet *set;
    EngBlendSpace1D loco;
    EngPose pose;   /* SORTIE : pose locale */
    EngPose target; /* programme courant, avant fondu */
    EngPose source; /* pose figée au dernier changement */
    EngActorProgram program;
    uint32_t serial;
    float fade_left;
    float fade_total;
    float air_time;  /* s depuis le décollage */
    float land_left; /* s d'atterrissage restant à jouer */
    float loop_time; /* temps des programmes en boucle (garde, en l'air) */
    float speed;     /* vitesse lissée (speed_smooth) qui règle l'espace de mélange */
    bool grounded;
    bool started;
} EngActorAnim;

/* `set` et `skeleton` doivent vivre aussi longtemps que l'animateur. false (journalisé) si un clip
 * ne vise pas ce squelette, si les vitesses sont incohérentes ou la mémoire insuffisante. */
bool eng_actor_anim_init(EngActorAnim *a, EngArena *arena, const EngSkeleton *skeleton,
                         const EngActorSet *set);

/* Avance de `dt` secondes (>= 0) et calcule a->pose. false si les arguments sont invalides (la
 * pose garde sa valeur précédente). */
bool eng_actor_anim_update(EngActorAnim *a, const EngActorDrive *drive, float dt);

/* Nom stable d'un programme (« locomotion », « attack »...) ; « ? » hors bornes. */
const char *eng_actor_program_name(EngActorProgram program);
