#include "lighting.hlsli"

float4 main(in float4 Position : SV_Position, in float3 worldPos : TEXCOORD0, in float3 normal : TEXCOORD1, in float3 viewDir : TEXCOORD2) : SV_Target
{
    float3 n = normalize(normal);
    float3 l = float3(0, 3.7, 0) - worldPos;
    float lmag = length(l);

    PhongMaterialParams mat;
    mat.ka = 0.5f;
    mat.kd = 0.5f;
    mat.ks = 0.5f;
    mat.alpha = 10.0f;

    PhongLightParams light;
    light.ia = 0.2f;
    light.i = 1.0f / lmag*lmag;

    return float4(phong(mat, light, n, l / lmag, normalize(viewDir)), 1.0f);
};
