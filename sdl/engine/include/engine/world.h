#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/assets.h"
#include "engine/math.h"
#include "engine/pool.h"

/* Monde d'entités (M2-08, décision : docs/adr/0011-modele-d-entites-ecs-maison.md).
 *
 *   EngWorld world;
 *   eng_world_init(&world, 4096);                          // capacité d'entités fixe
 *   EngEntity tree = eng_world_create(&world, "arbre");
 *   EngTransform *t = eng_world_add(&world, tree, ENG_COMPONENT_TRANSFORM);   // identité par
 * défaut t->position[1] = 2.0F; EngEntity leaf = eng_world_create(&world, "feuille");
 *   eng_world_set_parent(&world, leaf, tree);
 *   mat4 m; eng_world_world_matrix(&world, leaf, m);       // la feuille suit l'arbre
 *   eng_world_destroy(&world, tree);                       // détruit aussi la feuille
 *
 * - Une ENTITÉ est un handle générationnel (EngHandle) : une entité détruite est détectée, même si
 * sa case est réutilisée. Une entité n'a rien en elle-même : elle a des COMPOSANTS (données), et un
 * parent.
 * - Un composant = un type enregistré (eng_world_register) avec sa description de champs
 *   (EngComponentDesc) : la même description sert à la sérialisation (scène) et à l'inspecteur. Les
 *   données d'un composant sont rangées dans une table dense, parcourue sans trou.
 * - Les composants natifs ENG_COMPONENT_NAME, _TRANSFORM, _MESH_RENDERER existent dès
 * eng_world_init.
 * - Hiérarchie : un parent, des enfants (liste chaînée), profondeur au plus ENG_WORLD_MAX_DEPTH
 * niveaux (racine = niveau 0). Détruire une entité détruit tout son sous-arbre.
 * - Une seule file d'exécution. Un pointeur rendu par add/get/table_data reste valide jusqu'au
 * prochain add/remove/destroy (la table peut déplacer des éléments), jamais plus longtemps. */

#define ENG_WORLD_MAX_COMPONENTS 32U
#define ENG_WORLD_MAX_FIELDS 32U
#define ENG_WORLD_MAX_DEPTH 32U
#define ENG_WORLD_MAX_COMPONENT_SIZE 4096U

typedef EngHandle EngEntity;
#define ENG_ENTITY_NULL ENG_HANDLE_NULL

/* ── Description d'un composant (réflexion) ── */

typedef enum EngFieldType {
    ENG_FIELD_BOOL = 1, /* bool (1 octet) */
    ENG_FIELD_I32,
    ENG_FIELD_U32,
    ENG_FIELD_FLOAT,
    ENG_FIELD_VEC3,   /* float[3] */
    ENG_FIELD_QUAT,   /* float[4] : x, y, z, w (cglm), normalisé */
    ENG_FIELD_STRING, /* char[length], terminé par un zéro */
} EngFieldType;

typedef struct EngFieldDesc {
    const char *name; /* unique dans le composant ; doit rester valide (littéral) */
    EngFieldType type;
    uint32_t offset; /* offsetof dans la structure du composant */
    uint32_t length; /* ENG_FIELD_STRING : taille du tableau de char (zéro compris) ; sinon 0 */
} EngFieldDesc;

typedef struct EngComponentDesc {
    const char *name;           /* unique dans le monde ; doit rester valide */
    uint32_t size;              /* sizeof de la structure ; alignement d'au plus 8 octets */
    const EngFieldDesc *fields; /* champs PERSISTANTS (scène, inspecteur) ; les autres octets sont
                                   de l'état d'exécution, jamais écrit ni édité */
    uint32_t field_count;
    const void *initial; /* valeur d'un composant ajouté (size octets) ; NULL : zéros */
} EngComponentDesc;

/* Taille en octets d'un champ ; 0 si le type est inconnu. */
uint32_t eng_field_size(const EngFieldDesc *field);

/* ── Composants natifs ── */

#define ENG_NAME_MAX 48U
#define ENG_MESH_PATH_MAX 192U

enum {
    ENG_COMPONENT_NAME = 1,
    ENG_COMPONENT_TRANSFORM = 2,
    ENG_COMPONENT_MESH_RENDERER = 3,
};

typedef struct EngName {
    char text[ENG_NAME_MAX];
} EngName;

/* Position, rotation et échelle LOCALES (par rapport au parent), repère du moteur (main droite, Y
 * haut). */
typedef struct EngTransform {
    float position[3];
    float rotation[4]; /* quaternion x, y, z, w ; (0, 0, 0, 1) = aucune rotation */
    float scale[3];
} EngTransform;

/* Un modèle à dessiner. `model` : chemin d'un .mesh cuit (persistant) ; `asset` : handle du
 * gestionnaire (engine/assets.h), rempli à l'exécution par celui qui charge, jamais écrit. */
typedef struct EngMeshRenderer {
    char model[ENG_MESH_PATH_MAX];
    bool visible;
    bool cast_shadow;
    EngAssetHandle asset;
} EngMeshRenderer;

/* Matrice locale = translation * rotation * échelle (rotation normalisée d'abord). */
void eng_transform_matrix(const EngTransform *transform, mat4 out);

/* ── Monde ── */

typedef struct EngEntityRecord {
    EngEntity parent;
    EngEntity first_child;
    EngEntity last_child;
    EngEntity prev_sibling;
    EngEntity next_sibling;
    uint32_t depth; /* 0 pour une racine */
} EngEntityRecord;

typedef struct EngComponentTable {
    EngComponentDesc desc;
    uint32_t stride;
    uint32_t count;
    uint32_t *sparse;    /* par case d'entité : indice dense + 1, 0 si absent */
    EngEntity *entities; /* dense : propriétaire de chaque élément */
    uint8_t *data;       /* dense : count * stride octets */
} EngComponentTable;

typedef struct EngWorld {
    EngPool entities; /* d'EngEntityRecord */
    EngComponentTable tables[ENG_WORLD_MAX_COMPONENTS];
    uint32_t component_count;
    uint32_t capacity;
} EngWorld;

/* `capacity` : nombre maximal d'entités vivantes en même temps (au plus 2^20). Enregistre les
 * composants natifs. false (journalisé) : capacité invalide ou mémoire insuffisante. */
bool eng_world_init(EngWorld *world, uint32_t capacity);
void eng_world_shutdown(EngWorld *world);

/* Enregistre un composant ; rend son identifiant (>= 1), 0 si la description est invalide
 * (journalisé) : nom vide ou déjà pris, taille nulle ou > ENG_WORLD_MAX_COMPONENT_SIZE, champ hors
 * de la structure, mal aligné, nom de champ vide ou répété, chaîne sans longueur, trop de
 * composants ou de champs. La mémoire des tables est réservée ici (capacité × taille) : aucune
 * allocation ensuite. */
uint32_t eng_world_register(EngWorld *world, const EngComponentDesc *desc);

uint32_t eng_world_component_count(const EngWorld *world);
const EngComponentDesc *eng_world_component(const EngWorld *world,
                                            uint32_t component);            /* NULL si inconnu */
uint32_t eng_world_find_component(const EngWorld *world, const char *name); /* 0 si absent */

/* Crée une entité (sans parent). `name` non NULL : lui ajoute un composant Name (tronqué à
 * ENG_NAME_MAX-1). ENG_ENTITY_NULL si le monde est plein (journalisé). */
EngEntity eng_world_create(EngWorld *world, const char *name);

/* Détruit l'entité, ses composants et TOUT son sous-arbre. false si le handle est invalide ou
 * périmé. */
bool eng_world_destroy(EngWorld *world, EngEntity entity);

bool eng_world_alive(const EngWorld *world, EngEntity entity);
uint32_t eng_world_entity_count(const EngWorld *world);

/* Itération sur toutes les entités vivantes : EngEntity e; uint32_t cursor = 0; while
 * (eng_world_next_entity(&world, &cursor, &e)) {...}. Détruire l'entité courante est permis. */
bool eng_world_next_entity(const EngWorld *world, uint32_t *cursor, EngEntity *out);

/* ── Composants d'une entité ── */

/* Ajoute le composant (valeur initiale de sa description) et rend ses données ; s'il est déjà là,
 * rend les données existantes sans les modifier. NULL (journalisé) : entité invalide ou composant
 * inconnu. */
void *eng_world_add(EngWorld *world, EngEntity entity, uint32_t component);
void *eng_world_get(const EngWorld *world, EngEntity entity,
                    uint32_t component); /* NULL si absent */
bool eng_world_has(const EngWorld *world, EngEntity entity, uint32_t component);
bool eng_world_remove(EngWorld *world, EngEntity entity, uint32_t component); /* false si absent */

/* Parcours d'une table de composant, sans trou : index dans [0, count). Retirer un composant ou
 * détruire une entité déplace le DERNIER élément à la place libérée : pour retirer pendant un
 * parcours, parcourir de la fin vers le début. */
uint32_t eng_world_table_count(const EngWorld *world, uint32_t component);
EngEntity eng_world_table_entity(const EngWorld *world, uint32_t component, uint32_t index);
void *eng_world_table_data(const EngWorld *world, uint32_t component, uint32_t index);

/* Première entité de ce nom (comparaison exacte) ; ENG_ENTITY_NULL si aucune. Parcours linéaire. */
EngEntity eng_world_find_by_name(const EngWorld *world, const char *name);

/* ── Hiérarchie ── */

/* Rattache `child` à `parent` (ENG_ENTITY_NULL : le détache, il devient racine). Les
 * transformations locales ne changent PAS (l'objet bouge s'il avait un autre parent). false
 * (journalisé) : entité invalide, `child` == `parent`, ou `parent` descend de `child` (boucle), ou
 * la profondeur dépasserait ENG_WORLD_MAX_DEPTH niveaux. Le nouvel enfant est ajouté EN DERNIER. */
bool eng_world_set_parent(EngWorld *world, EngEntity child, EngEntity parent);
EngEntity eng_world_parent(const EngWorld *world, EngEntity entity); /* NULL pour une racine */
EngEntity eng_world_first_child(const EngWorld *world, EngEntity entity);
EngEntity eng_world_next_sibling(const EngWorld *world, EngEntity entity);
/* Parcours en profondeur (préfixe) du sous-arbre de `root`, `root` compris : donner `root` pour
 * obtenir le premier enfant, puis rendre chaque résultat, jusqu'à ENG_ENTITY_NULL.
 *   for (EngEntity e = root; e.generation != 0; e = eng_world_next_in_subtree(w, root, e)) ...
 * NULL aussi si `root` ou `current` est invalide ou si `current` n'est pas dans le sous-arbre. */
EngEntity eng_world_next_in_subtree(const EngWorld *world, EngEntity root, EngEntity current);

uint32_t eng_world_depth(const EngWorld *world, EngEntity entity); /* 0 : racine ou invalide */

/* Matrice monde = produit des matrices locales de la racine à l'entité (une entité sans Transform
 * compte pour l'identité). false si l'entité est invalide (`out` : identité). Calculée à chaque
 * appel (coût proportionnel à la profondeur). */
bool eng_world_world_matrix(const EngWorld *world, EngEntity entity, mat4 out);
