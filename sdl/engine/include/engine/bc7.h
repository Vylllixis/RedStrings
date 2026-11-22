#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Compression BC7 d'un bloc de 4 x 4 texels (M2-03). Enveloppe de bc7enc (code tiers, MIT ou
 * domaine public, compilé sans -Werror dans engine/src/third_party_bc7.c) : le reste du moteur ne
 * voit jamais ses types.
 *
 * Qualité : bc7enc n'emploie que les modes 1 et 6 pour les blocs opaques et 5, 6, 7 pour les blocs
 * avec transparence. C'est moins fin qu'un encodeur complet (8 modes) ; la qualité mesurée sur
 * Sponza est donnée dans docs/asset-formats.md. */

#define ENG_BC7_BLOCK_BYTES 16U

/* À appeler une fois avant eng_bc7_encode_block (initialise des tables ; idempotent, mais pas sûr
 * si deux fils l'appellent en même temps pour la première fois). */
void eng_bc7_init(void);

/* Encode 16 texels RGBA8 (`rgba` : 4 x 4 x 4 octets, ligne par ligne, R en premier) en un bloc de
 * 16 octets. `perceptual` : l'erreur est mesurée en luminance/chrominance (couleurs) plutôt qu'en
 * RGB (données : normales...). `effort` : 0 (rapide) à 4 (lent, un peu meilleur). Toujours un bloc
 * valide. */
void eng_bc7_encode_block(uint8_t out[16], const uint8_t rgba[64], bool perceptual,
                          uint32_t effort);
