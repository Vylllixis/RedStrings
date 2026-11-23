// Lignes de debug (engine/debug_lines_gpu.h) : EngDebugVertex = position (float3) + couleur
// (4 octets lus comme 4 flottants dans [0, 1]). Uniform : matrice proj * vue, colonne-majeure.
cbuffer Uniforms : register(b0, space1)
{
    float4x4 view_proj;
};

struct Input {
    float3 position : TEXCOORD0;
    float4 color : TEXCOORD1;
};

struct Output {
    float4 color : TEXCOORD0;
    float4 position : SV_Position;
};

Output main(Input input)
{
    Output output;
    output.color = input.color;
    output.position = mul(view_proj, float4(input.position, 1.0f));
    return output;
}
