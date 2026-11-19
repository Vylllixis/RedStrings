#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/map.h"
#include "engine/pool.h"

/* Gestionnaire d'assets (M2-05) : charge un asset par TYPE et CHEMIN, une seule fois, et le garde
 * tant que quelqu'un s'en sert.
 *
 *   uint32_t type = eng_assets_register(&assets, &(EngAssetType){...});
 *   EngAssetHandle a = eng_assets_acquire(&assets, type, "assets/cooked/models/demo.mesh");
 *   Payload *p = eng_assets_get(&assets, a);   // NULL si le handle est périmé
 *   eng_assets_release(&assets, a);            // le dernier release décharge
 *
 * - Demander deux fois le même (type, chemin) rend le MÊME handle et ne charge qu'une fois : chaque
 *   acquire compte une référence, chaque release en rend une. À zéro, l'asset est déchargé et son
 *   handle périmé (eng_assets_get le refuse : jamais de pointeur qui pend).
 * - Un échec de chargement n'est pas mémorisé : rien n'est gardé, le prochain acquire réessaie.
 * - Un chargeur peut demander d'autres assets (le gestionnaire est ré-entrant), mais pas le même
 *   (type, chemin) que celui qu'il charge : refusé.
 * - Un type décrit comment charger et décharger un « payload » de taille fixe, que le gestionnaire
 *   alloue (mis à zéro) et libère. Il ne connaît ni fichier, ni GPU : voir engine/asset_types.h.
 * - Une seule file d'exécution (comme le reste du moteur) : pas de verrou.
 *
 * - Rechargement (M2-06) : eng_assets_reload recharge un asset EN PLACE (même handle, même adresse
 * de payload) ; si le nouveau chargement échoue, l'ancien reste en service. Avec
 *   eng_assets_set_hot_reload, eng_assets_poll surveille les fichiers déclarés par chaque asset
 *   (eng_asset_load_dependency) et recharge ceux qui ont changé. Un payload doit donc pouvoir être
 *   déplacé octet par octet (aucun pointeur vers lui-même).
 *
 * Les chemins sont comparés tels quels (« a/b.mesh » et « a//b.mesh » sont deux assets). */

#define ENG_ASSET_MAX_TYPES 16U
#define ENG_ASSET_PATH_MAX 256U

typedef EngHandle EngAssetHandle;

typedef struct EngAssets EngAssets;
typedef struct EngAssetLoad EngAssetLoad;

typedef struct EngAssetType {
    const char *name; /* pour le journal ; doit rester valide */
    size_t payload_size;
    /* Remplit `payload` (mis à zéro) depuis `path`. `load` : sert à déclarer les fichiers dont
     * l'asset dépend (eng_asset_load_dependency). false : échec (déjà journalisé par le chargeur),
     * rien n'est gardé, `unload` n'est PAS appelé (le chargeur nettoie lui-même). */
    bool (*load)(EngAssetLoad *load, void *payload, const char *path, void *user);
    /* Libère ce que `load` a pris ; le payload lui-même est libéré par le gestionnaire. */
    void (*unload)(void *payload, void *user);
    void *user;
} EngAssetType;

typedef struct EngAssetStats {
    uint32_t loaded;          /* assets présents */
    uint32_t loads;           /* chargements réussis depuis le début */
    uint32_t unloads;         /* déchargements */
    uint32_t hits;            /* acquire servis par le cache */
    uint32_t failures;        /* chargements échoués */
    uint32_t reloads;         /* rechargements réussis (M2-06) */
    uint32_t reload_failures; /* rechargements échoués : l'ancienne version est restée en service */
} EngAssetStats;

struct EngAssets {
    EngPool pool;
    EngMap index; /* hachage de (type, chemin) -> EngAssetHandle */
    EngAssetType types[ENG_ASSET_MAX_TYPES];
    uint32_t type_count;
    EngAssetStats stats;
    double hot_interval; /* période de contrôle des fichiers surveillés */
    bool hot;
};

/* Fichiers d'un asset : à appeler depuis `load`. `path` : tel qu'on le passe à la lecture de
 * fichiers (résolu par le chargeur). Au plus ENG_ASSET_MAX_DEPENDENCIES ; au-delà, false
 * (journalisé). */
#define ENG_ASSET_MAX_DEPENDENCIES 4096U
bool eng_asset_load_dependency(EngAssetLoad *load, const char *path);

/* `capacity` : nombre maximal d'assets présents en même temps. */
bool eng_assets_init(EngAssets *assets, uint32_t capacity);

/* Décharge tout. Un asset encore référencé est une FUITE de référence : journalisée (un
 * avertissement par asset) ; la valeur rendue est le NOMBRE D'ASSETS concernés (quel que soit leur
 * nombre de références). L'asset est déchargé quand même. */
uint32_t eng_assets_shutdown(EngAssets *assets);

/* Enregistre un type ; rend son identifiant (>= 1), 0 si la description est invalide (nom, taille
 * du payload, `load` manquants) ou si ENG_ASSET_MAX_TYPES est atteint. */
uint32_t eng_assets_register(EngAssets *assets, const EngAssetType *type);

/* Charge si besoin et prend une référence. ENG_HANDLE_NULL (journalisé) : type inconnu, chemin vide
 * ou trop long, plus de place, échec du chargeur. */
EngAssetHandle eng_assets_acquire(EngAssets *assets, uint32_t type, const char *path);

/* Prend une référence de plus sur un asset déjà présent (copie d'un handle) ; rend `handle`, ou
 * ENG_HANDLE_NULL s'il est invalide. */
EngAssetHandle eng_assets_retain(EngAssets *assets, EngAssetHandle handle);

/* Rend une référence ; décharge à zéro. false (journalisé) si le handle est invalide ou périmé. */
bool eng_assets_release(EngAssets *assets, EngAssetHandle handle);

/* Payload de l'asset (type + chemin gardés par le gestionnaire), NULL si le handle est invalide. */
void *eng_assets_get(const EngAssets *assets, EngAssetHandle handle);

uint32_t eng_assets_refs(const EngAssets *assets, EngAssetHandle handle);    /* 0 si invalide */
const char *eng_assets_path(const EngAssets *assets, EngAssetHandle handle); /* NULL si invalide */
uint32_t eng_assets_type_of(const EngAssets *assets, EngAssetHandle handle); /* 0 si invalide */

/* Les fichiers déclarés par le chargeur ; `index` < eng_assets_dependency_count. */
uint32_t eng_assets_dependency_count(const EngAssets *assets, EngAssetHandle handle);
const char *eng_assets_dependency(const EngAssets *assets, EngAssetHandle handle, uint32_t index);

EngAssetStats eng_assets_stats(const EngAssets *assets);

/* ── Rechargement (M2-06) ── */

/* Recharge `handle` depuis ses fichiers : le nouveau payload est chargé À CÔTÉ de l'ancien ; s'il
 * réussit, l'ancien est déchargé et sa place est reprise (le handle, le nombre de références et
 * l'adresse du payload ne changent pas), les dépendances sont celles du nouveau chargement. S'il
 * échoue : false (journalisé, compté dans reload_failures), l'ancien asset est intact. Un chargeur
 * qui redemande l'asset qu'il recharge est refusé. À appeler en dehors de la construction d'une
 * image (les dessins déjà en file garderaient des ressources déchargées). */
bool eng_assets_reload(EngAssets *assets, EngAssetHandle handle);

/* Active (ou coupe) la surveillance des fichiers déclarés par les assets ; `interval_seconds` <= 0
 * : période par défaut (engine/files.h). Désactivé par défaut. */
void eng_assets_set_hot_reload(EngAssets *assets, bool enabled, double interval_seconds);

/* À appeler une fois par image, avant de construire l'image, avec un temps croissant en secondes :
 * recharge chaque asset dont un fichier a changé (modification stable : voir EngFileWatch). Rend le
 * nombre d'assets rechargés avec succès. Sans effet si la surveillance est coupée. Un échec ne
 * recommence qu'à la modification suivante du fichier. */
uint32_t eng_assets_poll(EngAssets *assets, double now_seconds);
