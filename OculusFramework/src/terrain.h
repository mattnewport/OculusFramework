#pragma once

#include "Win32_DX11AppUtil.h"

#include <memory>

struct HeightField {
    struct Color {
        unsigned char R, G, B, A;

        Color(unsigned char r = 0, unsigned char g = 0, unsigned char b = 0, unsigned char a = 0xff)
            : R(r), G(g), B(b), A(a) {}
    };
    struct Vertex {
        Vector3f Pos;
        float u, v;
    };

    Vector3f Pos;
    Quatf Rot;
    Matrix4f Mat;
    std::vector<uint16_t> Indices;
    std::vector<std::unique_ptr<DataBuffer>> VertexBuffers;
    std::unique_ptr<DataBuffer> IndexBuffer;
    ID3D11ShaderResourceViewPtr shapesSRV;
    ID3D11SamplerStatePtr samplerState;
    ID3D11RasterizerStatePtr Rasterizer;

    HeightField(Vector3f arg_pos, ID3D11Device* device) : Pos{ arg_pos } {}
    Matrix4f& GetMatrix() {
        Mat = Matrix4f(Rot);
        Mat = Matrix4f::Translation(Pos) * Mat;
        return Mat;
    }

    void AddVertices(ID3D11Device* device);

    void Render(ID3D11DeviceContext* context, ShaderDatabase& shaderDatabase,
        DataBuffer* uniformBuffer);
};
