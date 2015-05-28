#include "terrain.h"

#include "pipelinestateobject.h"

#include "DDSTextureLoader.h"

#include "imgui/imgui.h"

#include "vector.h"

#include "cpl_serv.h"
#include "geodesic.h"
#include "geotiff.h"
#include "geo_normalize.h"
#include "geovalues.h"
#include "xtiffio.h"

#include <array>
#include <fstream>

#include "hlslmacros.h"
#include "../commonstructs.hlsli"

using namespace std;

using namespace mathlib;

static const char* CSVFileOverride(const char* pszInput) {
    static char szPath[1024];
    sprintf_s(szPath, "%s\\%s", "..\\libgeotiff-1.4.0\\csv", pszInput);
    return szPath;
}

void HeightField::AddVertices(ID3D11Device* device,
                              PipelineStateObjectManager& pipelineStateObjectManager,
                              Texture2DManager& texture2DManager) {
    auto tif = unique_ptr<TIFF, void (*)(TIFF*)>{
        XTIFFOpen(R"(data\cdem_dem_150528_015119.tif)", "r"), [](TIFF* tiff) { XTIFFClose(tiff); }};
    if (!tif) throw runtime_error{"Failed to load terrain elevation .tif"};
    const auto tifWidth = static_cast<int>([&tif] {
        uint32_t width = 0;
        TIFFGetField(tif.get(), TIFFTAG_IMAGEWIDTH, &width);
        return width;
    }());
    const auto tifHeight = static_cast<int>([&tif] {
        uint32_t length = 0;
        TIFFGetField(tif.get(), TIFFTAG_IMAGELENGTH, &length);
        return length;
    }());
    const auto tifBitsPerSample = [&tif] {
        uint32_t bps = 0;
        TIFFGetField(tif.get(), TIFFTAG_BITSPERSAMPLE, &bps);
        return bps;
    }();
    const auto tifSamplesPerPixel = [&tif] {
        uint16_t samplesPerPixel = 0;
        TIFFGetField(tif.get(), TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
        return samplesPerPixel;
    }();
    assert(tifBitsPerSample == 16 && tifSamplesPerPixel == 1);
    auto heights = vector<uint16_t>(tifWidth * tifHeight);
    [&tif, &heights] {
        const auto stripCount = TIFFNumberOfStrips(tif.get());
        const auto stripSize = TIFFStripSize(tif.get());
        auto offset = 0;
        for (tstrip_t strip = 0; strip < stripCount; ++strip) {
            offset +=
                TIFFReadEncodedStrip(tif.get(), strip, &heights[offset / sizeof(heights[0])], -1);
        }
    }();

    SetCSVFilenameHook(CSVFileOverride);

    auto gtif =
        unique_ptr<GTIF, void (*)(GTIF*)>{GTIFNew(tif.get()), [](GTIF* gtif) { GTIFFree(gtif); }};
    if (!gtif) throw runtime_error{"Failed to create geotiff for terrain elevation .tif"};
    auto print = [](char* s, void*) {
        OutputDebugStringA(s);
        return 0;
    };
    GTIFPrint(gtif.get(), print, nullptr);

    auto definition = GTIFDefn{};
    if (!GTIFGetDefn(gtif.get(), &definition))
        throw runtime_error{"Unable to read geotiff definition"};
    assert(definition.Model == ModelTypeGeographic);
    auto pixToLatLong = [&gtif](int x, int y) {
        double longitude = x;
        double latitude = y;
        if (!GTIFImageToPCS(gtif.get(), &longitude, &latitude))
            throw runtime_error{"Error converting pixel coordinates to lat/long."};
        return Vec2f{static_cast<float>(latitude), static_cast<float>(longitude)};
    };
    const auto topLeft = pixToLatLong(0, 0);
    const auto topRight = pixToLatLong(tifWidth, 0);
    const auto bottomLeft = pixToLatLong(0, tifHeight);
    const auto bottomRight = pixToLatLong(tifWidth, tifHeight);

    const auto latLongDist = [&definition](const Vec2f& a, const Vec2f& b) {
        const auto geodesic = [&definition] {
            auto ret = geod_geodesic{};
            const auto flattening =
                (definition.SemiMajor - definition.SemiMinor) / definition.SemiMajor;
            geod_init(&ret, definition.SemiMajor, flattening);
            return ret;
        }();
        auto dist = 0.0;
        geod_inverse(&geodesic, a.x(), a.y(), b.x(), b.y(), &dist, nullptr, nullptr);
        return static_cast<float>(dist);
    };

    const auto width = tifWidth;
    const auto widthM = latLongDist(topLeft, topRight);
    const auto height = tifHeight;
    const auto heightM = latLongDist(topLeft, bottomLeft);
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

    auto getElevation = [getHeight, width, height](int x, int y) {
        const auto gridHeight = getHeight(x, y);
        return gridHeight;
    };

    const auto gridStep = Vec2f{widthM / width, heightM / height};

    auto getNormal = [gridStep, getElevation](int x, int y) {
        auto yl = getElevation(x - 1, y);
        auto yr = getElevation(x + 1, y);
        auto yd = getElevation(x, y - 1);
        auto yu = getElevation(x, y + 1);
        return normalize(Vec3f{2.0f * gridStep.y() * (yl - yr), 4.0f * gridStep.x() * gridStep.y(),
                               2.0f * gridStep.x() * (yd - yu)});
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
                    v.pos = Vec2f{localX * gridStep.x(), localY * gridStep.y()};
                    v.uv = Vec2f{(localX + 0.5f) * uvStepX, (localY + 0.5f) * uvStepY};
                    vertices.push_back(v);
                }
            }
            VertexBuffers.push_back(
                std::make_unique<DataBuffer>(device, D3D11_BIND_VERTEX_BUFFER, vertices.data(),
                                             vertices.size() * sizeof(vertices[0])));
        }
    }

    shapesTex = texture2DManager.get(R"(data\fullarea.dds)");

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

void HeightField::showGui() {
    if (ImGui::CollapsingHeader("Terrain")) {
        ImGui::SliderFloat("Scale", &scale, 1e-5f, 1e-3f, "scale = %.6f", 3.0f);
    }
}
