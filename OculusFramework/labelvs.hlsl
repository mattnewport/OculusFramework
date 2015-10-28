#include "commoncbuffers.hlsli"

void main(in float4 Position : POSITION, in float4 Color : COLOR0, in float2 TexCoord0 : TEXCOORD0, in float2 TexCoord1 : TEXCOORD1,
          out float4 oPosition : SV_Position, out float4 oColor : COLOR0, out float2 oTexCoord : TEXCOORD0,
          out float3 worldPos : TEXCOORD1, out float3 eyePos : TEXCOORD2)
{
    float4 wp = mul(object.world, Position);
    TexCoord1 *= 0.0001f;
    wp.y += TexCoord1.y;
    float3 camForward = camera.view[2].xyz;
    float3 worldRight = normalize(cross(-camForward, float3(0, 1, 0)));
    wp.xz += worldRight.xz * TexCoord1.x;
    float4 vp = mul(camera.view, wp);
    oPosition = mul(camera.proj, vp);
    oTexCoord = TexCoord0;
    oColor = Color;
    worldPos = float3(wp.xyz);
    eyePos = camera.eye;
}
