#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/files.h"

/* Description d'une machine à états d'animation (M4-19) : un fichier TEXTE de données, relu à
 * chaud. Ce module ne contient que les DONNÉES (analyse + validation + rechargement) ; l'exécution
 * est dans anim_machine.h.
 *
 * Format (une instruction par ligne, `#` commence un commentaire, séparateurs : espaces) :
 *
 *   param NOM [valeur_initiale]                  paramètre flottant (vitesse, 0/1 pour un
 * booléen...) clip NOM chemin.anim [loop|once] [speed=F]   clip nommé (loop par défaut, vitesse 1
 * par défaut) state NOM clip NOM_DU_CLIP                   état qui joue un clip state NOM blend
 * PARAM CLIP:POS CLIP:POS ...  état en espace de mélange 1D (M4-18), POS croissantes transition DE
 * -> VERS when PARAM OP VALEUR [fade SECONDES] transition DE -> VERS when ended [fade SECONDES]
 * (clip « once » arrivé à sa fin) start NOM                                    état initial
 *
 * DE peut être `any` (depuis n'importe quel état, sauf VERS). OP : > < >= <=. Les transitions sont
 * essayées dans l'ordre du fichier, la première vraie gagne. Une transition SANS condition est une
 * erreur (elle partirait en boucle). `root_motion` (M4-20) : la machine rend alors le déplacement
 * du personnage (delta de cette articulation, voir anim_rootmotion.h) et une pose « en place ». Les
 * clips d'un espace de mélange doivent être `loop`.
 *
 * Toute erreur (référence inconnue, doublon, limite dépassée, nombre invalide...) refuse le fichier
 * ENTIER avec un message « ligne N : ... » : aucune définition à moitié lue. */

#define ENG_ANIMGRAPH_MAX_NAME 32U
#define ENG_ANIMGRAPH_MAX_PATH 128U
#define ENG_ANIMGRAPH_MAX_PARAMS 8U
#define ENG_ANIMGRAPH_MAX_CLIPS 32U
#define ENG_ANIMGRAPH_MAX_STATES 16U
#define ENG_ANIMGRAPH_MAX_TRANSITIONS 64U
#define ENG_ANIMGRAPH_MAX_SAMPLES 8U /* = ENG_BLENDSPACE_MAX_SAMPLES */
#define ENG_ANIMGRAPH_ANY UINT32_MAX
#define ENG_ANIMGRAPH_MAX_FADE 10.0F

typedef enum EngAnimCond {
    ENG_ANIMCOND_ENDED = 0,
    ENG_ANIMCOND_GT,
    ENG_ANIMCOND_LT,
    ENG_ANIMCOND_GE,
    ENG_ANIMCOND_LE
} EngAnimCond;

typedef struct EngAnimGraphParam {
    char name[ENG_ANIMGRAPH_MAX_NAME];
    float initial;
} EngAnimGraphParam;

typedef struct EngAnimGraphClip {
    char name[ENG_ANIMGRAPH_MAX_NAME];
    char path[ENG_ANIMGRAPH_MAX_PATH];
    bool loop;
    float speed;
} EngAnimGraphClip;

typedef enum EngAnimStateKind { ENG_ANIMSTATE_CLIP = 0, ENG_ANIMSTATE_BLEND = 1 } EngAnimStateKind;

typedef struct EngAnimGraphState {
    char name[ENG_ANIMGRAPH_MAX_NAME];
    EngAnimStateKind kind;
    uint32_t clip;  /* ENG_ANIMSTATE_CLIP : indice du clip */
    uint32_t param; /* ENG_ANIMSTATE_BLEND : indice du paramètre */
    uint32_t sample_count;
    uint32_t sample_clip[ENG_ANIMGRAPH_MAX_SAMPLES];
    float sample_position[ENG_ANIMGRAPH_MAX_SAMPLES];
} EngAnimGraphState;

typedef struct EngAnimGraphTransition {
    uint32_t from; /* indice d'état ou ENG_ANIMGRAPH_ANY */
    uint32_t to;
    EngAnimCond cond;
    uint32_t param; /* conditions de comparaison */
    float threshold;
    float fade; /* secondes, 0 : immédiat */
} EngAnimGraphTransition;

typedef struct EngAnimGraphDef {
    uint32_t param_count;
    uint32_t clip_count;
    uint32_t state_count;
    uint32_t transition_count;
    uint32_t start; /* indice de l'état initial */
    bool has_root_motion;
    char root_joint[ENG_ANIMGRAPH_MAX_NAME]; /* nom de l'articulation racine (root_motion) */
    EngAnimGraphParam params[ENG_ANIMGRAPH_MAX_PARAMS];
    EngAnimGraphClip clips[ENG_ANIMGRAPH_MAX_CLIPS];
    EngAnimGraphState states[ENG_ANIMGRAPH_MAX_STATES];
    EngAnimGraphTransition transitions[ENG_ANIMGRAPH_MAX_TRANSITIONS];
} EngAnimGraphDef;

/* Analyse `text` (terminé par un octet nul). true : `out` est complet et cohérent. false : `out`
 * est mis à zéro et `err` (si non NULL) reçoit « ligne N : message ». */
bool eng_animgraph_parse(const char *text, EngAnimGraphDef *out, char *err, size_t err_capacity);

/* Indice du paramètre / de l'état / du clip de ce nom, ou UINT32_MAX. */
uint32_t eng_animgraph_find_param(const EngAnimGraphDef *def, const char *name);
uint32_t eng_animgraph_find_state(const EngAnimGraphDef *def, const char *name);
uint32_t eng_animgraph_find_clip(const EngAnimGraphDef *def, const char *name);

/* ── Fichier surveillé (rechargement à chaud) ── */

typedef struct EngAnimGraphFile {
    char path[512];
    EngFileWatch watch;
    EngAnimGraphDef def; /* DERNIÈRE version valide */
    bool valid;
    uint32_t version; /* +1 à chaque version valide chargée */
    char error[256];  /* dernière erreur (vide si la dernière lecture est bonne) */
} EngAnimGraphFile;

/* Lit et analyse `path` (le tampon de lecture vit dans `scratch`, restitué ensuite). false si le
 * fichier manque ou est invalide (message dans file->error et journal). */
bool eng_animgraph_file_open(EngAnimGraphFile *file, EngArena *scratch, const char *path);

/* À appeler une fois par image avec un temps croissant en secondes. true UNE fois quand une
 * nouvelle version VALIDE vient d'être chargée (file->version a augmenté). Une version invalide est
 * journalisée, enregistrée dans file->error, et l'ancienne définition reste en place. */
bool eng_animgraph_file_poll(EngAnimGraphFile *file, EngArena *scratch, double now_seconds);
