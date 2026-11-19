#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/model.h"

/* Poids de skinning par sommet (M4-15). Le modèle reste un EngModel ordinaire (engine/model.h) ;
 * ce tableau, de MÊME longueur et dans le MÊME ordre que EngModel.vertices, dit à quelles
 * articulations chaque sommet obéit.
 *
 * Quatre influences au plus par sommet (JOINTS_0 / WEIGHTS_0 de glTF) : un sommet à plus de quatre
 * influences (JOINTS_1 / WEIGHTS_1) garde les quatre premières du jeu 0, renormalisées, avec un
 * avertissement. Poids en 8 bits non signés normalisés, de somme EXACTEMENT 255 : le vertex shader
 * lit des flottants dans [0, 1] dont la somme vaut 1 à 1/255 près. Les indices d'articulation sont
 * ceux du SQUELETTE (engine/skeleton.h), pas ceux du skin glTF : l'import les recode avec la
 * permutation `remap` de eng_skeleton_from_gltf. Une influence de poids nul porte l'indice 0. */
typedef struct EngSkinVertex {
    uint8_t joints[4];
    uint8_t weights[4];
} EngSkinVertex;

/* Comme eng_model_parse, pour un personnage animé : seuls les nœuds qui utilisent le skin
 * `skin_index` sont chargés, et leur matrice de nœud n'est PAS appliquée aux sommets (la
 * spécification glTF l'ignore pour un maillage skinné : les matrices d'articulation font tout). Les
 * sommets sont donc dans la pose de liaison du skin, exactement l'espace où `inverse_bind` les
 * attend. `remap` / `remap_count` : permutation rendue par eng_skeleton_from_gltf pour CE skin
 * (remap_count = nombre d'articulations du skin). `skin` : tableau de `model->vertex_count`
 * éléments alloué dans `arena`. Échec (false, journalisé), `model` et `*skin` mis à zéro, `arena`
 * restituée : indice de skin hors limites, `remap` incohérent, aucun maillage utilisant ce skin,
 * primitive sans JOINTS_0 / WEIGHTS_0 (vec4, même nombre d'éléments que POSITION), indice
 * d'articulation hors du skin, poids négatif, non fini, ou de somme nulle, et toutes les erreurs
 * de eng_model_parse. Les maillages qui n'utilisent pas ce skin sont ignorés (avertissement). */
bool eng_model_parse_skinned(EngArena *arena, EngArena *scratch, const void *data, size_t size,
                             const char *base_path, uint32_t skin_index, const uint32_t *remap,
                             uint32_t remap_count, EngModel *model, EngSkinVertex **skin);
