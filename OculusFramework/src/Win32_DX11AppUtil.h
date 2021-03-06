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

#include <OVR_CAPI.h>

#include "d3dhelper.h"
#include "d2dhelper.h"
#include "d3dresourcemanagers.h"
#include "pipelinestateobjectmanager.h"

struct ImageBuffer {
    ID3D11Texture2DPtr Tex;
    ID3D11ShaderResourceViewPtr TexSv;
    ID3D11RenderTargetViewPtr TexRtv;
    ovrSizei Size = ovrSizei{};

    ImageBuffer(const char* name, ID3D11Device* device, ovrSizei size);
};

struct DepthBuffer {
    ID3D11DepthStencilViewPtr TexDsv;

    DepthBuffer(const char* name, ID3D11Device* device, ovrSizei size);
};

struct DirectX11 {
    HINSTANCE hinst = nullptr;
    HWND Window = nullptr;
    bool Key[256] = {};
    bool keyPressed[256] = {};
    ovrSizei RenderTargetSize;
    ID3D11DevicePtr Device;
    ID3D11DeviceContextPtr Context;
    IDXGISwapChainPtr SwapChain;
    ID3D11Texture2DPtr BackBuffer;
    ID3D11RenderTargetViewPtr BackBufferRT;
    ID2D1Factory1Ptr d2d1Factory1;
    ID2D1DevicePtr d2d1Device;
    ID2D1DeviceContextPtr d2d1DeviceContext;

    std::unique_ptr<StateManagers> stateManagers;
    std::unique_ptr<PipelineStateObjectManager> pipelineStateObjectManager;

    Texture2DManager texture2DManager;

    DirectX11(HINSTANCE hinst_, ovrRecti vp, const LUID* pLuid) : hinst{hinst_} {
        if (!InitWindowAndDevice(vp, pLuid))
            throw std::runtime_error{"Failed to initialize D3D."};
    }
    ~DirectX11();
    void ClearAndSetRenderTarget(ID3D11RenderTargetView* rendertarget, ID3D11DepthStencilView* dsv);
    void setViewport(const ovrRecti& vp);
    void HandleMessages();

    void applyState(ID3D11DeviceContext& context, PipelineStateObject& pso);
private:
    bool InitWindowAndDevice(ovrRecti vp, const LUID* pLuid);
};

struct QuadRenderer {
    QuadRenderer(DirectX11& directX11_, const char* pixelShader, const bool alphaBlend = false)
        : directX11{directX11_} {
        [this, pixelShader, alphaBlend] {
            PipelineStateObjectDesc desc;
            desc.vertexShader = "quadvs.hlsl";
            desc.pixelShader = pixelShader;
            desc.depthStencilState = DepthStencilDesc{}.depthEnable(FALSE);
            desc.blendState =
                BlendDesc{}
                    .blendEnable(alphaBlend ? TRUE : FALSE)
                    .destBlend(alphaBlend ? D3D11_BLEND_INV_SRC_ALPHA : D3D11_BLEND_ZERO)
                    .destBlendAlpha(alphaBlend ? D3D11_BLEND_INV_SRC_ALPHA : D3D11_BLEND_ZERO);
            pipelineStateObject = directX11.pipelineStateObjectManager->get(desc);
        }();
    }

    void render(ID3D11RenderTargetView& rtv,
                gsl::array_view<ID3D11ShaderResourceView* const> sourceTexSRVs, int x, int y,
                int width, int height);

    DirectX11& directX11;

    PipelineStateObjectManager::ResourceHandle pipelineStateObject;
};
