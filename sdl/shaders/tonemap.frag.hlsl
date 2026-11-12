// Fragment de la passe de tonemapping (M1-14) : exposition, opérateur, encodage sRGB. Même calcul
// que eng_tonemap_apply (engine/tonemap.c), que les tests comparent pixel à pixel
// (docs/tonemapping.md).
Texture2D<float4> hdr : register(t0, space2);
SamplerState hdr_sampler : register(s0, space2);

cbuffer Tonemap : register(b0, space3)
{
    float4 params; // x : multiplicateur d'exposition (2^EV) ; y : opérateur (0 aucun, 1 ACES,
                   // 2 AgX) ; z : 1 = encoder en sRGB, 0 = la cible le fait (format _SRGB)
};

float3 aces(float3 x)
{
    return saturate((x * (2.51f * x + 0.03f)) / (x * (2.43f * x + 0.59f) + 0.14f));
}

// Équivalent de la mat3 GLSL de three.js (trois COLONNES) multipliée par v.
float3 mul_columns(float3 c0, float3 c1, float3 c2, float3 v)
{
    return c0 * v.x + c1 * v.y + c2 * v.z;
}

float3 agx_contrast(float3 x)
{
    float3 x2 = x * x;
    float3 x4 = x2 * x2;
    return 15.5f * x4 * x2 - 40.14f * x4 * x + 31.96f * x4 - 6.868f * x2 * x + 0.4298f * x2 +
           0.1191f * x - 0.00232f;
}

float3 agx(float3 color)
{
    color = mul_columns(float3(0.6274f, 0.0691f, 0.0164f), float3(0.3293f, 0.9195f, 0.0880f),
                        float3(0.0433f, 0.0113f, 0.8956f), color);
    color = mul_columns(float3(0.856627153315983f, 0.137318972929847f, 0.11189821299995f),
                        float3(0.0951212405381588f, 0.761241990602591f, 0.0767994186031903f),
                        float3(0.0482516061458583f, 0.101439036467562f, 0.811302368396859f), color);
    color = log2(max(color, 1e-10f));
    color = saturate((color - (-12.47393f)) / (4.026069f - (-12.47393f)));
    color = agx_contrast(color);
    color = mul_columns(float3(1.1271005818144368f, -0.1413297634984383f, -0.14132976349843826f),
                        float3(-0.11060664309660323f, 1.157823702216272f, -0.11060664309660294f),
                        float3(-0.016493938717834573f, -0.016493938717834257f, 1.2519364065950405f),
                        color);
    color = pow(max(color, 0.0f), 2.2f);
    color = mul_columns(float3(1.6605f, -0.1246f, -0.0182f), float3(-0.5876f, 1.1329f, -0.1006f),
                        float3(-0.0728f, -0.0083f, 1.1187f), color);
    return saturate(color);
}

float3 srgb_encode(float3 x)
{
    x = saturate(x);
    // step(x, seuil) vaut 1 quand x <= seuil (la condition scalaire d'un « ? : » ne vaut pas pour
    // un vecteur en HLSL)
    return lerp(1.055f * pow(x, 1.0f / 2.4f) - 0.055f, 12.92f * x, step(x, 0.0031308f));
}

float4 main(float2 uv : TEXCOORD0) : SV_Target0
{
    // 65504 : plus grand RGBA16F fini ; +inf (débordement) donnerait inf/inf = NaN, un pixel noir
    float3 color = clamp(hdr.Sample(hdr_sampler, uv).rgb, 0.0f, 65504.0f) * params.x;
    int op = (int)(params.y + 0.5f);
    if (op == 1) {
        color = aces(color);
    } else if (op == 2) {
        color = agx(color);
    } else {
        color = saturate(color);
    }
    if (params.z > 0.5f) {
        color = srgb_encode(color);
    }
    return float4(color, 1.0f);
}
