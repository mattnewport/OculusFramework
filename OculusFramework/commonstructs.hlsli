#ifndef COMMONSTRUCTS_HLSLI_INCLUDED
#define COMMONSTRUCTS_HLSLI_INCLUDED

struct Camera {
    float4x4 proj;
    float4x4 view;
    float3 eye;
};

struct Object {
    float4x4 world;
};

struct Lighting {
    float4 lightPos;// = float3(0, 3.7, -2);
    float4 lightColor;// = float3(1.0f, 1.0f, 1.0f) * 2.0f;
    float4 lightAmbient;// = lightColor * 0.01f;
};

#endif // COMMONSTRUCTS_HLSLI_INCLUDED
