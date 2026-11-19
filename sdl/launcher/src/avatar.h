#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/actor_anim.h"
#include "engine/anim_clip.h"
#include "engine/arena.h"
#include "engine/combat_geom.h"
#include "engine/environment.h"
#include "engine/lighting.h"
#include "engine/math.h"
#include "engine/model_gpu.h"
#include "engine/renderer.h"
#include "engine/skeleton.h"

/* Personnages animés du mode jouable (M4) : chevalier (joueur) et squelettes (ennemis), KayKit CC0
 * (assets/LICENCES.md). Cuits au build (tools/CMakeLists.txt : ow_cook_character) ; tous partagent
 * le squelette et les clips du chevalier (même rig, 41 articulations).
 *
 * Deux moitiés :
 *   - CPU (squelette, clips, réglages de l'animateur) : chargée même sans GPU (autotests) ;
 *   - GPU (maillages skinnés, armes, pipelines) : seulement quand un renderer existe.
 * Les armes sont des maillages statiques attachés à un os par un socket (handslot.r / handslot.l
 * du rig, plus le décalage que KayKit leur donne dans le fichier du chevalier).
 *
 * Repères : KayKit regarde vers +Z, le jeu vers -Z (lacet 0) : le modèle est tourné d'un demi-tour.
 * Échelle de rendu AVATAR_SCALE (le chevalier mesure 2,3 m dans son unité). */

#define AVATAR_SCALE 0.8F
#define AVATAR_MAX_JOINTS 64U

typedef enum AvatarBody {
    AVATAR_KNIGHT = 0,
    AVATAR_SKELETON_MINION,
    AVATAR_SKELETON_WARRIOR,
    AVATAR_SKELETON_ROGUE,
    AVATAR_SKELETON_MAGE,
    AVATAR_BODY_COUNT
} AvatarBody;

typedef enum AvatarProp {
    AVATAR_PROP_NONE = -1,
    AVATAR_SWORD = 0,
    AVATAR_GREATSWORD,
    AVATAR_AXE_2H,
    AVATAR_SHIELD,
    AVATAR_SKELETON_BLADE,
    AVATAR_SKELETON_AXE,
    AVATAR_SKELETON_SHIELD,
    AVATAR_SKELETON_CROSSBOW,
    AVATAR_SKELETON_STAFF,
    AVATAR_PROP_COUNT
} AvatarProp;

#define AVATAR_CLIP_COUNT 33U

typedef struct AvatarLibrary {
    bool cpu_ready;
    bool gpu_ready;
    EngArena arena; /* squelette, clips, tables des modèles GPU */
    EngSkeleton skeleton;
    EngAnimClip clips[AVATAR_CLIP_COUNT];
    bool clip_ok[AVATAR_CLIP_COUNT];
    EngActorSet set;
    uint32_t joint_root;
    uint32_t joint_hand_r;
    uint32_t joint_hand_l;
    EngModelGpu bodies[AVATAR_BODY_COUNT];
    bool body_ok[AVATAR_BODY_COUNT];
    EngModelGpu props[AVATAR_PROP_COUNT];
    bool prop_ok[AVATAR_PROP_COUNT];
    EngPipelineHandle skinned;
    EngPipelineHandle skinned_shadow;
    EngPipelineHandle prop;
    EngPipelineHandle prop_shadow;
} AvatarLibrary;

typedef struct Avatar {
    bool ready;
    AvatarBody body;
    AvatarProp right;
    AvatarProp left;
    float scale; /* multiplie AVATAR_SCALE (boss plus grand) */
    EngActorAnim anim;
    mat4 *joint_world; /* pose monde (repère du personnage), joint_count matrices */
    mat4 *palette;     /* matrices de skinning envoyées au GPU */
    mat4 model;        /* personnage -> monde */
} Avatar;

/* Partie CPU : lit assets/cooked/characters/knight.skel et les clips. false (journalisé) si un
 * fichier manque : le jeu garde alors ses capsules en fil de fer. */
bool avatar_library_load_cpu(AvatarLibrary *lib);
/* Partie GPU (après la CPU) : maillages, armes, pipelines. `env` (peut être NULL) : éclairage par
 * image de la scène. */
bool avatar_library_load_gpu(AvatarLibrary *lib, EngRenderer *renderer, const EngEnvironment *env);
void avatar_library_shutdown(AvatarLibrary *lib, EngRenderer *renderer);

/* Clip par nom (« slash_h »...) ; NULL si inconnu ou non chargé. */
const EngAnimClip *avatar_clip(const AvatarLibrary *lib, const char *name);

/* Un personnage : mémoire prise dans l'arène de la bibliothèque. */
bool avatar_init(Avatar *av, AvatarLibrary *lib, AvatarBody body, AvatarProp right, AvatarProp left,
                 float scale);

/* Une image : avance l'animation de `dt` avec `drive` (vitesses en m/s du MONDE : converties ici),
 * place le personnage (pieds `feet`, lacet `yaw` du jeu) et calcule la palette. */
void avatar_update(Avatar *av, const AvatarLibrary *lib, const EngActorDrive *drive, float dt,
                   const float feet[3], float yaw);

/* Passe principale et passe d'ombre. */
void avatar_draw(const Avatar *av, const AvatarLibrary *lib, EngRenderQueue *queue, mat4 view_proj,
                 const EngLighting *light, const EngModelShadow *shadow);
void avatar_draw_shadow(const Avatar *av, const AvatarLibrary *lib, EngRenderQueue *queue,
                        mat4 light_view_proj);

/* Matrice monde de l'arme tenue en main droite (ou gauche) ; false sans arme ou sans pose. */
bool avatar_prop_world(const Avatar *av, const AvatarLibrary *lib, bool right, mat4 out);

/* Placement personnage -> monde (pieds, lacet du jeu, échelle AVATAR_SCALE x `scale`). */
void avatar_model_matrix(const float feet[3], float yaw, float scale, mat4 out);
/* Matrice monde d'une arme `prop` pour une pose `joint_world` et un placement `model`. */
bool avatar_prop_matrix(const AvatarLibrary *lib, AvatarProp prop, mat4 *joint_world, mat4 model,
                        mat4 out);
/* Capsule de la LAME (M5-06) de `prop` dans le monde ; false pour une arme sans tranchant
 * (bouclier, arbalète) ou sans arme. `scale` : celui du personnage (x AVATAR_SCALE). */
bool avatar_blade_world(const AvatarLibrary *lib, AvatarProp prop, mat4 *joint_world, mat4 model,
                        float scale, EngCapsule *out);
