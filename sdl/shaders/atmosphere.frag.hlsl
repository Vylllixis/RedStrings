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
    float4 exposure; // x : échelle ; y : étoiles [0 ; 1] ; z : lune [0 ; 1] (M8-07)
    float4 moon;     // xyz : vers la lune
};

// M8-07 : étoiles fixes. La sphère est découpée en petites cases (angle ~0,35°) ; une case sur 400
// environ porte une étoile (hachage de ses indices), d'éclat tiré au hasard, ronde et adoucie ;
// atténuées près de l'horizon. Radiance à l'échelle du ciel de nuit (plancher `night`).
float hash3(float3 p)
{
    p = frac(p * 0.1031f);
    p += dot(p, p.yzx + 33.33f);
    return frac((p.x + p.y) * p.z);
}

float3 night_sky(float3 v)
{
    float3 add = float3(0.0f, 0.0f, 0.0f);
    if (exposure.y > 0.0f && v.y > 0.0f) {
        float3 cell = floor(v * 160.0f);
        float h = hash3(cell);
        if (h > 0.9975f) {
            float3 center = (cell + 0.5f + (float3(hash3(cell + 7.1f), hash3(cell + 3.7f),
                                                   hash3(cell + 1.3f)) - 0.5f) * 0.6f) / 160.0f;
            float d = length(normalize(center) - v) * 160.0f;
            float bright = 0.25f + 0.75f * hash3(cell + 11.0f);
            float horizon = saturate(v.y / 0.15f);
            add += float3(0.9f, 0.95f, 1.0f) * (bright * saturate(1.0f - d * 2.2f) * horizon *
                                                exposure.y * 0.6f);
        }
    }
    if (exposure.z > 0.0f) {
        float c = dot(normalize(moon.xyz), v);
        float edge = 0.99985f; // rayon apparent ~1° (agrandi pour être visible)
        if (c > edge) {
            float limb = saturate((c - edge) / (1.0f - edge) * 3.0f);
            add += float3(0.85f, 0.9f, 1.0f) * (1.2f * limb * exposure.z);
        } else if (c > 0.995f) { // halo
            add += float3(0.5f, 0.55f, 0.7f) * (0.06f * exposure.z * (c - 0.995f) / (edge - 0.995f));
        }
    }
    return add;
}

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
    color += night_sky(v);
    return float4(color * exposure.x, 1.0f);
}
