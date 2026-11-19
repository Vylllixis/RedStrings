#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/debug_lines.h"
#include "engine/loop.h"

/* Physique rigide (M4-01) : enveloppe de Jolt Physics. Aucun type de Jolt ni de joltc n'apparaît
 * ici ni dans game/ : seul engine/src/physics.c inclut l'en-tête de l'habillage C (voir
 * docs/physics.md, ADR 0024).
 *
 * Conventions : main droite, Y vers le haut, mètres, secondes, kg. Le monde est pas à pas : le jeu
 * appelle eng_physics_step() avec un pas FIXE (1/60 s, M4-05) ; la physique ne lit jamais
 * l'horloge.
 *
 * Mémoire : Jolt alloue lui-même (tas) ; tout est libéré par eng_physics_destroy(). Le monde est
 * mono-fil côté appelant (un seul fil appelle l'API), ses calculs internes peuvent utiliser un pool
 * de fils (le résultat ne dépend pas du nombre de fils : Jolt est déterministe à ce niveau). */

typedef struct EngPhysics EngPhysics;

/* Identifiant d'un corps ; ENG_BODY_INVALID pour « aucun ». Un identifiant d'un corps retiré n'est
 * plus valide (les fonctions ci-dessous l'ignorent ou renvoient la valeur neutre, sans planter). */
typedef uint32_t EngBody;
#define ENG_BODY_INVALID 0xFFFFFFFFU

/* Couches de collision (M4-02). Chaque corps appartient à UNE couche, fixée à sa création.
 *
 *   STATIC     décor immobile (terrain, rochers, murs)
 *   DYNAMIC    objets simulés (caisses, balles)
 *   CHARACTER  personnages (corps à rotation verrouillée : debout)
 *   HITBOX     volume d'attaque (arme, coup) : CAPTEUR
 *   TRIGGER    zone de déclenchement (porte, piège, changement de zone) : CAPTEUR
 *
 * Un corps des couches CAPTEUR (HITBOX, TRIGGER) ne produit JAMAIS de force : il est cinématique
 * (on le déplace avec eng_physics_move_kinematic), marqué « capteur » dans Jolt ET ses paires ne
 * sont pas déclarées au moteur de collision ; on ne l'observe qu'avec eng_physics_overlaps. Un coup
 * ne peut donc pas pousser un personnage, par construction.
 *
 * Matrice d'interaction (O : le couple interagit ; solide = contact physique, détection = vu par
 * eng_physics_overlaps seulement) :
 *
 *               STATIC  DYNAMIC  CHARACTER  HITBOX  TRIGGER
 *   STATIC        -     solide    solide      -       -
 *   DYNAMIC     solide  solide    solide    détect.  détect.
 *   CHARACTER   solide  solide    solide    détect.  détect.
 *   HITBOX        -     détect.   détect.      -       -
 *   TRIGGER       -     détect.   détect.      -       -
 *
 * (un capteur ne détecte ni le décor ni un autre capteur ; le décor ne se détecte pas lui-même.) */
typedef enum EngPhysicsLayer {
    ENG_LAYER_STATIC = 0,
    ENG_LAYER_DYNAMIC,
    ENG_LAYER_CHARACTER,
    ENG_LAYER_HITBOX,
    ENG_LAYER_TRIGGER,
    ENG_LAYER_COUNT
} EngPhysicsLayer;

typedef enum EngLayerInteraction {
    ENG_INTERACT_NONE = 0,
    ENG_INTERACT_SOLID, /* contact physique (forces) */
    ENG_INTERACT_DETECT /* détection seule : aucune force */
} EngLayerInteraction;

/* Interaction de deux couches (symétrique). Couche invalide : ENG_INTERACT_NONE. */
EngLayerInteraction eng_physics_layer_interaction(EngPhysicsLayer a, EngPhysicsLayer b);
/* Vrai pour HITBOX et TRIGGER. */
bool eng_physics_layer_is_sensor(EngPhysicsLayer layer);
/* Nom lisible (« static », « dynamic »...), « ? » si invalide. */
const char *eng_physics_layer_name(EngPhysicsLayer layer);

typedef struct EngPhysicsDesc {
    uint32_t max_bodies; /* 0 : 10 240 */
    int worker_threads;  /* 0 : aucun fil de travail (tout sur le fil appelant) ; > 0 : ce nombre ;
                            < 0 : un par cœur moins un */
    float gravity[3];    /* m/s² ; tout à 0 : (0, -9,81, 0) */
} EngPhysicsDesc;

/* Vrai si le moteur a été compilé avec Jolt (option CMake OW_PHYSICS). Sinon eng_physics_create
 * renvoie NULL (journalisé) et les autres fonctions sont sans effet. */
bool eng_physics_available(void);

/* Crée un monde. NULL en cas d'échec ou si Jolt n'est pas compilé. `desc` peut être NULL. */
EngPhysics *eng_physics_create(const EngPhysicsDesc *desc);
void eng_physics_destroy(EngPhysics *world); /* NULL accepté */

/* Avance la simulation de `dt` secondes (> 0, <= 0,1 ; sinon false et rien ne bouge). Un seul pas
 * de collision : le jeu garantit le pas fixe. */
bool eng_physics_step(EngPhysics *world, float dt);

/* Création d'un corps. `position` est le centre de masse (monde). Le comportement dépend de la
 * couche : STATIC immobile ; DYNAMIC simulé ; CHARACTER simulé, rotation verrouillée ; HITBOX /
 * TRIGGER cinématique et capteur (voir ci-dessus). Renvoie ENG_BODY_INVALID si les paramètres sont
 * invalides (dimension <= 0, NULL, NaN, couche hors borne) ou si le monde est plein. */
EngBody eng_physics_add_sphere(EngPhysics *world, float radius, const float position[3],
                               EngPhysicsLayer layer);
EngBody eng_physics_add_box(EngPhysics *world, const float half_extents[3], const float position[3],
                            EngPhysicsLayer layer);
/* Capsule verticale (axe Y) : `half_height` = demi-longueur du cylindre (hors demi-sphères),
 * `radius`. */
EngBody eng_physics_add_capsule(EngPhysics *world, float half_height, float radius,
                                const float position[3], EngPhysicsLayer layer);
void eng_physics_remove_body(EngPhysics *world, EngBody body);

/* Couche d'un corps ; ENG_LAYER_COUNT si le corps est inconnu. */
EngPhysicsLayer eng_physics_body_layer(const EngPhysics *world, EngBody body);

/* Déplace un corps CINÉMATIQUE (HITBOX, TRIGGER) vers `target` en `dt` secondes : il prend la
 * vitesse (target - position) / dt, il arrive à `target` à la fin du pas suivant. false si le corps
 * est inconnu, n'est pas cinématique, si target n'est pas fini ou si dt <= 0. */
bool eng_physics_move_kinematic(EngPhysics *world, EngBody body, const float target[3], float dt);

/* Corps dont le volume chevauche celui de `body` et dont la couche interagit avec la sienne
 * (matrice ci-dessus ; le corps lui-même est exclu). Écrit au plus `max` identifiants dans `out`,
 * triés par identifiant croissant, et renvoie le NOMBRE TOTAL de corps en chevauchement (peut
 * dépasser `max` ; `out` peut être NULL avec `max` = 0 pour compter). Au plus
 * ENG_PHYSICS_MAX_OVERLAPS corps sont examinés. Un contact tangent compte comme chevauchement
 * (tolérance de collision de Jolt, 1e-4 m). */
#define ENG_PHYSICS_MAX_OVERLAPS 256U
uint32_t eng_physics_overlaps(const EngPhysics *world, EngBody body, EngBody *out, uint32_t max);

/* Lecture / écriture (out inchangé si le corps est inconnu : renvoie false). */
bool eng_physics_body_position(const EngPhysics *world, EngBody body, float out[3]);
/* Orientation du corps en quaternion unitaire (x, y, z, w). */
bool eng_physics_body_rotation(const EngPhysics *world, EngBody body, float out[4]);
bool eng_physics_body_velocity(const EngPhysics *world, EngBody body, float out[3]);
bool eng_physics_set_body_velocity(EngPhysics *world, EngBody body, const float velocity[3]);
bool eng_physics_body_is_active(const EngPhysics *world, EngBody body);

uint32_t eng_physics_body_count(const EngPhysics *world);

/* ── Collisions du monde (M4-03) ── */

struct EngHeightmap;
struct EngModel;

/* Forme de collision immobile, indépendante des corps : on la crée une fois puis on l'ajoute au
 * monde autant de fois que voulu (un même rocher en 9 000 exemplaires partage UN maillage). Elle
 * reste valide tant qu'on ne l'a pas détruite ; l'ajouter au monde garde sa propre référence, la
 * détruire n'enlève donc pas les corps déjà créés. */
typedef struct EngPhysicsShape EngPhysicsShape;

/* Terrain : forme de champ de hauteurs de Jolt, aux mêmes positions que le terrain affiché :
 * l'échantillon (x, z) est en (x * spacing, altitude, z * spacing) dans le repère de la forme.
 * Hauteurs en 16 bits (quantification identique à celle du fichier .hmap : pas de perte). NULL
 * (journalisé) si le champ est invalide, plus petit que 4 x 4, plus grand que
 * ENG_PHYSICS_MAX_HEIGHTFIELD ou si le monde est NULL.
 *
 * ATTENTION : Jolt coupe chaque cellule selon la même diagonale (a-d), alors que le terrain affiché
 * alterne les diagonales (en damier, terrain.c). Entre les deux surfaces l'écart au centre d'une
 * cellule est |a + d - b - c| / 2 au plus (borne ; voir docs/physics.md). */
#define ENG_PHYSICS_MAX_HEIGHTFIELD 4097U
EngPhysicsShape *eng_physics_shape_heightfield(EngPhysics *world, const struct EngHeightmap *map);

/* Maillage de triangles immobile (rochers, murs, bâtiments). `positions` : `vertex_count` sommets
 * de 3 floats espacés de `stride_bytes` octets (>= 12) ; `indices` : triangles anti-horaires, des
 * multiples de 3, chacun < vertex_count. Les triangles dégénérés sont retirés. NULL si les entrées
 * sont invalides (NaN, indice hors borne, moins d'un triangle non dégénéré...). */
EngPhysicsShape *eng_physics_shape_mesh(EngPhysics *world, const float *positions,
                                        size_t stride_bytes, uint32_t vertex_count,
                                        const uint32_t *indices, uint32_t index_count);
/* Idem depuis un modèle chargé ou construit (toutes ses parties). */
EngPhysicsShape *eng_physics_shape_model(EngPhysics *world, const struct EngModel *model);
void eng_physics_shape_destroy(EngPhysicsShape *shape); /* NULL accepté */

/* Position, rotation (quaternion x, y, z, w unitaire) et échelle UNIFORME d'une instance. */
typedef struct EngPhysicsPose {
    float position[3];
    float rotation[4];
    float scale;
} EngPhysicsPose;
EngPhysicsPose eng_physics_pose_identity(void); /* origine, sans rotation, échelle 1 */

/* Ajoute la forme au monde, en couche STATIC (immobile). `pose` NULL : identité. L'échelle
 * différente de 1 n'est acceptée que pour un maillage (pas un champ de hauteurs). Renvoie
 * ENG_BODY_INVALID si un paramètre est invalide (rotation non unitaire à 1e-3 près, échelle <=
 * 0...) ou si le monde est plein. */
EngBody eng_physics_add_static_shape(EngPhysics *world, const EngPhysicsShape *shape,
                                     const EngPhysicsPose *pose);

/* Lancer de rayon : premier corps touché par le rayon `origin` + t * `direction` (direction non
 * nulle, normalisée ici) pour t dans [0, max_distance]. `layer_mask` : bits (1 << couche) des
 * couches considérées. */
#define ENG_LAYERS_SOLID                                                                           \
    ((1U << ENG_LAYER_STATIC) | (1U << ENG_LAYER_DYNAMIC) | (1U << ENG_LAYER_CHARACTER))
#define ENG_LAYERS_ALL ((1U << ENG_LAYER_COUNT) - 1U)
typedef struct EngRayHit {
    EngBody body;
    float distance; /* mètres depuis l'origine */
    float point[3];
    float normal[3]; /* normale de la surface au point touché (unitaire) */
} EngRayHit;
bool eng_physics_raycast(const EngPhysics *world, const float origin[3], const float direction[3],
                         float max_distance, uint32_t layer_mask, EngRayHit *hit);

/* Balayage d'une sphère (M4-11) : premier corps touché par une sphère de rayon `radius` dont le
 * centre va de `origin` à `origin` + `max_distance` * `direction` (non nulle, normalisée ici).
 * `hit->distance` : course du CENTRE avant contact (mètres) ; `point` : point de contact sur le
 * corps touché ; `normal` : unitaire, orientée vers la sphère. Une sphère qui démarre déjà en
 * contact renvoie distance 0 ; à l'intérieur d'un solide convexe fermé (faces arrière), rien n'est
 * touché. false : rien touché, ou argument invalide (radius > 0, <= 10 m). */
bool eng_physics_spherecast(const EngPhysics *world, const float origin[3],
                            const float direction[3], float max_distance, float radius,
                            uint32_t layer_mask, EngRayHit *hit);

/* ── Debug draw (M4-04) ──
 *
 * Dessine dans `lines` (engine/debug_lines.h) les formes que Jolt simule réellement : on relit la
 * forme et la pose de chaque corps dans Jolt, on ne garde pas de copie parallèle qui pourrait
 * se décaler. Une couleur par couche (eng_physics_layer_color), atténuée pour un corps DYNAMIC ou
 * CHARACTER endormi.
 *
 *   sphère    trois cercles
 *   boîte     12 arêtes orientées
 *   capsule   deux sphères aux extrémités du cylindre et quatre génératrices
 *   terrain   le maillage de Jolt (arêtes et diagonale a-d de chaque cellule) dans une fenêtre
 *             d'au plus ENG_PHYSICS_DEBUG_MAX_HF_CELLS cellules de côté autour de `focus` ;
 *             sans `radius`, seulement sa boîte englobante
 *   maillage  (ou toute autre forme) sa boîte englobante dans le monde
 */
#define ENG_PHYSICS_DEBUG_MAX_HF_CELLS 64U

typedef struct EngPhysicsDebugDraw {
    uint32_t layer_mask; /* bits (1 << couche) des couches dessinées ; 0 : rien */
    float focus[3];      /* centre d'intérêt (monde) : sert à `radius` */
    float radius; /* > 0 : seuls les corps à moins de `radius` de `focus` sont dessinés, et la
                   * fenêtre des terrains est centrée sur `focus` ; 0 : tous les corps, terrains
                   * en boîte englobante seule */
    uint32_t segments; /* segments par cercle (0 = valeur par défaut de debug_lines.h) */
} EngPhysicsDebugDraw;

/* Couleur d'une couche (une par couche, toutes différentes) ; magenta pour une valeur hors borne.
 */
EngDebugColor eng_physics_layer_color(EngPhysicsLayer layer);

/* Dessine les corps sélectionnés ; renvoie le nombre de corps ENTIÈREMENT dessinés (une forme qui
 * ne tient pas dans `lines` n'est pas ajoutée, voir `dropped`). 0, sans rien dessiner, si un
 * pointeur est NULL, si `focus` ou `radius` n'est pas fini ou si `radius` < 0. */
uint32_t eng_physics_debug_draw(const EngPhysics *world, EngDebugLines *lines,
                                const EngPhysicsDebugDraw *options);

/* ── Pas fixe (M4-05) ──
 *
 * La physique avance par pas IDENTIQUES (EngLoop : 60 Hz par défaut), quel que soit le rythme de
 * l'affichage : après la même durée réelle, le même nombre de pas a été simulé avec le même `dt`,
 * donc l'état est identique bit à bit à 30, 60 ou 144 images par seconde (la boucle compte le
 * temps en entiers, Jolt est déterministe pour un même enchaînement de pas, voir docs/physics.md).
 *
 *     uint32_t steps = eng_physics_advance(world, &loop, clock.dt_counter);
 *     ... dessiner avec eng_physics_body_interpolated(world, body, loop.alpha, pos, rot);
 *
 * Une image trop longue est plafonnée par la boucle (`max_frame_seconds`) : la simulation ralentit
 * au lieu de rattraper sans fin. */

/* Consomme la durée de l'image (ticks du compteur) avec `loop` et exécute les pas de simulation dus
 * (un eng_physics_step de `loop->tick_seconds` chacun). Renvoie le nombre de pas EXÉCUTÉS ; 0 si
 * `world` ou `loop` est NULL, ou si `loop->tick_seconds` n'est pas un pas valide (0 < dt <= 0,1) :
 * dans ce cas la boucle n'est pas avancée. Un pas refusé par Jolt s'arrête là (journalisé). */
uint32_t eng_physics_advance(EngPhysics *world, EngLoop *loop, uint64_t frame_delta_counter);

/* Nombre de pas exécutés avec succès depuis la création du monde (eng_physics_step compris). */
uint64_t eng_physics_step_count(const EngPhysics *world);

/* Position et rotation (quaternion x, y, z, w) à l'instant intermédiaire `alpha` (0 <= alpha <= 1)
 * entre l'avant-dernier et le dernier pas : position interpolée linéairement, rotation par slerp.
 * C'est ce que le rendu doit lire : l'état brut (eng_physics_body_position) saute d'un pas à
 * l'autre. `position` et `rotation` peuvent être NULL. alpha = 1 donne l'état courant, alpha = 0
 * celui d'avant le dernier pas. Un corps qui n'a pas encore vécu un pas complet, ou un décor
 * immobile, donne son état courant. false (sorties inchangées) si le corps est inconnu ou `alpha`
 * hors de [0, 1] ou non fini. */
bool eng_physics_body_interpolated(const EngPhysics *world, EngBody body, double alpha,
                                   float position[3], float rotation[4]);

/* ── Contrôleur de personnage (M4-06) ──
 *
 * Capsule « cinématique » (CharacterVirtual de Jolt) : elle se déplace en balayant son volume dans
 * le monde, donc elle ne traverse pas un mur mince même à grande vitesse, glisse le long des
 * obstacles, monte les pentes jusqu'à `max_slope_deg`, enjambe les marches jusqu'à `step_height`,
 * et redescend sans décoller du sol. Elle n'est pas poussée par la gravité d'un corps rigide : le
 * contrôleur gère sa propre vitesse verticale (gravité du monde, saut).
 *
 *     EngCharacter *c = eng_character_create(world, &desc);
 *     à CHAQUE pas fixe, après eng_physics_step :
 *         eng_character_update(c, dt, move);     // move : vitesse horizontale voulue (m/s)
 *     au rendu :
 *         eng_character_interpolated(c, loop.alpha, feet);
 *
 * Le personnage a aussi un corps dans le monde (couche CHARACTER, cinématique,
 * `eng_character_body`) : les hitbox et les zones le détectent (eng_physics_overlaps), les corps
 * dynamiques butent contre lui, le debug draw le dessine. Il compte dans eng_physics_body_count.
 *
 * Position : les PIEDS (centre du bas de la capsule). Le monde doit survivre à ses personnages :
 * détruire les personnages avant eng_physics_destroy. */
typedef struct EngCharacter EngCharacter;

typedef struct EngCharacterDesc {
    float radius;        /* m ; 0 = 0,3 */
    float height;        /* m, hauteur totale (calottes comprises), > 2 x radius ; 0 = 1,8 */
    float position[3];   /* pieds, monde */
    float max_slope_deg; /* pente la plus raide praticable, degrés dans ]0, 90[ ; 0 = 45 */
    float step_height;   /* marche la plus haute enjambée, m, >= 0 ; 0 = 0,3 */
    float mass;          /* kg ; 0 = 70 (sert aux interactions avec les corps dynamiques) */
} EngCharacterDesc;

typedef enum EngCharacterGround {
    ENG_GROUND_SUPPORTED, /* debout sur un sol praticable */
    ENG_GROUND_STEEP,     /* en contact avec un sol trop raide : glisse */
    ENG_GROUND_UNSUPPORTED,
    ENG_GROUND_AIR
} EngCharacterGround;

/* NULL si un paramètre est invalide (journalisé), si le monde est NULL ou plein, ou sans Jolt. */
EngCharacter *eng_character_create(EngPhysics *world, const EngCharacterDesc *desc);
void eng_character_destroy(EngCharacter *character); /* NULL accepté */

/* Avance le personnage de `dt` secondes (0 < dt <= 0,1) avec la vitesse horizontale voulue `move`
 * (m/s, repère du monde ; la composante y est ignorée). Applique la gravité, annule la vitesse
 * dirigée vers une pente trop raide, enjambe les marches, colle au sol en descente. false (rien ne
 * bouge) si un argument est invalide. */
bool eng_character_update(EngCharacter *character, float dt, const float move[3]);

/* Saute : donne la vitesse verticale `speed` (m/s, > 0) si le personnage est debout sur un sol
 * praticable. Renvoie false sinon (en l'air, sol trop raide, argument invalide). Le délai de grâce
 * et la mémorisation de la touche (M4-08) se construisent par-dessus. */
bool eng_character_jump(EngCharacter *character, float speed);

/* Donne la vitesse verticale `speed` (m/s, > 0) QUEL QUE SOIT l'état du sol : c'est le saut du
 * délai de grâce (engine/jump.h), accordé juste après avoir quitté un rebord. false si `speed`
 * n'est pas un nombre > 0. */
bool eng_character_launch(EngCharacter *character, float speed);

/* Coupe un saut en cours : si le personnage monte, sa vitesse verticale est multipliée par
 * `factor` (0 <= factor < 1). Relâcher la touche tôt donne un saut plus court. false si `factor`
 * est invalide. Sans effet (renvoie true) si le personnage ne monte pas. */
bool eng_character_cut_jump(EngCharacter *character, float factor);

/* Échelle de la gravité du monde en montée et en chute, chacune dans ]0 ; 5] (défaut 1 et 1). Une
 * chute plus forte que la montée donne un saut plus « sec » (apex en v²/(2 g rise), retour au sol
 * plus vite). false, inchangé, si une valeur est invalide. */
bool eng_character_set_gravity_scale(EngCharacter *character, float rise, float fall);

/* Téléporte les pieds en `feet` et annule la vitesse ; l'interpolation repart de ce point. */
bool eng_character_teleport(EngCharacter *character, const float feet[3]);

bool eng_character_position(const EngCharacter *character, float feet[3]);
/* Position des pieds entre l'avant-dernier et le dernier update (0 <= alpha <= 1), pour le rendu.
 */
bool eng_character_interpolated(const EngCharacter *character, double alpha, float feet[3]);
bool eng_character_velocity(const EngCharacter *character,
                            float out[3]); /* après le dernier update */
EngCharacterGround eng_character_ground(const EngCharacter *character);
bool eng_character_ground_normal(const EngCharacter *character, float normal[3]);
EngBody eng_character_body(const EngCharacter *character); /* ENG_BODY_INVALID si NULL */
