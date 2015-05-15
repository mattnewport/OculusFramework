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
    OVR::Quatf Rot;
    mathlib::Mat4f Mat;

    PipelineStateObjectManager::ResourceHandle pipelineStateObject;

    std::vector<uint16_t> Indices;
    std::vector<std::unique_ptr<DataBuffer>> VertexBuffers;
    std::unique_ptr<DataBuffer> IndexBuffer;
    ID3D11SamplerStatePtr samplerState;
    Texture2DManager::ResourceHandle shapesTex;
    Texture2DManager::ResourceHandle normalsTex;

    HeightField(const mathlib::Vec3f& arg_pos) : Pos{arg_pos} {}
    const mathlib::Mat4f& GetMatrix() {
        auto matTemp = OVR::Matrix4f(Rot);
        matTemp.Transpose();
        memcpy(&Mat, &matTemp, sizeof(Mat));
        Mat = Mat * Mat4fTranslation(Pos);
        return Mat;
    }

    void AddVertices(ID3D11Device* device, PipelineStateObjectManager& pipelineStateObjectManager,
                     Texture2DManager& texture2DManager);

    void Render(ID3D11DeviceContext* context, const mathlib::Vec3f& eye, const mathlib::Mat4f& view,
                const mathlib::Mat4f& proj, DataBuffer* uniformBuffer);
};
