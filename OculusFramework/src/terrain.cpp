#include "terrain.h"

#include "pipelinestateobject.h"

#include "DDSTextureLoader.h"

#include "imgui/imgui.h"

#include "mathfuncs.h"
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

    [this, device, &heights, tifWidth, tifHeight] {
        heightsTex =
            CreateTexture2D(device, Texture2DDesc{DXGI_FORMAT_R16_UINT, static_cast<UINT>(tifWidth),
                                                  static_cast<UINT>(tifHeight)}
                                        .mipLevels(1),
                            {heights.data(), tifWidth * sizeof(heights[0])});
        ThrowOnFailure(device->CreateShaderResourceView(heightsTex.Get(), nullptr, &heightsSRV));
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
    const auto gridStepX = widthM / width;
    const auto gridStepY = heightM / height;

    [this, device, width, height, &heights, gridStepX, gridStepY] {
        auto getHeight = [heights = heights.data(), width, height](int x, int y) {
            x = clamp(x, 0, width - 1);
            y = clamp(y, 0, height - 1);
            return heights[y * width + x];
        };

        vector<Vec2f> normals(width * height);
        for (auto y = 0; y < height; ++y) {
            for (auto x = 0; x < width; ++x) {
                const auto yl = getHeight(x - 1, y);
                const auto yr = getHeight(x + 1, y);
                const auto yd = getHeight(x, y - 1);
                const auto yu = getHeight(x, y + 1);
                const auto normal = normalize(Vec3f{2.0f * gridStepY * (yl - yr),
                                                    4.0f * gridStepX * gridStepY,
                                                    2.0f * gridStepX * (yd - yu)});
                assert(normal.y() > 0.0f);
                normals[y * width + x] = {normal.x(), normal.z()};
            }
        }
        normalsTex = CreateTexture2D(
            device, Texture2DDesc{DXGI_FORMAT_R32G32_FLOAT, static_cast<UINT>(width),
                                  static_cast<UINT>(height)}
                        .mipLevels(1),
            {normals.data(), width * sizeof(normals[0])});
        ThrowOnFailure(device->CreateShaderResourceView(normalsTex.Get(), nullptr, &normalsSRV));
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
        const auto tl = uint16_t(y * (blockSize + 1) + x);
        const auto tr = uint16_t(tl + 1);
        const auto bl = uint16_t(tl + (blockSize + 1));
        const auto br = uint16_t(tr + (blockSize + 1));
        Indices.insert(end(Indices), {tl, tr, bl, tr, br, bl});
    }
    IndexBuffer = std::make_unique<DataBuffer>(device, D3D11_BIND_INDEX_BUFFER, Indices.data(),
                                               Indices.size() * sizeof(Indices[0]));

    const auto uvStepX = 1.0f / width;
    const auto uvStepY = 1.0f / height;
    auto vertices = vector<Vertex>(square(blockSize + 1));
    for (auto y = 0; y < height; y += blockSize) {
        for (auto x = 0; x < width; x += blockSize) {
            auto destVertex = 0;
            for (auto blockY = 0; blockY <= blockSize; ++blockY) {
                for (auto blockX = 0; blockX <= blockSize; ++blockX) {
                    const auto localX = min(x + blockX, width);
                    const auto localY = min(y + blockY, height);
                    vertices[destVertex++] = {
                        {localX * gridStepX, localY * gridStepY},
                        {(localX + 0.5f) * uvStepX, (localY + 0.5f) * uvStepY}};
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
    desc.inputElementDescs = {MAKE_INPUT_ELEMENT_DESC(Vertex, position),
                              MAKE_INPUT_ELEMENT_DESC(Vertex, texcoord)};
    pipelineStateObject = pipelineStateObjectManager.get(desc);

    objectConstantBuffer = CreateBuffer(
        device, BufferDesc{roundUpConstantBufferSize(sizeof(Object)), D3D11_BIND_CONSTANT_BUFFER,
                           D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE});
}

void HeightField::Render(DirectX11& dx11, ID3D11DeviceContext* context) {
    dx11.applyState(*context, *pipelineStateObject.get());

    Object object;
    object.world = GetMatrix();

    [this, &object, &dx11] {
        auto mapHandle = MapHandle{dx11.Context.Get(), objectConstantBuffer.Get(), 0,
                                   D3D11_MAP_WRITE_DISCARD, 0};
        memcpy(mapHandle.mappedSubresource().pData, &object, sizeof(object));
    }();

    VSSetConstantBuffers(context, objectConstantBufferOffset,
                         {objectConstantBuffer.Get()});

    VSSetShaderResources(context, 0, {heightsSRV.Get()});

    context->IASetIndexBuffer(IndexBuffer->D3DBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

    PSSetShaderResources(context, materialSRVOffset,
                         {shapesTex.get(), normalsSRV.Get()});

    for (const auto& vertexBuffer : VertexBuffers) {
        IASetVertexBuffers(context, 0, {vertexBuffer->D3DBuffer.Get()},
                           {UINT(sizeof(Vertex))});
        context->DrawIndexed(Indices.size(), 0, 0);
    }
}

void HeightField::showGui() {
    if (ImGui::CollapsingHeader("Terrain")) {
        ImGui::SliderFloat("Scale", &scale, 1e-5f, 1e-3f, "scale = %.6f", 3.0f);
    }
}
