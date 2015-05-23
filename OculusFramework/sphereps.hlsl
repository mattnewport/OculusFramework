#include "lighting.hlsli"

float4 main(in float4 Position : SV_Position, in float3 worldPos : TEXCOORD0, in float3 normal : TEXCOORD1, in float3 viewDir : TEXCOORD2) : SV_Target
{
    float3 n = normalize(normal);
    float3 v = normalize(viewDir);

    MicrofacetMaterialParams mat = makeMaterial(float3(0.95f, 0.64f, 0.54f), 1.0f, 1.f);

    return float4(lightEnv(worldPos, mat, n, v), 1.0f);
};
