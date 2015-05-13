#include "lighting.hlsli"

Texture2D Texture : register(t0);
Texture2D normals : register(t1);
SamplerState Linear : register(s0);

float4 main(in float4 Position : SV_Position, in float4 Color : COLOR0,
            in float2 TexCoord : TEXCOORD0, in float3 worldPos : TEXCOORD1, in float3 viewDir : TEXCOORD2) : SV_Target
{
    float4 diffuse = Texture.Sample(Linear, TexCoord);

    PhongMaterialParams mat;
    mat.ka = Color.xyz * diffuse.xyz;
    mat.kd = Color.xyz * diffuse.xyz;
    mat.ks = Color.xyz * diffuse.xyz;
    mat.alpha = 30.0f;

    float3 n = normalize(normals.Sample(Linear, TexCoord).xzy * 2.0f - 1.0f);
    float3 l = lightPos - worldPos;
    float lmag = length(l);
    float3 v = normalize(viewDir);

    PhongLightParams light;
    light.ia = 0.2f;
    light.i = 1.0f / lmag*lmag;

    return float4(phong(mat, light, n, l / lmag, v), 1.0f);
};
