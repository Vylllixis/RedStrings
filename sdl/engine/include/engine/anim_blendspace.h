#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/anim_blend.h"
#include "engine/anim_clip.h"
#include "engine/anim_rootmotion.h"
#include "engine/arena.h"
#include "engine/pose.h"
#include "engine/skeleton.h"

/* Espace de mélange 1D (M4-18) : un paramètre (typiquement la vitesse au sol, m/s) choisit deux
 * clips voisins et leur poids. Exemple : repos à 0, marche à 1,5, course à 4,5.
 *
 *   paramètre p entre deux échantillons a < b : t = (p - a) / (b - a), pose = blend(clip_a, clip_b,
 * t). Hors de [premier ; dernier] : borné (pas d'extrapolation).
 *
 * SYNCHRONISATION DE PHASE : tous les clips d'un espace sont des cycles (le même pas de marche ou
 * de course) ; ils sont lus sur la MÊME phase p in [0 ; 1[, donc un pied est au même point du cycle
 * dans les deux clips (sans cela, marche -> course ferait glisser les pieds). Le temps de lecture
 * de chaque clip est phase x sa durée. La durée du cycle mélangé est l'interpolation des durées
 * (même poids) : à mi-chemin entre une marche de 1,2 s et une course de 0,6 s, un cycle dure 0,9 s.
 * La phase est conservée quand le paramètre change : pas de redémarrage.
 *
 * Hypothèses (documentées, non vérifiées par le moteur) : tous les clips commencent au même point
 * du cycle (ex. pied gauche posé). Un clip qui ne respecte pas ça se verra comme un glissement.
 *
 * Rien n'est alloué après eng_blendspace_init. */

#define ENG_BLENDSPACE_MAX_SAMPLES 8U

typedef struct EngBlendSample {
    const EngAnimClip *clip;
    float position; /* valeur du paramètre pour laquelle ce clip est joué seul */
} EngBlendSample;

typedef struct EngBlendSpace1D {
    const EngSkeleton *skeleton;
    EngBlendSample samples[ENG_BLENDSPACE_MAX_SAMPLES];
    uint32_t count;
    float parameter; /* dernier paramètre reçu, borné aux positions */
    float phase;     /* [0 ; 1[ */
    EngPose pose;    /* SORTIE : pose locale */
    EngPose scratch;
    uint32_t root_joint;  /* M4-20 : racine du root motion, UINT32_MAX : désactivé */
    EngRootMotion motion; /* SORTIE : delta de la dernière mise à jour (repère du personnage) */
} EngBlendSpace1D;

bool eng_blendspace_init(EngBlendSpace1D *bs, EngArena *arena, const EngSkeleton *skeleton);

/* Active le root motion (M4-20) sur l'articulation `joint` (UINT32_MAX : désactivé, défaut). La
 * pose produite est alors « en place » (eng_rootmotion_strip appliqué à chaque clip AVANT le
 * mélange) et `bs->motion` reçoit le delta de la mise à jour : le mélange pondéré des deltas des
 * deux clips, chacun lu sur la phase partagée. false si `joint` dépasse le squelette. */
bool eng_blendspace_set_root_joint(EngBlendSpace1D *bs, uint32_t joint);

/* Vide l'espace (échantillons, paramètre, phase) en gardant la mémoire : permet de le reconfigurer
 * sans allocation. */
void eng_blendspace_clear(EngBlendSpace1D *bs);

/* Ajoute un échantillon. Les positions doivent être STRICTEMENT croissantes (ajout dans l'ordre) ;
 * le clip doit avoir une durée > 0 finie et viser ce squelette ; au plus
 * ENG_BLENDSPACE_MAX_SAMPLES. false (état inchangé) sinon. */
bool eng_blendspace_add(EngBlendSpace1D *bs, const EngAnimClip *clip, float position);

/* Segment actif pour `parameter` : indices des deux échantillons (égaux hors de l'intervalle ou
 * avec un seul échantillon) et poids t du second. false s'il n'y a aucun échantillon. */
bool eng_blendspace_segment(const EngBlendSpace1D *bs, float parameter, uint32_t *lo, uint32_t *hi,
                            float *t);

/* Durée du cycle mélangé pour `parameter` (0 sans échantillon). */
float eng_blendspace_cycle(const EngBlendSpace1D *bs, float parameter);

/* Impose la phase (modulo 1 ; non fini : refusé). Sert à reprendre un cycle déjà en cours. */
bool eng_blendspace_set_phase(EngBlendSpace1D *bs, float phase);

/* Avance de `dt` secondes (négatif / non fini : ignoré) avec `parameter` (NaN : on garde le
 * précédent), puis évalue bs->pose. false s'il n'y a aucun échantillon ou si l'évaluation échoue.
 */
bool eng_blendspace_update(EngBlendSpace1D *bs, float dt, float parameter);
