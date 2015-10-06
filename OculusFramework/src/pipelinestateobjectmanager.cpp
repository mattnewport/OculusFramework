#include "pipelinestateobjectmanager.h"

#include "d3dhelper.h"
#include "pipelinestateobject.h"

using namespace std;

PipelineStateObjectManager::ResourceType* PipelineStateObjectManager::createResource(
    const KeyType& key) {
    PipelineStateObject* pso = new PipelineStateObject{};
    pso->vertexShader = stateManagers.vertexShaderManager.get(key.vertexShader);
    pso->pixelShader = stateManagers.pixelShaderManager.get(key.pixelShader);
    pso->blendState = stateManagers.blendStateManager.get(key.blendState);
    pso->rasterizerState = stateManagers.rasterizerStateManager.get(key.rasterizerState);
    pso->depthStencilState = stateManagers.depthStencilStateManager.get(key.depthStencilState);
    pso->inputLayout = stateManagers.inputLayoutManager.get(
        InputLayoutKey{ key.inputElementDescs, pso->vertexShader.get()->inputSignature.Get() });
    pso->primitiveTopology = key.primitiveTopology;
    return pso;
}

size_t hashHelper(const PipelineStateObjectDesc & x) {
    return hashCombine(hash<VertexShaderManager::KeyType>{}(x.vertexShader),
        hash<PixelShaderManager::KeyType>{}(x.pixelShader),
        hash<BlendStateManager::KeyType>{}(x.blendState),
        hash<RasterizerStateManager::KeyType>{}(x.rasterizerState),
        hash<DepthStencilStateManager::KeyType>{}(x.depthStencilState),
        hash<InputElementDescs>{}(x.inputElementDescs), x.primitiveTopology);
}

bool operator==(const PipelineStateObjectDesc& a, const PipelineStateObjectDesc& b) {
    auto tupleify = [](const PipelineStateObjectDesc& a) {
        return std::make_tuple(a.vertexShader, a.pixelShader, a.blendState, a.rasterizerState,
            a.depthStencilState, a.inputElementDescs, a.primitiveTopology);
    };
    return tupleify(a) == tupleify(b);
}

