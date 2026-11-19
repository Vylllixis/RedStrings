#pragma once

#include <stdint.h>

#include "engine/splatmap.h"

/* Textures de couche du terrain (M3-03) générées par le code : QUATRE textures d'albédo RGBA 8 bits
 * (sRGB), qui se raccordent sans couture quand on les répète (herbe, roche, terre, neige). Ce sont
 * des textures de remplacement : l'art définitif viendra des assets (le moteur accepte n'importe
 * quels pixels, voir EngTerrainGpuParams). Le bruit est une somme d'octaves de bruit de valeurs
 * PÉRIODIQUE : le réseau de points se répète sur la texture, donc le bord droit se raccorde au bord
 * gauche et le bas au haut. Le résultat est une fonction de (x / size, y / size) : une texture de
 * 2n x 2n a, aux positions paires, exactement les pixels de celle de n x n. */

#define ENG_TERRAIN_LAYER_TEXTURE_SIZE 256U

/* Écrit size x size pixels RGBA (alpha 255) dans `rgba` (4 * size * size octets). `size` >= 4 ;
 * `layer` hors des quatre couches : rien n'est écrit. */
void eng_terrain_layer_pixels(EngSplatLayer layer, uint32_t size, uint8_t *rgba);
