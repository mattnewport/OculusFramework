#pragma once

#include <d3d11_1.h>

#include "resourcemanager.h"

using VertexShaderKey = std::string;
using PixelShaderKey = std::string;
using BlendStateKey = CD3D11_BLEND_DESC;
using RasterizerStateKey = CD3D11_RASTERIZER_DESC;
using DepthStencilStateKey = CD3D11_DEPTH_STENCIL_DESC;
using InputElementDescs = std::vector<D3D11_INPUT_ELEMENT_DESC>;

struct PipelineStateObject;

struct PipelineStateObjectDesc {
    PipelineStateObjectDesc()
        : blendState{D3D11_DEFAULT},
          rasterizerState{D3D11_DEFAULT},
          depthStencilState{D3D11_DEFAULT},
          primitiveTopology{D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST} {}
    VertexShaderKey vertexShader;
    PixelShaderKey pixelShader;
    BlendStateKey blendState;
    RasterizerStateKey rasterizerState;
    DepthStencilStateKey depthStencilState;
    InputElementDescs inputElementDescs;
    D3D11_PRIMITIVE_TOPOLOGY primitiveTopology;
};

size_t hashHelper(const PipelineStateObjectDesc& x);

namespace std {
    template <>
    struct hash<PipelineStateObjectDesc> {
        size_t operator()(const PipelineStateObjectDesc& x) const {
            return hashHelper(x);
        }
    };
}

bool operator==(const PipelineStateObjectDesc& a, const PipelineStateObjectDesc& b);

struct StateManagers;

class PipelineStateObjectManager
    : public ResourceManagerBase<PipelineStateObjectDesc, PipelineStateObject> {
public:
    PipelineStateObjectManager(StateManagers& stateManagers_) : stateManagers(stateManagers_) {}

private:
    ResourceType* createResource(const KeyType& key);

    StateManagers& stateManagers;
};
