#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/noise.h"

/* Champ de hauteurs (M3-01) : une grille carrée de `size` x `size` échantillons 16 bits, espacés de
 * `spacing` mètres. L'échantillon (x, z) est à la position monde (x * spacing, z * spacing) ; z est
 * l'axe des lignes (la ligne 0 est la plus petite coordonnée z). Une valeur v donne l'altitude
 *   altitude_min + (v / 65535) * (altitude_max - altitude_min)   (mètres).
 *
 * Taille : pour des chunks de C cellules, size = k * C + 1 (1025 = 16 * 64 + 1 : un chunk de 64 m à
 * 1 m d'échantillonnage, 16 x 16 chunks = 1 km²). Le module n'impose que 2 <= size <= 8193.
 *
 * Fichier .hmap : conteneur de engine/asset_format.h, magic OWHF, version 1, sections INFO
 * (16 octets : size u32, spacing f32, altitude_min f32, altitude_max f32) et DATA (size * size
 * u16). Le lecteur vérifie tout (docs/terrain.md). */

#define ENG_HEIGHTMAP_MIN_SIZE 2U
#define ENG_HEIGHTMAP_MAX_SIZE 8193U

typedef struct EngHeightmap {
    uint32_t size;
    float spacing;           /* mètres entre deux échantillons voisins */
    float altitude_min;      /* mètres, valeur 0 */
    float altitude_max;      /* mètres, valeur 65535 ; strictement supérieure à altitude_min */
    const uint16_t *samples; /* size * size, ligne par ligne : samples[z * size + x] */
} EngHeightmap;

typedef struct EngHeightmapParams {
    uint32_t size;
    float spacing;
    float altitude_min;
    float altitude_max;
    int32_t seed;
    float wavelength; /* mètres : taille des plus grands reliefs (première octave) */
    uint32_t octaves; /* 1 à 12 */
    float lacunarity;
    float gain;
    EngNoiseFractal fractal;
} EngHeightmapParams;

/* 1025 x 1025, 1 m, de 0 à 120 m, graine 1337, reliefs de 512 m, 6 octaves, lacunarité 2, gain 0,5,
 * FBM. */
EngHeightmapParams eng_heightmap_default_params(void);

/* Vérifie les paramètres (journalisé) : tailles, valeurs finies, espacement > 0, altitude_max >
 * altitude_min, longueur d'onde > 0, 1 <= octaves <= 12, lacunarité et gain dans ]0, 8]. */
bool eng_heightmap_params_valid(const EngHeightmapParams *params);

/* Génère le champ par bruit fractal, puis l'étale sur toute la plage 16 bits : la valeur minimale
 * devient 0 (altitude_min) et la maximale 65535 (altitude_max), quelle que soit la graine. Un bruit
 * constant (tous les échantillons égaux) donne un champ nul. Les échantillons sont alloués dans
 * `arena` (2 * size * size octets, plus une ligne de travail de 4 * size octets qui reste dans
 * l'arène AVANT les échantillons). false (journalisé) : paramètres invalides ou arène pleine. */
bool eng_heightmap_generate(EngArena *arena, const EngHeightmapParams *params, EngHeightmap *out);

/* Altitude (mètres) de l'échantillon (x, z) ; altitude_min si x ou z sort de la grille. */
float eng_heightmap_altitude(const EngHeightmap *map, uint32_t x, uint32_t z);

/* Altitude (mètres) à la position (x_m, z_m) en mètres, par interpolation bilinéaire entre les
 * quatre échantillons voisins. Les positions hors de la grille sont ramenées sur son bord. */
float eng_heightmap_sample(const EngHeightmap *map, float x_m, float z_m);

/* Côté de la grille en mètres : (size - 1) * spacing. */
float eng_heightmap_extent(const EngHeightmap *map);

/* ── Fichiers ── */

/* Octets du .hmap de `map`, ou 0 si `map` est invalide. */
size_t eng_heightmap_file_size(const EngHeightmap *map);

/* Écrit le .hmap dans `out` (`capacity` >= eng_heightmap_file_size). Mêmes entrées, mêmes octets.
 */
bool eng_heightmap_encode(const EngHeightmap *map, void *out, size_t capacity, size_t *written);

/* Vérifie un .hmap en mémoire et le lit EN PLACE : `out->samples` pointe dans `data`, qui doit
 * rester valide (et aligné sur 2 octets : eng_file_read_all et les arènes le garantissent). false
 * (journalisé avec `name`) si le conteneur ou le contenu est refusé ; `out` est alors mis à zéro.
 */
bool eng_heightmap_open(const void *data, size_t size, const char *name, EngHeightmap *out);

/* Lit le fichier `path` (relatif à l'exécutable) dans `arena` puis l'ouvre. */
bool eng_heightmap_load(EngArena *arena, const char *path, EngHeightmap *out);

/* Écrit le .hmap de `map` dans `path` (relatif à l'exécutable, dossier existant). `scratch` sert de
 * tampon et est restituée. false (journalisé) : champ invalide, arène pleine ou écriture
 * impossible. */
bool eng_heightmap_save(EngArena *scratch, const EngHeightmap *map, const char *path);

/* ── PNG 16 bits (interopérabilité : éditeurs d'images, GIMP, Krita, World Machine...) ── */

/* Importe une image carrée (PNG 16 bits en niveaux de gris de préférence) comme champ de hauteurs.
 * Une image 8 bits ou en couleurs est acceptée : elle est convertie en niveaux de gris, et 8 bits
 * est étendu à 16 (v * 257) ; l'image n'est alors pas de la précision d'un vrai 16 bits. Refusée
 * (journalisé) : image non carrée, côté hors de [2, 8193], espacement ou altitudes invalides.
 * Échantillons alloués dans `arena`. */
bool eng_heightmap_import_image(EngArena *arena, const void *data, size_t size, float spacing,
                                float altitude_min, float altitude_max, EngHeightmap *out);

/* Encode `map` en PNG 16 bits en niveaux de gris (sans compression : environ 2 octets par
 * échantillon). Le PNG ne porte que les valeurs : espacement et altitudes ne sont pas conservés.
 * Alloué dans `arena`. */
bool eng_heightmap_encode_png(EngArena *arena, const EngHeightmap *map, const void **out,
                              size_t *out_size);
