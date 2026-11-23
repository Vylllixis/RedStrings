// Fragment de TEST du renderer (M1-15) : lit les cinq emplacements de texture d'un dessin
// (space2, t0..t4 avec leurs samplers s0..s4 : SDL_GPU lie des paires texture + sampler, le
// renderer met le même sampler sur chaque emplacement) et écrit le canal rouge de chacun dans une
// composante distincte, pour que les tests vérifient quelle texture est liée à quel emplacement.
//   R = t0, G = t1, B = t2, A = 0,25 * t3 + 0,75 * t4
Texture2D<float4> tex0 : register(t0, space2);
Texture2D<float4> tex1 : register(t1, space2);
Texture2D<float4> tex2 : register(t2, space2);
Texture2D<float4> tex3 : register(t3, space2);
Texture2D<float4> tex4 : register(t4, space2);
SamplerState smp0 : register(s0, space2);
SamplerState smp1 : register(s1, space2);
SamplerState smp2 : register(s2, space2);
SamplerState smp3 : register(s3, space2);
SamplerState smp4 : register(s4, space2);

float4 main(float4 tint : TEXCOORD0, float2 uv : TEXCOORD1) : SV_Target0
{
    return float4(tex0.Sample(smp0, uv).r, tex1.Sample(smp1, uv).r, tex2.Sample(smp2, uv).r,
                  0.25f * tex3.Sample(smp3, uv).r + 0.75f * tex4.Sample(smp4, uv).r);
}
