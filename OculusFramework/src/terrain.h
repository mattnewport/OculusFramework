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
    struct Vertex;

    mathlib::Vec3f Pos;
    mathlib::Quatf Rot;
    mathlib::Mat4f Mat;

    PipelineStateObjectManager::ResourceHandle pipelineStateObject;

    std::vector<uint16_t> Indices;
    std::vector<ID3D11BufferPtr> VertexBuffers;
    ID3D11BufferPtr IndexBuffer;
    Texture2DManager::ResourceHandle shapesTex;
    ID3D11BufferPtr objectConstantBuffer;
    ID3D11Texture2DPtr heightsTex;
    ID3D11ShaderResourceViewPtr heightsSRV;
    ID3D11Texture2DPtr normalsTex;
    ID3D11ShaderResourceViewPtr normalsSRV;
    float scale = 1e-4f;

    HeightField(const mathlib::Vec3f& arg_pos) : Pos{arg_pos}, Rot{{0.0f, 0.0f, 0.0f}, 0.0f} {}
    const mathlib::Mat4f& GetMatrix() {
        Mat = mathlib::Mat4fScale(scale) * Mat4FromQuat(Rot) * Mat4fTranslation(Pos);
        return Mat;
    }

    void AddVertices(ID3D11Device* device, PipelineStateObjectManager& pipelineStateObjectManager,
                     Texture2DManager& texture2DManager);

    void Render(DirectX11& dx11, ID3D11DeviceContext* context);

    void showGui();
};

struct HeightField::Vertex {
    mathlib::Vec2f position;
    mathlib::Vec2f texcoord;
};
static const auto HeightFieldVertexInputElementDescs = {
    MAKE_INPUT_ELEMENT_DESC(HeightField::Vertex, position),
    MAKE_INPUT_ELEMENT_DESC(HeightField::Vertex, texcoord)};
