SamplerState envMapSampler : register(s0);
TextureCube pmremEnvMap : register(t0);
TextureCube irradEnvMap : register(t1);

struct MicrofacetMaterialParams {
    float3 albedo;
    float3 specColor;
    float gloss;
    float specPower;
};

// metalicity below 0.2f used for monochrome spec color for non metals, use 1.0f for metals
MicrofacetMaterialParams makeMaterial(float3 color, float metalicity, float gloss) {
    MicrofacetMaterialParams ret;
    float metal = metalicity < 0.2f ? 0.0f : 1.0f;
    ret.albedo = color * (1.0f - metal);
    ret.specColor = lerp(float3(metalicity, metalicity, metalicity), color, metal);
    ret.gloss = gloss;
    ret.specPower = exp2(10 * gloss + 1);
    return ret;
}

float3 fresnelSchlick(float3 specColor, float3 e, float3 h) {
    return specColor + (1.0f - specColor) * pow(1.0f - saturate(dot(e, h)), 5.0f);
}

// http://renderwonk.com/publications/s2010-shading-course/hoffman/s2010_physically_based_shading_hoffman_b.pdf
float3 microfacet(MicrofacetMaterialParams mat, float3 lightColor, float3 n, float3 l, float3 h) {
    float specPower = exp2(10 * mat.gloss + 1);
    float3 specTerm = fresnelSchlick(mat.specColor, l, h) * ((specPower + 2.0f) / 8.0f) * pow(saturate(dot(n, h)), specPower);
    float nDotL = saturate(dot(n, l));
    return (mat.albedo + specTerm) * nDotL * lightColor;
}

static const float3 lightPos = float3(0, 3.7, -2);
static const float3 lightColor = float3(1.0f, 1.0f, 1.0f) * 16.0f;
static const float3 lightAmbient = lightColor * 0.01f;

float3 light(float3 worldPos, MicrofacetMaterialParams mat, float3 n, float3 v) {
    float3 ld = lightPos - worldPos;
    float lmag = length(ld);
    float3 l = ld / lmag;
    float3 h = normalize(l + v);
    float3 lightIntensity = lightColor / lmag*lmag;
    //return microfacet(mat, lightIntensity, n, l, h);
    return lightAmbient + microfacet(mat, lightIntensity, n, l, h);
}

float3 lightEnv(float3 worldPos, MicrofacetMaterialParams mat, float3 n, float3 v) {
    float3 ld = lightPos - worldPos;
    float lmag = length(ld);
    float3 l = ld / lmag;
    float3 h = normalize(l + v);
    float3 lightIntensity = lightColor / lmag*lmag;
    float3 r = 2.0f * dot(v, n) * n - v;
    float targetLod = -0.5f * log2(mat.specPower) + 5.5;
    float sampleLod = max(pmremEnvMap.CalculateLevelOfDetail(envMapSampler, r), targetLod);
    float3 env = fresnelSchlick(mat.specColor, r, n) * ((mat.specPower + 2.0f) / 8.0f) * pmremEnvMap.SampleLevel(envMapSampler, r, sampleLod).xyz;
    float3 amb = irradEnvMap.Sample(envMapSampler, n).xyz * mat.albedo;
    return amb + env + microfacet(mat, lightIntensity, n, l, h);
}
