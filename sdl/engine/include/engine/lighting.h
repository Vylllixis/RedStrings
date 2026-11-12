#pragma once

#include <stdbool.h>

/* Éclairage (M1-13, PBR depuis M1-15) : UNE lumière directionnelle (le soleil) et une lumière
 * ambiante uniforme.
 *
 * Tout est dans le MONDE (docs/conventions-maths.md : main droite, Y vers le haut, mètres) : la
 * lumière ne suit pas la caméra. Les couleurs et intensités sont en lumière LINÉAIRE (pas sRGB) ;
 * le résultat est écrit tel quel dans la cible HDR (valeurs au-delà de 1,0 permises) ;
 * l'exposition, le tonemapping et le gamma viennent ensuite (engine/tonemap.h,
 * docs/tonemapping.md).
 *
 * L'éclairage de chaque point est calculé par le matériau PBR (engine/pbr.h, docs/pbr.md) : cette
 * structure ne contient que la lumière, pas l'aspect de la surface. `sun_intensity` = 1 correspond
 * à un éclairement de pi : une surface blanche mate éclairée de face vaut 1. */

typedef struct EngLighting {
    float sun_direction[3]; /* UNITAIRE, du point éclairé VERS le soleil (monde) */
    float sun_color[3];     /* linéaire, [0, 1] par composante (valeurs plus grandes acceptées) */
    float sun_intensity;    /* >= 0 */
    float ambient_color[3];
    float ambient_intensity;  /* >= 0 */
    float camera_position[3]; /* monde, mètres : sert au vecteur V (vers la caméra) */
} EngLighting;

/* Valeurs par défaut : soleil blanc d'intensité 1 à 45° de hauteur, azimut 30°, lumière ambiante
 * 0,25 (blanche), caméra à l'origine. */
void eng_lighting_default(EngLighting *lighting);

/* Oriente le soleil. `azimuth_deg` : angle autour de la verticale, 0° = vers +Z, 90° = vers +X ;
 * `elevation_deg` : hauteur au-dessus de l'horizon, 90° = à la verticale (vers +Y), -90° = vers -Y
 * (la lumière vient d'en dessous). L'élévation est bornée à [-90, 90] ; une valeur non numérique
 * est refusée (false, éclairage inchangé). */
bool eng_lighting_set_sun_angles(EngLighting *lighting, float azimuth_deg, float elevation_deg);

/* Rend l'éclairage utilisable par le shader : direction normalisée (+Y si elle est nulle ou non
 * numérique), composantes ni négatives ni non numériques (ramenées à 0), intensités >= 0. Retourne
 * true si rien n'a dû être corrigé. Appelée par eng_model_gpu_draw. */
bool eng_lighting_sanitize(EngLighting *lighting);
