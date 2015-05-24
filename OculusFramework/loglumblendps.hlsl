Texture2D Texture : register(t0);
Texture2D PrevFrame : register(t1);
SamplerState Linear : register(s0);

float main(in float4 pos : SV_Position, in float2 uv : TEXCOORD0) : SV_TARGET{
    int3 texPos = int3(pos.x, pos.y, 0) * 2;
    float a = Texture.Load(texPos).x;
    float b = Texture.Load(texPos + int3(1, 0, 0)).x;
    float c = Texture.Load(texPos + int3(1, 1, 0)).x;
    float d = Texture.Load(texPos + int3(0, 1, 0)).x;
    float avgLogLum = (a + b + c + d) / 4;
    return lerp(PrevFrame.Load(int3(0, 0, 0)).x, avgLogLum, 0.01f);
}