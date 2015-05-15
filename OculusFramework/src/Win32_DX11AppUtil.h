/************************************************************************************
Filename    :   Win32_DX11AppUtil.h
Content     :   D3D11 and Application/Window setup functionality for RoomTiny
Created     :   October 20th, 2014
Author      :   Tom Heath
Copyright   :   Copyright 2014 Oculus, Inc. All Rights reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*************************************************************************************/

#pragma once

#include "resourcemanager.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "d3dhelper.h"

#include "OVR_Kernel.h"

struct DataBuffer {
    ID3D11BufferPtr D3DBuffer;
    size_t Size;

    DataBuffer(ID3D11Device* device, D3D11_BIND_FLAG use, const void* buffer, size_t size);

    void Refresh(ID3D11DeviceContext* deviceContext, const void* buffer, size_t size);
};

struct ImageBuffer {
    ID3D11Texture2DPtr Tex;
    ID3D11ShaderResourceViewPtr TexSv;
    ID3D11RenderTargetViewPtr TexRtv;
    ID3D11DepthStencilViewPtr TexDsv;
    OVR::Sizei Size = OVR::Sizei{};
    const char* name = nullptr;

    ImageBuffer() = default;
    ImageBuffer(const char* name, ID3D11Device* device, bool rendertarget, bool depth,
                OVR::Sizei size, int mipLevels = 1, bool aa = false);
};

using InputElementDescs = std::vector<D3D11_INPUT_ELEMENT_DESC>;

namespace std {
template <>
struct hash<D3D11_INPUT_ELEMENT_DESC> {
    size_t operator()(const D3D11_INPUT_ELEMENT_DESC& x) const {
        auto lcSemantic = std::string(x.SemanticName);
        for (auto& c : lcSemantic) c = static_cast<char>(tolower(c));
        return hashCombine(std::hash<std::string>{}(lcSemantic), x.SemanticIndex, x.Format,
                           x.InputSlot, x.AlignedByteOffset, x.InputSlotClass,
                           x.InstanceDataStepRate);
    }
};
}

inline bool operator==(const D3D11_INPUT_ELEMENT_DESC& x, const D3D11_INPUT_ELEMENT_DESC& y) {
    auto tupleify = [](const D3D11_INPUT_ELEMENT_DESC& x) {
        auto lcSemantic = std::string(x.SemanticName);
        for (auto& c : lcSemantic) c = static_cast<char>(tolower(c));
        return std::make_tuple(lcSemantic, x.SemanticIndex, x.Format, x.InputSlot,
                               x.AlignedByteOffset, x.InputSlotClass, x.InstanceDataStepRate);
    };
    return tupleify(x) == tupleify(y);
}

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

template <typename T>
struct D3DObjectDeleter {
    void operator()(T* t) { t->Release(); }
};

template <typename Key, typename Resource>
class D3DObjectManagerBase : public ResourceManagerBase<Key, Resource, D3DObjectDeleter<Resource>> {
public:
    void setDevice(ID3D11Device* device_) { device = device_; }

protected:
    ID3D11DevicePtr device;
};

namespace std {
template <>
struct hash<D3D11_RENDER_TARGET_BLEND_DESC> {
    size_t operator()(const D3D11_RENDER_TARGET_BLEND_DESC& x) const {
        return hashCombine(x.BlendEnable, x.SrcBlend, x.DestBlend, x.BlendOp, x.SrcBlendAlpha,
                           x.DestBlendAlpha, x.BlendOpAlpha, x.RenderTargetWriteMask);
    }
};

template <>
struct hash<CD3D11_BLEND_DESC> {
    size_t operator()(const CD3D11_BLEND_DESC& x) const {
        auto seed = hashCombineRange(begin(x.RenderTarget), end(x.RenderTarget));
        return hashCombineWithSeed(seed, x.AlphaToCoverageEnable, x.IndependentBlendEnable);
    }
};
}

inline bool operator==(const D3D11_RENDER_TARGET_BLEND_DESC& a,
                       const D3D11_RENDER_TARGET_BLEND_DESC& b) {
    auto tupleify = [](const D3D11_RENDER_TARGET_BLEND_DESC& x) {
        return std::make_tuple(x.BlendEnable, x.SrcBlend, x.DestBlend, x.BlendOp, x.SrcBlendAlpha,
                               x.DestBlendAlpha, x.BlendOpAlpha, x.RenderTargetWriteMask);
    };
    return tupleify(a) == tupleify(b);
}

inline bool operator==(const CD3D11_BLEND_DESC& a, const CD3D11_BLEND_DESC& b) {
    auto tupleify = [](const CD3D11_BLEND_DESC& x) {
        return std::make_tuple(x.AlphaToCoverageEnable, x.IndependentBlendEnable);
    };
    return tupleify(a) == tupleify(b) &&
           std::equal(std::begin(a.RenderTarget), std::end(a.RenderTarget),
                      std::begin(b.RenderTarget), std::end(b.RenderTarget));
}

class BlendStateManager : public D3DObjectManagerBase<CD3D11_BLEND_DESC, ID3D11BlendState> {
    ResourceType* createResource(const KeyType& key) override;
};

namespace std {
template <>
struct hash<CD3D11_RASTERIZER_DESC> {
    size_t operator()(const CD3D11_RASTERIZER_DESC& x) const {
        return hashCombine(x.FillMode, x.CullMode, x.FrontCounterClockwise, x.DepthBias,
                           x.DepthBiasClamp, x.SlopeScaledDepthBias, x.DepthClipEnable,
                           x.ScissorEnable, x.MultisampleEnable, x.AntialiasedLineEnable);
    }
};
}

inline bool operator==(const CD3D11_RASTERIZER_DESC& a, const CD3D11_RASTERIZER_DESC& b) {
    auto tupleify = [](const CD3D11_RASTERIZER_DESC& a) {
        return std::make_tuple(a.FillMode, a.CullMode, a.FrontCounterClockwise, a.DepthBias,
                               a.DepthBiasClamp, a.SlopeScaledDepthBias, a.DepthClipEnable,
                               a.ScissorEnable, a.MultisampleEnable, a.AntialiasedLineEnable);
    };
    return tupleify(a) == tupleify(b);
}

class RasterizerStateManager
    : public D3DObjectManagerBase<CD3D11_RASTERIZER_DESC, ID3D11RasterizerState> {
    ResourceType* createResource(const KeyType& key) override;
};

namespace std {
template <>
struct hash<D3D11_DEPTH_STENCILOP_DESC> {
    size_t operator()(const D3D11_DEPTH_STENCILOP_DESC& x) const {
        return hashCombine(x.StencilFailOp, x.StencilDepthFailOp, x.StencilPassOp, x.StencilFunc);
    }
};

template <>
struct hash<CD3D11_DEPTH_STENCIL_DESC> {
    size_t operator()(const CD3D11_DEPTH_STENCIL_DESC& x) const {
        return hashCombine(x.DepthEnable, x.DepthWriteMask, x.DepthFunc, x.StencilEnable,
                           x.StencilReadMask, x.StencilWriteMask,
                           std::hash<D3D11_DEPTH_STENCILOP_DESC>{}(x.FrontFace),
                           std::hash<D3D11_DEPTH_STENCILOP_DESC>{}(x.BackFace));
    }
};
}

inline bool operator==(const D3D11_DEPTH_STENCILOP_DESC& a, const D3D11_DEPTH_STENCILOP_DESC& b) {
    auto tupleify = [](const D3D11_DEPTH_STENCILOP_DESC& x) {
        return std::make_tuple(x.StencilFailOp, x.StencilDepthFailOp, x.StencilPassOp,
                               x.StencilFunc);
    };
    return tupleify(a) == tupleify(b);
}

inline bool operator==(const CD3D11_DEPTH_STENCIL_DESC& a, const CD3D11_DEPTH_STENCIL_DESC& b) {
    auto tupleify = [](const CD3D11_DEPTH_STENCIL_DESC& x) {
        return std::make_tuple(x.DepthEnable, x.DepthWriteMask, x.DepthFunc, x.StencilEnable,
                               x.StencilReadMask, x.StencilWriteMask, x.FrontFace, x.BackFace);
    };
    return tupleify(a) == tupleify(b);
}

class DepthStencilStateManager
    : public D3DObjectManagerBase<CD3D11_DEPTH_STENCIL_DESC, ID3D11DepthStencilState> {
    ResourceType* createResource(const KeyType& key) override;
};

class Texture2DManager : public D3DObjectManagerBase<std::string, ID3D11ShaderResourceView> {
    ResourceType* createResource(const KeyType& key) override;
};

class VertexShaderManager : public ResourceManagerBase<std::string, VertexShader> {
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
    InputLayoutKey(const InputElementDescs& inputElementDescs_, ID3DBlob* shaderInputSignature_)
        : inputElementDescs{inputElementDescs_},
          shaderInputSignature{shaderInputSignature_},
          hashVal{hashCombine(
              std::hash<InputElementDescs>{}(inputElementDescs),
              hashWithSeed(static_cast<const char*>(shaderInputSignature->GetBufferPointer()),
                           shaderInputSignature->GetBufferSize(), 0))} {}
    InputLayoutKey(const InputElementDescs& inputElementDescs, const std::string& shaderFilename,
                   VertexShaderManager& vertexShaderManager)
        : InputLayoutKey{inputElementDescs,
                         vertexShaderManager.get(shaderFilename).get()->inputSignature} {}
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

class ShaderDatabase {
public:
    void SetDevice(ID3D11Device* device_) {
        device = device_;
        vertexShaderManager.setDevice(device);
        pixelShaderManager.setDevice(device);
        inputLayoutManager.setDevice(device);
    }
    VertexShaderManager::ResourceHandle GetVertexShader(const char* filename);
    PixelShaderManager::ResourceHandle GetPixelShader(const char* filename);

    InputLayoutManager::ResourceHandle GetInputLayout(VertexShader* vs,
                                                      const InputElementDescs& layout) {
        return inputLayoutManager.get(InputLayoutKey{layout, vs->inputSignature});
    }

    void ReloadShaders();

private:
    ID3D11Device* device = nullptr;
    VertexShaderManager vertexShaderManager;
    PixelShaderManager pixelShaderManager;
    InputLayoutManager inputLayoutManager;
};

struct PipelineStateObjectDesc {
    PipelineStateObjectDesc()
        : blendState{D3D11_DEFAULT},
          rasterizerState{D3D11_DEFAULT},
          depthStencilState{D3D11_DEFAULT},
          primitiveTopology{D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST} {}
    VertexShaderManager::KeyType vertexShader;
    PixelShaderManager::KeyType pixelShader;
    BlendStateManager::KeyType blendState;
    RasterizerStateManager::KeyType rasterizerState;
    DepthStencilStateManager::KeyType depthStencilState;
    InputElementDescs inputElementDescs;
    D3D11_PRIMITIVE_TOPOLOGY primitiveTopology;
};

namespace std {
template <>
struct hash<PipelineStateObjectDesc> {
    size_t operator()(const PipelineStateObjectDesc& x) const {
        return hashCombine(hash<VertexShaderManager::KeyType>{}(x.vertexShader),
                           hash<PixelShaderManager::KeyType>{}(x.pixelShader),
                           hash<BlendStateManager::KeyType>{}(x.blendState),
                           hash<RasterizerStateManager::KeyType>{}(x.rasterizerState),
                           hash<DepthStencilStateManager::KeyType>{}(x.depthStencilState),
                           hash<InputElementDescs>{}(x.inputElementDescs), x.primitiveTopology);
    }
};
}

inline bool operator==(const PipelineStateObjectDesc& a, const PipelineStateObjectDesc& b) {
    auto tupleify = [](const PipelineStateObjectDesc& a) {
        return std::make_tuple(a.vertexShader, a.pixelShader, a.blendState, a.rasterizerState,
                               a.depthStencilState, a.inputElementDescs, a.primitiveTopology);
    };
    return tupleify(a) == tupleify(b);
}

struct PipelineStateObject {
    VertexShaderManager::ResourceHandle vertexShader;
    PixelShaderManager::ResourceHandle pixelShader;
    BlendStateManager::ResourceHandle blendState;
    RasterizerStateManager::ResourceHandle rasterizerState;
    DepthStencilStateManager::ResourceHandle depthStencilState;
    InputLayoutManager::ResourceHandle inputLayout;
    D3D11_PRIMITIVE_TOPOLOGY primitiveTopology;
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

class PipelineStateObjectManager
    : public ResourceManagerBase<PipelineStateObjectDesc, PipelineStateObject> {
public:
    PipelineStateObjectManager(StateManagers& stateManagers_) : stateManagers(stateManagers_) {}

private:
    ResourceType* createResource(const KeyType& key);

    StateManagers& stateManagers;
};

struct DirectX11 {
    HWND Window = nullptr;
    bool Key[256];
    OVR::Sizei RenderTargetSize;
    ID3D11DevicePtr Device;
    ID3D11DeviceContextPtr Context;
    IDXGISwapChainPtr SwapChain;
    ID3D11Texture2DPtr BackBuffer;
    ID3D11RenderTargetViewPtr BackBufferRT;

    std::unique_ptr<StateManagers> stateManagers;
    std::unique_ptr<PipelineStateObjectManager> pipelineStateObjectManager;

    Texture2DManager texture2DManager;

    DirectX11();
    ~DirectX11();
    bool InitWindowAndDevice(HINSTANCE hinst, OVR::Recti vp);
    void ClearAndSetRenderTarget(ID3D11RenderTargetView* rendertarget, ID3D11DepthStencilView* dsv);
    void setViewport(const OVR::Recti& vp);
    bool IsAnyKeyPressed() const;
    void SetMaxFrameLatency(int value);
    void HandleMessages();
    void OutputFrameTime(double currentTime);
    void ReleaseWindow(HINSTANCE hinst);
};
