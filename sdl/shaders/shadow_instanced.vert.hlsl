// Passe d'ombre des maillages instanciés (B8 : arbres et rochers du placement) : comme shadow.vert,
// ne lit que la POSITION (emplacement 0), placée par son instance comme dans instanced.vert
// (emplacements 1 et 2 : position + échelle, rotation ; la teinte est ignorée). Le bloc d'uniforms
// ne porte que la matrice monde -> découpage de la caméra du soleil.
// Les emplacements suivent l'ordre de déclaration (0, 1, 2) : la compilation SPIR-V les numérote
// ainsi quel que soit l'indice de TEXCOORD, d'où des indices contigus ici et dans le pipeline.
cbuffer Uniforms : register(b0, space1)
{
    float4x4 light_view_proj;
};

struct Input {
    float3 position : TEXCOORD0;
    float4 instance_position : TEXCOORD1; // xyz : position monde, w : échelle
    float4 instance_rotation : TEXCOORD2; // quaternion x y z w
};

float3 rotate(float4 q, float3 v)
{
    return v + 2.0f * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

float4 main(Input input) : SV_Position
{
    float3 world = rotate(input.instance_rotation, input.position * input.instance_position.w) +
                   input.instance_position.xyz;
    return mul(light_view_proj, float4(world, 1.0f));
}
