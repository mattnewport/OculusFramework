#include "commoncbuffers.hlsli"

void main(in float4 Position : POSITION, in float4 Color : COLOR0, in float2 TexCoord : TEXCOORD0,
          out float4 oPosition : SV_Position, out float4 oColor : COLOR0, out float2 oTexCoord : TEXCOORD0,
          out float3 worldPos : TEXCOORD1)
{
    float4 wp = mul(object.world, Position);
    oPosition = mul(camera.proj, mul(camera.view, wp));
    oTexCoord = TexCoord;
    oColor = Color;
    worldPos = float3(wp.xyz);
}
