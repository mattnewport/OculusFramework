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
        R"(data\cdem_dem_150508_205233.dat)", ios::in | ios::binary};
    file.seekg(0, ios::end);
    const auto endPos = file.tellg();
    file.seekg(0);
    const auto fileSize = endPos - file.tellg();

    const auto width = 907;
    const auto widthM = 21072.0f;
    const auto height = 882;
    const auto heightM = 20486.0f;
    const auto scale = 1 / 10000.0f;
    auto heights = vector<uint16_t>(width * height);
    file.read(reinterpret_cast<char*>(heights.data()), heights.size() * sizeof(uint16_t));
    const auto numRead = file.gcount();
    assert(numRead == fileSize);
    [this, device, &heights, width, height] {
        CD3D11_TEXTURE2D_DESC desc{DXGI_FORMAT_R16_UINT, static_cast<UINT>(width),
                                   static_cast<UINT>(height), 1u, 1u};
        D3D11_SUBRESOURCE_DATA data{heights.data(), width * sizeof(heights[0]), 0};
        ThrowOnFailure(device->CreateTexture2D(&desc, &data, &heightsTex));
        ThrowOnFailure(device->CreateShaderResourceView(heightsTex, nullptr, &heightsSRV));
    }();

    auto getHeight = [&heights, width, height](int x, int y) {
        x = min(max(0, x), width - 1);
        y = min(max(0, y), height - 1);
        return heights[y * width + x];
    };

    auto getElevation = [getHeight, width, scale](int x, int y) {
        const auto gridHeight = getHeight(width - 1 - x, y);
        return gridHeight * scale;
    };

    const auto gridStep = (widthM * scale) / width;

    auto getNormal = [gridStep, getElevation](int x, int y) {
        auto yl = getElevation(x - 1, y);
        auto yr = getElevation(x + 1, y);
        auto yd = getElevation(x, y - 1);
        auto yu = getElevation(x, y + 1);
        return normalize(Vec3f{2.0f * gridStep * (yl - yr), 4.0f * gridStep * gridStep,
                               2.0f * gridStep * (yd - yu)});
    };

    [this, device, width, height, getNormal] {
        vector<Vec2f> normals;
        normals.reserve(width * height);
        for (auto y = 0; y < height; ++y) {
            for (auto x = 0; x < width; ++x) {
                auto normal = getNormal(x, y);
                assert(normal.y() > 0.0f);
                normals.emplace_back(normal.x(), normal.z());
            }
        }
        CD3D11_TEXTURE2D_DESC desc{DXGI_FORMAT_R32G32_FLOAT, static_cast<UINT>(width),
                                   static_cast<UINT>(height), 1u, 1u};
        D3D11_SUBRESOURCE_DATA data{normals.data(), width * sizeof(normals[0])};
        ThrowOnFailure(device->CreateTexture2D(&desc, &data, &normalsTex));
        ThrowOnFailure(device->CreateShaderResourceView(normalsTex, nullptr, &normalsSRV));
    }();

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
    for (auto y = 0; y < height; y += blockSize) {
        for (auto x = 0; x < width; x += blockSize) {
            auto vertices = vector<Vertex>{};
            vertices.reserve((blockSize + 1) * (blockSize + 1));
            for (auto blockY = 0; blockY <= blockSize; ++blockY) {
                for (auto blockX = 0; blockX <= blockSize; ++blockX) {
                    Vertex v;
                    const auto localX = min(x + blockX, width);
                    const auto localY = min(y + blockY, height);
                    v.pos = Vec2f{localX * gridStep, localY * gridStep};
                    v.uv = Vec2f{(width - 1 - localX + 0.5f) * uvStepX,
                                 (height - 1 - localY + 0.5f) * uvStepY};
                    vertices.push_back(v);
                }
            }
            VertexBuffers.push_back(
                std::make_unique<DataBuffer>(device, D3D11_BIND_VERTEX_BUFFER, vertices.data(),
                                             vertices.size() * sizeof(vertices[0])));
        }
    }

    shapesTex = texture2DManager.get(R"(data\shapes2.dds)");

    PipelineStateObjectDesc desc;
    desc.vertexShader = "terrainvs.hlsl";
    desc.pixelShader = "terrainps.hlsl";
    desc.inputElementDescs = {MAKE_INPUT_ELEMENT_DESC(Vertex, pos, "POSITION"),
                              MAKE_INPUT_ELEMENT_DESC(Vertex, uv, "TEXCOORD")};
    pipelineStateObject = pipelineStateObjectManager.get(desc);

    [this, device] {
        const CD3D11_BUFFER_DESC desc{roundUpConstantBufferSize(sizeof(Object)),
                                      D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC,
                                      D3D11_CPU_ACCESS_WRITE};
        device->CreateBuffer(&desc, nullptr, &objectConstantBuffer);
    }();
}

void HeightField::Render(DirectX11& dx11, ID3D11DeviceContext* context) {
    dx11.applyState(*context, *pipelineStateObject.get());

    Object object;
    object.world = GetMatrix();

    [this, &object, &dx11] {
        D3D11_MAPPED_SUBRESOURCE mappedResource{};
        dx11.Context->Map(objectConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        memcpy(mappedResource.pData, &object, sizeof(object));
        dx11.Context->Unmap(objectConstantBuffer, 0);
    }();

    ID3D11Buffer* vsConstantBuffers[] = {objectConstantBuffer};
    context->VSSetConstantBuffers(objectConstantBufferOffset, size(vsConstantBuffers),
                                  vsConstantBuffers);

    ID3D11ShaderResourceView* vsSrvs[] = {heightsSRV};
    context->VSSetShaderResources(0, size(vsSrvs), vsSrvs);

    context->IASetIndexBuffer(IndexBuffer->D3DBuffer, DXGI_FORMAT_R16_UINT, 0);

    ID3D11ShaderResourceView* srvs[] = {shapesTex.get(), normalsSRV};
    context->PSSetShaderResources(materialSRVOffset, size(srvs), srvs);

    for (const auto& vertexBuffer : VertexBuffers) {
        ID3D11Buffer* vertexBuffers[] = {vertexBuffer->D3DBuffer};
        const UINT strides[] = {sizeof(Vertex)};
        const UINT offsets[] = {0};
        context->IASetVertexBuffers(0, size(vertexBuffers), vertexBuffers, strides, offsets);
        context->DrawIndexed(Indices.size(), 0, 0);
    }
}
