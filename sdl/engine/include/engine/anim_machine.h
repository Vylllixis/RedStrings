#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/anim_blend.h"
#include "engine/anim_blendspace.h"
#include "engine/anim_clip.h"
#include "engine/anim_graph.h"
#include "engine/anim_rootmotion.h"
#include "engine/arena.h"
#include "engine/pose.h"
#include "engine/skeleton.h"

/* Exécution d'une machine à états d'animation (M4-19) décrite par une EngAnimGraphDef.
 *
 * Un état joue soit un clip, soit un espace de mélange 1D (M4-18) piloté par un paramètre. Les
 * transitions (dans l'ordre du fichier, la première vraie gagne) basculent d'un état à l'autre avec
 * un fondu (poids smoothstep, comme EngCrossfade). Un changement d'état PENDANT un fondu fige la
 * pose affichée comme source du nouveau fondu : aucun saut visible.
 *
 * Rechargement à chaud : eng_animmachine_set_def peut être rappelée à tout moment avec une nouvelle
 * définition. Les clips sont résolus par le `finder` ; si l'un manque, TOUT est refusé et
 * l'ancienne définition continue. Si l'état courant existe encore (même nom), il garde son instant
 * / sa phase ; sinon on repart de l'état initial. Les paramètres gardent leur valeur par nom.
 *
 * ROOT MOTION (M4-20) : si la définition contient `root_motion ARTICULATION`, la pose rendue est
 * « en place » et `m->motion` donne le déplacement du personnage pour la dernière mise à jour
 * (translation horizontale + lacet, dans le repère du personnage, voir anim_rootmotion.h), mélangé
 * pendant les fondus avec les mêmes poids que la pose. L'articulation doit exister et être une
 * racine (sans parent) ; sinon set_def refuse. Source figée (changement d'état en plein fondu) :
 * elle ne contribue plus au déplacement.
 *
 * Rien n'est alloué après eng_animmachine_init (les clips sont fournis par l'appelant). */

/* Fournit le clip nommé (chemin tel qu'écrit dans le fichier) ; NULL si introuvable. */
typedef const EngAnimClip *(*EngAnimClipFinder)(void *user, const char *name, const char *path);

typedef struct EngAnimSlot {
    uint32_t state;
    EngAnimLayer layer; /* état clip */
    bool ended;
    EngBlendSpace1D bs; /* état blend */
    EngPose pose;       /* sortie d'un état clip */
} EngAnimSlot;

typedef struct EngAnimMachine {
    const EngSkeleton *skeleton;
    EngAnimGraphDef def;
    const EngAnimClip *clips[ENG_ANIMGRAPH_MAX_CLIPS];
    float params[ENG_ANIMGRAPH_MAX_PARAMS];
    EngAnimSlot slots[2];
    uint32_t cur; /* slots[cur] : état courant ; slots[1 - cur] : état sortant */
    bool has_from;
    bool from_frozen; /* la source est `snapshot` */
    float fade;
    float elapsed;
    EngPose snapshot;
    EngPose pose; /* SORTIE : pose locale */
    uint32_t
        root_joint; /* M4-20 : articulation racine résolue, UINT32_MAX : root motion désactivé */
    EngRootMotion motion; /* SORTIE : delta de la dernière mise à jour (repère du personnage) */
    bool ready;
    uint32_t transitions_taken; /* compteur, pour les tests et le débogage */
} EngAnimMachine;

bool eng_animmachine_init(EngAnimMachine *m, EngArena *arena, const EngSkeleton *skeleton);

/* Installe (ou remplace) la définition. false (état inchangé) si un clip est introuvable ou ne va
 * pas avec le squelette, ou si un clip d'espace de mélange a une durée nulle. */
bool eng_animmachine_set_def(EngAnimMachine *m, const EngAnimGraphDef *def,
                             EngAnimClipFinder finder, void *user);

/* Valeur d'un paramètre (nom ou indice). false si inconnu ou non fini. */
bool eng_animmachine_set_param(EngAnimMachine *m, const char *name, float value);
bool eng_animmachine_set_param_index(EngAnimMachine *m, uint32_t index, float value);

/* Évalue les transitions, avance de `dt` (négatif / non fini : 0) et calcule m->pose. false si la
 * machine n'a pas de définition ou si un clip échoue à s'évaluer. */
bool eng_animmachine_update(EngAnimMachine *m, float dt);

const char *eng_animmachine_state_name(const EngAnimMachine *m); /* "" sans définition */
bool eng_animmachine_in_transition(const EngAnimMachine *m);
