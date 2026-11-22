#pragma once

#include <stddef.h>
#include <stdint.h>

#include "engine/pbr.h"

/* Éclairage basé image (M1-16) : l'environnement (ciel) éclaire le matériau PBR de M1-15 à la place
 * de la lumière ambiante uniforme. Tout est CALCULÉ ICI, sur CPU, au chargement ; les shaders ne
 * font que lire les résultats (docs/ibl.md).
 *
 * Trois données dérivées de l'environnement L(w) (cubemap, lumière linéaire, RGB flottants) :
 *  - IRRADIANCE  E(n)/pi = (1/pi) ∫ L(w) max(n·w, 0) dw : le diffus vaut c_diff × irradiance(N) ;
 *  - PRÉ-FILTRÉE (« split sum », Karis 2013) : pour une rugosité r, la radiance de l'environnement
 *    filtrée par le lobe GGX (alpha = r²) avec N = V = R ; stockée par NIVEAU DE MIP, le niveau k
 *    de n niveaux valant r = k / (n - 1) ;
 *  - LUT BRDF : (A, B)(n·v, r) tels que le spéculaire vaut préfiltrée(R, r) × (F0 × A + B),
 *    intégrés avec la visibilité de Smith height-correlated (la même que le soleil).
 *
 * CUBEMAPS : 6 faces carrées dans l'ordre +X -X +Y -Y +Z -Z, convention OpenGL / Vulkan / Direct3D
 * (voir eng_ibl_cube_direction), directions en espace MONDE (Y vers le haut). Mémoire : floats RGB
 * (3 par texel), face par face, et pour chaque face ses niveaux de mip à la suite (niveau 0 =
 * `size` x `size`, puis size/2, … jusqu'à 1 x 1 pour `levels` niveaux). Pas d'allocation :
 * l'appelant fournit les tampons (eng_ibl_cube_floats donne la taille).
 *
 * Limites : l'échantillonnage d'un cube est bilinéaire DANS une face (pas de lissage au-dessus des
 * arêtes) ; un soleil très petit et très intense dans l'environnement est sous-échantillonné par
 * l'irradiance (calculée sur un niveau de mip bas, énergie conservée) ; pas d'harmoniques
 * sphériques ; l'éclairage ne change pas pendant le jeu (pas de sonde dynamique). */

#define ENG_IBL_BRDF_LUT_SIZE 64U

/* Nombre de floats d'un cube (6 faces, `levels` niveaux, 3 floats par texel). */
size_t eng_ibl_cube_floats(uint32_t size, uint32_t levels);
/* Nombre de niveaux de la chaîne complète d'un cube de côté `size` : 1 + log2(size) (size puissance
 * de 2). */
uint32_t eng_ibl_full_levels(uint32_t size);
/* Offset (en floats) du niveau `level` de la face `face`. */
size_t eng_ibl_cube_offset(uint32_t size, uint32_t levels, uint32_t face, uint32_t level);

/* Direction (non normalisée) du texel de coordonnées (s, t) dans [0, 1] de la face `face` ; t = 0
 * est la première ligne de l'image. Inverse : eng_ibl_cube_face_st. */
void eng_ibl_cube_direction(uint32_t face, float s, float t, float out[3]);
/* Face (0-5) et coordonnées (s, t) vues par la direction `dir` (non nulle). */
uint32_t eng_ibl_cube_face_st(const float dir[3], float *s, float *t);

/* Échantillonne un cube : bilinéaire dans la face, interpolation linéaire entre les niveaux
 * (`lod` flottant borné à [0, levels - 1]). `dir` non nulle, pas besoin d'être unitaire. */
void eng_ibl_cube_sample(const float *cube, uint32_t size, uint32_t levels, const float dir[3],
                         float lod, float out[3]);

/* Ciel procédural (niveau 0 seulement) : dégradé horizon -> zénith au-dessus de l'horizon, sol
 * uni, et un disque de soleil. Couleurs linéaires. */
typedef struct EngSkyDesc {
    float zenith[3];
    float horizon[3];
    float ground[3];
    float sun_direction[3]; /* unitaire, vers le soleil */
    float sun_radiance[3];  /* radiance du disque (peut être >> 1) */
    float sun_angular_radius_deg;
} EngSkyDesc;
void eng_ibl_sky_default(EngSkyDesc *sky);
/* Remplit le niveau 0 de `cube` ; `levels` niveaux alloués (les autres sont à remplir par
 * eng_ibl_build_mips). */
void eng_ibl_make_sky(float *cube, uint32_t size, uint32_t levels, const EngSkyDesc *sky);

/* Environnement depuis une image équirectangulaire RGB flottante (`width` x `height`, première
 * ligne = haut, convention de three.js : u = atan2(z, x) / 2pi + 0,5, v = asin(y) / pi + 0,5).
 * Échantillonnage bilinéaire. Remplit le niveau 0. */
void eng_ibl_from_equirect(float *cube, uint32_t size, uint32_t levels, const float *rgb,
                           uint32_t width, uint32_t height);

/* Niveaux 1.. de chaque face par moyenne 2x2 du niveau précédent. */
void eng_ibl_build_mips(float *cube, uint32_t size, uint32_t levels);

/* Irradiance/pi : `out` = cube de côté `out_size`, 1 niveau, intégrée sur le niveau de `source` de
 * côté <= 32 (le plus grand disponible). Quadrature exacte par angle solide de texel. */
void eng_ibl_irradiance(const float *source, uint32_t source_size, uint32_t source_levels,
                        float *out, uint32_t out_size);

/* Cube pré-filtré : `out` a `levels` niveaux de côté out_size, out_size/2, … ; niveau k : rugosité
 * k / (levels - 1). Importance sampling GGX avec échantillons filtrés par les mips de `source`
 * (Křivánek & Colbert, GPU Gems 3 / Filament), `samples` échantillons par texel. */
void eng_ibl_prefilter(const float *source, uint32_t source_size, uint32_t source_levels,
                       float *out, uint32_t out_size, uint32_t levels, uint32_t samples);

/* LUT BRDF, size x size, 2 floats (A, B) par texel ; ligne j : rugosité (j + 0,5) / size, colonne
 * i : n·v = (i + 0,5) / size (donc l'échantillonnage bilinéaire aux centres est exact). */
void eng_ibl_brdf_lut(float *out, uint32_t size, uint32_t samples);
/* Lecture bilinéaire de cette LUT (coordonnées bornées). */
void eng_ibl_brdf_lut_sample(const float *lut, uint32_t size, float n_dot_v, float roughness,
                             float *a, float *b);

/* Binaire16 <-> float (IEEE 754, arrondi au plus proche pair, dépassement -> inf). */
uint16_t eng_float_to_half(float value);
float eng_half_to_float(uint16_t half);
/* Convertit `count` floats ; si `src_stride` == 3 et `dst_channels` == 4, ajoute alpha = 1. */
void eng_ibl_pack_rgb_to_rgba16f(const float *rgb, size_t texels, uint16_t *out);
void eng_ibl_pack_rg_to_rg16f(const float *rg, size_t texels, uint16_t *out);

/* Données dérivées d'un environnement, tout calculé. Les pointeurs appartiennent à l'appelant. */
typedef struct EngIbl {
    const float *irradiance; /* cube, 1 niveau */
    uint32_t irradiance_size;
    const float *prefiltered; /* cube, prefiltered_levels niveaux */
    uint32_t prefiltered_size;
    uint32_t prefiltered_levels;
    const float *brdf_lut; /* ENG_IBL_BRDF_LUT_SIZE² x 2 */
} EngIbl;

/* Référence CPU de model.frag avec éclairage par image : eng_pbr_shade avec ambiante nulle (soleil
 * direct), plus
 *     ambiante × occlusion × (c_diff × irradiance(N) + prefiltrée(R, lod(r)) × (F0 × A + B))
 * avec R = reflect(-V, N), lod(r) = r × (prefiltered_levels - 1) et ambiante = ambient_color ×
 * ambient_intensity de `lighting` : elle devient l'INTENSITÉ DE L'ENVIRONNEMENT (1 : tel quel). */
void eng_pbr_shade_ibl(const EngLighting *lighting, const EngIbl *ibl, const EngPbrSurface *surface,
                       const float normal[3], const float position[3], float out[3]);
