#pragma once

#include <stdbool.h>

#include "engine/arena.h"
#include "engine/debug_lines.h"
#include "engine/heightmap.h"
#include "engine/input_state.h"
#include "engine/jump.h"
#include "engine/locomotion.h"
#include "engine/orbit_camera.h"
#include "engine/physics.h"
#include "engine/stamina.h"
#include "engine/tweak.h"
#include "avatar.h"
#include "play_combat.h"

/* Mode jouable du lanceur (M4, option --play) : assemble ce que M4-06 à M4-11 ont construit, dans
 * une arène en boîtes grises posée sur le sol de la démonstration. Rien ici n'est nouveau : chaque
 * brique a ses tests ; ce module les branche et son autotest (--play-selftest, sans fenêtre)
 * vérifie la chaîne complète par des entrées scriptées.
 *
 *   clavier/souris/manette : ZQSD/WASD déplacement relatif à la caméra, souris = regard,
 *   molette = distance de la caméra, Espace = saut, Maj = sprint (consomme l'endurance).
 *
 * Le personnage est dessiné en fil de fer (capsule du debug draw physique + flèche de direction) :
 * pas de modèle animé avant les tâches d'animation de M4. Pas fixe 60 Hz, appelé depuis la boucle.
 */

#define PLAY_MAX_SLOPE_DEG 45.0F /* personnage Jolt et grille de l'IA (M6-03) */
#define PLAY_SPAWN_X 4.0F
#define PLAY_SPAWN_Z 4.0F
#define PLAY_GROUND_TOP (-0.5F) /* dessus du sol de la démonstration (demo_scene.c) */
#define PLAY_PIVOT_HEIGHT 1.5F  /* m au-dessus des pieds : le haut du dos */
#define PLAY_ARENA_PIECES 6U

typedef struct PlayMode {
    EngPhysics *world;
    EngCharacter *character;
    EngOrbitCamera orbit;
    EngLocomotion loco;
    EngJump jump;
    EngStamina stamina;
    float jump_speed;   /* m/s */
    float air_control;  /* ]0 ; 1] : part des accélérations disponible en l'air */
    float gravity_fall; /* ]0 ; 5], x la gravité en chute (la montée garde 1) */
    float cut_factor;   /* [0 ; 1[ : vitesse verticale gardée si on relâche Espace en montant */
    uint32_t jumps;     /* sauts effectués (autotest, journal) */
    uint32_t landings;  /* atterrissages */
    PlayCombat *combat; /* NULL sans --combat */
    EngArena terrain_arena;
    EngHeightmap map;
    float terrain_half;
    float combat_time; /* secondes de jeu (rechargement des attaques) */
    uint32_t ticks;
    AvatarLibrary *avatars; /* M4 : personnages animés ; NULL : capsules en fil de fer */
    EngModelGpu arena_models[PLAY_ARENA_PIECES]; /* arène dessinée (marches, mur, rampe...) */
    mat4 arena_place[PLAY_ARENA_PIECES];
    uint32_t arena_count;
    Avatar avatar; /* le chevalier du joueur */
    bool sprinting;
    bool ready;
    bool on_terrain; /* --terrain : sol = champ de hauteurs (collision Jolt) au lieu de l'arène */
    bool autoplay;   /* --combat-autoplay : le pilote automatique joue */
    bool show_debug; /* F3 : capsules, boîtes et flèche en plus des modèles */
} PlayMode;

/* Crée le monde (sol, cubes de la démonstration rendus solides, marches, mur, rampe, plateforme),
 * le personnage et la caméra. `tweaks` (peut être NULL) reçoit les réglages et l'endurance. Sans
 * Jolt (OW_PHYSICS=OFF) : false, journalisé. `camera_sensitivity` ]0 ; 10], `invert_y`. */
bool play_mode_init(PlayMode *play, EngTweaks *tweaks, float camera_sensitivity, bool invert_y);
void play_mode_shutdown(PlayMode *play);

/* Comme play_mode_init, mais sur le terrain `heightmap_path` (.hmap, celui que dessine --terrain) :
 * collision par champ de hauteurs Jolt, centré comme le rendu (origine -côté/2), départ posé sur
 * le relief. Pas d'arène en boîtes. */
bool play_mode_init_terrain(PlayMode *play, EngTweaks *tweaks, float camera_sensitivity,
                            bool invert_y, const char *heightmap_path);

/* Hauteur du sol au point (x, z) : relief en mode terrain, PLAY_GROUND_TOP sinon. */
float play_mode_ground(const PlayMode *play, float x, float z);

/* Point de départ d'une nouvelle partie : pieds posés sur le sol en (PLAY_SPAWN_X, PLAY_SPAWN_Z),
 * lacet de départ (regard vers +X). Aucun paramètre ne peut être NULL. */
void play_mode_spawn_point(const PlayMode *play, float feet[3], float *yaw);

/* Replace le personnage (pieds en `feet`), immobile, caméra derrière lui regardant vers `yaw`.
 * Les réglages (vitesses, sensibilité, inversion de la caméra) sont conservés ; l'endurance non
 * plus que le combat ne sont touchés (à l'appelant). */
void play_mode_teleport(PlayMode *play, const float feet[3], float yaw);

/* Active le combat (--combat) : mannequin, camp de 3 ennemis, verrouillage (voir play_combat.h).
 * `audio` : ouvre le son. false (journalisé) si les données de combat sont invalides. */
bool play_mode_enable_combat(PlayMode *play, bool audio);

/* Un pas fixe de `dt` secondes (0 < dt <= 0,1). */
void play_mode_tick(PlayMode *play, const EngInputState *input, float dt);

/* M4 : branche les personnages animés (bibliothèque chargée, GPU compris) ; le joueur devient un
 * chevalier, les combattants des squelettes. false (journalisé) : capsules conservées. */
bool play_mode_attach_avatars(PlayMode *play, AvatarLibrary *lib);
/* Une fois par IMAGE, avant le dessin : anime les personnages (`alpha` : interpolation entre deux
 * pas, `dt` : durée réelle de l'image). */
void play_mode_update_visuals(PlayMode *play, double alpha, float dt);
/* Arène en volumes (au lieu du fil de fer) : modèles GPU des marches, du mur, de la plateforme et
 * de la rampe, éclairés par `env` (peut être NULL). Sans effet sur le terrain. */
bool play_mode_load_arena_visuals(PlayMode *play, EngRenderer *renderer, EngArena *arena,
                                  const EngEnvironment *env);
void play_mode_unload_visuals(PlayMode *play, EngRenderer *renderer);

/* Dessine les personnages (crochet de modèles de la scène, demo_scene_set_model_hook). */
void play_mode_draw_models(const PlayMode *play, EngRenderQueue *queue, mat4 view_proj,
                           const EngLighting *light, const EngModelShadow *shadow,
                           bool shadow_pass);

/* Écrit position et orientation de la caméra du pas courant dans `out` (champ de vision et plans
 * inchangés). */
void play_mode_view(const PlayMode *play, EngCamera *out);

/* Dessin de debug : arène, capsule du personnage (interpolée avec `alpha`), flèche de direction. */
void play_mode_debug_draw(const PlayMode *play, EngDebugLines *lines, double alpha);

/* Autotest sans fenêtre : entrées scriptées, vérifications chiffrées journalisées. true si tout
 * passe. Journalise « AUTOTEST JEU : OK » ou la première erreur. */
bool play_mode_selftest(void);

/* Autotest sur un terrain .hmap : posé, suit le relief, caméra au-dessus. « AUTOTEST TERRAIN : OK
 * ». */
bool play_mode_terrain_selftest(const char *heightmap_path);
