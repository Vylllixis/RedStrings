#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/world.h"

/* Scène : un monde d'entités écrit en texte lisible et versionnable (M2-09, docs/scene.md).
 *
 *   openworld-scene 1
 *
 *   entity 0 parent -
 *   [Name]
 *   text = "arbre"
 *   [Transform]
 *   position = 0 1 0
 *   rotation = 0 0 0 1
 *   scale = 1 1 1
 *
 *   entity 1 parent 0
 *   [MeshRenderer]
 *   model = "assets/cooked/models/demo.mesh"
 *   visible = true
 *   cast_shadow = true
 *
 * Le format est PILOTÉ PAR LA DESCRIPTION DES COMPOSANTS (EngComponentDesc) : tout composant
 * enregistré avec des champs est lu et écrit sans code supplémentaire. Les composants sans champ
 * (état d'exécution pur) ne sont pas écrits.
 *
 * Aller-retour : écrire, relire dans un monde vide, réécrire redonne EXACTEMENT les mêmes octets
 * (les float sont écrits avec 9 chiffres significatifs, ce qui les restitue au bit près). L'ordre
 * des entités est un parcours en profondeur (parents avant enfants), racines dans l'ordre du monde.
 *
 * Lecture stricte et atomique : au premier défaut (entête, composant ou champ inconnu, valeur
 * invalide, parent introuvable, boucle, monde plein...) rien n'est ajouté au monde et l'erreur
 * donne le numéro de ligne. La lecture AJOUTE les entités au monde (elle ne le vide pas). */

#define ENG_SCENE_MAGIC "openworld-scene"
#define ENG_SCENE_VERSION 1U
#define ENG_SCENE_MAX_LINE 1024U

typedef struct EngSceneError {
    char message[160];
    uint32_t line; /* 1 pour la première ligne ; 0 : erreur hors texte (fichier, mémoire) */
} EngSceneError;

/* Écrit la scène dans `out` (`capacity` octets, terminée par un zéro si elle tient) et met dans
 * `*needed` la taille TOTALE nécessaire, zéro final compris ; `out` peut être NULL pour ne
 * mesurer que. Si `*needed > capacity`, le contenu de `out` est incomplet. false : une valeur ne
 * peut pas s'écrire (float non fini) ; journalisé. */
bool eng_scene_write(const EngWorld *world, char *out, size_t capacity, size_t *needed);

/* Lit `length` octets de texte (zéro final inutile) et ajoute les entités à `world`. `error` peut
 * être NULL. */
bool eng_scene_read(EngWorld *world, const char *text, size_t length, EngSceneError *error);

/* Fichier : `path` est relatif à l'exécutable (eng_path_resolve). L'écriture passe par un fichier
 * temporaire puis un renommage : un crash ne laisse jamais une scène à moitié écrite. */
bool eng_scene_save(const EngWorld *world, const char *path);
bool eng_scene_load(EngWorld *world, const char *path, EngSceneError *error);
