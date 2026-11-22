// Sommet de la passe de tonemapping (M1-14) : UN triangle plein écran fabriqué à partir de
// SV_VertexID, sans buffer de sommets. Les sommets 0, 1, 2 donnent les coordonnées de texture
// (0,0), (2,0), (0,2) : le triangle déborde de l'écran, le rasteriseur ne garde que le carré
// [0,1]². Le coin HAUT-GAUCHE de l'image est uv (0,0) (docs/renderer.md).
struct Output {
    float2 uv : TEXCOORD0;
    float4 position : SV_Position;
};

Output main(uint id : SV_VertexID)
{
    Output output;
    output.uv = float2((id << 1) & 2, id & 2);
    // y de l'écran vers le haut : v = 0 en haut => position.y = 1 - 2v
    output.position = float4(output.uv.x * 2.0f - 1.0f, 1.0f - output.uv.y * 2.0f, 0.0f, 1.0f);
    return output;
}
