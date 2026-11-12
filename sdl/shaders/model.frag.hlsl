// Fragment d'un modèle : matériau PBR métal/rugosité (M1-15), une lumière directionnelle et une
// ambiante uniforme, en espace MONDE. Même formules que eng_pbr_shade (engine/pbr.c), que les tests
// comparent pixel à pixel (docs/pbr.md) : Cook-Torrance, distribution GGX, visibilité de Smith
// corrélée, Fresnel de Schlick, diffus de Lambert pondéré par (1 - F) et par (1 - métal).
//   direct   = ombre * soleil * N.L * ((1 - F) * c_diff + pi * F * D * Vis)
//   ambiante = occlusion * ambiante * (c_diff * irradiance(N) + préfiltrée(R, r) * (F0 * A + B))
//   couleur  = direct + ambiante + émissif
// L'ambiante est l'ÉCLAIRAGE PAR IMAGE de M1-16 (engine/ibl.h, docs/ibl.md) : irradiance/pi
// (cubemap), radiance préfiltrée par rugosité (cubemap, un niveau de mip par rugosité) et LUT BRDF
// (A, B) ; le facteur `ambient` est l'intensité de l'environnement.
// Huit textures, une paire texture + sampler par emplacement (space2) : couleur de base (sRGB),
// métal/rugosité (G = rugosité, B = métal), normale, occlusion (R), émissif (sRGB). Les matériaux
// sans texture en reçoivent une blanche de 1 x 1 (le facteur seul compte). Les emplacements 5 à 7
// sont l'environnement : irradiance (cube), préfiltrée (cube), LUT BRDF.
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
    float4 sun_direction;   // xyz : vers le soleil (unitaire)
    float4 sun_radiance;    // rgb : couleur * intensité du soleil
    float4 ambient;         // rgb : couleur * intensité ambiante
    float4 camera_position; // xyz : position de la caméra (monde)
    float4 material;        // x : métal, y : rugosité, z : échelle de la normale, w : force de l'occlusion
    float4 emissive;        // rgb : facteur émissif ; w : 1 si le matériau a une normal map
    float4 environment;     // x : dernier niveau de mip du cube préfiltré (nombre de niveaux - 1)
    float4x4 shadow_matrix; // monde -> (u, v, profondeur) de la carte d'ombre
    float4 shadow_a;        // xyz : vers le soleil (unitaire), w : côté d'un texel en mètres
    float4 shadow_b;        // x : taille de la carte (texels), y : biais constant, z : biais de pente, w : biais normal
    float4 shadow_c;        // x : rayon du PCF, y : force de l'ombre, z : 1 si une ombre est active
};

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

float4 main(float3 normal : TEXCOORD0, float2 uv : TEXCOORD1, float4 color : TEXCOORD2,
            float3 world_position : TEXCOORD3, float4 tangent : TEXCOORD4) : SV_Target0
{
    float4 base = base_color_map.Sample(base_color_sampler, uv) * color;
    float4 mr = metal_rough_map.Sample(metal_rough_sampler, uv);
    float metallic = saturate(material.x * mr.b);
    float roughness = max(saturate(material.y * mr.g), ROUGHNESS_MIN);
    float occlusion = 1.0f + material.w * (occlusion_map.Sample(occlusion_sampler, uv).r - 1.0f);
    float3 emission = emissive.rgb * emissive_map.Sample(emissive_sampler, uv).rgb;

    float3 n = normalize(normal);
    float3 geometric_normal = n; // la normale du sommet : les ombres ignorent la normal map
    if (emissive.w > 0.5f) {
        n = perturb_normal(n, tangent, normal_map.Sample(normal_sampler, uv).rgb, material.z);
    }
    float3 l = sun_direction.xyz;
    float3 v = normalize(camera_position.xyz - world_position);
    float3 h = normalize(l + v);

    float alpha = roughness * roughness;
    float a2 = alpha * alpha;
    float n_dot_l = dot(n, l);
    float n_dot_v = max(dot(n, v), MIN_DOT);
    float n_dot_h = max(dot(n, h), 0.0f);
    float v_dot_h = max(dot(v, h), 0.0f);

    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), base.rgb, metallic);
    float3 c_diff = base.rgb * (1.0f - metallic);
    float m = 1.0f - saturate(v_dot_h);
    float m2 = m * m;
    float3 fresnel = f0 + (1.0f - f0) * (m2 * m2 * m);

    float3 direct = float3(0.0f, 0.0f, 0.0f);
    if (n_dot_l > 0.0f) {
        float d_term = n_dot_h * n_dot_h * (a2 - 1.0f) + 1.0f;
        float d = a2 / (PI * d_term * d_term);
        float ggx_v = n_dot_l * sqrt(n_dot_v * n_dot_v * (1.0f - a2) + a2);
        float ggx_l = n_dot_v * sqrt(n_dot_l * n_dot_l * (1.0f - a2) + a2);
        float sum = ggx_v + ggx_l;
        float vis = sum > 0.0f ? 0.5f / sum : 0.0f;
        direct = sun_radiance.rgb * n_dot_l * ((1.0f - fresnel) * c_diff + PI * fresnel * (d * vis));
        direct *= shadow_factor(world_position, geometric_normal);
    }

    // Éclairage par image. Le sampler du modèle répète les bords : la LUT est donc lue à un demi-texel
    // du bord au plus près (la LUT fait 64 x 64, centres de texels à (i + 0,5) / 64).
    float3 irradiance = irradiance_map.SampleLevel(irradiance_sampler, n, 0.0f).rgb;
    float3 reflected = reflect(-v, n);
    float3 prefiltered =
        prefiltered_map.SampleLevel(prefiltered_sampler, reflected, roughness * environment.x).rgb;
    float2 lut_uv = clamp(float2(n_dot_v, roughness), 0.5f / 64.0f, 1.0f - 0.5f / 64.0f);
    float2 ab = brdf_lut.SampleLevel(brdf_sampler, lut_uv, 0.0f).rg;
    float3 indirect =
        occlusion * ambient.rgb * (c_diff * irradiance + prefiltered * (f0 * ab.x + ab.y));

    return float4(direct + indirect + emission, base.a);
}
