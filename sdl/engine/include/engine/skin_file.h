#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/skin.h"

/* Fichiers .skin (M4-21) : les poids de skinning d'un .mesh cuit. Disposition :
 * docs/asset-formats.md §7. Le .mesh (v1) ne porte pas de poids ; assetc mesh --skin N écrit donc
 * <sortie>.skin À CÔTÉ : un EngSkinVertex par sommet, dans le MÊME ordre que les sommets du .mesh.
 * Les indices d'articulation sont ceux du .skel cuit du même skin (permutation déjà appliquée).
 *
 * Le décodeur vérifie : nombre de sommets (comme le .mesh), nombre d'articulations dans
 * [1 ; 256], somme des poids de chaque sommet = 255, indice d'articulation < nombre
 * d'articulations pour toute influence de poids non nul, indice 0 pour une influence de poids nul.
 */

#define ENG_SKIN_FILE_MAX_VERTICES 0x3FFFFFFFU

/* Construit un .skin pour `vertex_count` sommets et un squelette de `joint_count` articulations.
 * Les mêmes règles que le décodeur s'appliquent à l'entrée. Le fichier est alloué dans `arena`.
 * Échec (false, journalisé) : arène restituée. */
bool eng_skin_encode(EngArena *arena, const EngSkinVertex *skin, uint32_t vertex_count,
                     uint32_t joint_count, const void **out, size_t *out_size);

/* Vérifie puis décode. `expected_vertices` : nombre de sommets du .mesh associé (refus si
 * différent). `*joint_count` (peut être NULL) reçoit le nombre d'articulations. `*skin` est alloué
 * dans `arena`. Échec : `*skin` NULL, arène restituée. */
bool eng_skin_decode(EngArena *arena, const void *data, size_t size, const char *name,
                     uint32_t expected_vertices, EngSkinVertex **skin, uint32_t *joint_count);

/* Lit (chemin relatif à l'exécutable) et décode. `scratch` est restituée. */
bool eng_skin_load(EngArena *arena, EngArena *scratch, const char *path, uint32_t expected_vertices,
                   EngSkinVertex **skin, uint32_t *joint_count);
