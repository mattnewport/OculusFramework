float4x4 Proj, View;
void main(in float4 Position : POSITION, in float4 Color : COLOR0, in float2 TexCoord : TEXCOORD0, 
          out float4 oPosition : SV_Position, out float4 oColor : COLOR0, out float2 oTexCoord : TEXCOORD0,
          out float3 worldPos : TEXCOORD1)
{
    oPosition = mul(Proj, mul(View, Position));
    oTexCoord = TexCoord; oColor = Color;
    worldPos = Position;
}
