float4 main(in float4 Position : SV_Position, in float3 worldPos : TEXCOORD0, in float3 normal : TEXCOORD1, in float3 viewDir : TEXCOORD2) : SV_Target
{
    float3 n = normalize(normal);
    float3 l = float3(0, 3.7, 0) - worldPos;
    float r = length(l);
    float3 ln = l / r;
    float d = saturate(dot(n, ln));
    float ka = 0.2f;
    float kd = 0.5f;
    float i = 1.0f / r*r;
    float ks = 0.5;
    float spec = 10.0f;
    float3 rm = 2 * dot(ln, n) * n - ln;
    float s = pow(saturate(dot(rm, normalize(viewDir))) * sign(d), spec);
    return ka + kd * d * i + ks * s * i;
};
