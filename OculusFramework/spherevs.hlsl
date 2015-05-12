float4x4 Proj, View, World;
float3 eye;
void main(in float4 Position : POSITION, out float4 oPosition : SV_Position, out float3 worldPos : TEXCOORD0, out float3 normal : TEXCOORD1, out float3 viewDir : TEXCOORD2)
{
    float4 wp = mul(World, Position);
    oPosition = mul(Proj, mul(View, wp));
    worldPos = float3(wp.xyz);
    normal = normalize(Position);
    viewDir = normalize(eye - worldPos);
}
