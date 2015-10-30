#include "commoncbuffers.hlsli"

Texture2D<uint> heightsTex;

void main(in float2 Position : POSITION, in float2 TexCoord : TEXCOORD0,
          out float4 oPosition : SV_Position, out float4 oColor : COLOR0, out float2 oTexCoord : TEXCOORD0,
          out float3 worldPos : TEXCOORD1, out float3 viewDir : TEXCOORD2)
{
    int2 heightsTexSize = int2(0, 0);
    heightsTex.GetDimensions(heightsTexSize.x, heightsTexSize.y);
    int2 heightCoords = TexCoord * (heightsTexSize - int2(1, 1));
    float height = heightsTex.Load(int3(heightCoords, 0));
    float4 pos = float4(Position.x, height, Position.y, 1.0f);
    float4 wp = mul(object.world, pos);
    oPosition = mul(camera.proj, mul(camera.view, wp));
    oTexCoord = TexCoord;
    oColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
    worldPos = wp.xyz;
    viewDir = normalize(camera.eye - worldPos);
}
