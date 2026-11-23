// Arbres réalistes (2026-11-22, engine/tree_render.h) : maillage instancié comme instanced.vert,
// plus le VENT et la forme d'ensemble du houppier. eng_tree_vertex_world est la référence CPU de ce
// shader (test engine.tree_render_gpu).
//   h      = clamp(y / hauteur, 0, 1)                       hauteur dans l'arbre
//   phase  = 0,37 x_instance + 0,23 z_instance              chaque arbre a son rythme
//   rafale = 0,6 + 0,4 sin(0,7 t + phase)
//   pli    = force x (hauteur x échelle / 10) x h^2 x rafale        le tronc plie vers la cime
//   portée = clamp(distance à l'axe / rayon, 0, 1)
//   onde   = sin(2,3 t + phase + 1,7 x + 2,9 y + 1,3 z)
//   frémi  = frémissement x portée x h x onde x échelle             le bout des branches frémit
//   monde  = rotation(local x échelle) + position + direction x (pli + 0,4 frémi) + Y x 0,6 frémi
cbuffer Uniforms : register(b0, space1)
{
    float4x4 view_proj;
    float4 color; // facteur de couleur de base du matériau
    float4 wind;  // xy : direction (unitaire), z : force (m), w : temps (s)
    float4 shape; // x : hauteur du modèle, y : hauteur du centre du houppier, z : rayon, w : frémissement (m)
};

struct Input {
    float3 position : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float4 tangent : TEXCOORD3; // w : signe de la bitangente x part du ciel vue par le sommet
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
    float4 crown : TEXCOORD5; // xyz : direction du centre du houppier vers le sommet (monde)
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
    float3 world = rotate(q, local * scale) + base + float3(wind.x * along, 0.6f * flap, wind.y * along);

    // le houppier vu comme une boule un peu aplatie : sa normale donne le volume d'ensemble
    float3 from_center = float3(local.x, (local.y - shape.y) * 0.7f, local.z);
    float distance_to_center = length(from_center);
    float3 crown = distance_to_center > 1e-5f ? from_center / distance_to_center : float3(0.0f, 1.0f, 0.0f);

    output.normal = rotate(q, input.normal);
    output.uv = input.uv;
    output.color = color * input.instance_tint;
    output.world_position = world;
    output.tangent = float4(rotate(q, input.tangent.xyz), input.tangent.w);
    output.crown = float4(rotate(q, crown), h);
    output.position = mul(view_proj, float4(world, 1.0f));
    return output;
}
