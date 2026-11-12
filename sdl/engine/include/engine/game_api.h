#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/debug_draw.h"
#include "engine/input_state.h"
#include "engine/log.h"

/* Contrat entre le moteur (exécutable) et le jeu (bibliothèque dynamique rechargeable).
 *
 * Le jeu NE LIE PAS le moteur : il n'a donc aucune copie du journal, des arènes, etc. Il reçoit
 * du moteur une table de fonctions (EngServices) et exporte UNE seule fonction,
 * eng_game_get_api, qui retourne sa table d'entrées (EngGameApi).
 *
 * RÈGLES POUR QUE LE RECHARGEMENT À CHAUD FONCTIONNE
 *  1. Tout l'état du jeu vit dans le bloc `state`, alloué par le moteur (api->state_size octets,
 *     rempli de zéros au premier chargement). Pas de variable globale modifiable dans le jeu :
 *     elle serait perdue (ou, pire, laissée dans l'ancienne bibliothèque) au rechargement.
 *  2. Le bloc d'état ne doit contenir AUCUN pointeur vers le code ou les données de la
 *     bibliothèque (pointeurs de fonction, littéraux de chaîne comme "orc", adresses de
 *     variables statiques) : ils pointeraient dans du code déchargé. Utiliser des indices, des
 *     identifiants ou des handles. Les pointeurs vers la mémoire du moteur (arènes) sont sûrs.
 *  3. Changer la TAILLE de la structure d'état est détecté : le rechargement est refusé et le
 *     jeu continue avec l'ancien code (il faut relancer). Changer la disposition sans changer la
 *     taille (réordonner des champs) n'est PAS détecté : relancer aussi.
 *  4. Le jeu ne doit pas démarrer de thread qui survive à un appel. */

#define ENG_GAME_API_VERSION 2

#if defined(_WIN32)
#define ENG_GAME_EXPORT __declspec(dllexport)
#else
#define ENG_GAME_EXPORT __attribute__((visibility("default")))
#endif

typedef enum EngMemKind {
    ENG_MEM_PERMANENT, /* vit toute l'exécution */
    ENG_MEM_LEVEL,     /* vit le temps d'un niveau */
    ENG_MEM_FRAME,     /* vidée au début de chaque image */
} EngMemKind;

/* Services du moteur. `userdata` est à repasser tel quel à chaque appel. */
typedef struct EngServices {
    uint32_t api_version;
    void *userdata;
    void (*log)(void *userdata, EngLogLevel level, const char *file, int line, const char *message);
    /* Retourne true s'il faut s'arrêter dans le débogueur. `message` peut être NULL. */
    bool (*assert_fail)(void *userdata, const char *expr, const char *file, int line,
                        const char *message);
    /* Mémoire mise à zéro prise dans une arène du moteur ; NULL si elle est pleine. */
    void *(*alloc)(void *userdata, EngMemKind kind, size_t size, size_t align);
} EngServices;

typedef struct EngGameApi {
    uint32_t api_version;
    size_t state_size;
    /* Premier chargement seulement : `state` est rempli de zéros. */
    void (*init)(void *state, const EngServices *services);
    /* Après chaque rechargement : `state` est déjà valide, le nouveau code reprend la main. */
    void (*reload)(void *state, const EngServices *services);
    /* Juste avant le déchargement de l'ancien code : dernière occasion de nettoyer. */
    void (*unload)(void *state);
    /* Un pas de simulation de `dt` secondes (durée fixe), avec les entrées de ce pas. */
    void (*update)(void *state, const EngInputState *input, double dt);
    /* Décrit l'image à dessiner ; `alpha` (0..1) interpole entre les deux derniers pas. */
    void (*render)(const void *state, double alpha, EngDebugSquare *out);
    /* Arrêt définitif du jeu (pas un rechargement). */
    void (*shutdown)(void *state);
} EngGameApi;

#define ENG_GAME_ENTRY_NAME "eng_game_get_api"
typedef const EngGameApi *(*EngGameGetApiFn)(void);
