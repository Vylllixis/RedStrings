// Maillage instancié (M3-07) : le MÊME maillage (sommets EngModelVertex, flux 0) dessiné N fois, une
// fois par instance du flux 1 (EngInstance, engine/instancing.h) en un seul dessin. Les sorties sont
// celles de model.vert : le fragment shader est model.frag, inchangé.
// Instance : position monde + échelle uniforme, rotation (quaternion unitaire x y z w), teinte RGBA.
// Équivalence avec model.vert : modèle = translation(position) * rotation(q) * échelle(s) ; une
// échelle uniforme n'a pas besoin de la transposée de l'inverse pour la normale. Test
// engine.instancing_gpu : N instances == N dessins ordinaires.
cbuffer Uniforms : register(b0, space1)
{
    float4x4 view_proj;
    float4 color; // facteur de couleur de base du matériau
};

struct Input {
    float3 position : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float4 tangent : TEXCOORD3;
    float4 instance_position : TEXCOORD4; // xyz : position monde, w : échelle
    float4 instance_rotation : TEXCOORD5; // quaternion x y z w
    float4 instance_tint : TEXCOORD6;     // RGBA dans [0, 1]
};

struct Output {
    float3 normal : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 color : TEXCOORD2;
    float3 world_position : TEXCOORD3;
    float4 tangent : TEXCOORD4;
    float4 position : SV_Position;
};

float3 rotate(float4 q, float3 v)
{
    return v + 2.0f * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

Output main(Input input)
{
    Output output;
    float4 q = input.instance_rotation;
    float3 world = rotate(q, input.position * input.instance_position.w) + input.instance_position.xyz;
    output.normal = rotate(q, input.normal);
    output.uv = input.uv;
    output.color = color * input.instance_tint;
    output.world_position = world;
    output.tangent = float4(rotate(q, input.tangent.xyz), input.tangent.w);
    output.position = mul(view_proj, float4(world, 1.0f));
    return output;
}
