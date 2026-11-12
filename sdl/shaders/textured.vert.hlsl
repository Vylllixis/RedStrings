// Sommet : position transformée par la matrice MVP (voir cube.vert.hlsl), plus une teinte et des
// coordonnées de texture passées au fragment. (0, 0) = coin haut-gauche de l'image, v vers le bas.
cbuffer Uniforms : register(b0, space1)
{
    float4x4 mvp;
};

struct Input {
    float3 position : TEXCOORD0;
    float3 tint : TEXCOORD1;
    float2 uv : TEXCOORD2;
};

struct Output {
    float4 tint : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 position : SV_Position;
};

Output main(Input input)
{
    Output output;
    output.tint = float4(input.tint, 1.0f);
    output.uv = input.uv;
    output.position = mul(mvp, float4(input.position, 1.0f));
    return output;
}
