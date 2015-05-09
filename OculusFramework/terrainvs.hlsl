float4x4 Proj, View, World;
void main(in float4 Position : POSITION, in float2 TexCoord : TEXCOORD0,
          out float4 oPosition : SV_Position, out float4 oColor : COLOR0, out float2 oTexCoord : TEXCOORD0,
          out float3 worldPos : TEXCOORD1)
{
    float4 wp = mul(World, Position);
    oPosition = mul(Proj, mul(View, wp));
    oTexCoord = TexCoord;
    oColor = float4(0.33, 0.33, 0.33, 1.0);
    worldPos = wp;
}
