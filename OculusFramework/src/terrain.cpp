#include "terrain.h"

#include "pipelinestateobject.h"

#include "DDSTextureLoader.h"

#include "imgui/imgui.h"

#include "mathfuncs.h"
#include "mathio.h"
#include "vector.h"

#include "cpl_serv.h"
#include "geo_normalize.h"
#include "geodesic.h"
#include "geotiff.h"
#include "geovalues.h"
#include "shapefil.h"
#include "xtiffio.h"

#pragma warning(push)
#pragma warning(disable: 4245)
#include <array_view.h>
#pragma warning(pop)

#include "hlslmacros.h"
#include "../commonstructs.hlsli"

#include <fstream>
#include <string>
#include <sstream>
#include <unordered_map>

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
        heightsSRV = CreateShaderResourceView(device, heightsTex.Get());
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
    auto latLongToPix = [&gtif](double latitude, double longitude) {
        if (!GTIFPCSToImage(gtif.get(), &longitude, &latitude))
            throw runtime_error{ "Error converting pixel coordinates to lat/long." };
        return Vec2i{ static_cast<int>(latitude), static_cast<int>(longitude) };
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

    auto heightsView = gsl::as_array_view(heights.data(), gsl::dim<>(height), gsl::dim<>(width));
    auto getHeight = [
        heightsView,
        w = heightsView.bounds().index_bounds()[1],
        h = heightsView.bounds().index_bounds()[0]
    ](auto idx, int xOff, int yOff) {
        idx[0] = clamp(int(idx[0]) + yOff, 0, int(h) - 1);
        idx[1] = clamp(int(idx[1]) + xOff, 0, int(w) - 1);
        return heightsView[idx];
    };

    [this, device, width, height, &heights, gridStepX, gridStepY, &getHeight] {

        vector<Vec2f> normals(width * height);
        auto normalsView =
            gsl::as_array_view(normals.data(), gsl::dim<>(height), gsl::dim<>(width));
        for (auto idx : normalsView.bounds()) {
            const auto normal =
                normalize(Vec3f{2.0f * gridStepY * (getHeight(idx, -1, 0) - getHeight(idx, 1, 0)),
                                4.0f * gridStepX * gridStepY,
                                2.0f * gridStepX * (getHeight(idx, 0, -1) - getHeight(idx, 0, 1))});
            normalsView[idx] = normal.xz();
        }

        normalsTex = CreateTexture2D(
            device, Texture2DDesc{DXGI_FORMAT_R32G32_FLOAT, static_cast<UINT>(width),
                                  static_cast<UINT>(height)}
                        .mipLevels(1),
            {normals.data(), width * sizeof(normals[0])});
        normalsSRV= CreateShaderResourceView(device, normalsTex.Get());
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
    IndexBuffer = CreateBuffer(
        device, BufferDesc{Indices.size() * sizeof(Indices[0]), D3D11_BIND_INDEX_BUFFER},
        {Indices.data()});

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
            VertexBuffers.push_back(CreateBuffer(
                device, BufferDesc{vertices.size() * sizeof(vertices[0]), D3D11_BIND_VERTEX_BUFFER},
                {vertices.data()}));
        }
    }

    shapesTex = texture2DManager.get(R"(data\fullarea.dds)");

    PipelineStateObjectDesc desc;
    desc.vertexShader = "terrainvs.hlsl";
    desc.pixelShader = "terrainps.hlsl";
    desc.inputElementDescs = HeightFieldVertexInputElementDescs;
    pipelineStateObject = pipelineStateObjectManager.get(desc);

    objectConstantBuffer = CreateBuffer(
        device, BufferDesc{roundUpConstantBufferSize(sizeof(Object)), D3D11_BIND_CONSTANT_BUFFER,
                           D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE});

    loadShapeFile();
    topographicFeatureLabels.emplace_back(device, topographicFeatures[0].label.c_str());
    const auto labelPixelPos =
        latLongToPix(topographicFeatures[0].latLong.x(), topographicFeatures[0].latLong.y());
    const auto labelHeight = float(getHeight(gsl::index<2, int>{labelPixelPos.x(), labelPixelPos.y()}, 0, 0)) - 1000.0f;
    const auto labelZ = latLongDist(topLeft, Vec2f{ topographicFeatures[0].latLong.x(), topLeft.y() });
    const auto labelX = latLongDist(topLeft, Vec2f{ topLeft.x(), topographicFeatures[0].latLong.y() });
    const auto labelSize = Vec2f{ topographicFeatureLabels[0].getWidth(), topographicFeatureLabels[0].getHeight() } * 20.0f;
    labelsVertices.push_back({ Vec3f{labelX, float(labelHeight), labelZ}, 0xffffffff, Vec2f{1.0f, 1.0f} });
    labelsVertices.push_back({ Vec3f{labelX + labelSize.x(), labelHeight, labelZ}, 0xffffffff, Vec2f{0.0f, 1.0f} });
    labelsVertices.push_back({ Vec3f{ labelX + labelSize.x(), labelHeight + labelSize.y(), labelZ }, 0xffffffff, Vec2f{ 0.0f, 0.0f } });
    labelsVertices.push_back({ Vec3f{ labelX, labelHeight + labelSize.y(), labelZ }, 0xffffffff, Vec2f{ 1.0f, 0.0f } });
    labelsIndices.push_back(0);
    labelsIndices.push_back(1);
    labelsIndices.push_back(2);
    labelsIndices.push_back(0);
    labelsIndices.push_back(2);
    labelsIndices.push_back(3);
    labelsVertexBuffer = CreateBuffer(
        device,
        BufferDesc{labelsVertices.size() * sizeof(labelsVertices[0]), D3D11_BIND_VERTEX_BUFFER},
        {labelsVertices.data()});
    labelsIndexBuffer = CreateBuffer(
        device,
        BufferDesc{labelsIndices.size() * sizeof(labelsIndices[0]), D3D11_BIND_INDEX_BUFFER},
        {labelsIndices.data()});

    PipelineStateObjectDesc labelsDesc;
    labelsDesc.vertexShader = "simplevs.hlsl";
    labelsDesc.pixelShader = "simpleps.hlsl";
    labelsDesc.inputElementDescs = HeightFieldLabelVertexInputElementDescs;
    labelsPipelineStateObject = pipelineStateObjectManager.get(labelsDesc);
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

    VSSetConstantBuffers(context, objectConstantBufferOffset, {objectConstantBuffer.Get()});
    VSSetShaderResources(context, 0, {heightsSRV.Get()});
    context->IASetIndexBuffer(IndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    PSSetShaderResources(context, materialSRVOffset, {shapesTex.get(), normalsSRV.Get()});
    for (const auto& vertexBuffer : VertexBuffers) {
        IASetVertexBuffers(context, 0, {vertexBuffer.Get()}, {UINT(sizeof(Vertex))});
        context->DrawIndexed(Indices.size(), 0, 0);
    }

    dx11.applyState(*context, *labelsPipelineStateObject.get());
    context->IASetIndexBuffer(labelsIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    PSSetShaderResources(context, materialSRVOffset, { topographicFeatureLabels[0].srv() });
    IASetVertexBuffers(context, 0, { labelsVertexBuffer.Get() }, { UINT(sizeof(LabelVertex)) });
    context->DrawIndexed(labelsIndices.size(), 0, 0);
}

auto shapeTypeToString(int shapeType) {
    switch (shapeType) {
        case SHPT_POINT:
            return "SHPT_POINT"s;
        case SHPT_ARC:
            return "SHPT_ARC"s;
        case SHPT_POLYGON:
            return "SHPT_POLYGON"s;
        case SHPT_MULTIPOINT:
            return "SHPT_MULTIPOINT"s;
        default:
            return to_string(shapeType);
    }
}

auto toString(DBFFieldType fieldType) {
    switch (fieldType) {
        case FTString:
            return "FTString";
        case FTInteger:
            return "FTInteger";
        case FTDouble:
            return "FTDouble";
        case FTLogical:
            return "FTLogical";
        case FTInvalid:
            return "FTInvalid";
        default:
            return "Unrecognized";
    }
}

struct DBFFieldInfo {
    int index = 0;
    DBFFieldType type = FTInvalid;
    char name[12] = {};
    int width = 0;
    int decimals = 0;
};

void HeightField::loadShapeFile() {
    const auto shapeHandle = unique_ptr<SHPInfo, void (*)(SHPHandle)>{
        SHPOpen(
            R"(E:\Users\Matt\Documents\Dropbox2\Dropbox\Projects\OculusFramework\OculusFramework\data\canvec_150528_015119_shp\to_1580009_0.shp)",
            "rb"),
        SHPClose};

    auto numEntities = 0;
    auto shapeType = 0;
    auto minBounds = Vector<double, 4>{};
    auto maxBounds = Vector<double, 4>{};
    SHPGetInfo(shapeHandle.get(), &numEntities, &shapeType, minBounds.data(), maxBounds.data());
    stringstream info;
    info << "numEntities: " << numEntities << ", "
         << "shapeType: " << shapeTypeToString(shapeType) << ", minBounds: " << minBounds
         << ", maxBounds: " << maxBounds << '\n';

    using ShpObjectPtr = unique_ptr<SHPObject, void(*)(SHPObject*)>;
    vector<ShpObjectPtr> shapes;
    shapes.reserve(numEntities);
    for (int i = 0; i < numEntities; ++i) {
        shapes.emplace_back(SHPReadObject(shapeHandle.get(), i), SHPDestroyObject);
    }

    const auto dbfHandle = unique_ptr<DBFInfo, void (*)(DBFHandle)>{DBFOpen(
        R"(E:\Users\Matt\Documents\Dropbox2\Dropbox\Projects\OculusFramework\OculusFramework\data\canvec_150528_015119_shp\to_1580009_0.dbf)",
        "rb"), DBFClose};
    const auto dbfFieldCount = DBFGetFieldCount(dbfHandle.get());
    const auto dbfRecordCount = DBFGetRecordCount(dbfHandle.get());
    assert(dbfRecordCount == numEntities);
    unordered_map<string, DBFFieldInfo> dbfFieldInfos;
    for (int i = 0; i < dbfFieldCount; ++i) {
        auto fieldInfo = DBFFieldInfo{};
        fieldInfo.type = DBFGetFieldInfo(dbfHandle.get(), i, fieldInfo.name, &fieldInfo.width, &fieldInfo.decimals);
        dbfFieldInfos[fieldInfo.name] = fieldInfo;
        info << "DBF Field " << i << ": " << toString(fieldInfo.type) << ", " << fieldInfo.name
             << ", width = " << fieldInfo.width << ", decimals = " << fieldInfo.decimals << '\n';
    }

    auto dbfReadString = [dbf = dbfHandle.get(), &dbfFieldInfos](int shapeId, const char* fieldName) {
        const auto fieldIndex = DBFGetFieldIndex(dbf, fieldName);
        assert(dbfFieldInfos[fieldName].type == FTString);
        const auto fieldChars = DBFReadStringAttribute(dbf, shapeId, fieldIndex);
        return string{fieldChars};
    };

    for (const auto& up : shapes) {
        const auto id = dbfReadString(up->nShapeId, "id");
        const auto nameid = dbfReadString(up->nShapeId, "nameid");
        const auto geonamedb = dbfReadString(up->nShapeId, "geonamedb");
        const auto nameen = dbfReadString(up->nShapeId, "nameen");
        info << shapeTypeToString(up->nSHPType) << ", nShapeId: " << up->nShapeId
             << ", nParts: " << up->nParts << ", id: " << id << ", nameid: " << nameid
             << ", geonamedb: " << geonamedb << ", nameen: " << nameen << '\n';

        const auto longitude = float(*up->padfX);
        const auto latitude = float(*up->padfY);
        topographicFeatures.push_back({Vec2f{latitude, longitude}, nameen});
    }

    OutputDebugStringA(info.str().c_str());
}

void HeightField::showGui() {
    if (ImGui::CollapsingHeader("Terrain")) {
        ImGui::SliderFloat("Scale", &scale, 1e-5f, 1e-3f, "scale = %.6f", 3.0f);
        const auto imageSize =
            ImVec2{topographicFeatureLabels[0].getWidth(), topographicFeatureLabels[0].getHeight()};
        const auto uvs = topographicFeatureLabels[0].getUvs();
        ImGui::Image(reinterpret_cast<ImTextureID>(topographicFeatureLabels[0].srv()), imageSize,
                     {uvs.first.x(), uvs.first.y()}, {uvs.second.x(), uvs.second.y()});
    }
}
