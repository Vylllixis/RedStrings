// Passe d'ombre des arbres réalistes (engine/tree_render.h) : la position placée par son instance
// et déplacée par le vent EXACTEMENT comme dans tree.vert (l'ombre bouge avec l'arbre), et les UV
// pour que tree_shadow.frag découpe le feuillage.
// Les emplacements suivent l'ordre de déclaration : 0 position, 1 UV, 2 et 3 l'instance.
cbuffer Uniforms : register(b0, space1)
{
    float4x4 light_view_proj;
    float4 wind;  // xy : direction (unitaire), z : force (m), w : temps (s)
    float4 shape; // x : hauteur du modèle, y : hauteur du centre du houppier, z : rayon, w : frémissement (m)
};

struct Input {
    float3 position : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 instance_position : TEXCOORD2; // xyz : position monde, w : échelle
    float4 instance_rotation : TEXCOORD3; // quaternion x y z w
};

struct Output {
    float2 uv : TEXCOORD0;
    float4 position : SV_Position;
};

float3 rotate(float4 q, float3 v)
{
    return v + 2.0f * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

Output main(Input input)
{
    Output output;
    float3 base = input.instance_position.xyz;
    float scale = input.instance_position.w;
    float3 local = input.position;

    float h = saturate(local.y / shape.x);
    float phase = 0.37f * base.x + 0.23f * base.z;
    float gust = 0.6f + 0.4f * sin(0.7f * wind.w + phase);
    float bend = wind.z * (shape.x * scale / 10.0f) * h * h * gust;
    float reach = saturate(length(local.xz) / shape.z);
    float wave = sin(2.3f * wind.w + phase + 1.7f * local.x + 2.9f * local.y + 1.3f * local.z);
    float flap = shape.w * reach * h * wave * scale;
    float along = bend + 0.4f * flap;
    float3 world = rotate(input.instance_rotation, local * scale) + base +
                   float3(wind.x * along, 0.6f * flap, wind.y * along);

    output.uv = input.uv;
    output.position = mul(light_view_proj, float4(world, 1.0f));
    return output;
}
