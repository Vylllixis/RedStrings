// Sommet des cubes de la démonstration (M8-07) : comme textured.vert, mais la teinte est multipliée
// par un facteur de lumière (`light.rgb`) calculé par la scène à partir du soleil (ou de la lune)
// et de l'ambiante : les cubes s'assombrissent la nuit au lieu de rester blancs. textured.vert,
// utilisé par les tests de rendu, ne change pas.
cbuffer Uniforms : register(b0, space1)
{
    float4x4 mvp;
    float4 light; // rgb : facteur de lumière ; a : inutilisé
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
    output.tint = float4(input.tint * light.rgb, 1.0f);
    output.uv = input.uv;
    output.position = mul(mvp, float4(input.position, 1.0f));
    return output;
}
