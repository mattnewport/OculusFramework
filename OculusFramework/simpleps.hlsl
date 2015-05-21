#include "lighting.hlsli"

Texture2D Texture : register(t0);
SamplerState Linear : register(s0);

float4 main(in float4 Position : SV_Position, in float4 Color : COLOR0, 
            in float2 TexCoord : TEXCOORD0, in float3 worldPos : TEXCOORD1, in float3 eyePos : TEXCOORD2) : SV_Target
{
    float3 tan = ddx(worldPos);
    float3 bin = ddy(worldPos);
    float3 n = normalize(cross(bin, tan));
    float3 v = normalize(eyePos - worldPos);

    MicrofacetMaterialParams mat;
    mat.albedo = Color.xyz * Texture.Sample(Linear, TexCoord).xyz;
    mat.specColor = float3(0.04f, 0.04f, 0.04f);
    mat.gloss = saturate(dot(mat.albedo, float3(0.66f, 0.66f, 0.66f)));

    return float4(light(worldPos, mat, n, v), 1.0f);
};
