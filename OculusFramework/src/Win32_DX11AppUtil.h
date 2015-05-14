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

#include "Kernel/OVR_Math.h"

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
    ImageBuffer(const char* name, ID3D11Device* device, 
                bool rendertarget, bool depth, OVR::Sizei size, int mipLevels = 1);
};

using InputLayoutKey = std::vector<D3D11_INPUT_ELEMENT_DESC>;

inline bool operator<(const D3D11_INPUT_ELEMENT_DESC& x, const D3D11_INPUT_ELEMENT_DESC& y) {
    auto tupleify = [](const D3D11_INPUT_ELEMENT_DESC& x) {
        return std::make_tuple(std::string{x.SemanticName}, x.SemanticIndex, x.Format, x.InputSlot,
                               x.AlignedByteOffset, x.InputSlotClass, x.InstanceDataStepRate);
    };
    return tupleify(x) < tupleify(y);
}

struct CompareInputLayoutKeys {
    bool operator()(const InputLayoutKey& x, const InputLayoutKey& y) const {
        return lexicographical_compare(begin(x), end(x), begin(y), end(y));
    }
};

struct VertexShader {
    ID3DBlobPtr byteCode;
    ID3D11VertexShaderPtr D3DVert;
    std::vector<unsigned char> UniformData;
    std::map<InputLayoutKey, ID3D11InputLayoutPtr, CompareInputLayoutKeys> inputLayoutMap;

    struct Uniform {
        char Name[40];
        int Offset, Size;
    };

    int numUniformInfo;
    Uniform UniformInfo[10];

    VertexShader(ID3D11Device* device, ID3DBlob* s, const char* name);

    void SetUniform(const char* name, int n, const float* v);
    ID3D11InputLayout* GetInputLayout(ID3D11Device* device, const InputLayoutKey& layout);
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

    PixelShader(ID3D11Device* device, ID3D10Blob* s, const char* name);

    void SetUniform(const char* name, int n, const float* v);
};

namespace std {
    template<> struct hash<D3D11_RASTERIZER_DESC> {
        size_t operator()(const D3D11_RASTERIZER_DESC& x) const {
            return hashCombine(x.FillMode, x.CullMode, x.FrontCounterClockwise, x.DepthBias,
                               x.DepthBiasClamp, x.SlopeScaledDepthBias, x.DepthClipEnable,
                               x.ScissorEnable, x.MultisampleEnable, x.AntialiasedLineEnable);
        }
    };
}

inline bool operator==(const D3D11_RASTERIZER_DESC& a, const D3D11_RASTERIZER_DESC& b) {
    return std::make_tuple(a.FillMode, a.CullMode, a.FrontCounterClockwise, a.DepthBias,
                           a.DepthBiasClamp, a.SlopeScaledDepthBias, a.DepthClipEnable,
                           a.ScissorEnable, a.MultisampleEnable, a.AntialiasedLineEnable) ==
           std::make_tuple(b.FillMode, b.CullMode, b.FrontCounterClockwise, b.DepthBias,
                           b.DepthBiasClamp, b.SlopeScaledDepthBias, b.DepthClipEnable,
                           b.ScissorEnable, b.MultisampleEnable, b.AntialiasedLineEnable);
}

class RasterizerStateManager : public ResourceManagerBase<D3D11_RASTERIZER_DESC, ID3D11RasterizerState> {
public:
    void setDevice(ID3D11Device* device_) { device = device_; }
private:
    ResourceType* createResource(const KeyType& key) override;
    void destroyResource(ResourceType* resource) override { resource->Release(); }
    ID3D11DevicePtr device;
};

class Texture2DManager : public ResourceManagerBase<std::string, ID3D11ShaderResourceView> {
public:
    void setDevice(ID3D11Device* device_) { device = device_; }
private:
    ResourceType* createResource(const KeyType& key) override;
    void destroyResource(ResourceType* resource) override { resource->Release(); }
    ID3D11DevicePtr device;
};

class VertexShaderManager : public ResourceManagerBase<std::string, VertexShader> {
public:
    void setDevice(ID3D11Device* device_) { device = device_; }
private:
    ResourceType* createResource(const KeyType& key) override;
    void destroyResource(ResourceType* resource) override { delete resource; }
    ID3D11DevicePtr device;
};

class PixelShaderManager : public ResourceManagerBase<std::string, PixelShader> {
public:
    void setDevice(ID3D11Device* device_) { device = device_; }
private:
    ResourceType* createResource(const KeyType& key) override;
    void destroyResource(ResourceType* resource) override { delete resource; }
    ID3D11DevicePtr device;
};

class ShaderDatabase {
public:
    void SetDevice(ID3D11Device* device_) {
        device = device_;
        vertexShaderManager.setDevice(device);
        pixelShaderManager.setDevice(device);
    }
    VertexShaderManager::ResourceHandle GetVertexShader(const char* filename);
    PixelShaderManager::ResourceHandle GetPixelShader(const char* filename);

    ID3D11InputLayout* GetInputLayout(VertexShader* vs, const InputLayoutKey& layout) {
        return vs->GetInputLayout(device, layout);
    }

    void ReloadShaders();

private:
    ID3D11Device* device = nullptr;
    VertexShaderManager vertexShaderManager;
    PixelShaderManager pixelShaderManager;
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
    ID3D11DepthStencilStatePtr depthStencilState;
    ShaderDatabase shaderDatabase;
    RasterizerStateManager rasterizerStateManager;
    Texture2DManager texture2DManager;

    DirectX11();
    ~DirectX11();
    bool InitWindowAndDevice(HINSTANCE hinst, OVR::Recti vp);
    void ClearAndSetRenderTarget(ID3D11RenderTargetView* rendertarget, ID3D11DepthStencilView* dsv,
                                 OVR::Recti vp);
    bool IsAnyKeyPressed() const;
    void SetMaxFrameLatency(int value);
    void HandleMessages();
    void OutputFrameTime(double currentTime);
    void ReleaseWindow(HINSTANCE hinst);
};
