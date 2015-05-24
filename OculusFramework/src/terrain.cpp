#include "terrain.h"

#include "pipelinestateobject.h"

#include "DDSTextureLoader.h"

#include "vector.h"

#include <array>
#include <fstream>

#include "hlslmacros.h"
#include "../commonstructs.hlsli"

using namespace std;

using namespace mathlib;

void HeightField::AddVertices(ID3D11Device* device,
                              PipelineStateObjectManager& pipelineStateObjectManager,
                              Texture2DManager& texture2DManager) {
    auto file = ifstream{
        R"(data\cdem_dem_150508_205233.dat)",
        ios::in | ios::binary};
    file.seekg(0, ios::end);
    const auto endPos = file.tellg();
    file.seekg(0);
    const auto fileSize = endPos - file.tellg();

    const auto width = 907;
    const auto widthM = 21072.0f;
    const auto height = 882;
    const auto heightM = 20486.0f;
    auto heights = vector<uint16_t>(width * height);
    file.read(reinterpret_cast<char*>(heights.data()), heights.size() * sizeof(uint16_t));
    const auto numRead = file.gcount();
    assert(numRead == fileSize);

    auto getHeight = [&heights, width, height](int x, int y) {
        x = min(max(0, x), width - 1);
        y = min(max(0, y), height - 1);
        return heights[y * width + x];
    };

    const auto blockPower = 6;
    const auto blockSize = 1 << blockPower;

    // Use Hilbert curve for better vertex cache efficiency
    auto d2xy = [](int n, int d, int* x, int* y) {
        auto rot = [](int n, int* x, int* y, int rx, int ry) {
            if (ry == 0) {
                if (rx == 1) {
                    *x = n - 1 - *x;
                    *y = n - 1 - *y;
                }

                swap(*x, *y);
            }
        };

        int rx, ry, s, t = d;
        *x = *y = 0;
        for (s = 1; s < n; s *= 2) {
            rx = 1 & (t / 2);
            ry = 1 & (t ^ rx);
            rot(s, x, y, rx, ry);
            *x += s * rx;
            *y += s * ry;
            t /= 4;
        }
    };

    const auto quadCount = blockSize * blockSize;
    const auto indexCount = 6 * quadCount;
    Indices.reserve(indexCount);
    for (auto d = 0; d < quadCount; ++d) {
        int x, y;
        d2xy(blockSize, d, &x, &y);
        const auto baseIdx = static_cast<uint16_t>(y * (blockSize + 1) + x);
        Indices.push_back(baseIdx);
        Indices.push_back(baseIdx + 1);
        Indices.push_back(baseIdx + (blockSize + 1));
        Indices.push_back(baseIdx + 1);
        Indices.push_back(baseIdx + (blockSize + 1) + 1);
        Indices.push_back(baseIdx + (blockSize + 1));
    }
    IndexBuffer = std::make_unique<DataBuffer>(device, D3D11_BIND_INDEX_BUFFER, Indices.data(),
                                               Indices.size() * sizeof(Indices[0]));

    const auto uvStepX = 1.0f / width;
    const auto uvStepY = 1.0f / height;
    auto getPos = [width, widthM, getHeight](int x, int y) {
        const auto gridWidth = widthM / 10000.0f;
        const auto gridStep = gridWidth / width;
        const auto gridElevationScale = 1.0f / 10000.0f;
        const auto gridHeight = getHeight(width - 1 - x, y);
        return Vec3f{x * gridStep, gridHeight * gridElevationScale, y * gridStep};
    };
    auto getNormal = [getPos](int x, int y) {
        const auto p = getPos(x, y);
        const Vec3f ps[] = {getPos(x - 1, y), getPos(x + 1, y), getPos(x, y - 1), getPos(x, y + 1)};
        const Vec3f vs[] = {ps[0] - p, ps[1] - p, ps[2] - p, ps[3] - p};
        const Vec3f ns[] = {cross(vs[0], vs[3]), cross(vs[1], vs[2]), cross(vs[2], vs[0]), cross(vs[3], vs[1])};
        return normalize(ns[0] + ns[1] + ns[2] + ns[3]);
    };
    for (auto y = 0; y < height; y += blockSize) {
        for (auto x = 0; x < width; x += blockSize) {
            auto vertices = vector<Vertex>{};
            vertices.reserve((blockSize + 1) * (blockSize + 1));
            for (auto blockY = 0; blockY <= blockSize; ++blockY) {
                for (auto blockX = 0; blockX <= blockSize; ++blockX) {
                    Vertex v;
                    const auto localX = min(x + blockX, width);
                    const auto localY = min(y + blockY, height);
                    v.pos = getPos(localX, localY);
                    v.normal = getNormal(localX, localY);
                    v.uv = Vec2f{1.0f - (localX * uvStepX), 1.0f - (localY * uvStepY)};
                    vertices.push_back(v);
                }
            }
            VertexBuffers.push_back(
                std::make_unique<DataBuffer>(device, D3D11_BIND_VERTEX_BUFFER, vertices.data(),
                                             vertices.size() * sizeof(vertices[0])));
        }
    }

    auto ss = CD3D11_SAMPLER_DESC{D3D11_DEFAULT};
    ss.Filter = D3D11_FILTER_ANISOTROPIC;
    ss.MaxAnisotropy = 8;
    device->CreateSamplerState(&ss, &samplerState);

    shapesTex = texture2DManager.get(R"(data\shapes2.dds)");

    PipelineStateObjectDesc desc;
    desc.vertexShader = "terrainvs.hlsl";
    desc.pixelShader = "terrainps.hlsl";
    desc.inputElementDescs = {MAKE_INPUT_ELEMENT_DESC(Vertex, pos, "POSITION"),
                              MAKE_INPUT_ELEMENT_DESC(Vertex, normal, "NORMAL"),
                              MAKE_INPUT_ELEMENT_DESC(Vertex, uv, "TEXCOORD")};
    pipelineStateObject = pipelineStateObjectManager.get(desc);

    [this, device] {
        const CD3D11_BUFFER_DESC desc{roundUpConstantBufferSize(sizeof(Object)), D3D11_BIND_CONSTANT_BUFFER,
                                      D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE};
        device->CreateBuffer(&desc, nullptr, &objectConstantBuffer);
    }();
}

void HeightField::Render(DirectX11& dx11, ID3D11DeviceContext* context, ID3D11Buffer& cameraConstantBuffer, ID3D11Buffer& lightingBuffer, ID3D11ShaderResourceView& pmremEnvMapSRV, ID3D11ShaderResourceView& irradEnvMapSRV, ID3D11SamplerState& cubeSampler) {
    dx11.applyState(*context, *pipelineStateObject.get());

    Object object;
    object.world = GetMatrix();

    [this, &object, &dx11] {
        D3D11_MAPPED_SUBRESOURCE mappedResource{};
        dx11.Context->Map(objectConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        memcpy(mappedResource.pData, &object, sizeof(object));
        dx11.Context->Unmap(objectConstantBuffer, 0);
    }();

    ID3D11Buffer* vsConstantBuffers[] = {&cameraConstantBuffer, objectConstantBuffer, &lightingBuffer};
    context->VSSetConstantBuffers(0, 3, vsConstantBuffers);

    context->IASetIndexBuffer(IndexBuffer->D3DBuffer, DXGI_FORMAT_R16_UINT, 0);

    ID3D11Buffer* psConstantBuffers[] = {&lightingBuffer};
    context->PSSetConstantBuffers(2, 1, psConstantBuffers);

    ID3D11SamplerState* samplerStates[] = {&cubeSampler, samplerState};
    context->PSSetSamplers(0, 2, samplerStates);
    ID3D11ShaderResourceView* srvs[] = {&pmremEnvMapSRV, &irradEnvMapSRV, shapesTex.get()};
    context->PSSetShaderResources(0, 3, srvs);

    for (const auto& vertexBuffer : VertexBuffers) {
        ID3D11Buffer* vertexBuffers[] = {vertexBuffer->D3DBuffer};
        const UINT strides[] = {sizeof(Vertex)};
        const UINT offsets[] = {0};
        context->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
        context->DrawIndexed(Indices.size(), 0, 0);
    }
}
