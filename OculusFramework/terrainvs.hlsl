#include "commoncbuffers.hlsli"

void main(in float4 Position : POSITION, in float3 Normal : NORMAL, in float2 TexCoord : TEXCOORD0,
          out float4 oPosition : SV_Position, out float4 oColor : COLOR0, out float2 oTexCoord : TEXCOORD0,
          out float3 worldPos : TEXCOORD1, out float3 viewDir : TEXCOORD2, out float3 oNormal : TEXCOORD3)
{
    float4 wp = mul(object.world, Position);
    oPosition = mul(camera.proj, mul(camera.view, wp));
    oTexCoord = TexCoord;
    oColor = float4(0.75f, 0.75f, 0.75f, 1.0f);
    worldPos = wp.xyz;
    viewDir = normalize(camera.eye - worldPos);
    oNormal = Normal;
}
