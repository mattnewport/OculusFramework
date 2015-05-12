#include "terrain.h"

#include "DDSTextureLoader.h"

#include "vector.h"

#include <array>
#include <fstream>

using namespace std;

using namespace mathlib;

void HeightField::AddVertices(ID3D11Device* device) {
    [this, device] {
        auto rs = CD3D11_RASTERIZER_DESC{D3D11_DEFAULT};
        // rs.FillMode = D3D11_FILL_WIREFRAME;
        ThrowOnFailure(device->CreateRasterizerState(&rs, &Rasterizer));
        SetDebugObjectName(Rasterizer, "HeightField::Rasterizer");
    }();

    auto file = ifstream{
        R"(E:\Users\Matt\Documents\Dropbox2\Dropbox\Projects\OculusFramework\OculusFramework\data\cdem_dem_150508_205233.dat)",
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
        const auto baseIdx = y * (blockSize + 1) + x;
        Indices.push_back(baseIdx);
        Indices.push_back(baseIdx + 1);
        Indices.push_back(baseIdx + (blockSize + 1));
        Indices.push_back(baseIdx + 1);
        Indices.push_back(baseIdx + (blockSize + 1) + 1);
        Indices.push_back(baseIdx + (blockSize + 1));
    }
    IndexBuffer = std::make_unique<DataBuffer>(device, D3D11_BIND_INDEX_BUFFER, Indices.data(),
                                               Indices.size() * sizeof(Indices[0]));

    auto center = Vec3f{0.0f, 0.0f, 0.0f};
    const auto uvStepX = 1.0f / width;
    const auto uvStepY = 1.0f / height;
    const auto gridWidth = widthM / 10000.0f;
    const auto gridStep = gridWidth / width;
    const auto gridHeight = height * gridStep;
    const auto gridElevationScale = 1.0f / 10000.0f;
    for (auto y = 0; y < height; y += blockSize) {
        for (auto x = 0; x < width; x += blockSize) {
            auto vertices = vector<Vertex>{};
            vertices.reserve((blockSize + 1) * (blockSize + 1));
            for (auto blockY = 0; blockY <= blockSize; ++blockY) {
                for (auto blockX = 0; blockX <= blockSize; ++blockX) {
                    Vertex v;
                    auto localX = x + blockX;
                    auto localY = y + blockY;
                    auto gridHeight = getHeight(width - 1 - localX, localY);
                    v.Pos = Vec3f{localX * gridStep, gridHeight * gridElevationScale,
                                  localY * gridStep};
                    v.u = 1.0f - (localX * uvStepX);
                    v.v = 1.0f - (localY * uvStepY);
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

    auto shapes = ID3D11ResourcePtr{};
    ThrowOnFailure(DirectX::CreateDDSTextureFromFile(
        device,
        LR"(E:\Users\Matt\Documents\Dropbox2\Dropbox\Projects\OculusFramework\OculusFramework\data\shapes2.dds)",
        &shapes, &shapesSRV));

    auto normals = ID3D11ResourcePtr{};
    ThrowOnFailure(DirectX::CreateDDSTextureFromFile(
        device,
        LR"(E:\Users\Matt\Documents\Dropbox2\Dropbox\Projects\OculusFramework\OculusFramework\data\cdem_dem_150508_205233_normal.dds)",
        &normals, &normalsSRV));
}

void HeightField::Render(ID3D11DeviceContext* context, ShaderDatabase& shaderDatabase,
                         DataBuffer* uniformBuffer) {
    context->RSSetState(Rasterizer);

    const auto VShader = shaderDatabase.GetVertexShader("terrainvs.hlsl");
    uniformBuffer->Refresh(context, VShader->UniformData.data(), VShader->UniformData.size());
    ID3D11Buffer* vsConstantBuffers[] = {uniformBuffer->D3DBuffer};
    context->VSSetConstantBuffers(0, 1, vsConstantBuffers);

    const auto modelVertexDesc = InputLayoutKey{
        D3D11_INPUT_ELEMENT_DESC{"Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
                                 offsetof(Vertex, Pos), D3D11_INPUT_PER_VERTEX_DATA, 0},
        D3D11_INPUT_ELEMENT_DESC{"TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, u),
                                 D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    context->IASetInputLayout(shaderDatabase.GetInputLayout(VShader, modelVertexDesc));
    context->IASetIndexBuffer(IndexBuffer->D3DBuffer, DXGI_FORMAT_R16_UINT, 0);

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(VShader->D3DVert, NULL, 0);

    const auto pixelShader = shaderDatabase.GetPixelShader("terrainps.hlsl");
    context->PSSetShader(pixelShader->D3DPix, NULL, 0);

    ID3D11SamplerState* samplerStates[] = {samplerState};
    context->PSSetSamplers(0, 1, samplerStates);
    ID3D11ShaderResourceView* srvs[] = {shapesSRV, normalsSRV};
    context->PSSetShaderResources(0, 2, srvs);

    for (const auto& vertexBuffer : VertexBuffers) {
        ID3D11Buffer* vertexBuffers[] = {vertexBuffer->D3DBuffer};
        const UINT strides[] = {sizeof(Vertex)};
        const UINT offsets[] = {0};
        context->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
        context->DrawIndexed(Indices.size(), 0, 0);
    }
}
