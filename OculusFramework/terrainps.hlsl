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

Texture2D<uint> Heights : register(t2);
Texture2D Normals : register(t3);
Texture2D Creeks : register(t4);
Texture2D Lakes : register(t5);

// Hacky experiment with raytracing shadow in pixel shader, too slow for use
float terrainShadow(float2 texCoord, float3 lightDir) {
    lightDir = float3(1.0f, 0.99f, 0.0f);
    float texWidth, texHeight;
    Heights.GetDimensions(texWidth, texHeight);
    if (abs(lightDir.x) > abs(lightDir.z)) {
        float3 step = lightDir * (1.0f / lightDir.x) * (terrainParameters.terrainWidthHeightMeters.x / texWidth);
        int2 intCoord = texCoord * int2(texWidth - 1, texHeight - 1);
        float height = Heights.Load(int3(intCoord, 0)).x;
        float3 pos = float3(0.0f, height, 0.0f);
        if (step.x > 0.0) {
            while (intCoord.x < texWidth) {
                pos += step;
                ++intCoord.x;
                height = Heights.Load(int3(intCoord, 0)).x;
                if (height > pos.y) return 0.1f;
                if (pos.y > terrainParameters.minMaxTerrainHeight.y) return 0.9f;
            }
            return 1.0f;
        }
        else {
            return 1.0f;
        }
    }
    else {
        return 1.0f;
    }
}

float4 main(in float4 Position : SV_Position, in float4 Color : COLOR0,
            in float2 TexCoord : TEXCOORD0, in float3 worldPos : TEXCOORD1, in float3 viewDir : TEXCOORD2, in float3 objectPos : TEXCOORD3) : SV_Target
{
    float4 base = float4(0.66, 0.6, 0.6, 1.0);
    float2 creeks = Creeks.Sample(StandardTexture, TexCoord).rg * terrainParameters.arcLayerAlphas.rg;
    float3 lakes = Lakes.Sample(StandardTexture, TexCoord).rgb * terrainParameters.hydroLayerAlphas.rga;
    float4 diffuse = lerp(base, float4(0.45f, 0.55f, 0.78f, 1.0f), creeks.r); // creeks
    diffuse = lerp(diffuse, float4(0.45f, 0.55f, 0.78f, 1.0f), lakes.r); // lake outlines
    diffuse = lerp(diffuse, float4(0.65f, 0.75f, 0.98f, 1.0f), lakes.g); // lake fill
    diffuse = lerp(diffuse, float4(0.4f, 0.1f, 0.5f, 1.0f), lakes.b); // glaciers
    diffuse = lerp(diffuse, float4(0.25f, 0.7f, 0.3f, 1.0f), creeks.g); // roads
    if (terrainParameters.contours > 0) {
        float contour = objectPos.y / 100;
        contour = 0.1 - abs(round(contour) - contour);
        contour = smoothstep(0, 0.1, contour) * abs(1.5 - Position.z);
        diffuse = lerp(diffuse, float4(0.5f, 0.15f, 0.05f, 1.0f), contour);
    }

    float2 normalTex = Normals.Sample(StandardTexture, TexCoord).xy;
    float3 normalFromTex = float3(normalTex.x, sqrt(saturate(1.0f - normalTex.x * normalTex.x - normalTex.y * normalTex.y)), normalTex.y);

    MicrofacetMaterialParams mat = makeMaterial(Color.xyz * diffuse.xyz, .04f, saturate(1.0f - dot(diffuse.xyz, float3(0.33f, 0.33f, 0.33f))));

    float3 n = normalize(mul(object.world, float4(normalFromTex, 0)).xyz);
    float3 v = normalize(viewDir);

    return float4(lightEnv(worldPos, mat, n, v), 1.0f);
};
