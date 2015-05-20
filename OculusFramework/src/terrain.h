#pragma once

#include "Win32_DX11AppUtil.h"

#include "vector.h"
#include "matrix.h"

#include <memory>

struct HeightField {
    struct Color {
        unsigned char R, G, B, A;

        Color(unsigned char r = 0, unsigned char g = 0, unsigned char b = 0, unsigned char a = 0xff)
            : R(r), G(g), B(b), A(a) {}
    };
    struct Vertex {
        mathlib::Vec3f pos;
        mathlib::Vec2f uv;
    };

    mathlib::Vec3f Pos;
    mathlib::Quatf Rot;
    mathlib::Mat4f Mat;

    PipelineStateObjectManager::ResourceHandle pipelineStateObject;

    std::vector<uint16_t> Indices;
    std::vector<std::unique_ptr<DataBuffer>> VertexBuffers;
    std::unique_ptr<DataBuffer> IndexBuffer;
    ID3D11SamplerStatePtr samplerState;
    Texture2DManager::ResourceHandle shapesTex;
    Texture2DManager::ResourceHandle normalsTex;
    ID3D11BufferPtr objectConstantBuffer;

    HeightField(const mathlib::Vec3f& arg_pos) : Pos{arg_pos}, Rot{{0.0f, 0.0f, 0.0f}, 0.0f} {}
    const mathlib::Mat4f& GetMatrix() {
        Mat = Mat4FromQuat(Rot) * Mat4fTranslation(Pos);
        return Mat;
    }

    void AddVertices(ID3D11Device* device, PipelineStateObjectManager& pipelineStateObjectManager,
                     Texture2DManager& texture2DManager);

    void Render(DirectX11& dx11, ID3D11DeviceContext* context, ID3D11Buffer& cameraBuffer);
};
