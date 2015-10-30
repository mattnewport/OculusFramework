#pragma once

#include "label.h"
#include "Win32_DX11AppUtil.h"

#include "mathconstants.h"
#include "vector.h"
#include "matrix.h"

#include <cstdint>
#include <memory>
#include <string>

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

    float getRotationAngle() const { return rotationAngle; }
    void setRotationAngle(float x) {
        rotationAngle = x < 0.0f ? x + 2.0f * mathlib::pif
                                 : x > 2.0f * mathlib::pif ? x - 2.0f * mathlib::pif : x;
    }

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
                        PipelineStateObjectManager& pipelineStateObjectManager);
    void loadCreeksShapeFile(const GeoTiff& geoTiff);
    void generateCreeksTexture(DirectX11& dx11, ID3D11Device* device, ID3D11DeviceContext* context,
                               PipelineStateObjectManager& pipelineStateObjectManager);
    void renderCreeksTexture(DirectX11& dx11, ID3D11Device* device, ID3D11DeviceContext* context,
                             PipelineStateObjectManager& pipelineStateObjectManager);
    void loadLakesShapeFile(const GeoTiff& geoTiff);
    void generateLakesTexture(DirectX11& dx11, ID3D11Device* device, ID3D11DeviceContext* context,
                              PipelineStateObjectManager& pipelineStateObjectManager);
    void renderLakesTexture(DirectX11& dx11, ID3D11Device* device, ID3D11DeviceContext* context,
                            PipelineStateObjectManager& pipelineStateObjectManager);

    struct LabeledPoint {
        mathlib::Vec2f latLong;
        std::string label;
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

    struct Arc {
        std::string name;
        std::vector<mathlib::Vec2f> latLongs;
        std::vector<mathlib::Vec2f> pixPositions;
    };
    std::vector<Arc> creeks;
    ID3D11Texture2DPtr creeksTex;
    ID3D11ShaderResourceViewPtr creeksSrv;

    struct Polygon {
        std::string laknameen;
        std::string rivnameen;
        std::vector<mathlib::Vec2f> latLongs;
        std::vector<mathlib::Vec2f> pixPositions;
        std::vector<int> partStarts;
        std::vector<int> partTypes;
    };
    std::vector<Polygon> lakes;
    ID3D11Texture2DPtr lakesTex;
    ID3D11ShaderResourceViewPtr lakesSrv;

    struct TerrainParameters {
        mathlib::Vec4f hydroLayerAlphas;
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

