#include "sphere.h"

#include <array>
#include <numeric>
#include <vector>

using namespace mathlib;

using namespace std;

void Sphere::GenerateVerts(ID3D11Device& device, PipelineStateObjectManager& pipelineStateObjectManager, VertexShaderManager& vertexShaderManager) {
    // generate cube
    auto verts = vector<Vertex>{Vec3f{-1.0f, -1.0f, -1.0f}, Vec3f{-1.0f, 1.0f, -1.0f},
                                Vec3f{1.0f, 1.0f, -1.0f},   Vec3f{1.0f, -1.0f, -1.0f},
                                Vec3f{-1.0f, -1.0f, 1.0f},  Vec3f{-1.0f, 1.0f, 1.0f},
                                Vec3f{1.0f, 1.0f, 1.0f},    Vec3f{1.0f, -1.0f, 1.0f}};

    using IndexT = uint16_t;
    using QuadIndices = array<IndexT, 4>;
    using QuadsIndices = vector<QuadIndices>;
    auto quadsIndices = QuadsIndices{{
                                         0, 3, 2, 1,
                                     },
                                     {
                                         3, 7, 6, 2,
                                     },
                                     {
                                         4, 5, 6, 7,
                                     },
                                     {
                                         4, 0, 1, 5,
                                     },
                                     {
                                         1, 2, 6, 5,
                                     },
                                     {0, 4, 7, 3}};

    // subdivide quads in cube
    {
        auto subDivideQuad = [](const QuadIndices& quadIndices, vector<Vertex>& verts) {
            constexpr auto numNewVerts = 5;
            Vec3f ps[numNewVerts];
            ps[0] = (verts[quadIndices[0]].pos + verts[quadIndices[1]].pos) * 0.5f;
            ps[1] = (verts[quadIndices[1]].pos + verts[quadIndices[2]].pos) * 0.5f;
            ps[2] = (verts[quadIndices[2]].pos + verts[quadIndices[3]].pos) * 0.5f;
            ps[3] = (verts[quadIndices[3]].pos + verts[quadIndices[0]].pos) * 0.5f;
            ps[4] = (ps[0] + ps[2]) * 0.5f;
            auto baseIndex = static_cast<IndexT>(verts.size());
            verts.insert(end(verts), begin(ps), end(ps));
            IndexT newIndices[numNewVerts];
            iota(begin(newIndices), end(newIndices), baseIndex);

            auto newQuadIndices = QuadsIndices{};
            newQuadIndices.emplace_back(
                QuadIndices{quadIndices[0], newIndices[0], newIndices[4], newIndices[3]});
            newQuadIndices.emplace_back(
                QuadIndices{baseIndex, quadIndices[1], newIndices[1], newIndices[4]});
            newQuadIndices.emplace_back(
                QuadIndices{newIndices[4], newIndices[1], quadIndices[2], newIndices[2]});
            newQuadIndices.emplace_back(
                QuadIndices{newIndices[3], newIndices[4], newIndices[2], quadIndices[3]});
            return newQuadIndices;
        };

        const auto subdivLevel = 4;
        for (auto i = 0; i < subdivLevel; ++i) {
            auto subdividedQuadIndices = QuadsIndices{};
            for (auto& qi : quadsIndices) {
                auto subdividedQuad = subDivideQuad(qi, verts);
                subdividedQuadIndices.insert(subdividedQuadIndices.end(), begin(subdividedQuad),
                                             end(subdividedQuad));
            }
            swap(quadsIndices, subdividedQuadIndices);
        }
    }

    // reposition verts to surface of sphere
    for (auto& v : verts) {
        v.pos = normalize(v.pos);
    }

    // Generate triangle indices

    auto triangleIndicesFromQuadIndices = [](uint16_t* quadIndices, vector<uint16_t>& triIndices) {
        triIndices.push_back(quadIndices[0]);
        triIndices.push_back(quadIndices[1]);
        triIndices.push_back(quadIndices[3]);
        triIndices.push_back(quadIndices[3]);
        triIndices.push_back(quadIndices[1]);
        triIndices.push_back(quadIndices[2]);
    };
    auto indices = vector<uint16_t>{};
    for (auto& qi : quadsIndices) {
        triangleIndicesFromQuadIndices(qi.data(), indices);
    }
    indexCount = indices.size();

    // Generate buffers
    vb = [&device, &verts] {
        auto buf = ID3D11BufferPtr{};
        auto desc = CD3D11_BUFFER_DESC{verts.size() * sizeof(verts[0]), D3D11_BIND_VERTEX_BUFFER};
        auto initialData = D3D11_SUBRESOURCE_DATA{verts.data()};
        ThrowOnFailure(device.CreateBuffer(&desc, &initialData, &buf));
        return buf;
    }();

    ib = [&device, &indices] {
        auto buf = ID3D11BufferPtr{};
        auto desc =
            CD3D11_BUFFER_DESC{indices.size() * sizeof(indices[0]), D3D11_BIND_INDEX_BUFFER};
        auto initialData = D3D11_SUBRESOURCE_DATA{indices.data()};
        ThrowOnFailure(device.CreateBuffer(&desc, &initialData, &buf));
        return buf;
    }();

    PipelineStateObjectDesc desc;
    desc.vertexShader = "spherevs.hlsl";
    desc.pixelShader = "sphereps.hlsl";
    desc.inputLayout =
        InputLayoutKey{InputElementDescs{MAKE_INPUT_ELEMENT_DESC(Vertex, pos, "POSITION")},
                       desc.vertexShader, vertexShaderManager};
    pipelineStateObject = pipelineStateObjectManager.get(desc);
}

void Sphere::Render(ID3D11DeviceContext* context, const mathlib::Vec3f& eye, const mathlib::Mat4f& view, const mathlib::Mat4f& proj, DataBuffer* uniformBuffer) {
    context->RSSetState(pipelineStateObject.get()->rasterizerState.get());

    auto vs = pipelineStateObject.get()->vertexShader.get();
    vs->SetUniform("World", 16, GetMatrix().data());
    vs->SetUniform("View", 16, view.data());
    vs->SetUniform("Proj", 16, proj.data());
    vs->SetUniform("eye", 3, &eye.x());

    uniformBuffer->Refresh(context, vs->UniformData.data(), vs->UniformData.size());
    ID3D11Buffer* vsConstantBuffers[] = {uniformBuffer->D3DBuffer};
    context->VSSetConstantBuffers(0, 1, vsConstantBuffers);

    context->IASetInputLayout(pipelineStateObject.get()->inputLayout.get());
    context->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, 0);

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vs->D3DVert, NULL, 0);

    context->PSSetShader(pipelineStateObject.get()->pixelShader.get()->D3DPix, NULL, 0);

    ID3D11Buffer* vertexBuffers[] = {vb};
    const UINT strides[] = {sizeof(Vertex)};
    const UINT offsets[] = {0};
    context->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    context->DrawIndexed(indexCount, 0, 0);
}
