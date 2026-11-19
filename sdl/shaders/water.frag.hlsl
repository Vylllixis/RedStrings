// Fragment de l'eau (M3-10) : couleur selon la profondeur (uv.x, calculée par sommet par
// eng_water_build), reflet du ciel de Fresnel (Schlick), rides par deux ondes planes, reflet du soleil.
// Les formules sont celles de eng_water_shade (engine/water.c) : ne modifier l'un qu'avec l'autre, le
// test engine.water_gpu les compare pixel pour pixel. Puis le brouillard de terrain.frag (mêmes formules
// que eng_fog_apply). Aucune texture : l'eau est entièrement procédurale.
//
// Le cbuffer est celui de model.frag / terrain.frag (le renderer et eng_model_gpu_draw_ex le remplissent
// pareil) suivi du brouillard (16 floats, eng_fog_pack) puis des uniforms de l'eau (20 floats,
// eng_water_pack).
cbuffer Lighting : register(b0, space3)
{
    float4 sun_direction;   // xyz : vers le soleil (unitaire)
    float4 sun_radiance;    // rgb : couleur * intensité du soleil
    float4 ambient;         // rgb : couleur * intensité ambiante
    float4 camera_position; // xyz : position de la caméra (monde)
    float4 material;        // (inutilisé ici)
    float4 emissive;
    float4 environment;
    float4x4 shadow_matrix;
    float4 shadow_a;
    float4 shadow_b;
    float4 shadow_c;
    float4 cascade_info;        // M7-09 (inutilisé ici)
    float4 cascade_texel;
    float4x4 cascade_matrix[4];
    // Brouillard (M3-06) ; fog_a.w = 0 : pas de brouillard
    float4 fog_a;
    float4 fog_rayleigh;
    float4 fog_night;
    float4 fog_sky;
    // Eau (M3-10, eng_water_pack)
    float4 water_horizon; // rgb : ciel à l'horizon ; w : f0
    float4 water_zenith;  // rgb : ciel au zénith ; w : absorption
    float4 water_shallow; // rgb : eau peu profonde ; w : amplitude des rides
    float4 water_deep;    // rgb : eau profonde ; w : phase de la première ride
    float4 water_ripple;  // x : nombre d'onde ; y : phase de la seconde ride
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
    float3 fog_color = fog_horizon(sun_direction.xyz, flat) * fog_sky.w;
    return lerp(lit_color, fog_color, f);
}


static const float PI = 3.14159265358979323846f;

float3 water_normal(float2 p)
{
    float k1 = water_ripple.x;
    float k2 = 1.7f * water_ripple.x;
    float w1 = cos(k1 * (0.8f * p.x + 0.6f * p.y) + water_deep.w);
    float w2 = cos(k2 * (-0.6f * p.x + 0.8f * p.y) + water_ripple.y);
    float gx = water_shallow.w * (w1 * 0.8f + 0.5f * w2 * -0.6f);
    float gz = water_shallow.w * (w1 * 0.6f + 0.5f * w2 * 0.8f);
    return normalize(float3(-gx, 1.0f, -gz));
}

float4 main(float3 normal : TEXCOORD0, float2 uv : TEXCOORD1, float4 color : TEXCOORD2,
            float3 world_position : TEXCOORD3, float4 tangent : TEXCOORD4) : SV_Target0
{
    float depth = max(uv.x, 0.0f);
    float t = 1.0f - exp(-water_zenith.w * depth);
    float3 body = lerp(water_shallow.rgb, water_deep.rgb, t);
    float3 n = water_normal(world_position.xz);
    float3 to_camera = camera_position.xyz - world_position;
    float to_camera_length = length(to_camera);
    float3 v = to_camera_length > 1e-12f ? to_camera / to_camera_length : float3(0.0f, 1.0f, 0.0f);
    float nv = dot(n, v);
    float m = 1.0f - saturate(nv);
    float m2 = m * m;
    float fresnel = water_horizon.w + (1.0f - water_horizon.w) * m2 * m2 * m;
    float3 r = 2.0f * nv * n - v;
    float sky_mix = sqrt(saturate(r.y));
    float3 reflected = lerp(water_horizon.rgb, water_zenith.rgb, sky_mix);
    float nl = saturate(dot(n, sun_direction.xyz));
    float rl = saturate(dot(r, sun_direction.xyz));
    float3 lit = body * (ambient.rgb + sun_radiance.rgb * nl / PI);
    float3 surface = lerp(lit, reflected, fresnel) + sun_radiance.rgb * (pow(rl, 256.0f) * fresnel);
    return float4(apply_fog(surface, world_position), 1.0f);
}
