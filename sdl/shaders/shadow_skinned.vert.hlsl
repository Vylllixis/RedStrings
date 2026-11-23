// Passe d'ombre d'un modèle skinné (M4-15) : le sommet est déformé par la palette (voir
// model_skinned.vert) puis projeté par la matrice « modèle -> découpage du soleil » qui ouvre le
// bloc d'uniforms ; la palette suit, trois float4 par articulation.
cbuffer Uniforms : register(b0, space1)
{
    float4x4 light_mvp;
    float4 palette[768];
};

struct Input {
    // Indices contigus DANS L'ORDRE de déclaration : la compilation SPIR-V numérote les
    // locations dans cet ordre (0, 1, 2) alors que D3D12 lit l'indice de TEXCOORD ; les deux
    // coïncident seulement ainsi (TEXCOORD0/4/5 laissait Vulkan lire les poids au mauvais endroit).
    float3 position : TEXCOORD0;
    float4 joints : TEXCOORD1;
    float4 weights : TEXCOORD2;
};

float4 main(Input input) : SV_Position
{
    float4 p = float4(input.position, 1.0f);
    float3 skinned = float3(0.0f, 0.0f, 0.0f);
    [unroll]
    for (int i = 0; i < 4; i++) {
        const int base = (int)round(input.joints[i] * 255.0f) * 3;
        skinned += input.weights[i] *
                   float3(dot(palette[base], p), dot(palette[base + 1], p), dot(palette[base + 2], p));
    }
    return mul(light_mvp, float4(skinned, 1.0f));
}
