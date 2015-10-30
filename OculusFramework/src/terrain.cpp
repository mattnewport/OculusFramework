#include "terrain.h"

#include "d2dhelper.h"
#include "d3dhelper.h"
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

#include <algorithm>
#include <fstream>
#include <string>
#include <sstream>
#include <tuple>
#include <unordered_map>


using namespace std;

using namespace mathlib;
using namespace util;

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
        const auto minmax = minmax_element(begin(heights), end(heights));
        minElevationMeters = *minmax.first;
        maxElevationMeters = *minmax.second;

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
        const auto topRight = pixXYToLatLong(tifWidth, 0);
        const auto bottomLeft = pixXYToLatLong(0, tifHeight);
        widthMeters = latLongDist(topLeft, topRight);
        heightMeters = latLongDist(topLeft, bottomLeft);
    }

    auto getTiffWidth() const { return tifWidth; }
    auto getWidthMeters() const { return widthMeters; }
    auto getGridStepMetersX() const { return widthMeters / tifWidth; }
    auto getTiffHeight() const { return tifHeight; }
    auto getHeightMeters() const { return heightMeters; }
    auto getGridStepMetersY() const { return heightMeters / tifHeight; }
    auto getMinElevationMeters() const { return minElevationMeters; }
    auto getMaxElevationMeters() const { return maxElevationMeters; }
    const auto& getHeights() const { return heights; }
    auto getHeightsView() const {
        return gsl::as_array_view(heights.data(), gsl::dim<>(tifHeight), gsl::dim<>(tifWidth));
    }
    const auto& getGtifDefinition() const { return gtifDefinition; }
    Vec2f pixXYToLatLong(int x, int y) const {
        double longitude = x;
        double latitude = y;
        if (!GTIFImageToPCS(gtif.get(), &longitude, &latitude))
            throw runtime_error{"Error converting pixel coordinates to lat/long."};
        return {static_cast<float>(latitude), static_cast<float>(longitude)};
    }
    Vec2f topLeftLatLong() const { return pixXYToLatLong(0, 0); }
    Vec2f latLongToPixXY(double latitude, double longitude) const {
        if (!GTIFPCSToImage(gtif.get(), &longitude, &latitude))
            throw runtime_error{"Error converting pixel coordinates to lat/long."};
        return {to<float>(longitude), to<float>(latitude)};
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
    float minElevationMeters = 0.0f;
    float maxElevationMeters = 0.0f;
    vector<uint16_t> heights;
    unique_ptr<TIFF, void (*)(TIFF*)> tif{nullptr, XTIFFClose};
    unique_ptr<GTIF, void (*)(GTIF*)> gtif{nullptr, GTIFFree};
    GTIFDefn gtifDefinition = {};
};

void HeightField::AddVertices(DirectX11& dx11, ID3D11Device* device, ID3D11DeviceContext* context,
                              PipelineStateObjectManager& pipelineStateObjectManager,
                              Texture2DManager& texture2DManager) {
    auto geoTiff = GeoTiff{R"(data\cdem_dem_150528_015119.tif)"};
    const auto& heights = geoTiff.getHeights();

    heightFieldWidth = geoTiff.getTiffWidth();
    heightFieldHeight = geoTiff.getTiffHeight();
    tie(heightsTex, heightsSRV) = CreateTexture2DAndShaderResourceView(
        device, Texture2DDesc{DXGI_FORMAT_R16_UINT, static_cast<UINT>(geoTiff.getTiffWidth()),
                              static_cast<UINT>(geoTiff.getTiffHeight())}
                    .mipLevels(1),
        {heights.data(), geoTiff.getTiffWidth() * sizeof(heights[0])});
    midElevationOffset = translationMat4f(
        {0.0f, -0.5f * (geoTiff.getMaxElevationMeters() - geoTiff.getMinElevationMeters()), 0.0f});

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

    loadTopographicFeaturesShapeFile();
    generateLabels(geoTiff, device, context, pipelineStateObjectManager);

    loadCreeksShapeFile(geoTiff);
    generateCreeksTexture(dx11, device, context, pipelineStateObjectManager);

    loadLakesShapeFile(geoTiff);
    generateLakesTexture(dx11, device, context, pipelineStateObjectManager);

    terrainParametersConstantBuffer =
        CreateBuffer(device, BufferDesc{roundUpConstantBufferSize(sizeof(TerrainParameters)),
                                        D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC}
                                 .cpuAccessFlags(D3D11_CPU_ACCESS_WRITE),
                     "HeightField::terrainParametersConstantBuffer");
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

    [this, &object, &dx11] {
        auto mapHandle = MapHandle{ dx11.Context.Get(), terrainParametersConstantBuffer.Get(), 0,
            D3D11_MAP_WRITE_DISCARD, 0 };
        memcpy(mapHandle.mappedSubresource().pData, &terrainParameters, sizeof(terrainParameters));
    }();

    VSSetConstantBuffers(context, objectConstantBufferOffset, {objectConstantBuffer.Get()});
    VSSetShaderResources(context, 0, {heightsSRV.Get()});
    context->IASetIndexBuffer(IndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    PSSetConstantBuffers(context, objectConstantBufferOffset, { objectConstantBuffer.Get() });
    PSSetConstantBuffers(context, 3, {terrainParametersConstantBuffer.Get()});
    PSSetShaderResources(context, materialSRVOffset,
                         {shapesTex.get(), normalsSRV.Get(), creeksSrv.Get(), lakesSrv.Get()});
    for (const auto& vertexBuffer : VertexBuffers) {
        IASetVertexBuffers(context, 0, {vertexBuffer.Get()}, {to<UINT>(sizeof(Vertex))});
        context->DrawIndexed(Indices.size(), 0, 0);
    }

    if (renderLabels) {
        dx11.applyState(*context, *labelsPipelineStateObject.get());
        context->IASetIndexBuffer(labelsIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
        IASetVertexBuffers(context, 0, {labelsVertexBuffer.Get()}, {to<UINT>(sizeof(LabelVertex))});
        for (auto i = 0u; i < topographicFeatureLabels.size(); ++i) {
            if (displayedConciscodes[topographicFeatures[i].conciscode]) {
                PSSetShaderResources(context, materialSRVOffset, { topographicFeatureLabels[i].srv() });
                context->DrawIndexed(6, i * 6, 0);
            }
        }

        dx11.applyState(*context, *labelFlagpolePso.get());
        IASetVertexBuffers(context, 0, {labelFlagpolesVertexBuffer.Get()},
                           {to<UINT>(sizeof(LabelFlagpoleVertex))});
        context->Draw(labelFlagpoleVertices.size(), 0);
    }
}

class ShapeFile {
public:
    struct DBFFieldInfo {
        int index = 0;
        DBFFieldType type = FTInvalid;
        char name[12] = {};
        int width = 0;
        int decimals = 0;
    };

    ShapeFile(const char* filename) {
        shapeHandle = {SHPOpen(filename, "rb"), SHPClose};
        if (!shapeHandle) throw runtime_error{"Failed to open shapefile."};
        auto numEntities = 0;
        auto shapeType = 0;
        auto minBounds = Vector<double, 4>{};
        auto maxBounds = Vector<double, 4>{};
        SHPGetInfo(shapeHandle.get(), &numEntities, &shapeType, minBounds.data(), maxBounds.data());
        shapes.reserve(numEntities);
        for (int i = 0; i < numEntities; ++i) {
            shapes.emplace_back(SHPReadObject(shapeHandle.get(), i), SHPDestroyObject);
        }

        dbfHandle = {DBFOpen(filename, "rb"), DBFClose};
        const auto dbfFieldCount = DBFGetFieldCount(dbfHandle.get());
        const auto dbfRecordCount = DBFGetRecordCount(dbfHandle.get());
        assert(dbfRecordCount == numEntities);
        for (int i = 0; i < dbfFieldCount; ++i) {
            auto fieldInfo = ShapeFile::DBFFieldInfo{};
            fieldInfo.type = DBFGetFieldInfo(dbfHandle.get(), i, fieldInfo.name, &fieldInfo.width,
                                             &fieldInfo.decimals);
            fieldInfos[fieldInfo.name] = fieldInfo;
        }
    }

    auto readString(int shapeId, const char* fieldName) {
        assert(to<size_t>(shapeId) < shapes.size());
        assert(fieldInfos.find(fieldName) != end(fieldInfos) &&
               fieldInfos[fieldName].type == FTString);
        const auto fieldIndex = DBFGetFieldIndex(dbfHandle.get(), fieldName);
        const auto fieldChars = DBFReadStringAttribute(dbfHandle.get(), shapeId, fieldIndex);
        return string{fieldChars};
    };

    auto readDouble(int shapeId, const char* fieldName) {
        assert(to<size_t>(shapeId) < shapes.size());
        assert(fieldInfos.find(fieldName) != end(fieldInfos) &&
               fieldInfos[fieldName].type == FTDouble);
        const auto fieldIndex = DBFGetFieldIndex(dbfHandle.get(), fieldName);
        const auto fieldValue = DBFReadDoubleAttribute(dbfHandle.get(), shapeId, fieldIndex);
        return fieldValue;
    }

    const auto& getShapes() { return shapes; }

private:
    unique_ptr<SHPInfo, void (*)(SHPHandle)> shapeHandle = {nullptr, SHPClose};
    using ShpObjectPtr = unique_ptr<SHPObject, void(*)(SHPObject*)>;
    vector<ShpObjectPtr> shapes;
    unique_ptr<DBFInfo, void (*)(DBFHandle)> dbfHandle{nullptr, DBFClose};
    unordered_map<string, DBFFieldInfo> fieldInfos;
};

void HeightField::loadTopographicFeaturesShapeFile() {
    auto shapeFile = ShapeFile{R"(data\canvec_150528_015119_shp\to_1580009_0.shp)"};

    for (const auto& shape : shapeFile.getShapes()) {
        const auto nameen = shapeFile.readString(shape->nShapeId, "nameen");
        const auto longitude = float(*shape->padfX);
        const auto latitude = float(*shape->padfY);
        const auto conciscode = to<int>(shapeFile.readDouble(shape->nShapeId, "conciscode"));
        displayedConciscodes[conciscode] = true;
        topographicFeatures.push_back({Vec2f{latitude, longitude}, nameen, conciscode});
    }
}

void HeightField::generateLabels(const GeoTiff& geoTiff, ID3D11Device* device,
                                 ID3D11DeviceContext* context,
                                 PipelineStateObjectManager& pipelineStateObjectManager) {
    for (const auto& feature : topographicFeatures) {
        topographicFeatureLabels.emplace_back(device, context, feature.label.c_str());
        const auto& label = topographicFeatureLabels.back();
        const auto labelPixelPos =
            Vec2i{geoTiff.latLongToPixXY(feature.latLong.x(), feature.latLong.y())};
        const auto labelSize = Vec2f{label.getWidth(), label.getHeight()} * 15.0f;
        const auto radius = to<int>(0.5f * std::max(labelSize.x() / geoTiff.getGridStepMetersX(),
                                                    labelSize.x() / geoTiff.getGridStepMetersY()));
        const auto top = labelPixelPos.y() - radius;
        const auto left = labelPixelPos.x() - radius;
        const auto bottom = labelPixelPos.y() + radius;
        const auto right = labelPixelPos.x() + radius;
        auto labelHeight = float(
            geoTiff.getHeightAt(gsl::index<2, int>{labelPixelPos.y(), labelPixelPos.x()}, 0, 0));
        const auto topLeft = geoTiff.topLeftLatLong();
        const auto xOffset = -0.5f * geoTiff.getWidthMeters();
        const auto yOffset = -0.5f * geoTiff.getHeightMeters();
        const auto labelX =
            geoTiff.latLongDist(topLeft, Vec2f{topLeft.x(), feature.latLong.y()}) + xOffset;
        const auto labelZ =
            geoTiff.latLongDist(topLeft, Vec2f{feature.latLong.x(), topLeft.y()}) + yOffset;
        labelFlagpoleVertices.push_back({Vec3f{labelX, labelHeight, labelZ}});
        for (int y = top; y < bottom; ++y) {
            for (int x = left; x < right; ++x) {
                labelHeight = std::max(labelHeight,
                                       float(geoTiff.getHeightAt(gsl::index<2, int>{y, x}, 0, 0)));
            }
        }
        labelFlagpoleVertices.push_back({Vec3f{labelX, labelHeight, labelZ}});
        const auto labelPos = Vec3f{labelX, labelHeight, labelZ};
        const auto labelColor = 0xffffffffu;
        const auto baseVertexIdx = uint16_t(labelsVertices.size());
        labelsVertices.push_back(
            {labelPos, labelColor, Vec2f{1.0f, 1.0f}, Vec2f{0.5f * labelSize.x(), 0.0f}});
        labelsVertices.push_back(
            {labelPos, labelColor, Vec2f{0.0f, 1.0f}, Vec2f{-0.5f * labelSize.x(), 0.0f}});
        labelsVertices.push_back(
            {labelPos, labelColor, Vec2f{0.0f, 0.0f}, Vec2f{-0.5f * labelSize.x(), labelSize.y()}});
        labelsVertices.push_back(
            {labelPos, labelColor, Vec2f{1.0f, 0.0f}, Vec2f{0.5f * labelSize.x(), labelSize.y()}});
        uint16_t indices[] = {0, 1, 2, 0, 2, 3};
        for (auto& idx : indices) idx += baseVertexIdx;
        labelsIndices.insert(end(labelsIndices), begin(indices), end(indices));
    }
    labelsVertexBuffer = CreateVertexBuffer(device, const_array_view(labelsVertices));
    labelsIndexBuffer = CreateIndexBuffer(device, const_array_view(labelsIndices));
    labelFlagpolesVertexBuffer =
        CreateVertexBuffer(device, const_array_view(labelFlagpoleVertices));

    PipelineStateObjectDesc labelsDesc;
    labelsDesc.vertexShader = "labelvs.hlsl";
    labelsDesc.pixelShader = "labelps.hlsl";
    labelsDesc.inputElementDescs = HeightFieldLabelVertexInputElementDescs;
    labelsPipelineStateObject = pipelineStateObjectManager.get(labelsDesc);

    PipelineStateObjectDesc labelFlagpolesDesc;
    labelFlagpolesDesc.vertexShader = "labelflagpolevs.hlsl";
    labelFlagpolesDesc.pixelShader = "labelflagpoleps.hlsl";
    labelFlagpolesDesc.inputElementDescs = HeightFieldLabelFlagpoleVertexInputElementDescs;
    labelFlagpolesDesc.primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
    labelFlagpolePso = pipelineStateObjectManager.get(labelFlagpolesDesc);
}

void HeightField::loadCreeksShapeFile(const GeoTiff& geoTiff) {
    auto shapeFile = ShapeFile{R"(data\canvec_150528_015119_shp\hd_1470009_1.shp)"};

    for (const auto& s : shapeFile.getShapes()) {
        assert(s->nSHPType == SHPT_ARC);
        assert(s->nParts == 1);
        const auto nameen = shapeFile.readString(s->nShapeId, "nameen");
        auto arc = Arc{nameen, {vector<Vec2f>(s->nVertices)}, {vector<Vec2f>(s->nVertices)}};
        for (size_t i = 0; i < arc.latLongs.size(); ++i) {
            arc.latLongs[i] = Vec2f{to<float>(s->padfY[i]), to<float>(s->padfX[i])};
            const auto pixelPos = geoTiff.latLongToPixXY(arc.latLongs[i].x(), arc.latLongs[i].y());
            arc.pixPositions[i] = Vec2f{pixelPos};
        }
        creeks.push_back(arc);
    }
}

void HeightField::generateCreeksTexture(DirectX11& dx11, ID3D11Device* device,
                                        ID3D11DeviceContext* context,
                                        PipelineStateObjectManager& pipelineStateObjectManager) {
    tie(creeksTex, creeksSrv) = CreateTexture2DAndShaderResourceView(
        device, Texture2DDesc{DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, to<UINT>(heightFieldWidth),
                              to<UINT>(heightFieldHeight)}
                    .bindFlags(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET)
                    .miscFlags(D3D11_RESOURCE_MISC_GENERATE_MIPS),
        "HeightField::creeks texture");
    renderCreeksTexture(dx11, device, context, pipelineStateObjectManager);
}

void HeightField::renderCreeksTexture(DirectX11& dx11, ID3D11Device* device,
    ID3D11DeviceContext* context,
    PipelineStateObjectManager& pipelineStateObjectManager) {
    auto renderTex = CreateTexture2DAndRenderTargetView(
        device, Texture2DDesc{DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, to<UINT>(heightFieldWidth),
                              to<UINT>(heightFieldHeight)}
                    .bindFlags(D3D11_BIND_RENDER_TARGET)
                    .mipLevels(1)
                    .sampleDesc({8, 0}),
        "HeightField::creeks render target texture");
    const auto longestArcLength =
        std::max_element(begin(creeks), end(creeks), [](const auto& x, const auto& y) {
            return x.latLongs.size() < y.latLongs.size();
        })->latLongs.size();

    ID3D11BufferPtr vb = CreateBuffer(
        device,
        BufferDesc{ longestArcLength * sizeof(Vec2f), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC }
        .cpuAccessFlags(D3D11_CPU_ACCESS_WRITE),
        __func__);
    struct Vertex2D {
        Vec2f position;
    };
    const auto Vertex2DInputElementDescs = { MAKE_INPUT_ELEMENT_DESC(Vertex2D, position) };
    PipelineStateObjectDesc psod{};
    psod.vertexShader = "simple2dvs.hlsl";
    psod.pixelShader = "simple2dps.hlsl";
    psod.inputElementDescs = Vertex2DInputElementDescs;
    psod.depthStencilState = DepthStencilDesc{}.depthEnable(FALSE);
    psod.rasterizerState = RasterizerDesc{}.multisampleEnable(TRUE).antialiasedLineEnable(TRUE);
    psod.primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
    auto pso = pipelineStateObjectManager.get(psod);
    dx11.applyState(*context, *pso.get());
    PSSetShaderResources(context, 0u, { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr });
    context->ClearRenderTargetView(get<1>(renderTex).Get(), std::data({0.0f, 0.0f, 0.0f, 0.0f}));
    OMSetRenderTargets(context, {get<1>(renderTex).Get()});
    RSSetViewports(context, {{0.0f, 0.0f, to<float>(heightFieldWidth), to<float>(heightFieldHeight),
                              0.0f, 1.0f}});
    for (const auto& c : creeks) {
        IASetVertexBuffers(context, 0u, { nullptr }, { 0u });
        {
            MapHandle vbh{ context, vb.Get() };
            std::transform(begin(c.pixPositions), end(c.pixPositions),
                std::raw_storage_iterator<Vec2f*, Vec2f>{
                reinterpret_cast<Vec2f*>(vbh.mappedSubresource().pData)},
                [this](const auto& pp) {
                    return 2.0f * Vec2f{pp.x() / heightFieldWidth, 1.0f - pp.y() / heightFieldHeight} - Vec2f{1.0f};
                });
        }
        IASetVertexBuffers(context, 0u, { vb.Get() }, { to<UINT>(sizeof(Vertex2D)) });
        context->Draw(c.latLongs.size(), 0);
    }
    OMSetRenderTargets(context, { nullptr });
    context->ResolveSubresource(creeksTex.Get(), 0, get<0>(renderTex).Get(), 0,
                                DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    context->GenerateMips(creeksSrv.Get());
}

void HeightField::generateLakesTexture(DirectX11& dx11, ID3D11Device* device,
                                       ID3D11DeviceContext* context,
                                       PipelineStateObjectManager& pipelineStateObjectManager) {
    tie(lakesTex, lakesSrv) = CreateTexture2DAndShaderResourceView(
        device, Texture2DDesc{DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, to<UINT>(heightFieldWidth),
                              to<UINT>(heightFieldHeight)}
                    .bindFlags(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET)
                    .miscFlags(D3D11_RESOURCE_MISC_GENERATE_MIPS),
        "HeightField::lakes texture");
    renderLakesTexture(dx11, device, context, pipelineStateObjectManager);
}

void HeightField::renderLakesTexture(DirectX11& /*dx11*/, ID3D11Device* device,
    ID3D11DeviceContext* context,
    PipelineStateObjectManager& /*pipelineStateObjectManager*/) {
    auto renderTex = CreateTexture2D(
        device, Texture2DDesc{DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, to<UINT>(heightFieldWidth),
                              to<UINT>(heightFieldHeight)}
                    .bindFlags(D3D11_BIND_RENDER_TARGET)
                    .mipLevels(1)
                    .sampleDesc({8, 0}),
        "HeightField::lakes render target texture");

    ID2D1FactoryPtr d2d1Factory;
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d1Factory.ReleaseAndGetAddressOf());
    DrawToRenderTargetTexture(d2d1Factory.Get(), renderTex.Get(), 
        [&lakes = this->lakes](ID2D1Factory* factory, ID2D1RenderTarget * d2d1Rt) {
        auto brush = CreateSolidColorBrush(d2d1Rt, D2D1::ColorF(D2D1::ColorF::Red));
        auto fillBrush = CreateSolidColorBrush(d2d1Rt, D2D1::ColorF(D2D1::ColorF::Lime));

        d2d1Rt->SetTransform(D2D1::IdentityMatrix());
        d2d1Rt->Clear(D2D1::ColorF{D2D1::ColorF::Black});
        for (const auto& c : lakes) {
            const auto numParts = to<int>(c.partStarts.size());
            auto pathGeometry = CreatePathGeometry(factory);
            auto geometrySink = Open(pathGeometry.Get());
            for (int i = 0; i < numParts; ++i) {
                const auto startIndex = c.partStarts[i];
                const auto startEndPoint = c.pixPositions[startIndex];
                geometrySink->BeginFigure(D2D1::Point2F(startEndPoint.x(), startEndPoint.y()),
                                          D2D1_FIGURE_BEGIN_FILLED);
                const auto endIndex =
                    i + 1 < numParts ? c.partStarts[i + 1] : to<int>(c.pixPositions.size());
                for (int j = startIndex + 1; j < endIndex; ++j) {
                    const auto point = c.pixPositions[j];
                    geometrySink->AddLine(D2D1::Point2F(point.x(), point.y()));
                }
                geometrySink->EndFigure(D2D1_FIGURE_END_CLOSED);
            }
            geometrySink->Close();
            d2d1Rt->DrawGeometry(pathGeometry.Get(), brush.Get(), 3.0f);
            d2d1Rt->FillGeometry(pathGeometry.Get(), fillBrush.Get());
        }
    });

    context->ResolveSubresource(lakesTex.Get(), 0, renderTex.Get(), 0,
                                DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    context->GenerateMips(lakesSrv.Get());
}

void HeightField::loadLakesShapeFile(const GeoTiff& geoTiff) {
    auto shapeFile = ShapeFile{R"(data\canvec_150528_015119_shp\hd_1480009_2.shp)"};
    for (const auto& s : shapeFile.getShapes()) {
        assert(s->nSHPType == SHPT_POLYGON);
        const auto laknameen = shapeFile.readString(s->nShapeId, "laknameen");
        const auto rivnameen = shapeFile.readString(s->nShapeId, "rivnameen");
        auto poly = Polygon{
            laknameen, rivnameen, {vector<Vec2f>(s->nVertices)}, {vector<Vec2f>(s->nVertices)}};
        assert(s->nParts > 0);
        for (int i = 0; i < s->nParts; ++i) {
            poly.partStarts.push_back(s->panPartStart[i]);
            const auto partType = s->panPartType[i];
            assert(partType == SHPP_RING);
            poly.partTypes.push_back(partType);
        }
        for (size_t i = 0; i < poly.latLongs.size(); ++i) {
            poly.latLongs[i] = Vec2f{to<float>(s->padfY[i]), to<float>(s->padfX[i])};
            const auto pixelPos =
                geoTiff.latLongToPixXY(poly.latLongs[i].x(), poly.latLongs[i].y());
            poly.pixPositions[i] = Vec2f{pixelPos};
        }
        lakes.push_back(poly);
    }
}

void HeightField::showGui() {
    if (ImGui::CollapsingHeader("Terrain")) {
        ImGui::SliderFloat("Scale", &scale, 1e-5f, 1e-3f, "scale = %.6f", 3.0f);
        ImGui::Checkbox("Show topographic feature labels", &renderLabels);
        static bool showLakeOutlines = true;
        ImGui::Checkbox("Show lake outlines", &showLakeOutlines);
        terrainParameters.hydroLayerAlphas.x() = showLakeOutlines ? 1.0f : 0.0f;
        static bool showLakes = true;
        ImGui::Checkbox("Show lakes", &showLakes);
        terrainParameters.hydroLayerAlphas.y() = showLakes ? 1.0f : 0.0f;
        static bool showCreeks = true;
        ImGui::Checkbox("Show creeks", &showCreeks);
        terrainParameters.hydroLayerAlphas.z() = showCreeks ? 1.0f : 0.0f;
        static bool showContours = false;
        ImGui::Checkbox("Show contours", &showContours);
        terrainParameters.contours = showContours ? 1.0f : 0.0f;
        if (ImGui::CollapsingHeader("Topographic features")) {
            for (auto& code : displayedConciscodes) {
                ImGui::Checkbox(conciscodeNameMap[code.first].c_str(), &code.second);
            }
        }
    }
}

void HeightField::generateNormalMap(ID3D11Device* device, const GeoTiff& geoTiff) {
    const auto width = geoTiff.getTiffWidth();
    const auto height = geoTiff.getTiffHeight();
    const auto gridStepX = geoTiff.getGridStepMetersX();
    const auto gridStepY = geoTiff.getGridStepMetersY();
    vector<Vec2f> normals(width * height);
    auto normalsView = gsl::as_array_view(normals.data(), gsl::dim<>(height), gsl::dim<>(width));
    for (auto idx : normalsView.bounds()) {
        const auto normal = normalize(Vec3f{
            2.0f * gridStepY * (geoTiff.getHeightAt(idx, -1, 0) - geoTiff.getHeightAt(idx, 1, 0)),
            4.0f * gridStepX * gridStepY,
            2.0f * gridStepX * (geoTiff.getHeightAt(idx, 0, -1) - geoTiff.getHeightAt(idx, 0, 1))});
        normalsView[idx] = normal.xz();
    }

    tie(normalsTex, normalsSRV) = CreateTexture2DAndShaderResourceView(
        device,
        Texture2DDesc{DXGI_FORMAT_R32G32_FLOAT, static_cast<UINT>(width), static_cast<UINT>(height)}
            .mipLevels(1),
        {normals.data(), width * sizeof(normals[0])});
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
    IndexBuffer = CreateIndexBuffer(device, const_array_view(Indices));

    const auto uvStepX = 1.0f / geoTiff.getTiffWidth();
    const auto uvStepY = 1.0f / geoTiff.getTiffHeight();
    const auto gridStepX = geoTiff.getGridStepMetersX();
    const auto gridStepY = geoTiff.getGridStepMetersY();
    const auto xOffset = -0.5f * geoTiff.getWidthMeters();
    const auto yOffset = -0.5f * geoTiff.getHeightMeters();
    auto vertices = vector<Vertex>(square(blockSize + 1));
    for (auto y = 0; y < geoTiff.getTiffHeight(); y += blockSize) {
        for (auto x = 0; x < geoTiff.getTiffWidth(); x += blockSize) {
            auto destVertex = 0;
            for (auto blockY = 0; blockY <= blockSize; ++blockY) {
                for (auto blockX = 0; blockX <= blockSize; ++blockX) {
                    const auto localX = min(x + blockX, geoTiff.getTiffWidth());
                    const auto localY = min(y + blockY, geoTiff.getTiffHeight());
                    vertices[destVertex++] = {
                        {localX * gridStepX + xOffset, localY * gridStepY + yOffset},
                        {(localX + 0.5f) * uvStepX, (localY + 0.5f) * uvStepY}};
                }
            }
            VertexBuffers.push_back(CreateVertexBuffer(device, const_array_view(vertices)));
        }
    }
}

std::unordered_map<int, std::string> HeightField::initConciscodeNameMap() {
    return std::unordered_map<int, string>{
        {5, "Province"},
        {7, "Territory"},
        {10, "City"},
        {20, "Town"},
        {30, "Village"},
        {40, "Hamlet"},
        {45, "District municipality"},
        {50, "Other municipal/district area - major agglomeration"},
        {60, "Other municipal/district area - miscellaneous"},
        {80, "Unincorporated area"},
        {90, "Indian Reserve"},
        {100, "Geographical area"},
        {110, "Conservation area"},
        {115, "Military area"},
        {120, "River"},
        {130, "River feature"},
        {140, "Falls"},
        {150, "Lake"},
        {160, "Spring"},
        {170, "Sea"},
        {180, "Sea"},
        {190, "Undersea feature"},
        {200, "Channel"},
        {210, "Rapids"},
        {220, "Bay"},
        {230, "Cape"},
        {240, "Beach"},
        {250, "Shoal"},
        {260, "Island"},
        {270, "Cliff"},
        {280, "Mountain"},
        {290, "Valley"},
        {300, "Plain"},
        {310, "Cave"},
        {320, "Crater"},
        {330, "Glacier"},
        {340, "Forest"},
        {350, "Low vegetation"},
        {370, "Miscellaneous"},
        {380, "Railway feature"},
        {390, "Road feature"},
        {400, "Air navigation feature"},
        {410, "Marine navigation feature"},
        {420, "Hydraulic construction"},
        {430, "Recreational site"},
        {440, "Natural resources site"},
        {450, "Miscellaneous campsite"},
        {460, "Miscellaneous site"},
    };
}
