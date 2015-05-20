struct Camera {
    float4x4 proj;
    float4x4 view;
    float3 eye;
};

struct Object {
    float4x4 world;
};
