#pragma once

#include "d3dhelper.h"
#include "d3dresourcemanagers.h"
#include "hashhelpers.h"
#include "pipelinestateobjectmanager.h"

struct VertexShader {
    ID3DBlobPtr byteCode;
    ID3D11VertexShaderPtr D3DVert;
    ID3DBlobPtr inputSignature;
    std::vector<unsigned char> UniformData;

    struct Uniform {
        char Name[40];
        int Offset, Size;
    };

    int numUniformInfo;
    Uniform UniformInfo[10];

    VertexShader(ID3D11Device* device, ID3DBlob* s, const char* name);

    void SetUniform(const char* name, int n, const float* v);
};

struct PixelShader {
    ID3D11PixelShaderPtr D3DPix;
    std::vector<unsigned char> UniformData;

    struct Uniform {
        char Name[40];
        int Offset, Size;
    };

    int numUniformInfo;
    Uniform UniformInfo[10];

    PixelShader(ID3D11Device* device, ID3DBlob* s, const char* name);

    void SetUniform(const char* name, int n, const float* v);
};

class BlendStateManager : public D3DObjectManagerBase<BlendStateKey, ID3D11BlendState> {
    ResourceType* createResource(const KeyType& key) override;
};

class RasterizerStateManager
    : public D3DObjectManagerBase<RasterizerStateKey, ID3D11RasterizerState> {
    ResourceType* createResource(const KeyType& key) override;
};

class DepthStencilStateManager
    : public D3DObjectManagerBase<DepthStencilStateKey, ID3D11DepthStencilState> {
    ResourceType* createResource(const KeyType& key) override;
};

class VertexShaderManager : public ResourceManagerBase<VertexShaderKey, VertexShader> {
public:
    void setDevice(ID3D11Device* device_) { device = device_; }

private:
    ResourceType* createResource(const KeyType& key) override;
    ID3D11DevicePtr device;
};

class PixelShaderManager : public ResourceManagerBase<std::string, PixelShader> {
public:
    void setDevice(ID3D11Device* device_) { device = device_; }

private:
    ResourceType* createResource(const KeyType& key) override;
    ID3D11DevicePtr device;
};

struct InputLayoutKey {
    InputLayoutKey() = default;
    InputLayoutKey(const InputElementDescs& inputElementDescs_, ID3DBlob* shaderInputSignature_);
    InputLayoutKey(const InputElementDescs& inputElementDescs, const std::string& shaderFilename,
        VertexShaderManager& vertexShaderManager)
        : InputLayoutKey{ inputElementDescs,
        vertexShaderManager.get(shaderFilename).get()->inputSignature.Get() } {}
    InputElementDescs inputElementDescs;
    ID3DBlobPtr shaderInputSignature;
    size_t hashVal;
};

inline bool operator==(const InputLayoutKey& a, const InputLayoutKey& b) {
    return a.hashVal == b.hashVal;
}

namespace std {
    template <>
    struct hash<InputLayoutKey> {
        size_t operator()(const InputLayoutKey& x) const { return x.hashVal; };
    };
}

class InputLayoutManager : public D3DObjectManagerBase<InputLayoutKey, ID3D11InputLayout> {
    ResourceType* createResource(const KeyType& key) override;
};

struct StateManagers {
    StateManagers(ID3D11Device* device) {
        vertexShaderManager.setDevice(device);
        pixelShaderManager.setDevice(device);
        blendStateManager.setDevice(device);
        rasterizerStateManager.setDevice(device);
        depthStencilStateManager.setDevice(device);
        inputLayoutManager.setDevice(device);
    }
    VertexShaderManager vertexShaderManager;
    PixelShaderManager pixelShaderManager;
    BlendStateManager blendStateManager;
    RasterizerStateManager rasterizerStateManager;
    DepthStencilStateManager depthStencilStateManager;
    InputLayoutManager inputLayoutManager;
};

