#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/arena.h"

/* Clip d'animation (M4-13) : pour chaque articulation d'un squelette (engine/skeleton.h), des
 * canaux de clés de translation, de rotation ou d'échelle LOCALES, au même sens que la pose de
 * repos du squelette (celle-ci est la valeur d'une articulation sans canal). Lu depuis un .anim
 * cuit (docs/asset-formats.md §6) ou importé d'une animation glTF (Mixamo, Blender...).
 *
 * Un canal = une articulation + une propriété (EngAnimPath) ; au plus un canal par couple, rangés
 * par (articulation, propriété) croissants : la recherche est dichotomique et la sortie
 * déterministe. Les clés d'un canal ont des instants STRICTEMENT croissants, en secondes, >= 0.
 * Entre deux clés : interpolation linéaire (translation, échelle) ou sphérique par le plus court
 * chemin (rotation) ; avant la première clé et après la dernière : valeur tenue. Le clip ne boucle
 * pas : c'est à l'appelant de ramener le temps dans [0 ; duration] (M4-14).
 *
 * Interpolations glTF STEP et CUBICSPLINE : non représentées dans la version 1 ; l'import les
 * refuse (message explicite) au lieu de les approcher en silence. */

#define ENG_ANIM_MAX_CHANNELS 768U       /* 256 articulations x 3 propriétés */
#define ENG_ANIM_MAX_KEYS 1048576U       /* clés par canal */
#define ENG_ANIM_MAX_TOTAL_KEYS 4194304U /* clés par clip */

typedef enum EngAnimPath {
    ENG_ANIM_TRANSLATION = 0, /* 3 flottants : x, y, z (mètres) */
    ENG_ANIM_ROTATION = 1,    /* 4 flottants : quaternion x, y, z, w unitaire */
    ENG_ANIM_SCALE = 2,       /* 3 flottants */
    ENG_ANIM_PATH_COUNT
} EngAnimPath;

/* 16 octets, identique au disque (docs/asset-formats.md §6). */
typedef struct EngAnimChannel {
    uint32_t joint;     /* indice dans le squelette (0 à 255) */
    uint32_t path;      /* EngAnimPath */
    uint32_t key_count; /* 1 à ENG_ANIM_MAX_KEYS */
    uint32_t first_key; /* indice de la première clé dans `times` (somme des canaux précédents) */
} EngAnimChannel;

typedef struct EngAnimClip {
    float duration; /* secondes, >= instant de la dernière clé de chaque canal */
    uint32_t channel_count;
    EngAnimChannel *channels;
    uint32_t key_total;    /* somme des key_count */
    float *times;          /* key_total instants */
    float *values;         /* 3 ou 4 flottants par clé selon la propriété, canal après canal */
    uint32_t *value_first; /* channel_count : indice du premier flottant du canal dans `values` */
    const char *skeleton_path; /* .skel visé, tel qu'écrit par le cuiseur */
} EngAnimClip;

/* Nombre de flottants d'une valeur de la propriété (3 ou 4) ; 0 si `path` est invalide. */
uint32_t eng_anim_path_components(uint32_t path);

/* Construit un .anim depuis `clip` (déterministe). Clip invalide (mêmes règles que le décodeur) ou
 * mémoire insuffisante : false (journalisé), arène restituée, sorties à zéro. */
bool eng_anim_encode(EngArena *arena, const EngAnimClip *clip, const void **out, size_t *out_size);

/* Vérifie puis décode un .anim en mémoire (tableaux et chaîne alloués dans `arena`). Vérifié :
 * conteneur, tailles et comptes cohérents, canaux rangés et uniques, instants finis, >= 0 et
 * strictement croissants, durée >= dernière clé, valeurs finies, quaternions unitaires, chemin du
 * .skel terminé par zéro. Refusé : false, `out` à zéro, `arena` restituée. */
bool eng_anim_decode(EngArena *arena, const void *data, size_t size, const char *name,
                     EngAnimClip *out);

/* Lit (chemin relatif à l'exécutable) et décode ; `scratch` est restituée. */
bool eng_anim_load(EngArena *arena, EngArena *scratch, const char *path, EngAnimClip *out);

/* Indice du canal (joint, path), ou UINT32_MAX s'il n'existe pas. */
uint32_t eng_anim_find_channel(const EngAnimClip *clip, uint32_t joint, uint32_t path);

/* Valeur du canal `channel` à l'instant `time` (secondes ; hors de la plage des clés : valeur
 * tenue). Écrit 3 ou 4 flottants dans `out` (rotation : unitaire) et renvoie ce nombre ; 0 si les
 * arguments sont invalides (clip NULL, canal inexistant, `time` non fini). */
uint32_t eng_anim_sample(const EngAnimClip *clip, uint32_t channel, float time, float out[4]);

/* Statistiques d'un import glTF. */
typedef struct EngAnimImportStats {
    uint32_t gltf_channels;    /* canaux de l'animation glTF */
    uint32_t kept_channels;    /* canaux du clip produit */
    uint32_t ignored_channels; /* cibles hors du skin, ou propriété « weights » */
    uint32_t
        rebaked_joints; /* articulations dont les canaux ont été recalculés (voir ci-dessous) */
} EngAnimImportStats;

/* Nombre d'animations d'un glTF en mémoire ; 0 si illisible (journalisé). */
uint32_t eng_anim_gltf_clip_count(const void *data, size_t size);

/* Importe l'animation `anim_index` d'un glTF / GLB en mémoire pour le squelette du skin
 * `skin_index` (le MÊME skin que celui cuit par eng_skeleton_from_gltf : les indices
 * d'articulation du clip sont ceux du squelette, parents d'abord). `skeleton_path` : chemin du
 * .skel à inscrire dans le clip (obligatoire, non vide, <= 255 octets).
 *
 * Les canaux dont la cible n'est pas une articulation du skin sont ignorés (comptés dans `stats`).
 * Une articulation dont les ancêtres NON articulations (ex. « Armature » à l'échelle 0,01 de
 * Mixamo) ont été repliés dans sa pose de repos par l'import du squelette est RECALCULÉE : ses
 * canaux sont échantillonnés à l'union de leurs instants, composés avec les ancêtres repliés, puis
 * redécomposés en translation / rotation / échelle, pour que le clip reste exprimé dans le même
 * repère que le squelette. Les autres articulations gardent leurs clés telles quelles.
 *
 * Refusé (false, journalisé, arènes restituées) : animation inexistante, interpolation STEP ou
 * CUBICSPLINE, instants non croissants ou négatifs, valeurs non finies, quaternion nul, aucun
 * canal utilisable, plus de ENG_ANIM_MAX_KEYS clés par canal, deux canaux sur la même cible,
 * pose non décomposable (échelle nulle, négative ou miroir). `stats` peut être NULL. */
bool eng_anim_from_gltf(EngArena *arena, EngArena *scratch, const void *data, size_t size,
                        const char *base_path, uint32_t anim_index, uint32_t skin_index,
                        const char *skeleton_path, EngAnimClip *out, EngAnimImportStats *stats);
