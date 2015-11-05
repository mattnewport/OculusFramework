#include "lighting.hlsli"

struct TerrainParameters {
    float2 minMaxTerrainHeight;
    float2 terrainWidthHeightMeters;
    float4 arcLayerAlphas;
    float4 hydroLayerAlphas;
    float contours;
};

cbuffer TerrainConstantBuffer : register(b3) {
    TerrainParameters terrainParameters;
}

float4 main(in float4 Position : SV_Position, in float4 Color : COLOR0,
            in float2 TexCoord : TEXCOORD0, in float3 worldPos : TEXCOORD1, in float3 viewDir : TEXCOORD2, in float3 objectPos : TEXCOORD3) : SV_Target
{
    return float4(0.1f, 0.1f, 0.1f, 1.0f);
};
