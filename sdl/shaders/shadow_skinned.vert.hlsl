// Passe d'ombre d'un modèle skinné (M4-15) : le sommet est déformé par la palette (voir
// model_skinned.vert) puis projeté par la matrice « modèle -> découpage du soleil » qui ouvre le
// bloc d'uniforms ; la palette suit, trois float4 par articulation.
cbuffer Uniforms : register(b0, space1)
{
    float4x4 light_mvp;
    float4 palette[768];
};

struct Input {
    float3 position : TEXCOORD0;
    float4 joints : TEXCOORD4;
    float4 weights : TEXCOORD5;
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
