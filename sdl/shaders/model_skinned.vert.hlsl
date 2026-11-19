// Sommet d'un modèle skinné (M4-15). Mêmes sorties que model.vert : le fragment shader est
// model.frag, inchangé.
// Entrée : le flux de model.vert (position, normale, UV, tangente) plus, dans le MÊME sommet,
// quatre indices d'articulation (TEXCOORD4, octets lus comme 4 flottants dans [0, 1] : x 255 pour
// retrouver l'indice) et quatre poids (TEXCOORD5, de somme 1).
// Uniforms : ceux de model.vert, puis la PALETTE : trois float4 par articulation (lignes 0 à 2 de
// la matrice 4x4 « monde de l'articulation x inverse de liaison », la quatrième ligne est toujours
// 0 0 0 1). Le sommet est d'abord déformé dans l'espace du personnage, puis placé dans le monde par
// `model`. Les normales et tangentes suivent la partie linéaire de la matrice mélangée : exact pour
// des articulations sans échelle non uniforme (cas des squelettes importés, voir docs/skinning.md).
cbuffer Uniforms : register(b0, space1)
{
    float4x4 mvp; // modèle -> découpage
    float4x4 model;
    float4x4 normal_matrix;
    float4 color;
    float4 palette[768]; // 256 articulations x 3 lignes
};

struct Input {
    float3 position : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float4 tangent : TEXCOORD3;
    float4 joints : TEXCOORD4;
    float4 weights : TEXCOORD5;
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
    float4 p = float4(input.position, 1.0f);
    float3 skinned_position = float3(0.0f, 0.0f, 0.0f);
    float3 skinned_normal = float3(0.0f, 0.0f, 0.0f);
    float3 skinned_tangent = float3(0.0f, 0.0f, 0.0f);
    [unroll]
    for (int i = 0; i < 4; i++) {
        const float w = input.weights[i];
        const int base = (int)round(input.joints[i] * 255.0f) * 3;
        const float4 r0 = palette[base];
        const float4 r1 = palette[base + 1];
        const float4 r2 = palette[base + 2];
        skinned_position += w * float3(dot(r0, p), dot(r1, p), dot(r2, p));
        skinned_normal += w * float3(dot(r0.xyz, input.normal), dot(r1.xyz, input.normal),
                                     dot(r2.xyz, input.normal));
        skinned_tangent += w * float3(dot(r0.xyz, input.tangent.xyz), dot(r1.xyz, input.tangent.xyz),
                                      dot(r2.xyz, input.tangent.xyz));
    }
    Output output;
    output.normal = normalize(mul(normal_matrix, float4(skinned_normal, 0.0f)).xyz);
    output.uv = input.uv;
    output.color = color;
    output.world_position = mul(model, float4(skinned_position, 1.0f)).xyz;
    output.tangent = float4(mul(model, float4(skinned_tangent, 0.0f)).xyz, input.tangent.w);
    output.position = mul(mvp, float4(skinned_position, 1.0f));
    return output;
}
