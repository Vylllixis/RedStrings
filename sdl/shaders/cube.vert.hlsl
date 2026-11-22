// Sommet : position transformée par la matrice MVP, couleur passée au fragment.
// La matrice vient du code C (une mat4 de cglm copiée telle quelle) : colonne-majeur, donc
// mul(mvp, v) calcule bien M * v. Tampon uniforme du vertex shader : register(b0, space1).
cbuffer Uniforms : register(b0, space1)
{
    float4x4 mvp;
};

struct Input {
    float3 position : TEXCOORD0;
    float3 color : TEXCOORD1;
};

struct Output {
    float4 color : TEXCOORD0;
    float4 position : SV_Position;
};

Output main(Input input)
{
    Output output;
    output.color = float4(input.color, 1.0f);
    output.position = mul(mvp, float4(input.position, 1.0f));
    return output;
}
