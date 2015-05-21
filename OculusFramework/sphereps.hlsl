#include "lighting.hlsli"

float4 main(in float4 Position : SV_Position, in float3 worldPos : TEXCOORD0, in float3 normal : TEXCOORD1, in float3 viewDir : TEXCOORD2) : SV_Target
{
    float3 n = normalize(normal);
    float3 v = normalize(viewDir);

    MicrofacetMaterialParams mat;
    mat.albedo = float3(0.5f, 0.5f, 0.5f);
    mat.specColor = float3(0.04f, 0.04f, 0.04f);
    mat.gloss = 0.1f;

    return float4(light(worldPos, mat, n, v), 1.0f);
};
