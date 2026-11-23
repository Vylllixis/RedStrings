// Fragment des ARBRES RÉALISTES (2026-11-22, engine/tree_render.h, docs/arbres-realistes.md) : le
// matériau PBR de model.frag (mêmes formules, mêmes ombres, même éclairage par image : HLSL n'a pas
// d'#include dans cette chaîne d'outils, le texte est recopié), plus ce qui fait un feuillage :
//
//   - DÉCOUPE : une partie dont le matériau a un seuil d'alpha (sun_direction.w > 0) ne dessine pas
//     ses pixels transparents. Le seuil est TRAMÉ : seuil + (bruit - 0,5) x largeur, le bruit étant
//     fixe par pixel d'écran (bruit à gradient entrelacé) et la largeur croissant avec la distance.
//     De près la découpe est nette ; de loin, un pixel couvert à 30 % est dessiné 3 fois sur 10 :
//     le feuillage garde sa densité au lieu de s'évanouir. Une carte vue par la tranche s'efface.
//   - LUMIÈRE : la normale du relief est mêlée à la forme d'ensemble du houppier (un volume, pas un
//     tas de cartes) ; le soleil « enveloppe » les feuilles et les TRAVERSE (contre-jour) ; les
//     cartes ont deux faces et la même normale des deux côtés.
//   - OMBRE DU HOUPPIER : |tangente.w| est la part du ciel vue par le sommet (calculée par
//     treebake) ; elle assombrit l'ambiante et le soleil au cœur de l'arbre. Le signe de
//     tangente.w reste celui de la bitangente.
//   - OMBRE PORTÉE : lue un peu plus près du soleil pour le feuillage (une feuille ne se fait pas
//     d'ombre à elle-même), sans biais de pente (une carte n'a pas d'orientation propre).
//   - BROUILLARD : celui du terrain (engine/fog.h), pour que les arbres lointains se fondent dans
//     l'air comme le sol qui les porte.
//
// Une partie opaque dont |tangente.w| vaut 1 donne exactement le résultat de model.frag.
Texture2D<float4> base_color_map : register(t0, space2);
Texture2D<float4> metal_rough_map : register(t1, space2);
Texture2D<float4> normal_map : register(t2, space2);
Texture2D<float4> occlusion_map : register(t3, space2);
Texture2D<float4> emissive_map : register(t4, space2);
TextureCube<float4> irradiance_map : register(t5, space2);
TextureCube<float4> prefiltered_map : register(t6, space2);
Texture2D<float4> brdf_lut : register(t7, space2);
SamplerState base_color_sampler : register(s0, space2);
SamplerState metal_rough_sampler : register(s1, space2);
SamplerState normal_sampler : register(s2, space2);
SamplerState occlusion_sampler : register(s3, space2);
SamplerState emissive_sampler : register(s4, space2);
SamplerState irradiance_sampler : register(s5, space2);
SamplerState prefiltered_sampler : register(s6, space2);
SamplerState brdf_sampler : register(s7, space2);
// Carte d'ombre (M1-17) : profondeur du soleil, lue au plus proche voisin (SampleLevel) ; les
// comparaisons, l'interpolation bilinéaire et le PCF sont refaits à la main, comme
// eng_shadow_visibility (engine/shadow.c).
Texture2D<float4> shadow_map : register(t8, space2);
SamplerState shadow_sampler : register(s8, space2);

cbuffer Lighting : register(b0, space3)
{
    float4 sun_direction;   // xyz : vers le soleil (unitaire) ; w : seuil de découpe du matériau (0 : opaque)
    float4 sun_radiance;    // rgb : couleur * intensité du soleil ; w : 1 si le matériau a deux faces
    float4 ambient;         // rgb : couleur * intensité ambiante
    float4 camera_position; // xyz : position de la caméra (monde)
    float4 material;        // x : métal, y : rugosité, z : échelle de la normale, w : force de l'occlusion
    float4 emissive;        // rgb : facteur émissif ; w : 1 si le matériau a une normal map
    float4 environment;     // x : dernier niveau de mip du cube préfiltré (nombre de niveaux - 1)
    float4x4 shadow_matrix; // monde -> (u, v, profondeur) de la carte d'ombre
    float4 shadow_a;        // xyz : vers le soleil (unitaire), w : côté d'un texel en mètres
    float4 shadow_b;        // x : taille de la carte (texels), y : biais constant, z : biais de pente, w : biais normal
    float4 shadow_c;        // x : rayon du PCF, y : force de l'ombre, z : 1 si une ombre est active
    float4 cascade_info;    // x : nombre de cascades (0 ou 1 : carte unique ; 2 à 4 : atlas 2 x 2)
    float4 cascade_texel;   // côté d'un texel (m) de chaque cascade
    float4x4 cascade_matrix[4]; // monde -> (u, v, profondeur) de chaque cascade
    // Réglages du feuillage (EngTreeLook, engine/tree_render.h), après les uniforms de model.frag :
    float4 foliage_a; // x : translucidité, y : part de la normale de houppier, z : occlusion du ciel sur l'ambiante, w : sur le soleil
    float4 foliage_b; // x : largeur du tramage de près, y : de loin, z : distance (m), w : décalage de l'ombre (m)
    float4 foliage_c; // x : fondu des cartes vues par la tranche (|cos|), y : lumière enveloppante, z : éclat des feuilles, w : reflet du ciel sur les feuilles
    // Brouillard (engine/fog.h : eng_fog_pack), comme terrain.frag ; fog_a.w = 0 : pas de brouillard
    float4 fog_a;           // x : densité, y : décroissance en hauteur, z : altitude de base, w : opacité maximale
    float4 fog_rayleigh;    // xyz : épaisseurs optiques de Rayleigh ; w : Mie
    float4 fog_night;       // xyz : plancher nocturne ; w : irradiance E
    float4 fog_sky;         // x : g de Mie, y : path_limit, z : sun_path_limit, w : échelle d'exposition du ciel
    float4 fog_sun;         // xyz : vers le vrai soleil ; w : 1 si donné, sinon sun_direction
};

// Brouillard : recopie de terrain.frag (les arbres lointains se fondent dans l'air comme le sol).
float air_mass(float mu)
{
    mu = clamp(mu, 0.0f, 1.0f);
    return 1.0f / (mu + 0.025f * exp(-11.0f * mu));
}

// Recopie de sky_above d'atmosphere.frag / atmosphere.c, à l'horizon : la couleur du brouillard est
// celle du ciel là où le terrain le rejoint (azimut `flat`, y = 0).
float3 fog_horizon(float3 s, float3 flat)
{
    float m_view = min(air_mass(0.0f), fog_sky.y);
    float m_sun = min(air_mass(s.y), fog_sky.z);
    float c = dot(s, flat);
    float phase_r = (3.0f / (16.0f * 3.14159265358979323846f)) * (1.0f + c * c);
    float g = fog_sky.x;
    float phase_m = (1.0f - g * g) / (4.0f * 3.14159265358979323846f * pow(1.0f + g * g - 2.0f * g * c, 1.5f));
    float twilight = saturate((s.y + 0.21f) / 0.21f);
    float lit = twilight * twilight;
    float3 ext = fog_rayleigh.xyz + fog_rayleigh.w;
    float3 scatter = fog_rayleigh.xyz * phase_r + fog_rayleigh.w * phase_m;
    float3 transmit_sun = exp(-ext * m_sun);
    float3 inscatter = (1.0f - exp(-ext * m_view)) / ext;
    return fog_night.w * scatter * inscatter * transmit_sun * lit + fog_night.xyz;
}

// Brouillard de distance et de hauteur : voir engine/fog.h (mêmes formules que eng_fog_apply).
float3 apply_fog(float3 lit_color, float3 world_position)
{
    if (fog_a.w <= 0.0f || fog_a.x <= 0.0f) {
        return lit_color;
    }
    float3 delta = world_position - camera_position.xyz;
    float dist = length(delta);
    float x = clamp(fog_a.y * delta.y, -80.0f, 80.0f);
    float shape = abs(x) < 1e-4f ? 1.0f - 0.5f * x : (1.0f - exp(-x)) / x;
    float at_camera = exp(clamp(-fog_a.y * (camera_position.y - fog_a.z), -80.0f, 80.0f));
    float tau = fog_a.x * at_camera * dist * shape;
    float f = fog_a.w * (1.0f - exp(-tau));
    float3 flat = float3(delta.x, 0.0f, delta.z);
    float flat_length = length(flat);
    flat = flat_length > 1e-12f ? flat / flat_length : float3(1.0f, 0.0f, 0.0f);
    float3 fog_sun_dir = fog_sun.w > 0.5f ? fog_sun.xyz : sun_direction.xyz;
    float3 fog_color = fog_horizon(fog_sun_dir, flat) * fog_sky.w;
    return lerp(lit_color, fog_color, f);
}


static const float PI = 3.14159265358979323846f;
static const float ROUGHNESS_MIN = 0.045f;
static const float MIN_DOT = 1e-4f;

static const int MAX_SHADOW_RADIUS = 3;
static const float MAX_SHADOW_SLOPE = 8.0f;

// Une comparaison « lumière » (1) ou « ombre » (0) de la profondeur de référence avec le texel (x, y) ;
// le sampler (plus proche voisin, bords prolongés) clampe les texels hors de la carte.
float shadow_lit(float2 texel, float reference, float size)
{
    return reference <= shadow_map.SampleLevel(shadow_sampler, (texel + 0.5f) / size, 0.0f).r ? 1.0f
                                                                                                 : 0.0f;
}

// Quatre comparaisons interpolées bilinéairement autour de (u, v) : bilinear_compare de shadow.c.
float shadow_bilinear(float2 uv, float reference, float size)
{
    float2 p = uv * size - 0.5f;
    float2 base = floor(p);
    float2 w = p - base;
    float a = shadow_lit(base, reference, size);
    float b = shadow_lit(base + float2(1.0f, 0.0f), reference, size);
    float c = shadow_lit(base + float2(0.0f, 1.0f), reference, size);
    float d = shadow_lit(base + float2(1.0f, 1.0f), reference, size);
    return lerp(lerp(a, b, w.x), lerp(c, d, w.x), w.y);
}

// M7-09 : ombre en cascade. La cascade la plus fine qui contient le point (marge du PCF comprise)
// est lue dans son quart de l'atlas 2 x 2 ; mêmes biais que la carte unique, à la taille de texel de
// la cascade. Hors de toutes les cascades : éclairé.
float shadow_factor_cascades(float3 position, float3 normal, float cos_theta, float sin_theta,
                             float tan_theta, float radius)
{
    float size = shadow_b.x;
    int count = min((int)cascade_info.x, 4);
    float margin = (radius + 2.0f) / size;
    for (int k = 0; k < count; k++) {
        float3 shifted = position + normal * (shadow_b.w * sin_theta * cascade_texel[k]);
        float3 uvd = mul(cascade_matrix[k], float4(shifted, 1.0f)).xyz;
        if (uvd.x < margin || uvd.x > 1.0f - margin || uvd.y < margin || uvd.y > 1.0f - margin ||
            uvd.z < 0.0f || uvd.z > 1.0f) {
            continue;
        }
        uvd.z -= (shadow_b.y + shadow_b.z * (radius + 1.0f) * tan_theta) / size;
        float2 corner = float2((float)(k % 2), (float)(k / 2)) * 0.5f;
        float2 uv = corner + uvd.xy * 0.5f;
        int r = (int)radius;
        float sum = 0.0f;
        for (int j = -r; j <= r; j++) {
            for (int i = -r; i <= r; i++) {
                sum += shadow_bilinear(uv + float2((float)i, (float)j) / (2.0f * size), uvd.z,
                                       2.0f * size);
            }
        }
        float taps = (float)(2 * r + 1);
        return 1.0f - shadow_c.y * (1.0f - sum / (taps * taps));
    }
    return 1.0f;
}

// Part de lumière du soleil reçue en `position` (monde) de normale géométrique `normal` : décalage le
// long de la normale, biais constant et de pente en texels de profondeur, puis PCF. Mêmes formules
// que eng_shadow_receiver, eng_shadow_visibility et eng_shadow_factor.
float shadow_factor(float3 position, float3 normal)
{
    if (shadow_c.z < 0.5f) {
        return 1.0f;
    }
    float size = shadow_b.x;
    float cos_theta = saturate(dot(normal, shadow_a.xyz));
    float sin_theta = sqrt(1.0f - cos_theta * cos_theta);
    float tan_theta = min(sin_theta / max(cos_theta, 1e-4f), MAX_SHADOW_SLOPE);
    float radius = min(shadow_c.x, (float)MAX_SHADOW_RADIUS);
    if (cascade_info.x > 1.5f) {
        return shadow_factor_cascades(position, normal, cos_theta, sin_theta, tan_theta, radius);
    }
    float3 shifted = position + normal * (shadow_b.w * sin_theta * shadow_a.w);
    float3 uvd = mul(shadow_matrix, float4(shifted, 1.0f)).xyz;
    uvd.z -= (shadow_b.y + shadow_b.z * (radius + 1.0f) * tan_theta) / size;
    if (uvd.x < 0.0f || uvd.x > 1.0f || uvd.y < 0.0f || uvd.y > 1.0f || uvd.z < 0.0f ||
        uvd.z > 1.0f) {
        return 1.0f;
    }
    int r = (int)radius;
    float sum = 0.0f;
    for (int j = -r; j <= r; j++) {
        for (int i = -r; i <= r; i++) {
            sum += shadow_bilinear(uvd.xy + float2((float)i, (float)j) / size, uvd.z, size);
        }
    }
    float taps = (float)(2 * r + 1);
    float visibility = sum / (taps * taps);
    return 1.0f - shadow_c.y * (1.0f - visibility);
}

float3 perturb_normal(float3 n, float4 tangent, float3 sample_rgb, float scale)
{
    float3 t = normalize(tangent.xyz - n * dot(n, tangent.xyz));
    float3 b = cross(n, t) * tangent.w;
    float3 tn = float3((2.0f * sample_rgb.xy - 1.0f) * scale, 2.0f * sample_rgb.z - 1.0f);
    return normalize(t * tn.x + b * tn.y + n * tn.z);
}

// Bruit à gradient entrelacé (Jimenez, 2014) du pixel d'écran, dans [0, 1[ : eng_tree_dither.
float dither(float2 pixel)
{
    return frac(52.9829189f * frac(dot(pixel, float2(0.06711056f, 0.00583715f))));
}

float4 main(float3 normal : TEXCOORD0, float2 uv : TEXCOORD1, float4 color : TEXCOORD2,
            float3 world_position : TEXCOORD3, float4 tangent : TEXCOORD4, float4 crown : TEXCOORD5,
            float4 pixel : SV_Position) : SV_Target0
{
    float cutoff = sun_direction.w;
    bool foliage = cutoff > 0.0f;
    float4 base = base_color_map.Sample(base_color_sampler, uv) * color;
    if (foliage) {
        base.rgb *= foliage_c.z; // éclat : les feuilles photographiées sont sombres
    }
    float3 to_camera = camera_position.xyz - world_position;
    float distance_to_camera = length(to_camera);
    float3 v = to_camera / max(distance_to_camera, 1e-6f);
    float3 n = normalize(normal);
    if (foliage) {
        float alpha = base.a;
        if (foliage_c.x > 0.0f) {
            alpha *= smoothstep(0.0f, foliage_c.x, abs(dot(n, v))); // carte vue par la tranche
        }
        float width = lerp(foliage_b.x, foliage_b.y, saturate(distance_to_camera / foliage_b.z));
        clip(alpha - (cutoff + (dither(pixel.xy) - 0.5f) * width));
    }
    float4 mr = metal_rough_map.Sample(metal_rough_sampler, uv);
    float metallic = saturate(material.x * mr.b);
    float roughness = max(saturate(material.y * mr.g), ROUGHNESS_MIN);
    float occlusion = 1.0f + material.w * (occlusion_map.Sample(occlusion_sampler, uv).r - 1.0f);
    float3 emission = emissive.rgb * emissive_map.Sample(emissive_sampler, uv).rgb;

    // part du ciel vue par le sommet ; la racine adoucit le cœur du houppier (la lumière y
    // arrive aussi par rebonds, que le calcul de treebake ne compte pas)
    float sky = sqrt(saturate(abs(tangent.w)));
    float3 geometric_normal = n;          // la normale du sommet : les ombres ignorent la normal map
    if (emissive.w > 0.5f) {
        float4 frame = float4(tangent.xyz, tangent.w < 0.0f ? -1.0f : 1.0f);
        n = perturb_normal(n, frame, normal_map.Sample(normal_sampler, uv).rgb, material.z);
    }
    float3 l = sun_direction.xyz;
    float translucency = 0.0f;
    float wrap = 0.0f;
    float3 shadow_position = world_position;
    float3 shadow_normal = geometric_normal;
    if (foliage) {
        n = normalize(lerp(n, normalize(crown.xyz), foliage_a.y));
        translucency = foliage_a.x;
        wrap = foliage_c.y;
        shadow_position = world_position + l * foliage_b.w;
        shadow_normal = l;
    }
    float3 h = normalize(l + v);

    float alpha = roughness * roughness;
    float a2 = alpha * alpha;
    float n_dot_l = dot(n, l);
    // une carte se voit des deux côtés avec la même normale : |N.V| pour le feuillage
    float n_dot_v = max(foliage ? abs(dot(n, v)) : dot(n, v), MIN_DOT);
    float n_dot_h = max(dot(n, h), 0.0f);
    float v_dot_h = max(dot(v, h), 0.0f);

    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), base.rgb, metallic);
    float3 c_diff = base.rgb * (1.0f - metallic);
    float m = 1.0f - saturate(v_dot_h);
    float m2 = m * m;
    float3 fresnel = f0 + (1.0f - f0) * (m2 * m2 * m);

    // lumière qui traverse la feuille : plus jaune et plus saturée que la lumière réfléchie
    float3 c_through = c_diff * float3(1.15f, 1.3f, 0.55f);
    float3 direct = float3(0.0f, 0.0f, 0.0f);
    float lit = saturate((n_dot_l + wrap) / (1.0f + wrap)); // = N.L sans enveloppe
    float through =
        translucency * (0.6f * saturate(-n_dot_l) + 0.4f * pow(saturate(dot(v, -l)), 3.0f));
    if (lit > 0.0f || through > 0.0f) {
        float3 specular = float3(0.0f, 0.0f, 0.0f);
        if (n_dot_l > 0.0f) {
            float d_term = n_dot_h * n_dot_h * (a2 - 1.0f) + 1.0f;
            float d = a2 / (PI * d_term * d_term);
            float ggx_v = n_dot_l * sqrt(n_dot_v * n_dot_v * (1.0f - a2) + a2);
            float ggx_l = n_dot_v * sqrt(n_dot_l * n_dot_l * (1.0f - a2) + a2);
            float sum = ggx_v + ggx_l;
            float vis = sum > 0.0f ? 0.5f / sum : 0.0f;
            specular = n_dot_l * PI * fresnel * (d * vis);
        }
        direct = sun_radiance.rgb * (lit * (1.0f - fresnel) * c_diff + specular + through * c_through);
        direct *= shadow_factor(shadow_position, shadow_normal);
        direct *= lerp(1.0f, sky, foliage_a.w);
    }

    // Éclairage par image. Le sampler du modèle répète les bords : la LUT est donc lue à un demi-texel
    // du bord au plus près (la LUT fait 64 x 64, centres de texels à (i + 0,5) / 64).
    float3 irradiance = irradiance_map.SampleLevel(irradiance_sampler, n, 0.0f).rgb;
    float3 reflected = reflect(-v, n);
    float3 prefiltered =
        prefiltered_map.SampleLevel(prefiltered_sampler, reflected, roughness * environment.x).rgb;
    float2 lut_uv = clamp(float2(n_dot_v, roughness), 0.5f / 64.0f, 1.0f - 0.5f / 64.0f);
    float2 ab = brdf_lut.SampleLevel(brdf_sampler, lut_uv, 0.0f).rg;
    float3 diffuse_light = c_diff * irradiance;
    if (foliage) { // le ciel de l'autre côté de la feuille l'éclaire aussi, par transparence
        diffuse_light += 0.5f * translucency * c_through *
                         irradiance_map.SampleLevel(irradiance_sampler, -n, 0.0f).rgb;
    }
    float3 reflection = prefiltered * (f0 * ab.x + ab.y);
    if (foliage) {
        reflection *= foliage_c.w; // des milliers de petites feuilles ne font pas un miroir du ciel
    }
    float3 indirect =
        occlusion * lerp(1.0f, sky, foliage_a.z) * ambient.rgb * (diffuse_light + reflection);

    return float4(apply_fog(direct + indirect + emission, world_position), 1.0f);
}
