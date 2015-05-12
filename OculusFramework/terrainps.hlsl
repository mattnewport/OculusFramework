Texture2D Texture : register(t0);
Texture2D normals : register(t1);
SamplerState Linear : register(s0);

float4 main(in float4 Position : SV_Position, in float4 Color : COLOR0,
            in float2 TexCoord : TEXCOORD0, in float3 worldPos : TEXCOORD1, in float3 viewDir : TEXCOORD2) : SV_Target
{
    //float3 tan = ddx(worldPos);
    //float3 bin = ddy(worldPos);
    //float3 n = normalize(cross(bin, tan));

    float3 n = normals.Sample(Linear, TexCoord).xzy * 2.0f - 1.0f;
    n = normalize(n);
    //return float4(n.x, n.y, n.z, 1.0f);

    float3 l = float3(0, 3.7, 0) - worldPos;
    float r = length(l);
    float3 ln = l / r;
    float d = saturate(dot(n, ln));
    float ka = 0.2f;
    float kd = 1.0f;
    float i = 1.0f / r*r;
    float ks = 0.75f;
    float spec = 30.0f;
    float3 rm = 2 * dot(ln, n) * n - ln;
    float s = pow(saturate(dot(rm, normalize(viewDir))) * sign(d), spec);
    //return float4(s.xxx, 1);
    return (ka + kd * d * i + ks * s * i) * Color * Texture.Sample(Linear, TexCoord);
};
