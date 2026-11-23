#pragma once

#include <SDL3/SDL_process.h>
#include <stdbool.h>
#include <stdint.h>

#include "engine/files.h"

/* Cuisson à la volée pendant le développement (M2-06) : surveille des fichiers SOURCES (PNG,
 * glTF...) et, quand l'un d'eux change, lance la commande qui recuit l'asset (en pratique `assetc`,
 * M2-04). Le gestionnaire d'assets (engine/assets.h, eng_assets_poll) voit ensuite changer le
 * fichier cuit et recharge l'asset : de la sauvegarde dans le logiciel de dessin à l'image à
 * l'écran, sans relancer le jeu ni la compilation.
 *
 *   const char *const argv[] = {"/chemin/assetc", "mesh", "src/a.gltf", "bin/a.mesh", NULL};
 *   EngCookRule rules[] = {{.name = "a", .source = "src/a.gltf", .argv = argv}};
 *   eng_asset_cook_init(&cook, rules, 1, 0.25);
 *   // à chaque image :
 *   eng_asset_cook_poll(&cook, now);
 *
 * - Une seule commande à la fois, lancée sans bloquer : la commande tourne pendant que les images
 *   continuent. Une règle déclenchée pendant qu'une autre tourne attend son tour.
 * - Une règle dont la source change PENDANT sa propre cuisson est relancée ensuite (la nouvelle
 *   version n'est pas perdue).
 * - Les chaînes des règles doivent rester valides jusqu'à eng_asset_cook_shutdown (elles ne sont
 * pas copiées). Les commandes ne passent pas par un shell : pas de redirection ni de variable. */

#define ENG_COOK_MAX_RULES 64U

typedef struct EngCookRule {
    const char *name;        /* pour le journal */
    const char *source;      /* fichier surveillé, chemin complet */
    const char *const *argv; /* commande et arguments, terminé par NULL */
} EngCookRule;

typedef struct EngCookState {
    EngCookRule rule;
    EngFileWatch watch;
    bool pending; /* source modifiée, cuisson à lancer */
} EngCookState;

typedef struct EngAssetCook {
    EngCookState rules[ENG_COOK_MAX_RULES];
    uint32_t rule_count;
    SDL_Process *process; /* commande en cours, NULL sinon */
    uint32_t running;     /* indice de la règle en cours */
    uint32_t succeeded;   /* cuissons terminées avec le code 0 */
    uint32_t failed;      /* cuissons échouées (code non nul ou lancement impossible) */
} EngAssetCook;

/* `interval_seconds` : période de contrôle des sources (<= 0 : défaut). false (journalisé) : règle
 * invalide (nom, source ou commande manquants), trop de règles. */
bool eng_asset_cook_init(EngAssetCook *cook, const EngCookRule *rules, uint32_t count,
                         double interval_seconds);

/* À appeler une fois par image avec un temps croissant en secondes. Détecte les sources modifiées,
 * recueille la commande terminée, en lance une autre si besoin. Rend le nombre de cuissons
 * terminées avec succès pendant cet appel. */
uint32_t eng_asset_cook_poll(EngAssetCook *cook, double now_seconds);

/* Vrai tant qu'une commande tourne ou qu'une cuisson attend. */
bool eng_asset_cook_busy(const EngAssetCook *cook);

/* Arrête la commande en cours, s'il y en a une. */
void eng_asset_cook_shutdown(EngAssetCook *cook);
