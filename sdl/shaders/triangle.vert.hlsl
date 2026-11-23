// Sommet : position (clip space) et couleur par sommet, passée telle quelle au fragment.
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
    output.position = float4(input.position, 1.0f);
    return output;
}
