Texture2D Texture : register(t0);
Texture2D normals : register(t1);
SamplerState Linear : register(s0);

float4 main(in float4 Position : SV_Position, in float4 Color : COLOR0,
            in float2 TexCoord : TEXCOORD0, in float3 worldPos : TEXCOORD1) : SV_Target
{
    float3 n = normals.Sample(Linear, TexCoord).xzy * 2.0f - 1.0f;
    n.x = -n.x;
    float3 l = float3(0, 3.7, 0) - worldPos;
    float r = length(l);
    float d = dot(n, l / r);
    return Color * (0.5 + 10 * d / r) * Texture.Sample(Linear, TexCoord) * 0.66f;
};
