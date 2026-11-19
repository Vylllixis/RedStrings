#pragma once

#include <stdbool.h>

#include "engine/ibl.h"
#include "engine/math.h"
#include "engine/renderer.h"

/* Ciel atmosphérique analytique simplifié (M3-05).
 *
 * Diffusion SIMPLE (un seul rebond) de Rayleigh et de Mie dans une atmosphère plane, avec des
 * masses d'air au lieu d'une intégration le long du rayon :
 *
 *   m(mu)   = 1 / (mu + 0,025 exp(-11 mu))      masse d'air, mu = cosinus de l'angle au zénith
 *                                               (1 au zénith, 40 à l'horizon)
 *   ext     = rayleigh + mie                    épaisseur optique au zénith, par canal RVB
 *   S(cos)  = rayleigh P_R(cos) + mie P_M(cos)  P_R = 3/(16 pi) (1 + cos^2), P_M =
 * Henyey-Greenstein L       = E S (1 - exp(-ext m_vue)) / ext exp(-ext m_soleil)
 *
 * m_vue est plafonnée (`path_limit`) et m_soleil aussi (`sun_path_limit`) : sans plafond, l'horizon
 * serait trop clair et un soleil bas éteindrait tout le ciel (les points diffusants en altitude
 * voient le soleil à travers moins d'air que le sol). Sous l'horizon le soleil éclaire le ciel
 * jusqu'à environ -12° (crépuscule), puis il reste le plancher `night`.
 *
 * C'est une approximation : elle donne les bons ORDRES de couleur et de luminosité (zénith bleu à
 * midi, horizon plus pâle, ciel et soleil rougeoyants au coucher, nuit sombre), pas les valeurs
 * d'un modèle de référence. Rien n'est validé contre une atmosphère mesurée ni contre Hillaire ou
 * Hosek-Wilkie. Tout est linéaire (pas sRGB), en radiance HDR. Voir docs/atmosphere.md.
 *
 * Le shader atmosphere.frag.hlsl recopie ces formules (HLSL n'a pas d'#include dans cette chaîne
 * d'outils) ; un test compare les deux, pixel par pixel, sur des directions et des hauteurs de
 * soleil variées. */

typedef struct EngAtmosphere {
    float rayleigh[3];    /* épaisseur optique de Rayleigh au zénith, par canal (rouge < bleu) */
    float mie;            /* épaisseur optique de Mie au zénith (grise) */
    float mie_g;          /* anisotropie de Mie, ]-1, 1[ (0,76 : très vers l'avant) */
    float irradiance;     /* E : échelle de la radiance du ciel */
    float sun_radiance;   /* radiance du disque avant atténuation */
    float sun_radius_deg; /* rayon angulaire du disque, ]0, 45] */
    float path_limit;     /* masse d'air maximale le long du rayon de vue, >= 1 */
    float sun_path_limit; /* masse d'air maximale du soleil au point diffusant, >= 1 */
    float ground_albedo[3]; /* sous l'horizon, [0, 1] */
    float night[3];         /* radiance ajoutée en permanence (ciel nocturne), >= 0 */
} EngAtmosphere;

/* Valeurs par défaut : Rayleigh (0,040 ; 0,094 ; 0,229) soit les épaisseurs optiques à 680, 550 et
 * 440 nm, Mie 0,03, g = 0,70, E = 50, disque de radiance 60 et de rayon 0,8°, plafonds 8 et 15, sol
 * brun clair 0,25, nuit (0,002 ; 0,003 ; 0,006). */
void eng_atmosphere_default(EngAtmosphere *atmosphere);

/* Tous les champs finis et dans leur plage. */
bool eng_atmosphere_valid(const EngAtmosphere *atmosphere);

/* Masse d'air m(mu) ci-dessus ; mu borné à [0, 1]. */
float eng_air_mass(float cos_zenith);

/* Radiance du ciel (SANS disque du soleil) vers `view` quand le soleil est dans la direction
 * `sun`. Les deux directions sont normalisées ici. Sous l'horizon (view.y < 0) : le sol, de
 * radiance albedo x ciel à l'horizon, avec un raccord sur 0,05 en y. Retourne false (et `rgb` = 0)
 * si l'atmosphère est invalide ou une direction est nulle ou non finie. */
bool eng_atmosphere_sky(const EngAtmosphere *atmosphere, const float sun[3], const float view[3],
                        float rgb[3]);

/* Radiance du ciel À L'HORIZON (y = 0) dans l'azimut de `direction` (seules ses composantes x et z
 * comptent ; verticale ou nulle : +X). C'est ce que dessine eng_atmosphere_sky juste au-dessus de
 * l'horizon dans cet azimut : la couleur du brouillard (engine/fog.h) y est raccordée. Mêmes
 * conditions d'erreur que eng_atmosphere_sky. */
bool eng_atmosphere_horizon(const EngAtmosphere *atmosphere, const float sun[3],
                            const float direction[3], float rgb[3]);

/* Le ciel plus le disque du soleil (au-dessus de l'horizon seulement), atténué comme la lumière
 * directe. Mêmes conditions que eng_atmosphere_sky. */
bool eng_atmosphere_radiance(const EngAtmosphere *atmosphere, const float sun[3],
                             const float view[3], float rgb[3]);

/* Lumière directe du soleil au sol : `color` = transmittance par canal RELATIVE à celle d'un
 * soleil au zénith (donc (1, 1, 1) à midi, de plus en plus rouge ensuite, chaque composante <= 1),
 * `visibility` dans [0, 1] : 1 au-dessus de 1,2°, 0 sous -1,2° (le soleil est derrière
 * l'horizon). À utiliser comme sun_color et facteur de sun_intensity. */
bool eng_atmosphere_sun_light(const EngAtmosphere *atmosphere, const float sun[3], float color[3],
                              float *visibility);

/* Lumière ambiante RELATIVE à midi : la radiance moyenne du ciel (9 directions de la demi-sphère)
 * pour ce soleil, divisée par canal par celle d'un soleil au zénith. 1 à midi, plus petit ensuite
 * (environ 0,9 à 45°, 0,15 à l'horizon, 0,003 la nuit ; peut dépasser 1 de 3 % quand l'une des
 * 9 directions échantillonnées tombe dans le halo du soleil), de plus en plus rouge au coucher. À
 * multiplier avec la couleur ambiante de la scène. */
bool eng_atmosphere_ambient(const EngAtmosphere *atmosphere, const float sun[3], float rgb[3]);

/* Dégradé équivalent pour eng_ibl_make_sky (éclairage par image de l'environnement, cuit une
 * fois) : zénith, horizon (perpendiculaire au soleil), sol et disque du soleil de CE ciel. */
bool eng_atmosphere_to_sky_desc(const EngAtmosphere *atmosphere, const float sun[3],
                                EngSkyDesc *out);

/* Dessin du ciel par le shader atmosphere.frag : triangle plein écran, sans profondeur, à mettre
 * en file AVANT la scène. */
void eng_atmosphere_pipeline_desc(EngPipelineDesc *desc);

/* Met le ciel en file. `dummy_vertex` : n'importe quel buffer de sommets valide (le renderer en
 * exige un, la skybox n'en lit pas : EngEnvironment.dummy_vertex convient). `view_proj` : monde ->
 * découpage ; `camera` : position monde ; `sun` : direction vers le soleil (normalisée ici ;
 * nulle ou non finie : +Y). `exposure_scale` >= 0 multiplie le ciel et le disque. false si
 * l'atmosphère est invalide ou si la file refuse le dessin. */
bool eng_atmosphere_draw(const EngAtmosphere *atmosphere, EngRenderQueue *queue,
                         EngPipelineHandle pipeline, EngBufferHandle dummy_vertex, mat4 view_proj,
                         const float camera[3], const float sun[3], float exposure_scale);
