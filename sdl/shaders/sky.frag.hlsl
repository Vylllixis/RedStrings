// Fragment de la skybox (M1-16) : lit la cubemap du ciel (RGBA16F, avec ses mips) dans la direction
// du pixel, multipliée par l'intensité. Le résultat reste en lumière linéaire HDR : l'exposition et le
// tonemapping viennent après, comme pour le reste de la scène.
TextureCube<float4> sky_map : register(t0, space2);
SamplerState sky_sampler : register(s0, space2);

cbuffer Sky : register(b0, space3)
{
    float4 params; // x : intensité
};

float4 main(float3 direction : TEXCOORD0) : SV_Target0
{
    return float4(sky_map.Sample(sky_sampler, direction).rgb * params.x, 1.0f);
}
