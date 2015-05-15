#pragma once

#include "d3dstatemanagers.h"

struct PipelineStateObject {
    VertexShaderManager::ResourceHandle vertexShader;
    PixelShaderManager::ResourceHandle pixelShader;
    BlendStateManager::ResourceHandle blendState;
    RasterizerStateManager::ResourceHandle rasterizerState;
    DepthStencilStateManager::ResourceHandle depthStencilState;
    InputLayoutManager::ResourceHandle inputLayout;
    D3D11_PRIMITIVE_TOPOLOGY primitiveTopology;
};
