#pragma once

#include <stdbool.h>

#include "engine/creature.h"
#include "engine/zone_decor.h"
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
#define PLAY_DECOR_MAX (2U * ENG_DECOR_MAX) /* décor d'une zone + château de départ */
#define PLAY_WALL_SEGMENTS 48U /* M6-09 : segments des murs de l'arène du boss (terrain) */

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
    PlayCombat *combat; /* NULL sans --combat */
    EngArena terrain_arena;
    EngHeightmap map;
    float terrain_half;
    float combat_time;      /* secondes de jeu (rechargement des attaques) */
    AvatarLibrary *avatars; /* M4 : personnages animés ; NULL : capsules en fil de fer */
    EngModelGpu arena_models[PLAY_ARENA_PIECES]; /* arène dessinée (marches, mur, rampe...) */
    mat4 arena_place[PLAY_ARENA_PIECES];
    mat4 wall_place[PLAY_WALL_SEGMENTS]; /* M6-09 : segment de mur sur le relief (boîte unité) */
    EngModelGpu wall_model;
    EngModelGpu creature_models[ENG_CREATURE_COLORS]; /* boîte unité par couleur (cheval, dragon) */
    /* grande carte : décor des zones (château, camps), posé sur le relief (play_decor.c) */
    mat4 decor_place[PLAY_DECOR_MAX];
    float decor_center[PLAY_DECOR_MAX][3];
    float decor_half[PLAY_DECOR_MAX][3];
    float decor_yaw[PLAY_DECOR_MAX];
    EngBody decor_body[PLAY_DECOR_MAX];
    EngModelGpu decor_models[ENG_DECOR_COLORS];
    uint32_t decor_count;
    uint8_t decor_color[PLAY_DECOR_MAX];
    bool decor_solid[PLAY_DECOR_MAX];
    bool decor_ready;      /* `decor_models` créés */
    bool creature_ready;   /* `creature_models` créés */
    bool safe_valid;       /* `safe` est un lieu sûr relevé */
    bool wading;           /* dernier tick : le joueur est dans l'eau jusqu'à la taille */
    float cinematic_blend; /* caméra : 0 sur le joueur, 1 sur la cinématique */
    /* carte du monde : eau (le joueur patauge sous ce niveau, plus lentement) et dernier lieu sûr
     * (au sol, hors de la lave) où il renaît */
    float water_level; /* niveau de l'eau, repère local ; -INFINITY : pas d'eau */
    float safe[3];
    uint32_t safe_ticks;
    uint32_t wall_count;
    float spawn[3];
    uint32_t arena_count;
    uint32_t ticks;
    uint32_t jumps;    /* sauts effectués (autotest, journal) */
    uint32_t landings; /* atterrissages */
    Avatar avatar;     /* le chevalier du joueur */
    bool sprinting;
    bool ready;
    bool on_terrain;
    bool creative; /* mode créatif (touche « , ») : invincible, vol libre, dégâts inchangés */
    bool wall_ready; /* `wall_model` créé (play_mode_load_arena_visuals) */
    /* M7 : point de départ (repère local ; suit le recentrage de l'origine) */ /* --terrain :
                                                                                   sol = champ
                                                                                   de hauteurs
                                                                                   (collision
                                                                                   Jolt) au lieu
                                                                                   de l'arène */
    bool autoplay;   /* --combat-autoplay : le pilote automatique joue */
    bool show_debug; /* F3 : capsules, boîtes et flèche en plus des modèles */
    /* M7 : sol donné par le monde streamé (play_mode_init_world) ; NULL : arène ou terrain */
    bool (*ground_fn)(void *user, float x, float z, float *out);
    void *ground_user;
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

/* Monde ouvert (M7, --world) : monde physique VIDE (les cellules du monde streamé y ajoutent
 * leurs corps), personnage aux pieds `spawn` (LOCAL), sol lu par `ground_fn`. */
bool play_mode_init_world(PlayMode *play, EngTweaks *tweaks, float camera_sensitivity,
                          bool invert_y, const float spawn[3],
                          bool (*ground_fn)(void *user, float x, float z, float *out),
                          void *ground_user);
/* Origine flottante (M7-10) : ajoute `shift` à la position du personnage et de la caméra. */
void play_mode_shift(PlayMode *play, const float shift[3]);

/* Hauteur du sol au point (x, z) : relief en mode terrain, PLAY_GROUND_TOP sinon. */
float play_mode_ground(const PlayMode *play, float x, float z);
/* Carte du monde : niveau de l'eau (repère local). Sous ce niveau le joueur patauge : il avance à
 * PLAY_WADE_SPEED fois sa vitesse. -INFINITY : pas d'eau (défaut). */
#define PLAY_WADE_SPEED 0.6F
void play_mode_set_water_level(PlayMode *play, float level);

/* Grande carte : décor des zones (engine/zone_decor.h). `boxes` sont relatives à (cx, cz), en
 * repère local ; chacune est posée sur le relief ; les solides bloquent le joueur, la caméra et les
 * ennemis. Renvoie le nombre de boîtes posées : moins que `count` si le relief n'est pas encore
 * chargé sous certaines (réessayer plus tard, après play_mode_clear_decor). */
uint32_t play_mode_add_decor(PlayMode *play, const EngDecorBox *boxes, uint32_t count, float cx,
                             float cz);
void play_mode_clear_decor(PlayMode *play);
/* Internes au mode jouable (play_decor.c) : modèles, recentrage de l'origine, dessin. */
bool play_decor_load_visuals(PlayMode *play, EngRenderer *renderer, EngArena *arena,
                             const EngEnvironment *env);
void play_decor_unload_visuals(PlayMode *play, EngRenderer *renderer);
void play_decor_shift(PlayMode *play, const float shift[3]);
void play_decor_draw(const PlayMode *play, EngRenderQueue *queue, mat4 view_proj,
                     const EngLighting *light, const EngModelShadow *shadow, bool shadow_pass);
/* Une boîte unité par ligne de `table` (r, g, b, rugosité, émissif r, g, b). Tout ou rien. */
bool play_mode_box_models(EngRenderer *renderer, EngArena *arena, const EngEnvironment *env,
                          const float (*table)[7], uint32_t count, EngModelGpu *models);

/* Point de départ d'une nouvelle partie : pieds posés sur le sol en (PLAY_SPAWN_X, PLAY_SPAWN_Z),
 * lacet de départ (regard vers +X). Aucun paramètre ne peut être NULL. */
void play_mode_spawn_point(const PlayMode *play, float feet[3], float *yaw);

/* Replace le personnage (pieds en `feet`), immobile, caméra derrière lui regardant vers `yaw`.
 * Les réglages (vitesses, sensibilité, inversion de la caméra) sont conservés ; l'endurance non
 * plus que le combat ne sont touchés (à l'appelant). */
void play_mode_teleport(PlayMode *play, const float feet[3], float yaw);

/* M7 : l'origine flottante du monde est recentrée de `shift` : le personnage, la caméra, le point
 * de départ et le combat (camp, ennemis, grille) sont décalés ensemble. */
void play_mode_origin_shift(PlayMode *play, const float shift[3]);

/* B8 : un obstacle statique du décor (tronc, rocher) : une capsule verticale de `radius` dont le
 * bas est à `base` et le haut à `base` + `height` (une sphère au sommet si `height` < 2 x
 * `radius`). Le joueur et la caméra butent dessus. Après un ajout en masse, appeler
 * play_mode_obstacles_done. Paramètres invalides ou monde plein : false. */
bool play_mode_add_obstacle(PlayMode *play, const float base[3], float radius, float height);
/* B8 : fin d'un ajout en masse d'obstacles (réorganise la broadphase de Jolt). */
void play_mode_obstacles_done(PlayMode *play);

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
