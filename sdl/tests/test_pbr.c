#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "engine/lighting.h"
#include "engine/pbr.h"
#include "utest.h"

/* Matériau PBR (M1-15), côté CPU. Les valeurs attendues viennent de trois sources, jamais du code
 * testé : (1) calcul À LA MAIN dans les cas où la formule se simplifie (incidence normale : D, V et
 * F ont une forme fermée) ; (2) une implémentation Python INDÉPENDANTE écrite d'après la
 * spécification glTF 2.0 (annexe B), en double précision, pour les cas obliques ; (3) des
 * propriétés (symétries, bornes, monotonie). Le shader model.frag est comparé à cette référence
 * dans test_model_gpu.c. */

#define EPS 1e-4F
#define PI_F 3.14159265358979323846F

static float length3(const float v[3])
{
    return sqrtf((v[0] * v[0]) + (v[1] * v[1]) + (v[2] * v[2]));
}

/* Soleil dans la direction `dir` (unitaire), camera en (0, 0, 5), ambiante nulle. */
static EngLighting sun_from(float x, float y, float z)
{
    EngLighting l = {0};
    l.sun_direction[0] = x;
    l.sun_direction[1] = y;
    l.sun_direction[2] = z;
    for (int c = 0; c < 3; c++) {
        l.sun_color[c] = 1.0F;
        l.ambient_color[c] = 1.0F;
    }
    l.sun_intensity = 1.0F;
    l.camera_position[2] = 5.0F;
    return l;
}

static EngPbrSurface surface(float r, float g, float b, float metallic, float roughness)
{
    EngPbrSurface s;
    eng_pbr_surface_default(&s);
    s.base_color[0] = r;
    s.base_color[1] = g;
    s.base_color[2] = b;
    s.metallic = metallic;
    s.roughness = roughness;
    return s;
}

static const float UP_Z[3] = {0.0F, 0.0F, 1.0F};
static const float ORIGIN[3] = {0.0F, 0.0F, 0.0F};

/* ── Facteurs du modèle ── */

UTEST(eng_pbr, the_ggx_distribution_has_its_closed_form_at_the_peak_and_for_alpha_one)
{
    /* n.h = 1 : D = 1 / (pi alpha^2) ; alpha = 0,5 : 1 / (0,25 pi) = 1,2732395 */
    EXPECT_NEAR(eng_pbr_ndf_ggx(1.0F, 0.5F), 1.2732395F, EPS);
    /* alpha = 1 : (n.h)^2 (alpha^2 - 1) = 0, donc D = 1 / pi quel que soit n.h (distribution
     * uniforme sur l'hémisphère, à la mesure cos près) */
    EXPECT_NEAR(eng_pbr_ndf_ggx(0.3F, 1.0F), 0.31830989F, EPS);
    EXPECT_NEAR(eng_pbr_ndf_ggx(1.0F, 1.0F), 0.31830989F, EPS);
    /* alpha = 0,5, n.h = 0,5 : d = 0,25 (0,25 - 1) + 1 = 0,8125 ; D = 0,25 / (pi 0,8125^2) */
    EXPECT_NEAR(eng_pbr_ndf_ggx(0.5F, 0.5F), 0.12054339F, EPS);
}

UTEST(eng_pbr, a_smaller_alpha_gives_a_taller_and_narrower_peak)
{
    const float broad = eng_pbr_ndf_ggx(1.0F, 0.5F);
    const float sharp = eng_pbr_ndf_ggx(1.0F, 0.1F);
    EXPECT_NEAR(sharp / broad, 25.0F, 1e-3F); /* D(1, alpha) = 1 / (pi alpha^2) : (0,5 / 0,1)^2 */
    EXPECT_TRUE(eng_pbr_ndf_ggx(0.9F, 0.1F) < 0.05F * sharp); /* et il décroît vite */
}

UTEST(eng_pbr, the_smith_visibility_is_a_quarter_at_normal_incidence_and_closed_form_for_alpha_one)
{
    /* n.l = n.v = 1 : Vis = 0,5 / (1 + 1) = 0,25 pour tout alpha */
    EXPECT_NEAR(eng_pbr_visibility_smith_ggx(1.0F, 1.0F, 0.5F), 0.25F, EPS);
    EXPECT_NEAR(eng_pbr_visibility_smith_ggx(1.0F, 1.0F, 0.05F), 0.25F, EPS);
    /* alpha = 1 : les racines valent 1, Vis = 0,5 / (n.l + n.v) : 0,5 / 1,5 = 1/3 */
    EXPECT_NEAR(eng_pbr_visibility_smith_ggx(0.5F, 1.0F, 1.0F), 0.33333333F, EPS);
    EXPECT_NEAR(eng_pbr_visibility_smith_ggx(1.0F, 0.5F, 1.0F), 0.33333333F, EPS); /* symétrique */
    /* valeur oblique, Python : Vis(n.l = 0,5 ; n.v = 0,25 ; alpha = 0,5) = 1,1420984 */
    EXPECT_NEAR(eng_pbr_visibility_smith_ggx(0.5F, 0.25F, 0.5F), 1.1420984F, EPS);
    EXPECT_NEAR(eng_pbr_visibility_smith_ggx(0.25F, 0.5F, 0.5F), 1.1420984F, EPS);
    /* rien à éclairer : 0 et non une division par zéro */
    EXPECT_EQ(eng_pbr_visibility_smith_ggx(0.0F, 0.0F, 0.5F), 0.0F);
}

UTEST(eng_pbr, the_schlick_fresnel_goes_from_f0_to_one)
{
    EXPECT_NEAR(eng_pbr_fresnel_schlick(0.04F, 1.0F), 0.04F, EPS);
    EXPECT_NEAR(eng_pbr_fresnel_schlick(0.04F, 0.0F), 1.0F, EPS);
    /* v.h = 0,5 : 0,04 + 0,96 x 0,5^5 = 0,04 + 0,03 = 0,07 */
    EXPECT_NEAR(eng_pbr_fresnel_schlick(0.04F, 0.5F), 0.07F, EPS);
    /* f0 = 0,8, v.h = 0,25 : 0,8 + 0,2 x 0,75^5 = 0,8 + 0,2 x 0,2373047 = 0,8474609 */
    EXPECT_NEAR(eng_pbr_fresnel_schlick(0.8F, 0.25F), 0.84746094F, EPS);
    /* v.h hors de [0, 1] : borné, pas d'extrapolation */
    EXPECT_NEAR(eng_pbr_fresnel_schlick(0.04F, 1.5F), 0.04F, EPS);
    EXPECT_NEAR(eng_pbr_fresnel_schlick(0.04F, -0.5F), 1.0F, EPS);
}

UTEST(eng_pbr, the_environment_brdf_matches_the_karis_fit)
{
    float a;
    float b;
    /* valeurs de la formule de Karis évaluée en Python (double précision) */
    eng_pbr_env_brdf(1.0F, 0.0F, &a, &b);
    EXPECT_NEAR(a, 0.99412708F, EPS);
    EXPECT_NEAR(b, 0.00587292F, EPS);
    eng_pbr_env_brdf(1.0F, 1.0F, &a, &b);
    EXPECT_NEAR(a, 0.4524F, EPS);
    EXPECT_NEAR(b, -0.0024F, EPS);
    eng_pbr_env_brdf(0.5F, 0.5F, &a, &b);
    EXPECT_NEAR(a, 0.70324433F, EPS);
    EXPECT_NEAR(b, 0.02175567F, EPS);
    eng_pbr_env_brdf(0.1F, 0.3F, &a, &b);
    EXPECT_NEAR(a, 0.47606F, EPS);
    EXPECT_NEAR(b, 0.35894F, EPS);
}

/* ── Normal map ── */

UTEST(eng_pbr_perturb_normal, a_flat_sample_leaves_the_normal_alone)
{
    const float tangent[4] = {1.0F, 0.0F, 0.0F, 1.0F};
    const float flat[3] = {0.5F, 0.5F, 1.0F}; /* (0, 0, 1) en espace tangent */
    float out[3];
    eng_pbr_perturb_normal(UP_Z, tangent, flat, 1.0F, out);
    EXPECT_NEAR(out[0], 0.0F, EPS);
    EXPECT_NEAR(out[1], 0.0F, EPS);
    EXPECT_NEAR(out[2], 1.0F, EPS);
}

UTEST(eng_pbr_perturb_normal, red_tilts_along_the_tangent_and_green_along_the_bitangent)
{
    /* N = +Z, T = +X : B = cross(N, T) = Z x X = +Y, vers le HAUT de l'image (convention glTF) */
    const float tangent[4] = {1.0F, 0.0F, 0.0F, 1.0F};
    float out[3];
    const float right[3] = {1.0F, 0.5F, 0.5F}; /* (1, 0, 0) */
    eng_pbr_perturb_normal(UP_Z, tangent, right, 1.0F, out);
    EXPECT_NEAR(out[0], 1.0F, EPS);
    EXPECT_NEAR(out[1], 0.0F, EPS);
    EXPECT_NEAR(out[2], 0.0F, EPS);
    const float up[3] = {0.5F, 1.0F, 0.5F}; /* (0, 1, 0) */
    eng_pbr_perturb_normal(UP_Z, tangent, up, 1.0F, out);
    EXPECT_NEAR(out[1], 1.0F, EPS);
    /* w = -1 : la bitangente est inversée, pas la tangente */
    const float mirrored[4] = {1.0F, 0.0F, 0.0F, -1.0F};
    eng_pbr_perturb_normal(UP_Z, mirrored, up, 1.0F, out);
    EXPECT_NEAR(out[1], -1.0F, EPS);
    eng_pbr_perturb_normal(UP_Z, mirrored, right, 1.0F, out);
    EXPECT_NEAR(out[0], 1.0F, EPS);
}

UTEST(eng_pbr_perturb_normal, the_scale_weakens_x_and_y_and_the_result_is_unit_length)
{
    const float tangent[4] = {1.0F, 0.0F, 0.0F, 1.0F};
    float out[3];
    /* (1, 0, 1) x échelle 0,5 -> (0,5 ; 0 ; 1), norme sqrt(1,25) : (0,4472136 ; 0 ; 0,8944272) */
    const float tilted[3] = {1.0F, 0.5F, 1.0F};
    eng_pbr_perturb_normal(UP_Z, tangent, tilted, 0.5F, out);
    EXPECT_NEAR(out[0], 0.4472136F, EPS);
    EXPECT_NEAR(out[2], 0.8944272F, EPS);
    EXPECT_NEAR(length3(out), 1.0F, EPS);
    /* (1, 1, 1) x échelle 0,5 -> (0,5 ; 0,5 ; 1), norme sqrt(1,5) : les DEUX composantes du plan
     * tangent sont réduites, pas seulement x */
    const float corner[3] = {1.0F, 1.0F, 1.0F};
    eng_pbr_perturb_normal(UP_Z, tangent, corner, 0.5F, out);
    EXPECT_NEAR(out[0], 0.4082483F, EPS);
    EXPECT_NEAR(out[1], 0.4082483F, EPS);
    EXPECT_NEAR(out[2], 0.8164966F, EPS);
    /* échelle 0 : plus de relief, la normale d'origine */
    eng_pbr_perturb_normal(UP_Z, tangent, tilted, 0.0F, out);
    EXPECT_NEAR(out[0], 0.0F, EPS);
    EXPECT_NEAR(out[2], 1.0F, EPS);
}

UTEST(eng_pbr_perturb_normal, a_tangent_that_is_not_orthogonal_is_projected_first)
{
    /* tangente (1, 0, 1), normale +Z : Gram-Schmidt donne (1, 0, 0) */
    const float tangent[4] = {1.0F, 0.0F, 1.0F, 1.0F};
    const float right[3] = {1.0F, 0.5F, 0.5F};
    float out[3];
    eng_pbr_perturb_normal(UP_Z, tangent, right, 1.0F, out);
    EXPECT_NEAR(out[0], 1.0F, EPS);
    EXPECT_NEAR(out[2], 0.0F, EPS);
}

/* ── Couleur d'une surface ── */

UTEST(eng_pbr_shade, a_white_dielectric_lit_head_on_is_097)
{
    /* Soleil, normale et caméra alignés : H = N, v.h = 1, F = F0 = 0,04 ; alpha = 1 (rugosité 1) :
     * D = 1/pi, Vis = 0,25. direct = (1 - 0,04) x 1 + pi x 0,04 x (1/pi) x 0,25 = 0,96 + 0,01 */
    const EngLighting l = sun_from(0, 0, 1);
    const EngPbrSurface s = surface(1, 1, 1, 0.0F, 1.0F);
    float out[3];
    eng_pbr_shade(&l, &s, UP_Z, ORIGIN, out);
    for (int c = 0; c < 3; c++) {
        EXPECT_NEAR(out[c], 0.97F, EPS);
    }
}

UTEST(eng_pbr_shade, a_white_metal_lit_head_on_keeps_only_the_specular_term)
{
    /* métal 1 : F0 = base = 1, c_diff = 0. alpha = 1 : pi x 1 x (1/pi) x 0,25 = 0,25 */
    const EngLighting l = sun_from(0, 0, 1);
    const EngPbrSurface s = surface(1, 1, 1, 1.0F, 1.0F);
    float out[3];
    eng_pbr_shade(&l, &s, UP_Z, ORIGIN, out);
    for (int c = 0; c < 3; c++) {
        EXPECT_NEAR(out[c], 0.25F, EPS);
    }
}

UTEST(eng_pbr_shade, the_specular_peak_grows_as_one_over_alpha_squared)
{
    /* métal de base 0,8, rugosité 0,5 : alpha = 0,25, alpha^2 = 0,0625. Face au soleil et à la
     * caméra : D = 1 / (pi alpha^2), Vis = 0,25, F = F0 = 0,8, donc pi x 0,8 x D x 0,25 =
     * 0,8 x 0,25 / 0,0625 = 3,2. La valeur dépasse 1 : c'est du HDR, pas une erreur. */
    const EngLighting l = sun_from(0, 0, 1);
    const EngPbrSurface s = surface(0.8F, 0.8F, 0.8F, 1.0F, 0.5F);
    float out[3];
    eng_pbr_shade(&l, &s, UP_Z, ORIGIN, out);
    EXPECT_NEAR(out[0], 3.2F, 1e-3F);
    /* même valeur quand c'est la normale qui est la bissectrice de (L, V), loin de l'axe :
     * L = V = (0,6 ; 0 ; 0,8), normale (0,6 ; 0 ; 0,8) (caméra à (6, 0, 8), 10 m) */
    EngLighting oblique = sun_from(0.6F, 0.0F, 0.8F);
    oblique.camera_position[0] = 6.0F;
    oblique.camera_position[2] = 8.0F;
    const float n[3] = {0.6F, 0.0F, 0.8F};
    eng_pbr_shade(&oblique, &s, n, ORIGIN, out);
    EXPECT_NEAR(out[0], 3.2F, 1e-3F);
}

UTEST(eng_pbr_shade, matches_the_independent_reference_on_oblique_cases)
{
    float out[3];
    /* soleil (0,6 ; 0 ; 0,8) de couleur (1 ; 0,9 ; 0,8), ambiante (0,2 ; 0,2 ; 0,3), caméra en
     * (3, 0, 4) ; valeurs de pbr_ref.py (double précision) */
    EngLighting l = sun_from(0.6F, 0.0F, 0.8F);
    l.sun_color[1] = 0.9F;
    l.sun_color[2] = 0.8F;
    l.ambient_color[0] = 0.2F;
    l.ambient_color[1] = 0.2F;
    l.ambient_color[2] = 0.3F;
    l.camera_position[0] = 3.0F;
    l.camera_position[1] = 0.0F;
    l.camera_position[2] = 4.0F;
    l.ambient_intensity = 1.0F;

    /* aucune ambiante : on isole le direct, puis on réactive */
    EngLighting direct_only = l;
    direct_only.ambient_intensity = 0.0F;
    EngPbrSurface s = surface(0.5F, 0.25F, 1.0F, 0.0F, 0.4F);
    eng_pbr_shade(&direct_only, &s, UP_Z, ORIGIN, out);
    /* (ambiante nulle : la référence Python, avec amb = 0, donne ces trois valeurs) */
    EXPECT_NEAR(out[0], 0.3862428F, 2e-4F);
    EXPECT_NEAR(out[1], 0.1748185F, 2e-4F);
    EXPECT_NEAR(out[2], 0.6161942F, 2e-4F);
    eng_pbr_shade(&l, &s, UP_Z, ORIGIN, out);
    EXPECT_NEAR(out[0], 0.4934799F, 2e-4F);
    EXPECT_NEAR(out[1], 0.2320557F, 2e-4F);
    EXPECT_NEAR(out[2], 0.9270500F, 2e-4F);

    /* métal à moitié : base (0,9 ; 0,5 ; 0,1), rugosité 0,3 */
    s = surface(0.9F, 0.5F, 0.1F, 0.5F, 0.3F);
    eng_pbr_shade(&l, &s, UP_Z, ORIGIN, out);
    EXPECT_NEAR(out[0], 0.3688753F, 2e-4F);
    EXPECT_NEAR(out[1], 0.2320349F, 2e-4F);
    EXPECT_NEAR(out[2], 0.0651576F, 2e-4F);

    /* même matériau, normale inclinée (0,3 ; 0,1 ; 0,95), non normalisée : elle l'est ici */
    const float tilted[3] = {0.3F, 0.1F, 0.95F};
    eng_pbr_shade(&l, &s, tilted, ORIGIN, out);
    EXPECT_NEAR(out[0], 0.4569013F, 2e-4F);
    EXPECT_NEAR(out[1], 0.2830015F, 2e-4F);
    EXPECT_NEAR(out[2], 0.0761983F, 2e-4F);
}

UTEST(eng_pbr_shade, the_ambient_term_is_the_diffuse_color_plus_the_environment_specular)
{
    /* pas de soleil ; ambiante 0,5 ; caméra de face (n.v = 1) ; diélectrique de rugosité 1 :
     * (A, B) = (0,4524 ; -0,0024), F0 = 0,04 -> 0,5 x (base + 0,04 x 0,4524 - 0,0024)
     *                                         = 0,5 x (base + 0,015696) */
    EngLighting l = sun_from(0, 0, 1);
    l.sun_intensity = 0.0F;
    l.ambient_intensity = 0.5F;
    const EngPbrSurface s = surface(0.5F, 0.25F, 1.0F, 0.0F, 1.0F);
    float out[3];
    eng_pbr_shade(&l, &s, UP_Z, ORIGIN, out);
    EXPECT_NEAR(out[0], 0.257848F, EPS);
    EXPECT_NEAR(out[1], 0.132848F, EPS);
    EXPECT_NEAR(out[2], 0.507848F, EPS);
}

UTEST(eng_pbr_shade, occlusion_dims_the_ambient_only_and_the_emissive_is_added_at_the_end)
{
    EngLighting l = sun_from(0, 0, 1);
    l.sun_color[0] = 2.0F;
    l.sun_color[1] = 1.0F;
    l.sun_color[2] = 0.5F;
    l.ambient_intensity = 0.5F;
    EngPbrSurface s = surface(0.5F, 0.25F, 1.0F, 0.0F, 1.0F);
    s.emissive[0] = 0.1F;
    s.emissive[1] = 0.2F;
    s.emissive[2] = 0.3F;
    float out[3];
    eng_pbr_shade(&l, &s, UP_Z, ORIGIN, out);
    /* valeurs de pbr_ref.py : occlusion 1 */
    EXPECT_NEAR(out[0], 1.337848F, 2e-4F);
    EXPECT_NEAR(out[1], 0.582848F, 2e-4F);
    EXPECT_NEAR(out[2], 1.292848F, 2e-4F);
    s.occlusion = 0.5F;
    eng_pbr_shade(&l, &s, UP_Z, ORIGIN, out);
    /* l'écart avec le cas précédent est EXACTEMENT la moitié de l'ambiante : 0,128924 en rouge
     * (0,257848 / 2) ; ni le direct ni l'émissif ne bougent */
    EXPECT_NEAR(out[0], 1.208924F, 2e-4F);
    EXPECT_NEAR(out[1], 0.516424F, 2e-4F);
    EXPECT_NEAR(out[2], 1.038924F, 2e-4F);
    /* occlusion 0 : plus d'ambiante du tout */
    s.occlusion = 0.0F;
    eng_pbr_shade(&l, &s, UP_Z, ORIGIN, out);
    EXPECT_NEAR(out[0], 1.337848F - 0.257848F, 2e-4F);
}

UTEST(eng_pbr_shade, a_surface_facing_away_from_the_sun_gets_no_direct_light)
{
    /* soleil (0,6 ; 0 ; -0,8), caméra en (0, 0, 5) : H n'est pas nul (avec un soleil exactement
     * opposé à la vue, H = 0 donne F = 1 et le diffus s'annule de lui-même : le test ne prouverait
     * rien) */
    EngLighting l = sun_from(0.6F, 0, -0.8F);
    const EngPbrSurface s = surface(0.9F, 0.5F, 0.1F, 0.5F, 0.3F);
    float out[3];
    eng_pbr_shade(&l, &s, UP_Z, ORIGIN, out);
    for (int c = 0; c < 3; c++) {
        EXPECT_EQ(out[c], 0.0F); /* ni direct, ni ambiante (nulle ici) */
    }
    l.ambient_intensity = 1.0F;
    eng_pbr_shade(&l, &s, UP_Z, ORIGIN, out);
    EXPECT_TRUE(out[0] > 0.0F); /* l'ambiante, elle, éclaire de tous côtés */
}

UTEST(eng_pbr_shade, direct_light_scales_with_the_sun_color_and_intensity_per_channel)
{
    EngLighting l = sun_from(0, 0, 1);
    const EngPbrSurface s = surface(1, 1, 1, 0.0F, 1.0F);
    float one[3];
    eng_pbr_shade(&l, &s, UP_Z, ORIGIN, one);
    l.sun_intensity = 3.0F;
    l.sun_color[1] = 0.5F;
    l.sun_color[2] = 0.0F;
    float out[3];
    eng_pbr_shade(&l, &s, UP_Z, ORIGIN, out);
    EXPECT_NEAR(out[0], 3.0F * one[0], EPS);
    EXPECT_NEAR(out[1], 1.5F * one[1], EPS);
    EXPECT_NEAR(out[2], 0.0F, EPS);
}

UTEST(eng_pbr_shade, a_roughness_below_the_minimum_behaves_like_the_minimum)
{
    EngLighting l = sun_from(0.6F, 0.0F, 0.8F);
    l.camera_position[0] = 3.0F;
    l.camera_position[2] = 4.0F;
    const EngPbrSurface smooth = surface(0.9F, 0.5F, 0.1F, 0.5F, 0.0F);
    const EngPbrSurface minimum = surface(0.9F, 0.5F, 0.1F, 0.5F, ENG_ROUGHNESS_MIN);
    float a[3];
    float b[3];
    eng_pbr_shade(&l, &smooth, UP_Z, ORIGIN, a);
    eng_pbr_shade(&l, &minimum, UP_Z, ORIGIN, b);
    for (int c = 0; c < 3; c++) {
        EXPECT_TRUE(isfinite(a[c]));
        EXPECT_NEAR(a[c], b[c], 1e-6F);
    }
    EXPECT_NEAR(a[0], 0.1908046F, 2e-4F); /* valeur de pbr_ref.py */
}

UTEST(eng_pbr_shade, grazing_and_back_facing_views_stay_finite_and_non_negative)
{
    EngLighting l = sun_from(0, 0, 1);
    const EngPbrSurface s = surface(0.9F, 0.5F, 0.1F, 1.0F, 0.05F);
    const float cameras[4][3] = {{5, 0, 0}, {5, 0, 1e-6F}, {0, 0, -5}, {0.001F, 0, 1e-5F}};
    for (int i = 0; i < 4; i++) {
        l.camera_position[0] = cameras[i][0];
        l.camera_position[1] = cameras[i][1];
        l.camera_position[2] = cameras[i][2];
        float out[3];
        eng_pbr_shade(&l, &s, UP_Z, ORIGIN, out);
        for (int c = 0; c < 3; c++) {
            EXPECT_TRUE(isfinite(out[c]));
            EXPECT_TRUE(out[c] >= 0.0F);
        }
    }
}

UTEST(eng_pbr_shade, a_null_normal_gives_a_finite_color)
{
    /* la normale nulle ne se normalise pas : elle reste nulle, aucun éclairage direct, pas de NaN
     */
    const EngLighting l = sun_from(0, 0, 1);
    const EngPbrSurface s = surface(1, 1, 1, 0.0F, 0.5F);
    const float zero[3] = {0, 0, 0};
    float out[3];
    eng_pbr_shade(&l, &s, zero, ORIGIN, out);
    for (int c = 0; c < 3; c++) {
        EXPECT_TRUE(isfinite(out[c]));
    }
}

/* ── Surface ── */

UTEST(eng_pbr_surface, the_default_is_a_matte_white_without_occlusion_or_emission)
{
    EngPbrSurface s;
    memset(&s, 0xAB, sizeof s);
    eng_pbr_surface_default(&s);
    for (int c = 0; c < 3; c++) {
        EXPECT_EQ(s.base_color[c], 1.0F);
        EXPECT_EQ(s.emissive[c], 0.0F);
    }
    EXPECT_EQ(s.metallic, 0.0F);
    EXPECT_EQ(s.roughness, 1.0F);
    EXPECT_EQ(s.occlusion, 1.0F);
    EXPECT_TRUE(eng_pbr_sanitize(&s));
}

UTEST(eng_pbr_surface, sanitize_brings_every_field_back_into_its_domain)
{
    EngPbrSurface s = surface(1, 1, 1, 1.5F, 1.0F);
    EXPECT_FALSE(eng_pbr_sanitize(&s));
    EXPECT_EQ(s.metallic, 1.0F);
    s = surface(1, 1, 1, -0.5F, 1.0F);
    EXPECT_FALSE(eng_pbr_sanitize(&s));
    EXPECT_EQ(s.metallic, 0.0F);
    s = surface(1, 1, 1, NAN, 1.0F);
    EXPECT_FALSE(eng_pbr_sanitize(&s));
    EXPECT_EQ(s.metallic, 0.0F);
    s = surface(1, 1, 1, 0.0F, NAN);
    EXPECT_FALSE(eng_pbr_sanitize(&s));
    EXPECT_EQ(s.roughness, 1.0F); /* une rugosité inconnue est la plus rugueuse */
    s = surface(1, 1, 1, 0.0F, 2.0F);
    EXPECT_FALSE(eng_pbr_sanitize(&s));
    EXPECT_EQ(s.roughness, 1.0F);
    s = surface(1, 1, 1, 0.0F, 1.0F);
    s.occlusion = NAN;
    EXPECT_FALSE(eng_pbr_sanitize(&s));
    EXPECT_EQ(s.occlusion, 1.0F); /* une occlusion inconnue n'assombrit pas */
    s.occlusion = -1.0F;
    EXPECT_FALSE(eng_pbr_sanitize(&s));
    EXPECT_EQ(s.occlusion, 0.0F);
    s = surface(-1, NAN, INFINITY, 0.0F, 1.0F);
    s.emissive[1] = -2.0F;
    s.emissive[2] = INFINITY;
    EXPECT_FALSE(eng_pbr_sanitize(&s));
    for (int c = 0; c < 3; c++) {
        EXPECT_EQ(s.base_color[c], 0.0F);
        EXPECT_EQ(s.emissive[c], c == 0 ? s.emissive[0] : 0.0F);
    }
    /* un champ valide à la limite n'est pas touché */
    s = surface(0.0F, 100.0F, 0.0F, 1.0F, 0.0F);
    s.occlusion = 0.0F;
    EXPECT_TRUE(eng_pbr_sanitize(&s));
    EXPECT_EQ(s.base_color[1], 100.0F);
}

UTEST(eng_pbr_shade, pi_appears_exactly_once_between_the_sun_and_a_white_wall)
{
    /* garde-fou de la convention d'unités : un mur blanc mat, soleil de face d'intensité 1,
     * ambiante nulle, doit valoir (1 - F0) + le pic spéculaire, soit moins de 1 pour de la
     * rugosité 1 et jamais ~pi ou ~1/pi */
    const EngLighting l = sun_from(0, 0, 1);
    const EngPbrSurface s = surface(1, 1, 1, 0.0F, 1.0F);
    float out[3];
    eng_pbr_shade(&l, &s, UP_Z, ORIGIN, out);
    EXPECT_TRUE(out[0] > 0.9F && out[0] < 1.0F);
    EXPECT_NEAR(out[0] * PI_F, 0.97F * PI_F, 1e-3F);
}
