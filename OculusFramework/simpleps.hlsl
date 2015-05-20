#include "lighting.hlsli"

Texture2D Texture : register(t0);
SamplerState Linear : register(s0);

float4 main(in float4 Position : SV_Position, in float4 Color : COLOR0, 
            in float2 TexCoord : TEXCOORD0, in float3 worldPos : TEXCOORD1, in float3 viewDir : TEXCOORD2) : SV_Target
{
    float3 tan = ddx(worldPos);
    float3 bin = ddy(worldPos);
    float3 n = normalize(cross(bin, tan));
    float3 l = lightPos - worldPos;
    float lmag = length(l);

    PhongMaterialParams mat;
    float3 color = Color.xyz * Texture.Sample(Linear, TexCoord).xyz;
    mat.ka = color;
    mat.kd = color;
    mat.ks = color;
    mat.alpha = 10.0f;

    PhongLightParams light;
    light.ia = 0.2f;
    light.i = 1.0f / lmag*lmag;

    return float4(phong(mat, light, n, l / lmag, normalize(viewDir)), 1.0f);
};
