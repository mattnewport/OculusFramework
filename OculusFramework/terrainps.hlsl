#include "lighting.hlsli"

struct TerrainParameters {
    float4 hydroLayerAlphas;
};

cbuffer TerrainConstantBuffer : register(b3) {
    TerrainParameters terrainParameters;
}

Texture2D Texture : register(t2);
Texture2D Normals : register(t3);
Texture2D Creeks : register(t4);
Texture2D Lakes : register(t5);

float4 main(in float4 Position : SV_Position, in float4 Color : COLOR0,
            in float2 TexCoord : TEXCOORD0, in float3 worldPos : TEXCOORD1, in float3 viewDir : TEXCOORD2) : SV_Target
{
    float4 base = float4(0.66, 0.66, 0.66, 1.0);
    float creeks = Creeks.Sample(StandardTexture, TexCoord).r * terrainParameters.hydroLayerAlphas.b;
    float2 lakes = Lakes.Sample(StandardTexture, TexCoord).rg * terrainParameters.hydroLayerAlphas.rg;
    float4 diffuse = lerp(lerp(lerp(base, float4(0.45f, 0.55f, 0.78f, 1.0f), creeks), float4(0.45f, 0.55f, 0.78f, 1.0f), lakes.r), float4(0.65f, 0.75f, 0.98f, 1.0f), lakes.g);

    float2 normalTex = Normals.Sample(StandardTexture, TexCoord).xy;
    float3 normalFromTex = float3(normalTex.x, sqrt(saturate(1.0f - normalTex.x * normalTex.x - normalTex.y * normalTex.y)), normalTex.y);

    MicrofacetMaterialParams mat = makeMaterial(Color.xyz * diffuse.xyz, .04f, saturate(1.0f - dot(diffuse.xyz, float3(0.33f, 0.33f, 0.33f))));

    float3 n = normalize(mul(object.world, float4(normalFromTex, 0)).xyz);
    float3 v = normalize(viewDir);

    return float4(lightEnv(worldPos, mat, n, v), 1.0f);
};
