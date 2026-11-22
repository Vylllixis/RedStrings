#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/anim_clip.h"
#include "engine/arena.h"
#include "engine/debug_lines.h"
#include "engine/pose.h"
#include "engine/skeleton.h"

/* Débogueur d'animation (M4-16) : lit UN clip sur UN squelette et le montre en os (debug draw),
 * avec les commandes d'un lecteur vidéo : pause, image par image, vitesse (négative : à l'envers),
 * boucle, saut à un instant. On peut y suivre une articulation : ses axes et la TRAJECTOIRE qu'elle
 * décrit pendant tout le clip (c'est ainsi qu'on voit un pied glisser ou une main faire un à-coup).
 * Le squelette de repos peut être dessiné en gris derrière, pour comparer.
 *
 * Les réglages sont des champs publics (EngAnimDebug) pour que l'interface de debug les lie
 * directement (engine/tweak.h) ; chaque mise à jour les VÉRIFIE et les ramène dans leur plage
 * (instant dans [0 ; durée], vitesse bornée, articulation existante, cadence > 0) : une valeur
 * absurde saisie à la souris n'a donc aucun effet durable.
 *
 * Rien n'est alloué après eng_anim_debug_init. `skeleton` et `clip` doivent vivre aussi longtemps
 * que le débogueur (non copiés). */

#define ENG_ANIM_DEBUG_TRAIL_SEGMENTS 96U /* segments de la trajectoire d'une articulation */
#define ENG_ANIM_DEBUG_MAX_SPEED 4.0F     /* vitesse de lecture maximale, en valeur absolue */
#define ENG_ANIM_DEBUG_DEFAULT_RATE 30.0F /* images par seconde de l'affichage « image n / N » */

typedef struct EngAnimDebug {
    const EngSkeleton *skeleton;
    const EngAnimClip *clip;
    /* Réglages (modifiables par l'appelant ou l'interface) */
    float time;       /* secondes, dans [0 ; durée] */
    float speed;      /* facteur de lecture, dans [-4 ; 4] ; 1 : temps réel */
    float frame_rate; /* Hz : pas d'« image suivante » et numérotation ; > 0 */
    bool paused;      /* true : le temps n'avance plus */
    bool loop;        /* true : le temps tourne ; false : lecture jusqu'au bout puis pause */
    bool show_rest;   /* dessine aussi la pose de repos, en gris */
    bool show_trail;  /* dessine la trajectoire de l'articulation suivie */
    int32_t selected; /* articulation suivie, -1 : aucune */
    /* État calculé */
    EngPose pose;
    mat4 *world;        /* joint_count : poses monde à `time` */
    mat4 *rest_world;   /* joint_count : pose de repos */
    EngPose trail_pose; /* travail de la trajectoire */
    mat4 *trail_world;
} EngAnimDebug;

/* Prépare le débogueur (allocations dans `arena`) : temps 0, vitesse 1, boucle, en lecture, aucune
 * articulation suivie, trajectoire affichée. Refusé (false, journalisé, arène restituée) : argument
 * NULL, squelette invalide, clip dont un canal vise une articulation absente du squelette, clip de
 * durée non finie, mémoire insuffisante. */
bool eng_anim_debug_init(EngAnimDebug *dbg, EngArena *arena, const EngSkeleton *skeleton,
                         const EngAnimClip *clip);

/* Fait avancer le temps de `dt` secondes (x vitesse) si on n'est pas en pause, vérifie les réglages
 * puis recalcule la pose. Sans boucle, le temps s'arrête à la fin (ou au début en marche arrière)
 * et la lecture passe en pause. `dt` non fini ou négatif : ignoré (la pose est quand même
 * recalculée). false si `dbg` n'est pas initialisé ou si le calcul de pose échoue. */
bool eng_anim_debug_update(EngAnimDebug *dbg, float dt);

/* Avance de `frames` images (négatif : recule) à `frame_rate`, en pause. Boucle : le temps tourne ;
 * sinon il est borné à [0 ; durée]. */
bool eng_anim_debug_step(EngAnimDebug *dbg, int32_t frames);

/* Va à l'instant `time` (borné à [0 ; durée]). false si `time` n'est pas fini. */
bool eng_anim_debug_seek(EngAnimDebug *dbg, float time);

/* Numéro de l'image courante (à partir de 0) et nombre d'images du clip à `frame_rate`. */
uint32_t eng_anim_debug_frame(const EngAnimDebug *dbg);
uint32_t eng_anim_debug_frame_count(const EngAnimDebug *dbg);

/* Dessine dans `lines`, SUPERPOSÉ (visible à travers la matière ; le mode de la liste est rétabli
 * ensuite) : os cyan (celui de l'articulation suivie en jaune), pose de repos grise si `show_rest`,
 * et pour l'articulation suivie ses axes et, si `show_trail`, sa trajectoire magenta sur tout le
 * clip. `root` : placement du personnage dans le monde. Retourne le nombre d'os dessinés dans la
 * pose courante ; 0 si un argument est NULL. */
uint32_t eng_anim_debug_draw(EngAnimDebug *dbg, mat4 root, EngDebugLines *lines);

/* Ligne d'état pour l'interface : « t 0.53 / 1.20 s · image 16 / 36 · x1.00 · lecture · boucle ·
 * suivi : 2 Spine ». Tronquée à `capacity` ; retourne la longueur écrite (sans le zéro). */
size_t eng_anim_debug_describe(const EngAnimDebug *dbg, char *out, size_t capacity);
