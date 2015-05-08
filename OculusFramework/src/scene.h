#pragma once

#include "Win32_DX11AppUtil.h"

#include <memory>
#include <string>

struct DataBuffer {
    ID3D11BufferPtr D3DBuffer;
    size_t Size;

    DataBuffer(ID3D11Device* device, D3D11_BIND_FLAG use, const void* buffer, size_t size);

    void Refresh(ID3D11DeviceContext* deviceContext, const void* buffer, size_t size);
};

struct Texture {
    ID3D11Texture2DPtr Tex;
    ID3D11ShaderResourceViewPtr TexSv;
    std::string name;

    Texture(const char* name, ID3D11Device* device, ID3D11DeviceContext* deviceContext, Sizei size,
        int mipLevels = 1, unsigned char* data = NULL);
};

struct ShaderFill {
    std::unique_ptr<Texture> OneTexture;
    ID3D11SamplerStatePtr SamplerState;

    ShaderFill(ID3D11Device* device, std::unique_ptr<Texture>&& t, bool wrap = 1);
};

struct Model {
    struct Color {
        unsigned char R, G, B, A;

        Color(unsigned char r = 0, unsigned char g = 0, unsigned char b = 0, unsigned char a = 0xff)
            : R(r), G(g), B(b), A(a) {}
    };
    struct Vertex {
        Vector3f Pos;
        Color C;
        float U, V;
    };

    Vector3f Pos;
    Quatf Rot;
    Matrix4f Mat;
    std::vector<Vertex> Vertices;
    std::vector<uint32_t> Indices;
    std::unique_ptr<ShaderFill> Fill;
    std::unique_ptr<DataBuffer> VertexBuffer;
    std::unique_ptr<DataBuffer> IndexBuffer;

    Model(Vector3f arg_pos, std::unique_ptr<ShaderFill>&& arg_Fill)
        : Pos{arg_pos}, Fill{std::move(arg_Fill)} {}
    Matrix4f& GetMatrix() {
        Mat = Matrix4f(Rot);
        Mat = Matrix4f::Translation(Pos) * Mat;
        return Mat;
    }
    void AddVertex(const Vertex& v) { Vertices.push_back(v); }
    void AddIndex(uint32_t a) { Indices.push_back(a); }

    void AllocateBuffers(ID3D11Device* device);

    void AddSolidColorBox(float x1, float y1, float z1, float x2, float y2, float z2,
                                 Color c);
};

struct HeightField {
    struct Color {
        unsigned char R, G, B, A;

        Color(unsigned char r = 0, unsigned char g = 0, unsigned char b = 0, unsigned char a = 0xff)
            : R(r), G(g), B(b), A(a) {}
    };
    struct Vertex {
        Vector3f Pos;
    };

    Vector3f Pos;
    Quatf Rot;
    Matrix4f Mat;
    std::vector<std::vector<Vertex>> Vertices;
    std::vector<uint32_t> Indices;
    std::vector<std::unique_ptr<DataBuffer>> VertexBuffers;
    std::unique_ptr<DataBuffer> IndexBuffer;

    HeightField(Vector3f arg_pos, ID3D11Device* device)
        : Pos{ arg_pos } {}
    Matrix4f& GetMatrix() {
        Mat = Matrix4f(Rot);
        Mat = Matrix4f::Translation(Pos) * Mat;
        return Mat;
    }

    void AddVertices(ID3D11Device* device);

    void Render(ID3D11DeviceContext* context, ShaderDatabase& shaderDatabase, DataBuffer* uniformBuffer);
};

struct Scene {
    std::vector<std::unique_ptr<Model>> Models;
    std::unique_ptr<HeightField> heightField;

    void Add(std::unique_ptr<Model>&& n) { Models.emplace_back(move(n)); }

    Scene(ID3D11Device* device, ID3D11DeviceContext* deviceContext, int reducedVersion);

    void Render(DirectX11& dx11, Matrix4f view, Matrix4f proj);
    void Render(ID3D11DeviceContext* context, ShaderDatabase& shaderDatabase, ShaderFill* fill,
                DataBuffer* vertices, DataBuffer* indices, UINT stride, int count);

    std::unique_ptr<DataBuffer> UniformBufferGen;
};
