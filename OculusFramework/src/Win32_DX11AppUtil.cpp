#include "Win32_DX11AppUtil.h"

#include "pipelinestateobject.h"
#include "scene.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace std;

using namespace OVR;

DataBuffer::DataBuffer(ID3D11Device* device, D3D11_BIND_FLAG use, const void* buffer, size_t size)
    : Size(size) {
    D3D11_BUFFER_DESC desc{};
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.BindFlags = use;
    desc.ByteWidth = (unsigned)size;
    D3D11_SUBRESOURCE_DATA sr;
    sr.pSysMem = buffer;
    sr.SysMemPitch = sr.SysMemSlicePitch = 0;
    ThrowOnFailure(device->CreateBuffer(&desc, buffer ? &sr : nullptr, &D3DBuffer));
    SetDebugObjectName(D3DBuffer, "DataBuffer::D3DBuffer");
}

void DataBuffer::Refresh(ID3D11DeviceContext* deviceContext, const void* buffer, size_t size) {
    D3D11_MAPPED_SUBRESOURCE map;
    deviceContext->Map(D3DBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
    memcpy((void*)map.pData, buffer, size);
    deviceContext->Unmap(D3DBuffer, 0);
}

ImageBuffer::ImageBuffer(const char* name_, ID3D11Device* device, bool rendertarget, bool depth,
                         Sizei size, int mipLevels, bool aa)
    : name(name_), Size(size) {
    CD3D11_TEXTURE2D_DESC dsDesc(
        depth ? DXGI_FORMAT_D24_UNORM_S8_UINT : DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, size.w, size.h, 1,
        mipLevels);

    if (rendertarget) {
        if (aa) dsDesc.SampleDesc.Count = 4;
        if (depth)
            dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        else
            dsDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    }

    device->CreateTexture2D(&dsDesc, nullptr, &Tex);
    SetDebugObjectName(Tex, string("ImageBuffer::Tex - ") + name);

    if (!depth) {
        device->CreateShaderResourceView(Tex, nullptr, &TexSv);
        SetDebugObjectName(TexSv, string("ImageBuffer::TexSv - ") + name);
    }

    if (rendertarget) {
        if (depth) {
            device->CreateDepthStencilView(Tex, nullptr, &TexDsv);
            SetDebugObjectName(TexDsv, string("ImageBuffer::TexDsv - ") + name);
        } else {
            device->CreateRenderTargetView(Tex, nullptr, &TexRtv);
            SetDebugObjectName(TexRtv, string("ImageBuffer::TexRtv - ") + name);
        }
    }
}

DirectX11::DirectX11() { fill(begin(Key), end(Key), false); }

DirectX11::~DirectX11() {}

void DirectX11::ClearAndSetRenderTarget(ID3D11RenderTargetView* rendertarget,
                                        ID3D11DepthStencilView* dsv) {
    float black[] = {0, 0, 0, 1};
    Context->OMSetRenderTargets(1, &rendertarget, dsv);
    Context->ClearRenderTargetView(rendertarget, black);
    Context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
}

void DirectX11::setViewport(const OVR::Recti& vp) {
    D3D11_VIEWPORT D3Dvp;
    D3Dvp.Width = (float)vp.w;
    D3Dvp.Height = (float)vp.h;
    D3Dvp.MinDepth = 0;
    D3Dvp.MaxDepth = 1;
    D3Dvp.TopLeftX = (float)vp.x;
    D3Dvp.TopLeftY = (float)vp.y;
    Context->RSSetViewports(1, &D3Dvp);
}

LRESULT CALLBACK SystemWindowProc(HWND arg_hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static DirectX11* dx11 = nullptr;

    switch (msg) {
        case (WM_NCCREATE): {
            CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lp);
            if (createStruct->lpCreateParams) {
                dx11 = reinterpret_cast<DirectX11*>(createStruct->lpCreateParams);
                dx11->Window = arg_hwnd;
            }
            break;
        }
        case WM_KEYDOWN:
            dx11->Key[(unsigned)wp] = true;
            break;
        case WM_KEYUP:
            dx11->Key[(unsigned)wp] = false;
            break;
        case WM_SETFOCUS:
            SetCapture(dx11->Window);
            ShowCursor(FALSE);
            break;
        case WM_KILLFOCUS:
            ReleaseCapture();
            ShowCursor(TRUE);
            break;
    }
    return DefWindowProc(arg_hwnd, msg, wp, lp);
}

bool DirectX11::InitWindowAndDevice(HINSTANCE hinst, Recti vp) {
    Window = [hinst, vp, this] {
        const auto className = L"OVRAppWindow";
        WNDCLASSW wc{};
        wc.lpszClassName = className;
        wc.lpfnWndProc = SystemWindowProc;
        RegisterClassW(&wc);

        const DWORD wsStyle = WS_POPUP | WS_OVERLAPPEDWINDOW;
        RECT winSize = {0, 0, vp.w, vp.h};
        AdjustWindowRect(&winSize, wsStyle, false);
        return CreateWindowW(className, L"OculusRoomTiny", wsStyle | WS_VISIBLE, vp.x, vp.y,
                             winSize.right - winSize.left, winSize.bottom - winSize.top, nullptr,
                             nullptr, hinst, this);
    }();
    if (!Window) return false;

    RenderTargetSize = vp.GetSize();

    [this] {
        IDXGIFactoryPtr DXGIFactory;
        ThrowOnFailure(
            CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&DXGIFactory)));

        IDXGIAdapterPtr Adapter;
        ThrowOnFailure(DXGIFactory->EnumAdapters(0, &Adapter));

        const UINT creationFlags = [] {
#ifdef _DEBUG
            return D3D11_CREATE_DEVICE_DEBUG;
#else
            return 0u;
#endif
        }();

        DXGI_SWAP_CHAIN_DESC scDesc{};
        scDesc.BufferDesc.Width = RenderTargetSize.w;
        scDesc.BufferDesc.Height = RenderTargetSize.h;
        scDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        scDesc.SampleDesc.Count = 1;
        scDesc.SampleDesc.Quality = 0;
        scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scDesc.BufferCount = 2;
        scDesc.OutputWindow = Window;
        scDesc.Windowed = TRUE;
        scDesc.Flags = 0;

        ThrowOnFailure(D3D11CreateDeviceAndSwapChain(
            Adapter, Adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, nullptr,
            creationFlags, nullptr, 0, D3D11_SDK_VERSION, &scDesc, &SwapChain, &Device, nullptr,
            &Context));

        SetDebugObjectName(SwapChain, "Direct3D11::SwapChain");
    }();

    stateManagers = make_unique<StateManagers>(Device);
    pipelineStateObjectManager = make_unique<PipelineStateObjectManager>(*stateManagers);
    texture2DManager.setDevice(Device);

    ThrowOnFailure(
        SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&BackBuffer)));
    SetDebugObjectName(BackBuffer, "Direct3D11::BackBuffer");

    ThrowOnFailure(Device->CreateRenderTargetView(BackBuffer, nullptr, &BackBufferRT));
    SetDebugObjectName(BackBufferRT, "Direct3D11::BackBufferRT");

    return true;
}

bool DirectX11::IsAnyKeyPressed() const {
    for (unsigned i = 0; i < (sizeof(Key) / sizeof(Key[0])); i++)
        if (Key[i]) return true;
    return false;
}

void DirectX11::SetMaxFrameLatency(int value) {
    IDXGIDevice1Ptr DXGIDevice1;
    if (FAILED(Device->QueryInterface(__uuidof(IDXGIDevice1),
                                      reinterpret_cast<void**>(&DXGIDevice1))) ||
        !DXGIDevice1)
        return;
    DXGIDevice1->SetMaximumFrameLatency(value);
}

void DirectX11::HandleMessages() {
    MSG msg;
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void DirectX11::OutputFrameTime(double currentTime) {
    static double lastTime = 0;
    char tempString[100];
    sprintf_s(tempString, "Frame time = %0.2f ms\n", (currentTime - lastTime) * 1000.0f);
    OutputDebugStringA(tempString);
    lastTime = currentTime;
}

void DirectX11::ReleaseWindow(HINSTANCE hinst) {
    DestroyWindow(Window);
    UnregisterClassW(L"OVRAppWindow", hinst);
}

void DirectX11::applyState(ID3D11DeviceContext& context, PipelineStateObject& pso) {
    context.VSSetShader(pso.vertexShader.get()->D3DVert, nullptr, 0);
    context.PSSetShader(pso.pixelShader.get()->D3DPix, nullptr, 0);
    context.OMSetBlendState(pso.blendState.get(), nullptr, 0xffffffff);
    context.RSSetState(pso.rasterizerState.get());
    context.OMSetDepthStencilState(pso.depthStencilState.get(), 0);
    context.IASetInputLayout(pso.inputLayout.get());
    context.IASetPrimitiveTopology(pso.primitiveTopology);
}
