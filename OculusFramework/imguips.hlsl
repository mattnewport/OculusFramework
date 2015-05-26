#include "commonsamplers.hlsli"

Texture2D Texture : register(t0);

float4 main(in float4 pos : SV_Position, in float2 uv : TEXCOORD0) : SV_TARGET {
    float4 tex = Texture.Sample(Linear, uv);
    return float4(tex.xyz * tex.a, tex.a);
}