// Ciel atmosphérique analytique (M3-05). Recopie EXACTEMENT les formules de
// engine/src/atmosphere.c (sky_above, eng_atmosphere_sky, eng_atmosphere_radiance) : HLSL n'a pas
// d'#include dans cette chaîne d'outils ; un test compare le rendu à la version C. Le résultat est
// de la radiance linéaire HDR, multipliée par l'échelle d'exposition ; le tonemapping vient après.
cbuffer Atmosphere : register(b0, space3)
{
    float4 sun;      // xyz : vers le soleil (unitaire) ; w : cos du rayon du disque
    float4 rayleigh; // xyz : épaisseurs optiques de Rayleigh ; w : Mie
    float4 ground;   // xyz : albédo du sol ; w : g de Mie
    float4 night;    // xyz : plancher nocturne ; w : irradiance E
    float4 limits;   // x : radiance du disque ; y : path_limit ; z : sun_path_limit ; w : cos du coeur
    float4 exposure; // x : échelle
};

static const float PI = 3.14159265358979323846f;
static const float TWILIGHT_SIN = 0.21f;
static const float HORIZON_BLEND = 0.05f;

float air_mass(float mu)
{
    mu = clamp(mu, 0.0f, 1.0f);
    return 1.0f / (mu + 0.025f * exp(-11.0f * mu));
}

float3 sky_above(float3 s, float3 v)
{
    float m_view = min(air_mass(v.y), limits.y);
    float m_sun = min(air_mass(s.y), limits.z);
    float c = dot(s, v);
    float phase_r = (3.0f / (16.0f * PI)) * (1.0f + c * c);
    float g = ground.w;
    float phase_m = (1.0f - g * g) / (4.0f * PI * pow(1.0f + g * g - 2.0f * g * c, 1.5f));
    float twilight = saturate((s.y + TWILIGHT_SIN) / TWILIGHT_SIN);
    float lit = twilight * twilight;
    float3 ext = rayleigh.xyz + rayleigh.w;
    float3 scatter = rayleigh.xyz * phase_r + rayleigh.w * phase_m;
    float3 transmit_sun = exp(-ext * m_sun);
    float3 inscatter = (1.0f - exp(-ext * m_view)) / ext;
    return night.w * scatter * inscatter * transmit_sun * lit + night.xyz;
}

float4 main(float3 direction : TEXCOORD0) : SV_Target0
{
    float3 v = normalize(direction);
    float3 s = sun.xyz;
    float3 color = sky_above(s, v);
    if (v.y < 0.0f) {
        float3 flat = float3(v.x, 0.0f, v.z);
        float flat_length = length(flat);
        flat = flat_length > 1e-12f ? flat / flat_length : float3(1.0f, 0.0f, 0.0f);
        float3 horizon = sky_above(s, flat);
        float t = saturate(-v.y / HORIZON_BLEND);
        float3 ground_color = ground.xyz * horizon;
        color = horizon + (ground_color - horizon) * t;
    }
    float c = dot(s, v);
    float cos_edge = sun.w;
    float cos_core = limits.w;
    if (v.y >= 0.0f && c > cos_edge) {
        float limb = saturate((c - cos_edge) / (cos_core - cos_edge));
        float m = air_mass(s.y);
        float3 transmit = exp(-(rayleigh.xyz + rayleigh.w) * m);
        color += limits.x * transmit * limb;
    }
    return float4(color * exposure.x, 1.0f);
}
