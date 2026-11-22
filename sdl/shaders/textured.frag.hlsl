// Fragment : couleur de la texture multipliée par la teinte du sommet. Texture et sampler du
// fragment shader : space2 (voir docs/shaders.md).
Texture2D<float4> albedo : register(t0, space2);
SamplerState albedo_sampler : register(s0, space2);

float4 main(float4 tint : TEXCOORD0, float2 uv : TEXCOORD1) : SV_Target0
{
    return albedo.Sample(albedo_sampler, uv) * tint;
}
