#pragma once

#include "d3dhelper.h"
#include "matrix.h"
#include "sphere.h"
#include "terrain.h"
#include "Win32_DX11AppUtil.h"

#include "hlslmacros.h"
#include "../commonstructs.hlsli"

#include <memory>
#include <string>

struct Texture {
    ID3D11Texture2DPtr Tex;
    ID3D11ShaderResourceViewPtr TexSv;
    std::string name;

    Texture(const char* name, ID3D11Device* device, ID3D11DeviceContext* deviceContext,
            ovrSizei size, int mipLevels = 1, unsigned char* data = NULL);
};

struct ShaderFill {
    std::unique_ptr<Texture> OneTexture;

    ShaderFill(std::unique_ptr<Texture>&& t);
};

struct Model {
    struct Color {
        unsigned char R, G, B, A;

        Color(unsigned char r = 0, unsigned char g = 0, unsigned char b = 0, unsigned char a = 0xff)
            : R(r), G(g), B(b), A(a) {}
    };
    struct Vertex {
        mathlib::Vec3f position;
        Color color;
        mathlib::Vec2f texcoord;
    };

    mathlib::Vec3f Pos;
    mathlib::Quatf Rot;
    mathlib::Mat4f Mat;

    std::vector<Vertex> Vertices;
    std::vector<uint16_t> Indices;
    std::unique_ptr<ShaderFill> Fill;
    ID3D11BufferPtr VertexBuffer;
    ID3D11BufferPtr IndexBuffer;
    ID3D11BufferPtr objectConstantBuffer;

    Model(mathlib::Vec3f pos_, std::unique_ptr<ShaderFill>&& arg_Fill)
        : Pos{pos_}, Rot{{0.0f, 0.0f, 0.0f}, 0.0f}, Fill{std::move(arg_Fill)} {}
    const mathlib::Mat4f& GetMatrix() {
        Mat = Mat4FromQuat(Rot) * Mat4fTranslation(Pos);
        return Mat;
    }
    void AddVertex(const Vertex& v) { Vertices.push_back(v); }
    void AddIndex(uint16_t a) { Indices.push_back(a); }

    void AllocateBuffers(ID3D11Device* device);

    void AddSolidColorBox(float x1, float y1, float z1, float x2, float y2, float z2, Color c);
};

struct Scene {
    std::vector<std::unique_ptr<Model>> Models;
    std::unique_ptr<HeightField> heightField;
    std::unique_ptr<Sphere> sphere;

    void Add(std::unique_ptr<Model>&& n) { Models.emplace_back(move(n)); }

    Scene(ID3D11Device* device, ID3D11DeviceContext* deviceContext,
          PipelineStateObjectManager& pipelineStateObjectManager,
          Texture2DManager& texture2DManager);

    void showGui();

    void Render(DirectX11& dx11, const mathlib::Vec3f& eye, const mathlib::Mat4f& view,
                const mathlib::Mat4f& proj);
    void Render(ID3D11DeviceContext* context, ShaderFill* fill, ID3D11Buffer* vertices,
                ID3D11Buffer* indices, UINT stride, int count, ID3D11Buffer& objectConstantBuffer);

    PipelineStateObjectManager::ResourceHandle pipelineStateObject;

    ID3D11BufferPtr cameraConstantBuffer;
    ID3D11ResourcePtr pmremEnvMapTex;
    ID3D11ShaderResourceViewPtr pmremEnvMapSRV;
    ID3D11ResourcePtr irradEnvMapTex;
    ID3D11ShaderResourceViewPtr irradEnvMapSRV;
    ID3D11SamplerStatePtr linearSampler;
    ID3D11SamplerStatePtr standardTextureSampler;

    Lighting lighting;
    ID3D11BufferPtr lightingConstantBuffer;
};
