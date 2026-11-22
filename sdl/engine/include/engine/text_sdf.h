#ifndef ENG_TEXT_SDF_H
#define ENG_TEXT_SDF_H

/* Texte en champ de distance (2026-11-22) : une ligne de texte rasterisée avec une police TrueType
 * (stb_truetype) dans une image à UN canal où chaque octet est la distance au bord du glyphe :
 * ENG_TEXT_SDF_EDGE (128) sur le bord, plus à l'intérieur, moins à l'extérieur, sur `spread`
 * pixels de part et d'autre. Lue avec un filtre bilinéaire et comparée à 0,5, elle donne des
 * lettres nettes à toute taille, et la distance elle-même donne les halos, les contours et les
 * apparitions progressives (écran de chargement du lanceur). */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/arena.h"

#define ENG_TEXT_SDF_EDGE 128U
#define ENG_TEXT_SDF_MAX_SIDE 4096U

typedef struct EngTextSdf {
    uint8_t *pixels; /* width x height octets, ligne du haut d'abord ; dans l'arène */
    uint32_t width;
    uint32_t height;
    float spread;   /* distance (pixels) couverte de chaque côté du bord */
    float baseline; /* ligne de base, en pixels depuis le haut de l'image */
} EngTextSdf;

/* Rasterise `text` (UTF-8, une ligne) en lettres de `pixel_height` pixels de haut (8 à 512), avec
 * `tracking` pixels ajoutés entre les lettres (>= 0) et un champ de `spread` pixels (2 à 64) ;
 * l'image fait la taille du texte plus `spread` + 1 pixels de marge de chaque côté. `font` : les
 * octets d'un fichier .ttf. false (journalisé, `out` à zéro, arène restituée) si la police est
 * illisible, le texte vide ou sans glyphe visible, un paramètre hors bornes, l'image de plus de
 * ENG_TEXT_SDF_MAX_SIDE pixels de côté ou l'arène trop petite. */
bool eng_text_sdf_build(EngArena *arena, const uint8_t *font, size_t font_size, const char *text,
                        float pixel_height, float tracking, float spread, EngTextSdf *out);

/* Valeur du champ en (x, y) pixels (filtre bilinéaire, bords répétés) : la référence CPU de la
 * lecture du shader. Renvoie 0 pour une image vide. */
float eng_text_sdf_sample(const EngTextSdf *sdf, float x, float y);

#endif
