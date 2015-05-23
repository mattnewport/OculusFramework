Texture2D Texture : register(t0);
SamplerState Linear : register(s0);

float logLuminance(float3 x) {
    float delta = 1e-6f;
    return log(delta + 0.27 * x.x + 0.67 * x.y + 0.06 * x.z);
}

float main(in float4 pos : SV_Position, in float2 uv : TEXCOORD0) : SV_TARGET {
    float4 a = Texture.Sample(Linear, uv);
    return logLuminance(a.xyz);
}