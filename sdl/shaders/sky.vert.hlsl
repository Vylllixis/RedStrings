// Sommet de la skybox (M1-16) : UN triangle plein écran fabriqué à partir de SV_VertexID (comme
// tonemap.vert, sans attribut), plus la DIRECTION MONDE du pixel : le point du rayon à la profondeur
// 0,5 (dans la plage [0, 1]), dé-projeté par l'inverse de view_proj, moins la position de la caméra.
// z = 0 : le fragment est tout au fond (le ciel est dessiné avant la scène, sans profondeur).
cbuffer Sky : register(b0, space1)
{
    float4x4 inv_view_proj;
    float4 camera; // xyz : position de la caméra (monde)
};

struct Output {
    float3 direction : TEXCOORD0;
    float4 position : SV_Position;
};

Output main(uint id : SV_VertexID)
{
    float2 uv = float2((id << 1) & 2, id & 2);
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 world = mul(inv_view_proj, float4(ndc, 0.5f, 1.0f));
    Output output;
    output.direction = world.xyz / world.w - camera.xyz;
    output.position = float4(ndc, 0.0f, 1.0f);
    return output;
}
