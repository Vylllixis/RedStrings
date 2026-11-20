#ifndef ENG_BIOME_H
#define ENG_BIOME_H

/* Biomes de la grande carte (demande de Théo, 2026-11-20) : quatre zones qui s'enchaînent le long
 * de l'axe +X depuis le camp de réveil : plaine, montagne enneigée, désert, volcan. La frontière
 * entre deux biomes ondule (± `wobble_m` selon z) et se fond sur `blend_m` mètres. Tout est une
 * fonction pure de la position monde : deux cellules voisines se raccordent sans couture.
 *
 *   x < start_x + band_m                    : plaine (aussi tout ce qui est derrière le départ)
 *   [start_x + band_m ; start_x + 2 band_m] : montagne enneigée
 *   [start_x + 2 band_m ; + 3 band_m]       : désert
 *   au-delà                                 : volcan
 *
 * Chaque biome a une échelle de relief et une règle de matières (herbe, roche, terre, neige de la
 * splatmap). `enabled` faux (défaut de eng_world_gen_default) : plaine partout, échelle 1 : le
 * monde d'avant, au bit près. */

#include <stdbool.h>
#include <stdint.h>

#include "engine/splatmap.h"

typedef enum EngBiome {
    ENG_BIOME_PLAIN = 0,
    ENG_BIOME_SNOW = 1,
    ENG_BIOME_DESERT = 2,
    ENG_BIOME_VOLCANO = 3,
    ENG_BIOME_COUNT
} EngBiome;

typedef struct EngBiomeMap {
    bool enabled;
    float start_x;             /* x monde où commence la première bande (la plaine) */
    float band_m;              /* longueur d'un biome le long de X, > blend_m */
    float blend_m;             /* largeur du fondu entre deux biomes, > 0 */
    float wobble_m;            /* amplitude de l'ondulation de la frontière, >= 0 */
    float wobble_wavelength_m; /* longueur d'onde de l'ondulation le long de z, > 0 */
} EngBiomeMap;

/* Bandes de 800 m à partir de x = 0, fondu de 160 m, ondulation de ± 70 m tous les 900 m. */
EngBiomeMap eng_biome_map_default(void);
bool eng_biome_map_valid(const EngBiomeMap *map);

/* Poids des quatre biomes au point monde (x, z) : positifs, de somme 1. Carte NULL, invalide ou
 * désactivée : plaine seule. */
void eng_biome_weights(const EngBiomeMap *map, double x, double z, float out[ENG_BIOME_COUNT]);
/* Le biome de plus grand poids. */
EngBiome eng_biome_at(const EngBiomeMap *map, double x, double z);
const char *eng_biome_name(EngBiome biome);

/* Échelle du relief (multiplie l'altitude du générateur) pour ces poids : plaine 0,45, montagne
 * 1,8, désert 0,35, volcan 1,2 ; 1 si la carte est désactivée (poids NULL). */
float eng_biome_altitude_scale(const float weights[ENG_BIOME_COUNT]);

/* Volcan : coulées de lave. Motif fixe du monde (deux familles de lignes sinueuses qui se croisent)
 * : 1 au cœur d'une coulée (écart <= CORE), 0 hors des coulées (écart >= EDGE), fondu entre les
 * deux. `x`, `z` en mètres monde ; non finis : 0. Environ un dixième du sol, en coulées de 5 à 6 m
 * de large. */
#define ENG_BIOME_LAVA_CORE 0.025F
#define ENG_BIOME_LAVA_EDGE 0.075F
float eng_biome_lava_river(double x, double z);

/* Matières d'un point : `base` est le mélange de la splatmap ordinaire (herbe, roche, terre,
 * neige selon la pente et l'altitude) ; chaque biome le transforme, le résultat est le mélange des
 * quatre selon `weights` (somme conservée) :
 *   plaine  : herbe et terre, roche sur les pentes, jamais de neige ;
 *   montagne: la neige remplace l'herbe et la terre, roche sur les pentes ;
 *   désert  : du sable (dans le canal de l'herbe) à la place de l'herbe, de la terre et de la
 *             neige, roche sur les pentes raides ;
 *   volcan  : roche partout, un peu de terre (cendre), de la lave (dans le canal de la neige)
 *             selon `river` (eng_biome_lava_river, [0, 1]), sauf sur la roche des pentes. */
void eng_biome_splat(const float weights[ENG_BIOME_COUNT], const float base[ENG_SPLAT_LAYERS],
                     float river, float out[ENG_SPLAT_LAYERS]);

/* Luminosité du sol selon le biome, à multiplier aux poids de la splatmap (le shader du terrain
 * somme les couches sans les normaliser) : 1 partout, 0,45 dans le volcan (basalte sombre), fondu
 * entre les deux selon `weights`. NULL ou poids nuls : 1. */
float eng_biome_shade(const float weights[ENG_BIOME_COUNT]);

/* Le terrain n'a que quatre couches de texture ; le désert et le volcan en réutilisent deux :
 * `sand` : dans une cellule dont le bord ouest est à `x_min`, le canal de l'herbe est du SABLE (la
 * plaine n'y arrive plus) ; `lava` : le canal de la neige est de la LAVE (la montagne n'y arrive
 * plus). Vrai pour toute cellule au plus large que band_m - 2 x (wobble_m + blend_m / 2). Carte
 * désactivée ou NULL : faux. */
void eng_biome_cell_layers(const EngBiomeMap *map, double x_min, bool *sand, bool *lava);

#endif
