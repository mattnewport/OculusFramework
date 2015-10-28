#include "lighting.hlsli"

Texture2D Texture : register(t2);

float4 main(in float4 Position : SV_Position, in float4 Color : COLOR0) : SV_Target
{
    return Color;
};
