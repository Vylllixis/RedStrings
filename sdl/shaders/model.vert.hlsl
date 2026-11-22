// Sommet d'un modèle glTF (engine/model_gpu.h) : position, normale, UV et tangente. Les uniforms
// sont ceux de ModelUniforms (model_gpu.c) : MVP, matrice du modèle (position MONDE), matrice
// normale (inverse transposée du modèle) et couleur de base du matériau, transmise au fragment.
// Les normales, tangentes et positions sont envoyées au fragment en espace MONDE, où l'éclairage
// est calculé.
cbuffer Uniforms : register(b0, space1)
{
    float4x4 mvp;
    float4x4 model;
    float4x4 normal_matrix;
    float4 color;
};

struct Input {
    float3 position : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float4 tangent : TEXCOORD3; // xyz : tangente, w : signe de la bitangente
};

struct Output {
    float3 normal : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 color : TEXCOORD2;
    float3 world_position : TEXCOORD3;
    float4 tangent : TEXCOORD4;
    float4 position : SV_Position;
};

Output main(Input input)
{
    Output output;
    output.normal = mul(normal_matrix, float4(input.normal, 0.0f)).xyz;
    output.uv = input.uv;
    output.color = color;
    output.world_position = mul(model, float4(input.position, 1.0f)).xyz;
    // une tangente est une DIRECTION de la surface : elle suit la matrice du modèle, pas la matrice
    // normale
    output.tangent = float4(mul(model, float4(input.tangent.xyz, 0.0f)).xyz, input.tangent.w);
    output.position = mul(mvp, float4(input.position, 1.0f));
    return output;
}
