Texture2D Texture : register(t0);
SamplerState Linear : register(s0);

float4 main(in float4 pos : SV_Position, in float2 uv : TEXCOORD0) : SV_TARGET {
    return float4(Texture.Sample(Linear, uv).xyz, 1.0f);
}