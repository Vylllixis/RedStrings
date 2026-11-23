#pragma once

#include "engine/lighting.h"

/* Matériau PBR métal/rugosité (M1-15) : référence CPU de ce que calcule le fragment shader
 * `model.frag`. Le modèle est celui de glTF 2.0 (annexe B de la spécification) et du viewer de
 * Khronos : Cook-Torrance avec une distribution GGX, la visibilité de Smith « height-correlated »
 * et le Fresnel de Schlick ; diffus de Lambert pondéré par (1 - F) et par (1 - métal).
 *
 * Tout est linéaire et en espace MONDE (docs/conventions-maths.md). Pour une surface de couleur de
 * base `base`, de métal `m`, de rugosité `r`, de normale N, vue depuis V, éclairée par le soleil
 * (L, H = normalize(L + V)) :
 *
 *     alpha   = max(r, ENG_ROUGHNESS_MIN)^2
 *     F0      = lerp(0,04, base, m)                    c_diff = base × (1 - m)
 *     D       = alpha^2 / (pi × ((N·H)^2 × (alpha^2 - 1) + 1)^2)
 *     Vis     = 0,5 / (N·L × sqrt((N·V)^2 × (1 - alpha^2) + alpha^2)
 *                      + N·V × sqrt((N·L)^2 × (1 - alpha^2) + alpha^2))
 *     F       = F0 + (1 - F0) × (1 - V·H)^5
 *     direct  = (1 - F) × c_diff / pi + F × D × Vis ; multiplié par E × N·L
 *
 * UNITÉS : `sun_intensity` = 1 correspond à un éclairement de pi, de sorte qu'une surface blanche
 * mate éclairée de face vaut 1 (c'est la convention « sans pi » des moteurs temps réel, et celle de
 * l'éclairage Blinn-Phong d'avant M1-15 : les intensités gardent le même sens). Le terme direct est
 * donc sun × N·L × ((1 - F) × c_diff + pi × F × D × Vis)       avec sun = sun_color ×
 * sun_intensity.
 *
 * LUMIÈRE AMBIANTE (avant l'éclairage par image de M1-16) : un environnement uniforme de radiance
 * `ambient` = ambient_color × ambient_intensity. Son diffus vaut c_diff × ambient ; son spéculaire
 * vaut ambient × (F0 × A + B), avec (A, B) l'approximation analytique de Karis du « split sum »
 * (eng_pbr_env_brdf). Les deux sont multipliés par l'OCCLUSION ; l'émissif s'ajoute à la fin :
 *
 *     couleur = direct + occlusion × ambient × (c_diff + F0 × A + B) + emissif
 *
 * Limites : un seul soleil, pas d'ombre, pas de diffusion multiple, pas de transparence. */

#define ENG_ROUGHNESS_MIN 0.045F

typedef struct EngPbrSurface {
    float base_color[3]; /* linéaire (facteur × texture sRGB décodée) */
    float metallic;      /* [0, 1] (facteur × canal B de la texture) */
    float roughness;     /* [0, 1] (facteur × canal G de la texture) */
    float occlusion;     /* [0, 1], 1 : aucune (déjà pondérée par la force du matériau) */
    float emissive[3];   /* linéaire, ajouté tel quel */
} EngPbrSurface;

/* Valeurs d'un matériau « blanc mat » : base 1, métal 0, rugosité 1, occlusion 1, pas d'émission.
 */
void eng_pbr_surface_default(EngPbrSurface *surface);

/* Ramène une surface dans son domaine : métal, rugosité et occlusion dans [0, 1], couleurs et
 * émissif >= 0, valeurs non numériques remplacées par 1 (rugosité), 0 (le reste) ou 1
 * (occlusion). Retourne true si rien n'a dû être corrigé. */
bool eng_pbr_sanitize(EngPbrSurface *surface);

/* Facteurs du modèle, exposés pour les tests. n_dot_x >= 0 ; `alpha` = rugosité². */
float eng_pbr_ndf_ggx(float n_dot_h, float alpha);
float eng_pbr_visibility_smith_ggx(float n_dot_l, float n_dot_v, float alpha);
float eng_pbr_fresnel_schlick(float f0, float v_dot_h);

/* Approximation de Karis du terme du « split sum » : écrit (A, B) tels que le spéculaire d'un
 * environnement uniforme de radiance 1 vaut F0 × A + B. */
void eng_pbr_env_brdf(float n_dot_v, float roughness, float *a, float *b);

/* Normal map : `sample` est la valeur lue dans la texture, dans [0, 1] par canal ; `scale` la
 * force (échelle de x et y) ; `normal` unitaire et `tangent` (xyz unitaire, w = ±1) viennent du
 * sommet (engine/model.h). Écrit la normale monde unitaire, perturbée :
 *     N' = normalize(T × (2 sx - 1) × scale + B × (2 sy - 1) × scale + N × (2 sz - 1))
 * avec T = normalize(tangent - N (N·tangent)) et B = cross(N, T) × tangent.w. */
void eng_pbr_perturb_normal(const float normal[3], const float tangent[4], const float sample[3],
                            float scale, float out[3]);

/* Couleur linéaire (RGB) d'un point : voir plus haut. `normal` : normale de SHADING (déjà
 * perturbée si le matériau a une normal map), normalisée ici ; `position` : monde, mètres ;
 * l'éclairage est supposé valide (eng_lighting_sanitize) et la surface aussi (eng_pbr_sanitize). */
void eng_pbr_shade(const EngLighting *lighting, const EngPbrSurface *surface, const float normal[3],
                   const float position[3], float out[3]);
