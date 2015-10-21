#pragma once

#include "Win32_DX11AppUtil.h"

#include "matrix.h"
#include "vector.h"

class Sphere {
public:
    struct Vertex {
        Vertex() = default;
        Vertex(const mathlib::Vec3f& pos_) : pos(pos_) {}
        mathlib::Vec3f pos;
    };

    Sphere() = default;

    void GenerateVerts(ID3D11Device& device,
                       PipelineStateObjectManager& pipelineStateObjectManager);

    void Render(DirectX11& dx11, ID3D11DeviceContext* context);

    const mathlib::Mat4f GetMatrix() {
        auto scale = mathlib::scaleMat4f(0.25f);
        scale.row(3).y() = 1.0f;
        return scale;
    }

private:
    ID3D11BufferPtr vb;
    ID3D11BufferPtr ib;
    size_t indexCount = 0;
    PipelineStateObjectManager::ResourceHandle pipelineStateObject;
    ID3D11BufferPtr objectConstantBuffer;
};
