#include "commoncbuffers.hlsli"

void main(in float4 Position
          : POSITION, out float4 oPosition
          : SV_Position, out float3 worldPos
          : TEXCOORD0, out float3 normal
          : TEXCOORD1, out float3 viewDir
          : TEXCOORD2) {
    float4 wp = mul(object.world, Position);
    oPosition = mul(camera.proj, mul(camera.view, wp));
    worldPos = float3(wp.xyz);
    normal = normalize(Position.xyz);
    viewDir = normalize(camera.eye - worldPos);
}
