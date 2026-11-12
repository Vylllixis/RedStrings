#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/arena.h"

/* Images (M1-08) : décodage (PNG, JPEG, TGA, BMP... via stb_image) et chaîne de mipmaps, sans GPU.
 *
 * Les pixels sont toujours RGBA 8 bits, ligne par ligne, la PREMIÈRE ligne étant le HAUT de
 * l'image (convention des fichiers image) ; une image sans alpha reçoit alpha = 255. */

typedef struct EngImage {
    uint32_t width;
    uint32_t height;
    uint8_t *pixels; /* width * height * 4 octets, propriété de l'image (eng_image_free) */
} EngImage;

#define ENG_IMAGE_MAX_SIZE 16384U /* côté maximal accepté : au-delà, refusé */

/* Décode une image en mémoire. false (journalisé) si le format est inconnu, les données
 * corrompues ou la taille hors limites ; `out` est alors mis à zéro. */
bool eng_image_decode(const void *data, size_t size, EngImage *out);

/* Lit puis décode un fichier (chemin relatif à l'exécutable, voir engine/files.h). `scratch`
 * sert à lire le fichier et est restituée dans son état d'origine. */
bool eng_image_load(EngArena *scratch, const char *path, EngImage *out);

/* Libère les pixels. Sans effet sur une image déjà libérée ou nulle. */
void eng_image_free(EngImage *image);

/* ── Mipmaps ── */

/* Nombre de niveaux d'une chaîne complète : 1 + floor(log2(max(width, height))). 0 si une
 * dimension est nulle. Exemples : 1 x 1 -> 1, 64 x 64 -> 7, 5 x 3 -> 3. */
uint32_t eng_image_mip_count(uint32_t width, uint32_t height);

/* Dimensions du niveau `level` (0 = image de base) : chaque niveau divise par 2, au minimum 1. */
void eng_image_mip_size(uint32_t width, uint32_t height, uint32_t level, uint32_t *out_width,
                        uint32_t *out_height);

/* Octets d'une chaîne de `levels` niveaux, base comprise, niveaux consécutifs sans remplissage. */
size_t eng_image_mip_chain_size(uint32_t width, uint32_t height, uint32_t levels);

/* Remplit `out` (eng_image_mip_chain_size octets) avec les niveaux 1 à levels-1 UNIQUEMENT, à la
 * suite l'un de l'autre (le niveau 0 est l'image elle-même). Chaque pixel d'un niveau est la
 * moyenne de la zone de l'image du niveau précédent qu'il recouvre (filtre boîte : exact pour les
 * puissances de 2 ; sinon chaque pixel source est compté une fois, avec des poids un peu inégaux
 * entre pixels voisins).
 *
 * `srgb` : les couleurs (R, V, B) sont encodées en sRGB. La moyenne se fait alors en lumière
 * LINÉAIRE puis se ré-encode : moyenner les valeurs encodées éclaircit mal ou assombrit trop (le
 * gris moyen d'un damier noir et blanc est 188 en sRGB, pas 128). L'alpha est toujours linéaire et
 * n'est pas prémultiplié. false si `levels` est hors de [1, chaîne complète] ou si un pointeur est
 * nul ; `levels` = 1 n'écrit rien et réussit. */
bool eng_image_generate_mips(const uint8_t *base, uint32_t width, uint32_t height, uint32_t levels,
                             bool srgb, uint8_t *out);
