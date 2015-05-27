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

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "d3dresourcemanagers.h"
#include "pipelinestateobjectmanager.h"

#pragma warning(push)
#pragma warning(disable : 4244 4127)
#include "OVR_Kernel.h"
#pragma warning(pop)

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

struct DirectX11 {
    HWND Window = nullptr;
    bool Key[256];
    bool keyPressed[256];
    bool imguiActive = false;
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

    void applyState(ID3D11DeviceContext& context, PipelineStateObject& pso);
};

struct QuadRenderer {
    QuadRenderer(DirectX11& directX11_, const char* pixelShader,
                 const bool alphaBlendEnable = false)
        : directX11{directX11_} {
        [this, pixelShader, alphaBlendEnable] {
            PipelineStateObjectDesc desc;
            desc.vertexShader = "quadvs.hlsl";
            desc.pixelShader = pixelShader;
            desc.depthStencilState = [] {
                CD3D11_DEPTH_STENCIL_DESC desc{D3D11_DEFAULT};
                desc.DepthEnable = FALSE;
                return desc;
            }();
            desc.blendState = [alphaBlendEnable] {
                CD3D11_BLEND_DESC desc{D3D11_DEFAULT};
                auto& rt0Blend = desc.RenderTarget[0];
                rt0Blend.BlendEnable = alphaBlendEnable ? TRUE : FALSE;
                rt0Blend.SrcBlend = D3D11_BLEND_ONE;
                rt0Blend.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
                rt0Blend.BlendOp = D3D11_BLEND_OP_ADD;
                rt0Blend.SrcBlendAlpha = D3D11_BLEND_ONE;
                rt0Blend.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
                rt0Blend.BlendOpAlpha = D3D11_BLEND_OP_ADD;
                rt0Blend.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
                return desc;
            }();
            pipelineStateObject = directX11.pipelineStateObjectManager->get(desc);
        }();
    }

    void render(ID3D11RenderTargetView& rtv,
                std::initializer_list<ID3D11ShaderResourceView*> sourceTexSRVs, int x, int y,
                int width, int height);

    DirectX11& directX11;

    PipelineStateObjectManager::ResourceHandle pipelineStateObject;
};
