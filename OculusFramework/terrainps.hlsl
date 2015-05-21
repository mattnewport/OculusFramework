#include "lighting.hlsli"

Texture2D Texture : register(t0);
Texture2D normals : register(t1);
SamplerState Linear : register(s0);

float4 main(in float4 Position : SV_Position, in float4 Color : COLOR0,
            in float2 TexCoord : TEXCOORD0, in float3 worldPos : TEXCOORD1, in float3 viewDir : TEXCOORD2) : SV_Target
{
    float4 diffuse = Texture.Sample(Linear, TexCoord);

    MicrofacetMaterialParams mat;
    mat.albedo = Color.xyz * diffuse.xyz;
    mat.specColor = float3(0.04f, 0.04f, 0.04f);// Color.xyz * diffuse.xyz;
    mat.gloss = saturate(1.0f - dot(diffuse.xyz, float3(0.33f, 0.33f, 0.33f)));

    float3 n = normalize(normals.Sample(Linear, TexCoord).xzy * 2.0f - 1.0f);
    float3 v = normalize(viewDir);

    return float4(light(worldPos, mat, n, v), 1.0f);
};
