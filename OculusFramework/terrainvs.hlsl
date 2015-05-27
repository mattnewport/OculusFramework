#include "commoncbuffers.hlsli"

Texture2D<uint> heightsTex;

void main(in float4 Position : POSITION, in float3 Normal : NORMAL, in float2 TexCoord : TEXCOORD0,
          out float4 oPosition : SV_Position, out float4 oColor : COLOR0, out float2 oTexCoord : TEXCOORD0,
          out float3 worldPos : TEXCOORD1, out float3 viewDir : TEXCOORD2, out float3 oNormal : TEXCOORD3)
{
    int2 heightsTexSize = int2(0, 0);
    heightsTex.GetDimensions(heightsTexSize.x, heightsTexSize.y);
    int2 heightCoords = TexCoord * (heightsTexSize - int2(1, 1));
    heightCoords.y = heightsTexSize.y - 1 - heightCoords.y;
    float height = heightsTex.Load(int3(heightCoords, 0));
    Position.y = height * (1.0f / 10000.0f);
    float4 wp = mul(object.world, Position);
    oPosition = mul(camera.proj, mul(camera.view, wp));
    oTexCoord = TexCoord;
    oColor = float4(0.75f, 0.75f, 0.75f, 1.0f);
    worldPos = wp.xyz;
    viewDir = normalize(camera.eye - worldPos);
    oNormal = Normal;
}
