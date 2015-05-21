struct MicrofacetMaterialParams {
    float3 albedo;
    float3 specColor;
    float gloss;
};

float3 fresnelSchlick(float3 specColor, float3 e, float3 h) {
    return specColor + (1.0f - specColor) * pow(1.0f - saturate(dot(e, h)), 5.0f);
}

// http://renderwonk.com/publications/s2010-shading-course/hoffman/s2010_physically_based_shading_hoffman_b.pdf
float3 microfacet(MicrofacetMaterialParams mat, float3 lightColor, float3 n, float3 l, float3 h) {
    float specPower = exp2(10 * mat.gloss + 1);
    float3 specTerm = fresnelSchlick(mat.specColor, l, h) * ((specPower + 2.0f) / 8.0f) * pow(saturate(dot(n, h)), specPower);
    float nDotL = saturate(dot(n, l));
    //return fresnelSchlick(mat.specColor, l, h);
    //return specTerm * nDotL;
    return (mat.albedo + specTerm) * nDotL * lightColor;
}

static const float3 lightPos = float3(0, 3.7, -2);
static const float3 lightColor = float3(1.0f, 1.0f, 1.0f);
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
