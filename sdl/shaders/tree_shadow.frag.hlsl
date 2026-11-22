// Passe d'ombre des arbres réalistes (engine/tree_render.h) : seule la profondeur est écrite ; une
// partie DÉCOUPÉE (feuillage) n'écrit rien là où sa texture est transparente : le soleil passe
// entre les feuilles. Et un feuillage n'arrête pas toute la lumière : seule une part `densité` de
// ses texels d'ombre est écrite (tramage fixe par texel de la carte d'ombre) ; le filtrage de
// l'ombre (PCF) en fait une pénombre, le sous-bois et le cœur du houppier gardent un peu de soleil.
Texture2D<float4> base_color_map : register(t0, space2);
SamplerState base_color_sampler : register(s0, space2);

cbuffer Cut : register(b0, space3)
{
    float4 cut; // x : seuil de découpe de la partie (0 : opaque), y : densité de l'ombre du feuillage
};

// Bruit à gradient entrelacé (Jimenez, 2014) du texel, dans [0, 1[ : eng_tree_dither.
float dither(float2 pixel)
{
    return frac(52.9829189f * frac(dot(pixel, float2(0.06711056f, 0.00583715f))));
}

void main(float2 uv : TEXCOORD0, float4 pixel : SV_Position)
{
    if (cut.x > 0.0f) {
        clip(base_color_map.Sample(base_color_sampler, uv).a - cut.x);
        clip(cut.y - dither(pixel.xy)); // gardé si bruit < densité
    }
}
