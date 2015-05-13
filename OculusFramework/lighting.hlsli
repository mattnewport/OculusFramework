struct PhongMaterialParams {
    float3 ka;
    float3 kd;
    float3 ks;
    float alpha; // spec power
};

struct PhongLightParams {
    float3 ia; // ambient intensity
    float3 i; // diffuse / spec intensity
};

float3 phong(PhongMaterialParams mat, PhongLightParams light, float3 n, float3 l, float3 v) {
    float d = saturate(dot(n, l));
    float3 r = 2 * d * n - l;
    float s = pow(saturate(dot(r, v)), mat.alpha);
    return mat.ka * light.ia + (mat.kd * d + mat.ks * s) * light.i;
}
