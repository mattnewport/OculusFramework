#include "commoncbuffers.hlsli"

void main(in float4 Position : POSITION,
          out float4 oPosition : SV_Position, out float4 oColor : COLOR0)
{
    float4 wp = mul(object.world, Position);
    oPosition = mul(camera.proj, mul(camera.view, wp));
    oColor = float4(0.1f, 0.9f, 0.1f, 1.0f);
}
