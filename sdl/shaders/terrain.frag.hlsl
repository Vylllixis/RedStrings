// Fragment du terrain (M3-03) : les formules d'éclairage de model.frag (PBR métal/rugosité, soleil,
// éclairage par image, ombre PCF : ne modifier l'un qu'avec l'autre, le test engine.terrain_gpu
// compare les deux), mais la couleur de base vient d'un MÉLANGE de quatre textures de couche piloté
// par une carte de poids (« splatmap »), avec un mapping TRIPLANAIRE.
//
// Emplacements (space2, même sampler pour 0 à 7) :
//   t0 : splatmap RGBA8 linéaire, un texel par sommet du terrain : R herbe, G roche, B terre,
//        A neige ; les quatre poids somment 1 (engine/splatmap.h) ;
//   t1 à t4 : albédo (sRGB, répétées, mipmaps) de l'herbe, de la roche, de la terre, de la neige ;
//   t5 à t7 : environnement (irradiance, préfiltrée, LUT BRDF) ; t8 : carte d'ombre ;
//   t9 : seconde carte de poids (carte du monde, 2026-11-21) : R sable, G lave (B, A inutilisés) ;
//        les six poids des deux cartes somment 1 ; t10, t11 : albédo du sable et de la lave.
//        La lave rayonne (émissive) : elle reste visible la nuit.
// Réglages dans `environment` du cbuffer : y = longueur en mètres d'une répétition d'une texture de
// couche, z = netteté du triplanaire (exposant des poids : 1 = doux, grand = net) ; x reste le
// dernier niveau de mip du cube préfiltré.
//
// Triplanaire : la texture est projetée selon les trois axes du MONDE (plan YZ, XZ, XY) avec des
// coordonnées en mètres divisées par la répétition, et les trois résultats sont mélangés par
// |n|^netteté (normalisés). Sur un sol plat seule la projection du dessus pèse (la texture est
// alors à l'échelle exacte) ; sur une falaise, la projection de côté prend le relais : aucune
// texture n'est étirée le long de la pente, contrairement à un mapping (x, z) simple.
Texture2D<float4> splat_map : register(t0, space2);
Texture2D<float4> grass_map : register(t1, space2);
Texture2D<float4> rock_map : register(t2, space2);
Texture2D<float4> dirt_map : register(t3, space2);
Texture2D<float4> snow_map : register(t4, space2);
TextureCube<float4> irradiance_map : register(t5, space2);
TextureCube<float4> prefiltered_map : register(t6, space2);
Texture2D<float4> brdf_lut : register(t7, space2);
SamplerState splat_sampler : register(s0, space2);
SamplerState grass_sampler : register(s1, space2);
SamplerState rock_sampler : register(s2, space2);
SamplerState dirt_sampler : register(s3, space2);
SamplerState snow_sampler : register(s4, space2);
SamplerState irradiance_sampler : register(s5, space2);
SamplerState prefiltered_sampler : register(s6, space2);
SamplerState brdf_sampler : register(s7, space2);
// Carte d'ombre (M1-17) : voir model.frag.
Texture2D<float4> shadow_map : register(t8, space2);
SamplerState shadow_sampler : register(s8, space2);
// Six matières : seconde carte de poids, sable, lave (même sampler que t0 à t7).
Texture2D<float4> splat_map2 : register(t9, space2);
Texture2D<float4> sand_map : register(t10, space2);
Texture2D<float4> lava_map : register(t11, space2);
SamplerState splat2_sampler : register(s9, space2);
SamplerState sand_sampler : register(s10, space2);
SamplerState lava_sampler : register(s11, space2);

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
    float4 cascade_info;    // x : nombre de cascades (0 ou 1 : carte unique ; 2 à 4 : atlas 2 x 2)
    float4 cascade_texel;   // côté d'un texel (m) de chaque cascade
    float4x4 cascade_matrix[4]; // monde -> (u, v, profondeur) de chaque cascade
    // Brouillard (M3-06, engine/fog.h : eng_fog_pack) ; fog_a.w = 0 : pas de brouillard
    float4 fog_a;           // x : densité, y : décroissance en hauteur, z : altitude de base, w : opacité maximale
    float4 fog_rayleigh;    // xyz : épaisseurs optiques de Rayleigh ; w : Mie
    float4 fog_night;       // xyz : plancher nocturne ; w : irradiance E
    float4 fog_sky;         // x : g de Mie, y : path_limit, z : sun_path_limit, w : échelle d'exposition du ciel
    float4 fog_sun;         // xyz : vers le vrai soleil ; w : 1 si donné (M8-07), sinon sun_direction
};

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

// Couleur d'une couche en `world_position` : trois projections pondérées par `weights` (somme 1).
float3 triplanar(Texture2D<float4> tex, SamplerState smp, float3 world_position, float3 weights,
                 float tile)
{
    float3 x_proj = tex.Sample(smp, world_position.zy / tile).rgb;
    float3 y_proj = tex.Sample(smp, world_position.xz / tile).rgb;
    float3 z_proj = tex.Sample(smp, world_position.xy / tile).rgb;
    return x_proj * weights.x + y_proj * weights.y + z_proj * weights.z;
}

float4 main(float3 normal : TEXCOORD0, float2 uv : TEXCOORD1, float4 color : TEXCOORD2,
            float3 world_position : TEXCOORD3, float4 tangent : TEXCOORD4) : SV_Target0
{
    float3 n = normalize(normal);
    float3 geometric_normal = n;

    // poids des couches : le texel (i, j) de la splatmap est AU sommet (i, j), dont l'uv est
    // i / (size - 1) ; le centre du texel i est à (i + 0,5) / size
    uint splat_w;
    uint splat_h;
    splat_map.GetDimensions(splat_w, splat_h);
    float2 splat_size = float2((float)splat_w, (float)splat_h);
    float4 w = splat_map.Sample(splat_sampler, (uv * (splat_size - 1.0f) + 0.5f) / splat_size);

    float tile = max(environment.y, 0.01f);
    float3 tri = pow(abs(n), max(environment.z, 1.0f));
    tri /= max(tri.x + tri.y + tri.z, 1e-5f);
    float3 albedo = triplanar(grass_map, grass_sampler, world_position, tri, tile) * w.r +
                    triplanar(rock_map, rock_sampler, world_position, tri, tile) * w.g +
                    triplanar(dirt_map, dirt_sampler, world_position, tri, tile) * w.b +
                    triplanar(snow_map, snow_sampler, world_position, tri, tile) * w.a;
    float4 w2 = splat_map2.Sample(splat2_sampler, (uv * (splat_size - 1.0f) + 0.5f) / splat_size);
    float3 lava_albedo = triplanar(lava_map, lava_sampler, world_position, tri, tile);
    albedo += triplanar(sand_map, sand_sampler, world_position, tri, tile) * w2.r +
              lava_albedo * w2.g;
    float4 base = float4(albedo * color.rgb, color.a);
    float metallic = saturate(material.x);
    float roughness = max(saturate(material.y), ROUGHNESS_MIN);
    float occlusion = 1.0f;
    float3 emission = emissive.rgb + (lava_albedo * lava_albedo * 3.0f * w2.g);
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

    return float4(apply_fog(direct + indirect + emission, world_position), base.a);
}
