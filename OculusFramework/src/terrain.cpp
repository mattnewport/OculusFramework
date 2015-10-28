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

class GeoTiff {
public:
    template <typename T>
    T getTiffField(ttag_t tag) {
        auto res = T{};
        if (TIFFGetField(tif.get(), tag, &res) != 1)
            throw runtime_error{"Failed to read TIFF field."};
        return res;
    }

    GeoTiff(const char* filename) {
        // Open filename as a TIFF and read height data
        tif = {XTIFFOpen(filename, "r"), XTIFFClose};
        if (!tif) throw runtime_error{"Failed to load terrain elevation .tif"};
        tifWidth = static_cast<int>(getTiffField<uint32_t>(TIFFTAG_IMAGEWIDTH));
        tifHeight = static_cast<int>(getTiffField<uint32_t>(TIFFTAG_IMAGELENGTH));
        assert(getTiffField<uint32_t>(TIFFTAG_BITSPERSAMPLE) == 16 &&
               getTiffField<uint32_t>(TIFFTAG_SAMPLESPERPIXEL) == 1);
        heights = readHeightData(tif.get(), tifWidth * tifHeight);

        // View TIFF as a GeoTIFF and calculate geo coordinates
        SetCSVFilenameHook([](const char* pszInput) -> const char* {
            static char szPath[1024];
            sprintf_s(szPath, "%s\\%s", "..\\libgeotiff-1.4.0\\csv", pszInput);
            return szPath;
        });
        gtif = {GTIFNew(tif.get()), GTIFFree};
        if (!gtif) throw runtime_error{"Failed to create geotiff for terrain elevation .tif"};
        logGeoTiffInfo();

        if (!GTIFGetDefn(gtif.get(), &gtifDefinition))
            throw runtime_error{"Unable to read geotiff definition"};
        assert(gtifDefinition.Model == ModelTypeGeographic);

        const auto topLeft = topLeftLatLong();
        const auto topRight = pixToLatLong(tifWidth, 0);
        const auto bottomLeft = pixToLatLong(0, tifHeight);
        widthMeters = latLongDist(topLeft, topRight);
        heightMeters = latLongDist(topLeft, bottomLeft);
    }

    auto getTiffWidth() const { return tifWidth; }
    auto getWidthMeters() const { return widthMeters; }
    auto getGridStepMetersX() const { return widthMeters / tifWidth; }
    auto getTiffHeight() const { return tifHeight; }
    auto getHeightMeters() const { return heightMeters; }
    auto getGridStepMetersY() const { return heightMeters / tifHeight; }
    const auto& getHeights() const { return heights; }
    auto getHeightsView() const {
        return gsl::as_array_view(heights.data(), gsl::dim<>(tifHeight), gsl::dim<>(tifWidth));
    }
    const auto& getGtifDefinition() const { return gtifDefinition; }
    Vec2f pixToLatLong(int x, int y) const {
        double longitude = x;
        double latitude = y;
        if (!GTIFImageToPCS(gtif.get(), &longitude, &latitude))
            throw runtime_error{"Error converting pixel coordinates to lat/long."};
        return {static_cast<float>(latitude), static_cast<float>(longitude)};
    }
    Vec2f topLeftLatLong() const { return pixToLatLong(0, 0); }
    Vec2i latLongToPix(double latitude, double longitude) const {
        if (!GTIFPCSToImage(gtif.get(), &longitude, &latitude))
            throw runtime_error{"Error converting pixel coordinates to lat/long."};
        return {static_cast<int>(latitude), static_cast<int>(longitude)};
    }

    float latLongDist(const Vec2f& a, const Vec2f& b) const {
        const auto geodesic = [this] {
            auto ret = geod_geodesic{};
            const auto flattening =
                (gtifDefinition.SemiMajor - gtifDefinition.SemiMinor) / gtifDefinition.SemiMajor;
            geod_init(&ret, gtifDefinition.SemiMajor, flattening);
            return ret;
        }();
        auto dist = 0.0;
        geod_inverse(&geodesic, a.x(), a.y(), b.x(), b.y(), &dist, nullptr, nullptr);
        return static_cast<float>(dist);
    }
    auto getHeightAt(gsl::index<2> idx, int xOff, int yOff) const {
        idx[0] = clamp(int(idx[0]) + yOff, 0, int(tifHeight) - 1);
        idx[1] = clamp(int(idx[1]) + xOff, 0, int(tifWidth) - 1);
        return getHeightsView()[idx];
    }

    void logGeoTiffInfo() {
        GTIFPrint(gtif.get(),
                  [](char* s, void*) {
                      OutputDebugStringA(s);
                      return 0;
                  },
                  nullptr);
    }

private:
    static std::vector<uint16_t> readHeightData(TIFF* t, int sz) {
        auto res = vector<uint16_t>(sz);
        const auto stripCount = TIFFNumberOfStrips(t);
        const auto stripSize = TIFFStripSize(t);
        auto offset = 0;
        for (tstrip_t strip = 0; strip < stripCount; ++strip) {
            offset += TIFFReadEncodedStrip(t, strip, &res[offset / sizeof(res[0])], -1);
        }
        return res;
    }

    int tifWidth = 0;
    int tifHeight = 0;
    float widthMeters = 0.0f;
    float heightMeters = 0.0f;
    vector<uint16_t> heights;
    unique_ptr<TIFF, void (*)(TIFF*)> tif{nullptr, XTIFFClose};
    unique_ptr<GTIF, void (*)(GTIF*)> gtif{nullptr, GTIFFree};
    GTIFDefn gtifDefinition = {};
};

void HeightField::AddVertices(ID3D11Device* device, ID3D11DeviceContext* context,
                              PipelineStateObjectManager& pipelineStateObjectManager,
                              Texture2DManager& texture2DManager) {
    auto geoTiff = GeoTiff{R"(data\cdem_dem_150528_015119.tif)"};
    const auto& heights = geoTiff.getHeights();

    heightsTex = CreateTexture2D(
        device, Texture2DDesc{DXGI_FORMAT_R16_UINT, static_cast<UINT>(geoTiff.getTiffWidth()),
                              static_cast<UINT>(geoTiff.getTiffHeight())}
                    .mipLevels(1),
        {heights.data(), geoTiff.getTiffWidth() * sizeof(heights[0])});
    heightsSRV = CreateShaderResourceView(device, heightsTex.Get());

    generateNormalMap(device, geoTiff);
    generateHeightFieldGeometry(device, geoTiff);

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
    for (const auto& feature : topographicFeatures) {
        topographicFeatureLabels.emplace_back(device, context, feature.label.c_str());
        const auto& label = topographicFeatureLabels.back();
        const auto labelPixelPos = geoTiff.latLongToPix(feature.latLong.x(), feature.latLong.y());
        const auto labelHeight =
            float(geoTiff.getHeightAt(gsl::index<2, int>{labelPixelPos.x(), labelPixelPos.y()}, 0,
                                      0)) -
            1000.0f;
        const auto topLeft = geoTiff.topLeftLatLong();
        const auto labelZ = geoTiff.latLongDist(topLeft, Vec2f{feature.latLong.x(), topLeft.y()});
        const auto labelX = geoTiff.latLongDist(topLeft, Vec2f{topLeft.x(), feature.latLong.y()});
        const auto labelPos = Vec3f{labelX, labelHeight, labelZ};
        const auto labelColor = 0xffffffffu;
        const auto labelSize = Vec2f{label.getWidth(), label.getHeight()} * 20.0f;
        const auto baseVertexIdx = uint16_t(labelsVertices.size());
        labelsVertices.push_back(
            {labelPos, labelColor, Vec2f{1.0f, 1.0f}, Vec2f{labelSize.x(), 0.0f}});
        labelsVertices.push_back({labelPos, labelColor, Vec2f{0.0f, 1.0f}, Vec2f{0.0f, 0.0f}});
        labelsVertices.push_back(
            {labelPos, labelColor, Vec2f{0.0f, 0.0f}, Vec2f{0.0f, labelSize.y()}});
        labelsVertices.push_back({labelPos, labelColor, Vec2f{1.0f, 0.0f}, labelSize});
        labelsIndices.push_back(baseVertexIdx + 0);
        labelsIndices.push_back(baseVertexIdx + 1);
        labelsIndices.push_back(baseVertexIdx + 2);
        labelsIndices.push_back(baseVertexIdx + 0);
        labelsIndices.push_back(baseVertexIdx + 2);
        labelsIndices.push_back(baseVertexIdx + 3);
    }
    labelsVertexBuffer = CreateBuffer(
        device,
        BufferDesc{labelsVertices.size() * sizeof(labelsVertices[0]), D3D11_BIND_VERTEX_BUFFER},
        {labelsVertices.data()});
    labelsIndexBuffer = CreateBuffer(
        device,
        BufferDesc{labelsIndices.size() * sizeof(labelsIndices[0]), D3D11_BIND_INDEX_BUFFER},
        {labelsIndices.data()});

    PipelineStateObjectDesc labelsDesc;
    labelsDesc.vertexShader = "labelvs.hlsl";
    labelsDesc.pixelShader = "labelps.hlsl";
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
    IASetVertexBuffers(context, 0, { labelsVertexBuffer.Get() }, { UINT(sizeof(LabelVertex)) });
    for (auto i = 0u; i < topographicFeatureLabels.size(); ++i) {
        PSSetShaderResources(context, materialSRVOffset, { topographicFeatureLabels[i].srv() });
        context->DrawIndexed(6, i * 6, 0);
    }
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
            R"(data\canvec_150528_015119_shp\to_1580009_0.shp)", "rb"),
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

    const auto dbfHandle = unique_ptr<DBFInfo, void (*)(DBFHandle)>{
        DBFOpen(
            R"(data\canvec_150528_015119_shp\to_1580009_0.dbf)", "rb"),
        DBFClose};
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

void HeightField::generateNormalMap(ID3D11Device* device, const GeoTiff& geoTiff) {
    const auto width = geoTiff.getTiffWidth();
    const auto height = geoTiff.getTiffHeight();
    const auto gridStepX = geoTiff.getGridStepMetersX();
    const auto gridStepY = geoTiff.getGridStepMetersY();
    vector<Vec2f> normals(width * height);
    auto normalsView =
        gsl::as_array_view(normals.data(), gsl::dim<>(height), gsl::dim<>(width));
    for (auto idx : normalsView.bounds()) {
        const auto normal =
            normalize(Vec3f{ 2.0f * gridStepY * (geoTiff.getHeightAt(idx, -1, 0) - geoTiff.getHeightAt(idx, 1, 0)),
                4.0f * gridStepX * gridStepY,
                2.0f * gridStepX * (geoTiff.getHeightAt(idx, 0, -1) - geoTiff.getHeightAt(idx, 0, 1)) });
        normalsView[idx] = normal.xz();
    }

    normalsTex = CreateTexture2D(
        device, Texture2DDesc{ DXGI_FORMAT_R32G32_FLOAT, static_cast<UINT>(width),
        static_cast<UINT>(height) }
        .mipLevels(1),
        { normals.data(), width * sizeof(normals[0]) });
    normalsSRV = CreateShaderResourceView(device, normalsTex.Get());
}

void HeightField::generateHeightFieldGeometry(ID3D11Device* device, const GeoTiff& geoTiff) {
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

    const auto uvStepX = 1.0f / geoTiff.getTiffWidth();
    const auto uvStepY = 1.0f / geoTiff.getTiffHeight();
    const auto gridStepX = geoTiff.getGridStepMetersX();
    const auto gridStepY = geoTiff.getGridStepMetersY();
    auto vertices = vector<Vertex>(square(blockSize + 1));
    for (auto y = 0; y < geoTiff.getTiffHeight(); y += blockSize) {
        for (auto x = 0; x < geoTiff.getTiffWidth(); x += blockSize) {
            auto destVertex = 0;
            for (auto blockY = 0; blockY <= blockSize; ++blockY) {
                for (auto blockX = 0; blockX <= blockSize; ++blockX) {
                    const auto localX = min(x + blockX, geoTiff.getTiffWidth());
                    const auto localY = min(y + blockY, geoTiff.getTiffHeight());
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
}
