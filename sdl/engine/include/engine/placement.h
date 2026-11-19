#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/heightmap.h"
#include "engine/instancing.h"
#include "engine/splatmap.h"

/* Placement procédural d'instances sur le terrain (M3-08) : végétation, rochers, cailloux. Une
 * règle (EngPlacementRule) dit OÙ et avec quelle densité, le résultat est un tableau d'EngInstance
 * prêt pour l'instancing (engine/instancing.h).
 *
 * Principe : une grille de cellules de `cell_meters` mètres ; chaque cellule propose UN candidat, à
 * une position jitterée dans la cellule (hachage de la graine et de la cellule, sans état ni ordre
 * : le résultat dépend seulement de la graine, de la règle et du terrain). Le candidat est accepté
 * avec la probabilité « adéquation » (de 0 à 1) :
 *
 *   adéquation = pente x altitude x couche
 *   pente     = 1 jusqu'à slope_max - slope_falloff, puis décroît (smoothstep) jusqu'à 0 à
 * slope_max altitude  = 1 si l'altitude relative est dans [altitude_min, altitude_max], 0 sinon
 *   couche    = poids de density_layer (0 à 1) si density_layer >= 0 et >= layer_weight_min, sinon
 * 0 ; 1 si density_layer < 0 et 0 partout où le poids de la roche dépasse rock_weight_max (pas de
 * végétation sur la roche).
 *
 * La densité moyenne vaut donc adéquation / cell_meters² instances par m² : elle SUIT la splatmap.
 * Mêmes entrées, mêmes instances (octet pour octet) ; une autre graine donne un autre semis.
 * Voir docs/placement.md. */

typedef struct EngPlacementRule {
    uint32_t seed;
    float cell_meters;       /* > 0 : côté d'une cellule ; densité maximale 1 / cell_meters² */
    float slope_max_deg;     /* ]0, 90] : au-delà, rien */
    float slope_falloff_deg; /* [0, slope_max_deg] : largeur de la zone de décroissance */
    float altitude_min;      /* altitude relative du champ [0, 1], bornes incluses */
    float altitude_max;      /* >= altitude_min, <= 1 */
    int32_t density_layer;   /* ENG_SPLAT_* : la probabilité suit ce poids ; -1 : pas de couche */
    float layer_weight_min;  /* [0, 1] : poids minimum de density_layer pour pouvoir placer */
    float rock_weight_max;   /* [0, 1] : poids maximum de la roche ; 1 : pas de limite */
    float scale_min;         /* > 0 */
    float scale_max;         /* >= scale_min */
    float sink;          /* [0, 1[ : fraction de l'échelle dont l'instance s'enfonce dans le sol */
    uint8_t tint_min;    /* luminosité de la teinte tirée dans [tint_min, 255] (gris) */
    float margin_meters; /* >= 0 : bande de bord laissée vide */
} EngPlacementRule;

/* Une règle de végétation : cellule de 2 m, pente jusqu'à 28 degrés (décroissance sur 8), toute
 * altitude, herbe (poids >= 0,2), pas de roche au-delà de 0,15, échelle 0,6 à 1,2, enfoncement 5 %.
 */
EngPlacementRule eng_placement_vegetation_rule(void);
/* Une règle de rochers : cellule de 6 m, toute pente, roche (poids >= 0,35), échelle 0,5 à 2,0,
 * enfoncement 15 %. */
EngPlacementRule eng_placement_rock_rule(void);
/* Une règle d'herbe (M3-09) : cellule de 1 m, pente jusqu'à 30 degrés (décroissance sur 10), herbe
 * (poids >= 0,3), pas de roche au-delà de 0,1, échelle 0,7 à 1,4, sans enfoncement. */
EngPlacementRule eng_placement_grass_rule(void);
bool eng_placement_rule_valid(const EngPlacementRule *rule);

/* Adéquation (probabilité d'acceptation) à la position LOCALE (x_m, z_m) du champ, en mètres.
 * `splat` peut être NULL si la règle n'utilise ni couche ni limite de roche. 0 pour une position
 * hors de la grille ou dans la marge. Fonction pure : sert au placement et aux tests. */
float eng_placement_suitability(const EngHeightmap *map, const EngSplatmap *splat,
                                const EngPlacementRule *rule, float x_m, float z_m);

/* Poids bilinéaire (0 à 1) de la couche à la position locale ; 0 si la splatmap est absente. */
float eng_splatmap_weight_at(const EngHeightmap *map, const EngSplatmap *splat, uint32_t layer,
                             float x_m, float z_m);

/* Nombre d'instances que la règle produit (même calcul que eng_placement_generate, sans stockage).
 * `splat` doit avoir le côté du champ si elle est utilisée. Règle invalide : 0 (journalisé). */
uint32_t eng_placement_count(const EngHeightmap *map, const EngSplatmap *splat,
                             const EngPlacementRule *rule);

/* Génère les instances dans `arena` (le tableau est `*count` EngInstance). `origin_x`, `origin_z` :
 * position MONDE du coin (0, 0) du champ (celle de EngTerrainParams) ; la hauteur y est celle du
 * champ. Aucune instance : `*instances` = NULL, `*count` = 0, true. Règle ou splatmap invalide, ou
 * arène trop petite : false (journalisé), arène restituée. */
bool eng_placement_generate(EngArena *arena, const EngHeightmap *map, const EngSplatmap *splat,
                            const EngPlacementRule *rule, float origin_x, float origin_z,
                            EngInstance **instances, uint32_t *count);
