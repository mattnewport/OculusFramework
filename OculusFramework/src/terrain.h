#pragma once

#include "label.h"
#include "Win32_DX11AppUtil.h"

#include "mathconstants.h"
#include "vector.h"
#include "matrix.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

class GeoTiff;

struct HeightField {
    struct Color {
        unsigned char R, G, B, A;

        Color(unsigned char r = 0, unsigned char g = 0, unsigned char b = 0, unsigned char a = 0xff)
            : R(r), G(g), B(b), A(a) {}
    };
    struct Vertex;
    struct LabelVertex;
    struct LabelFlagpoleVertex;

    HeightField(const mathlib::Vec3f& arg_pos) : Pos{arg_pos}, Rot{0.0f} {}
    void AddVertices(DirectX11& dx11, ID3D11Device* device, ID3D11DeviceContext* context,
                     PipelineStateObjectManager& pipelineStateObjectManager,
                     Texture2DManager& texture2DManager);

    void Render(DirectX11& dx11, ID3D11DeviceContext* context);

    void showGui();

    bool toggleRenderLabels() { return renderLabels = !renderLabels; }

    void setPosition(const mathlib::Vec3f& x) { Pos = x; }
    auto getPosition() const { return Pos; }

    float getRotationAngle() const { return rotationAngle; }
    void setRotationAngle(float x) {
        rotationAngle = x < 0.0f ? x + 2.0f * mathlib::pif
                                 : x > 2.0f * mathlib::pif ? x - 2.0f * mathlib::pif : x;
    }

    float getTerrainScale() { return scale; }
    void setTerrainScale(float x) { scale = mathlib::clamp(x, 1e-6f, 1e-2f); }

private:
    const mathlib::Mat4f& GetMatrix() {
        using namespace mathlib;
        Rot = QuaternionFromAxisAngle<float>(basisVector<Vec3f>(Y), rotationAngle);
        return Mat = midElevationOffset * scaleMat4f(scale) * Mat4FromQuat(Rot) *
                     translationMat4f(Pos);
    }
    void generateNormalMap(ID3D11Device* device, const GeoTiff& geoTiff);
    void generateHeightFieldGeometry(ID3D11Device* device, const GeoTiff& geoTiff);
    void loadTopographicFeaturesShapeFile();
    void generateLabels(const GeoTiff& geoTiff, ID3D11Device* device, ID3D11DeviceContext* context,
                        PipelineStateObjectManager& pipelineStateObjectManager, DirectX11& dx11);
    void loadCreeksShapeFile(const GeoTiff& geoTiff);
    void generateCreeksTexture(DirectX11& dx11);
    void renderCreeksTexture(DirectX11& dx11);
    void loadRoadsShapeFile(const GeoTiff& geoTiff);
    void generateRoadsTexture(DirectX11& dx11);
    void renderRoadsTexture(DirectX11& dx11);
    void loadLakesShapeFile(const GeoTiff& geoTiff);
    void generateLakesTexture(DirectX11& dx11);
    void renderLakesTexture(DirectX11& dx11);
    void loadGlaciersShapeFile(const GeoTiff& geoTiff);
    void generateGlaciersTexture(DirectX11& dx11);
    void renderGlaciersTexture(DirectX11& dx11);

    static std::unordered_map<int, std::string> initConciscodeNameMap();

    struct LabeledPoint {
        mathlib::Vec2f latLong;
        std::string label;
        int conciscode;
    };

    mathlib::Vec3f Pos;
    mathlib::Quatf Rot;
    mathlib::Mat4f Mat;
    mathlib::Mat4f midElevationOffset = mathlib::identityMatrix<mathlib::Mat4f>();
    float rotationAngle = 0.0f;

    PipelineStateObjectManager::ResourceHandle pipelineStateObject;

    std::vector<uint16_t> Indices;
    std::vector<ID3D11BufferPtr> VertexBuffers;
    ID3D11BufferPtr IndexBuffer;
    Texture2DManager::ResourceHandle shapesTex;
    ID3D11BufferPtr objectConstantBuffer;
    ID3D11Texture2DPtr heightsTex;
    ID3D11ShaderResourceViewPtr heightsSRV;
    ID3D11Texture2DPtr normalsTex;
    ID3D11ShaderResourceViewPtr normalsSRV;
    float scale = 1e-4f;

    std::vector<LabeledPoint> topographicFeatures;
    std::vector<Label> topographicFeatureLabels;
    std::vector<LabelVertex> labelsVertices;
    std::vector<uint16_t> labelsIndices;
    ID3D11BufferPtr labelsVertexBuffer;
    ID3D11BufferPtr labelsIndexBuffer;
    PipelineStateObjectManager::ResourceHandle labelsPipelineStateObject;
    std::vector<LabelFlagpoleVertex> labelFlagpoleVertices;
    ID3D11BufferPtr labelFlagpolesVertexBuffer;
    PipelineStateObjectManager::ResourceHandle labelFlagpolePso;
    bool renderLabels = true;
    std::unordered_map<int, std::string> conciscodeNameMap = initConciscodeNameMap();
    std::unordered_map<int, bool> displayedConciscodes;

    struct Arc {
        std::vector<mathlib::Vec2f> latLongs;
        std::vector<mathlib::Vec2f> pixPositions;
        std::vector<std::pair<std::string, std::string>> stringAttributes;
    };
    static std::vector<Arc> loadArcShapeFile(
        const char* filename, const GeoTiff& geoTiff,
        const std::vector<std::string>& requestedStringAttributes);
    static void renderArcsToTexture(const std::vector<Arc>& arcs, ID3D11Texture2D* tex,
                                    DirectX11& dx11, D2D1::ColorF arcColor);

    std::vector<Arc> creeks;
    ID3D11Texture2DPtr creeksTex;
    ID3D11ShaderResourceViewPtr creeksSrv;

    std::vector<Arc> roads;

    struct Polygon {
        std::vector<mathlib::Vec2f> latLongs;
        std::vector<mathlib::Vec2f> pixPositions;
        std::vector<int> partStarts;
        std::vector<std::pair<std::string, std::string>> stringAttributes;
    };
    static std::vector<Polygon> loadPolygonShapeFile(
        const char* filename, const GeoTiff& geoTiff,
        const std::vector<std::string>& requestedStringAttributes);
    static void renderPolygonsToTexture(const std::vector<Polygon>& polygons, ID3D11Texture2D* tex,
                                        DirectX11& dx11, D2D1::ColorF outlineColor,
                                        D2D1::ColorF fillColor);

    std::vector<Polygon> lakes;
    ID3D11Texture2DPtr lakesAndGlaciersTex;
    ID3D11ShaderResourceViewPtr lakesAndGlaciersSrv;

    std::vector<Polygon> glaciers;

    struct TerrainParameters {
        mathlib::Vec4f arcLayerAlphas = {1.0f, 1.0f, 1.0f, 1.0f};
        mathlib::Vec4f hydroLayerAlphas = {1.0f, 1.0f, 1.0f, 1.0f};
        float contours = 0.0f;
    } terrainParameters;
    ID3D11BufferPtr terrainParametersConstantBuffer;

    int heightFieldWidth = 0;
    int heightFieldHeight = 0;
};

struct HeightField::Vertex {
    mathlib::Vec2f position;
    mathlib::Vec2f texcoord;
};
static const auto HeightFieldVertexInputElementDescs = {
    MAKE_INPUT_ELEMENT_DESC(HeightField::Vertex, position),
    MAKE_INPUT_ELEMENT_DESC(HeightField::Vertex, texcoord)};

struct HeightField::LabelVertex {
    mathlib::Vec3f position;
    std::uint32_t color;
    mathlib::Vec2f texcoord;
    mathlib::Vec2f labelSize;
};
static const auto HeightFieldLabelVertexInputElementDescs = {
    MAKE_INPUT_ELEMENT_DESC(HeightField::LabelVertex, position),
    MAKE_INPUT_ELEMENT_DESC(HeightField::LabelVertex, color),
    MAKE_INPUT_ELEMENT_DESC(HeightField::LabelVertex, texcoord),
    MAKE_INPUT_ELEMENT_DESC(HeightField::LabelVertex, labelSize, "texcoord", 1)
};

struct HeightField::LabelFlagpoleVertex {
    mathlib::Vec3f position;
};
static const auto HeightFieldLabelFlagpoleVertexInputElementDescs = {
    MAKE_INPUT_ELEMENT_DESC(HeightField::LabelVertex, position)
};

