// Herbe animée (M3-09) : maillage de brin instancié (comme instanced.vert, mêmes sorties : le fragment
// shader est model.frag) avec le vent et la disparition à distance. Le brin fait 1 m de haut dans le
// repère du modèle. Voir engine/grass.h : eng_grass_vertex_world est la référence CPU de ce shader.
//   échelle effective = échelle de l'instance x (1 - smoothstep(fade_start, fade_end, distance))
//   décalage de la pointe = direction x force x échelle effective x y^2 x onde
//   onde = 0,5 + 0,5 sin(phase + k <direction, position de l'instance>)
cbuffer Uniforms : register(b0, space1)
{
    float4x4 view_proj;
    float4 color;      // facteur de couleur de base du matériau
    float4 camera_fade; // xyz : caméra, w : fade_start
    float4 wind;        // xy : direction (unitaire), z : force, w : k (rad/m)
    float4 extra;       // x : phase (rad), y : fade_end
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
    float3 base = input.instance_position.xyz;
    float distance_to_camera = length(base - camera_fade.xyz);
    float fade = 1.0f - smoothstep(camera_fade.w, extra.y, distance_to_camera);
    float scale = input.instance_position.w * fade;
    float wave = 0.5f + 0.5f * sin(extra.x + wind.w * dot(wind.xy, base.xz));
    float bend = wind.z * scale * input.position.y * input.position.y * wave;
    float3 world = rotate(q, input.position * scale) + base + float3(wind.x * bend, 0.0f, wind.y * bend);
    output.normal = rotate(q, input.normal);
    output.uv = input.uv;
    output.color = color * input.instance_tint;
    output.world_position = world;
    output.tangent = float4(rotate(q, input.tangent.xyz), input.tangent.w);
    output.position = mul(view_proj, float4(world, 1.0f));
    return output;
}
