#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/assets.h"
#include "engine/environment.h"
#include "engine/model_gpu.h"
#include "engine/renderer.h"

/* Types d'assets fournis par le moteur pour le gestionnaire (engine/assets.h).
 *
 * MODÈLE : un .mesh cuit (M2-02) et les .tex qu'il désigne, chargés sans aucun parsing glTF ni
 * décodage d'image, puis envoyés au GPU. Le payload est un EngModelAsset ; `eng_model_asset_gpu`
 * donne le modèle à dessiner. Les fichiers déclarés comme dépendances de l'asset (pour le
 * rechargement à chaud, M2-06) sont le .mesh et chacun de ses .tex, chemins résolus (ceux que
 * eng_file_read_all reçoit).
 *
 * Le chemin passé à eng_assets_acquire est celui de eng_mesh_load : relatif à l'exécutable. */

typedef struct EngModelAsset {
    EngModelGpu gpu;
    EngArena arena; /* tableaux de `gpu` : parties, matériaux, handles de textures */
} EngModelAsset;

/* Réglages communs aux modèles chargés ; la structure doit VIVRE AUSSI LONGTEMPS que le
 * gestionnaire (il en garde l'adresse). Les champs peuvent être changés entre deux chargements. */
typedef struct EngModelAssetContext {
    EngRenderer *renderer;
    float anisotropy; /* filtrage anisotrope des textures (0 : aucun, jusqu'à 16) */
    const EngEnvironment *environment; /* éclairage par image des modèles chargés ; NULL : défaut.
                                          Doit vivre plus longtemps que les modèles. */
    size_t cpu_bytes; /* mémoire de travail d'UN chargement (modèle décodé, puis
                         fichiers lus) : 0 = ENG_MODEL_ASSET_DEFAULT_CPU_BYTES.
                         Réservée à chaque chargement et rendue après. */
} EngModelAssetContext;

#define ENG_MODEL_ASSET_DEFAULT_CPU_BYTES ((size_t)64 * 1024 * 1024)

/* Enregistre le type « modèle » ; rend son identifiant (0 : échec journalisé). */
uint32_t eng_model_assets_register(EngAssets *assets, EngModelAssetContext *context);

/* Le modèle GPU d'un asset de type modèle ; NULL si le handle est invalide ou d'un autre type. */
const EngModelGpu *eng_model_asset_gpu(const EngAssets *assets, uint32_t model_type,
                                       EngAssetHandle handle);
