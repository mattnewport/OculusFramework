Texture2D Texture : register(t0);
Texture2D Luminance : register(t1);
SamplerState Linear : register(s0);

// hejl tonemap with different scale factor
float4 main(in float4 pos : SV_Position, in float2 uv : TEXCOORD0) : SV_TARGET {
    float avgLuminance = exp(Luminance.Load(int3(0, 0, 0)).x);
    float lumScale = (0.5f / 8.0f) / avgLuminance;
    float4 texColor = float4(Texture.Sample(Linear, uv).xyz, 1.0f) * lumScale;
    float3 x = max(0, texColor.xyz - 0.004);
    float3 retColor = (x*(6.2*x + .5)) / (x*(6.2*x + 1.7) + 0.06);
    return float4(retColor, 1);
}