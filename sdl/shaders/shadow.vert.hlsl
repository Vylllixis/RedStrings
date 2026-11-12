// Passe d'ombre (M1-17) : ne lit que la POSITION (emplacement 0) et la matrice monde -> découpage de
// la caméra du soleil, premier membre du bloc d'uniforms. Les modèles (ModelUniforms) et les
// pipelines de géométrie simple l'envoient en tête de leur bloc : les membres suivants sont ignorés.
cbuffer Uniforms : register(b0, space1)
{
    float4x4 light_mvp;
};

float4 main(float3 position : TEXCOORD0) : SV_Position
{
    return mul(light_mvp, float4(position, 1.0f));
}
