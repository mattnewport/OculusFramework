#include "Win32_DX11AppUtil.h"

#include "pipelinestateobject.h"
#include "scene.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

extern LRESULT ImGui_ImplDX11_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using namespace std;

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
    SetDebugObjectName(D3DBuffer.Get(), "DataBuffer::D3DBuffer");
}

void DataBuffer::Refresh(ID3D11DeviceContext* deviceContext, const void* buffer, size_t size) {
    D3D11_MAPPED_SUBRESOURCE map;
    deviceContext->Map(D3DBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
    memcpy((void*)map.pData, buffer, size);
    deviceContext->Unmap(D3DBuffer.Get(), 0);
}

ImageBuffer::ImageBuffer(const char* name_, ID3D11Device* device, bool rendertarget, bool depth,
                         ovrSizei size, int mipLevels, bool aa)
    : name(name_), Size{size} {
    CD3D11_TEXTURE2D_DESC dsDesc(
        depth ? DXGI_FORMAT_D24_UNORM_S8_UINT : DXGI_FORMAT_R16G16B16A16_FLOAT, size.w, size.h, 1,
        mipLevels);

    if (rendertarget) {
        if (aa) dsDesc.SampleDesc.Count = 4;
        if (depth)
            dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        else
            dsDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    }

    device->CreateTexture2D(&dsDesc, nullptr, &Tex);
    SetDebugObjectName(Tex.Get(), string("ImageBuffer::Tex - ") + name);

    if (!depth) {
        device->CreateShaderResourceView(Tex.Get(), nullptr, &TexSv);
        SetDebugObjectName(TexSv.Get(), string("ImageBuffer::TexSv - ") + name);
    }

    if (rendertarget) {
        if (depth) {
            device->CreateDepthStencilView(Tex.Get(), nullptr, &TexDsv);
            SetDebugObjectName(TexDsv.Get(), string("ImageBuffer::TexDsv - ") + name);
        } else {
            device->CreateRenderTargetView(Tex.Get(), nullptr, &TexRtv);
            SetDebugObjectName(TexRtv.Get(), string("ImageBuffer::TexRtv - ") + name);
        }
    }
}

DirectX11::DirectX11() {
    fill(begin(Key), end(Key), false);
    fill(begin(keyPressed), end(keyPressed), false);
}

DirectX11::~DirectX11() {
    Context->ClearState();
    Context->Flush();
}

void DirectX11::ClearAndSetRenderTarget(ID3D11RenderTargetView* rendertarget,
                                        ID3D11DepthStencilView* dsv) {
    float black[] = {0, 0, 0, 1};
    OMSetRenderTargets(Context.Get(), {rendertarget}, dsv);
    Context->ClearRenderTargetView(rendertarget, black);
    Context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
}

void DirectX11::setViewport(const ovrRecti& vp) {
    RSSetViewports(Context, {{float(vp.Pos.x), float(vp.Pos.y), float(vp.Size.w), float(vp.Size.h),
                              0.0f, 1.0f}});
}

LRESULT CALLBACK SystemWindowProc(HWND arg_hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static DirectX11* dx11 = nullptr;

    ImGuiIO& io = ImGui::GetIO();
    switch (msg) {
        case (WM_NCCREATE): {
            CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lp);
            if (createStruct->lpCreateParams) {
                dx11 = reinterpret_cast<DirectX11*>(createStruct->lpCreateParams);
                dx11->Window = arg_hwnd;
            }
            break;
        }
        case WM_LBUTTONDOWN:
            io.MouseDown[0] = true;
            return true;
        case WM_LBUTTONUP:
            io.MouseDown[0] = false;
            return true;
        case WM_RBUTTONDOWN:
            io.MouseDown[1] = true;
            return true;
        case WM_RBUTTONUP:
            io.MouseDown[1] = false;
            return true;
        case WM_MOUSEWHEEL:
            io.MouseWheel += GET_WHEEL_DELTA_WPARAM(wp) > 0 ? +1.0f : -1.0f;
            return true;
        case WM_MOUSEMOVE:
            io.MousePos.x = (signed short)(lp);
            io.MousePos.y = (signed short)(lp >> 16);
            return true;
        case WM_KEYDOWN:
            if (wp < 256) {
                dx11->Key[(unsigned)wp] = true;
                io.KeysDown[wp] = 1;
            }
            return true;
        case WM_KEYUP:
            if (wp < 256) {
                dx11->Key[(unsigned)wp] = false;
                dx11->keyPressed[(unsigned)wp] = true;
                io.KeysDown[wp] = 0;
            }
            return true;
        case WM_CHAR:
            // You can also use ToAscii()+GetKeyboardState() to retrieve characters.
            if (wp > 0 && wp < 0x10000) io.AddInputCharacter((unsigned short)wp);
            return true;
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

bool DirectX11::InitWindowAndDevice(HINSTANCE hinst, ovrRecti vp, const LUID* pLuid) {
    Window = [hinst, vp, this] {
        const auto className = L"OVRAppWindow";
        WNDCLASSW wc{};
        wc.lpszClassName = className;
        wc.lpfnWndProc = SystemWindowProc;
        RegisterClassW(&wc);

        const DWORD wsStyle = WS_POPUP | WS_OVERLAPPEDWINDOW;
        RECT winSize = {0, 0, vp.Size.w, vp.Size.h};
        AdjustWindowRect(&winSize, wsStyle, false);
        return CreateWindowW(className, L"OculusRoomTiny", wsStyle | WS_VISIBLE, vp.Pos.x, vp.Pos.y,
                             winSize.right - winSize.left, winSize.bottom - winSize.top, nullptr,
                             nullptr, hinst, this);
    }();
    if (!Window) return false;

    RenderTargetSize = vp.Size;

    [this, pLuid] {
        IDXGIFactoryPtr DXGIFactory;
        ThrowOnFailure(
            CreateDXGIFactory1(__uuidof(IDXGIFactory),
                               reinterpret_cast<void**>(DXGIFactory.ReleaseAndGetAddressOf())));

        IDXGIAdapterPtr Adapter;
        for (UINT iAdapter = 0;
             DXGIFactory->EnumAdapters(iAdapter, &Adapter) != DXGI_ERROR_NOT_FOUND; ++iAdapter) {
            DXGI_ADAPTER_DESC adapterDesc{};
            Adapter->GetDesc(&adapterDesc);
            if ((pLuid == nullptr) || memcmp(&adapterDesc.AdapterLuid, pLuid, sizeof(LUID)) == 0)
                break;
        }

        const auto driverType = Adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;
        const UINT creationFlags = [] {
#ifdef _DEBUG
            return D3D11_CREATE_DEVICE_DEBUG;
#else
            return 0u;
#endif
        }();
        ThrowOnFailure(D3D11CreateDevice(Adapter.Get(), driverType, 0, creationFlags, nullptr, 0,
                                         D3D11_SDK_VERSION, &Device, nullptr, &Context));

        DXGI_SWAP_CHAIN_DESC scDesc{};
        scDesc.BufferDesc.Width = RenderTargetSize.w;
        scDesc.BufferDesc.Height = RenderTargetSize.h;
        scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        scDesc.SampleDesc.Count = 1;
        scDesc.SampleDesc.Quality = 0;
        scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scDesc.BufferCount = 2;
        scDesc.OutputWindow = Window;
        scDesc.Windowed = TRUE;
        scDesc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;
        scDesc.Flags = 0;

        ThrowOnFailure(DXGIFactory->CreateSwapChain(Device.Get(), &scDesc, &SwapChain));

        SetDebugObjectName(Device.Get(), "Direct3D11::Device");
        SetDebugObjectName(Context.Get(), "Direct3D11::Context");
        SetDebugObjectName(SwapChain.Get(), "Direct3D11::SwapChain");
    }();

    stateManagers = make_unique<StateManagers>(Device.Get());
    pipelineStateObjectManager = make_unique<PipelineStateObjectManager>(*stateManagers);
    texture2DManager.setDevice(Device.Get());

    ThrowOnFailure(
        SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                             reinterpret_cast<void**>(BackBuffer.ReleaseAndGetAddressOf())));
    SetDebugObjectName(BackBuffer.Get(), "Direct3D11::BackBuffer");

    ThrowOnFailure(Device->CreateRenderTargetView(BackBuffer.Get(), nullptr, &BackBufferRT));
    SetDebugObjectName(BackBufferRT.Get(), "Direct3D11::BackBufferRT");

    [this] {
        IDXGIDevice1Ptr DXGIDevice1;
        if (FAILED(Device.As<IDXGIDevice1>(&DXGIDevice1)) || !DXGIDevice1)
            return;
        DXGIDevice1->SetMaximumFrameLatency(1);
    }();

    return true;
}

bool DirectX11::IsAnyKeyPressed() const {
    for (unsigned i = 0; i < (sizeof(Key) / sizeof(Key[0])); i++)
        if (Key[i]) return true;
    return false;
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
    context.VSSetShader(pso.vertexShader.get()->D3DVert.Get(), nullptr, 0);
    context.PSSetShader(pso.pixelShader.get()->D3DPix.Get(), nullptr, 0);
    context.OMSetBlendState(pso.blendState.get(), nullptr, 0xffffffff);
    context.RSSetState(pso.rasterizerState.get());
    context.OMSetDepthStencilState(pso.depthStencilState.get(), 0);
    context.IASetInputLayout(pso.inputLayout.get());
    context.IASetPrimitiveTopology(pso.primitiveTopology);
}

void QuadRenderer::render(ID3D11RenderTargetView & rtv, std::initializer_list<ID3D11ShaderResourceView*> sourceTexSRVs, int x, int y, int width, int height) {
    directX11.applyState(*directX11.Context.Get(), *pipelineStateObject.get());
    OMSetRenderTargets(directX11.Context, {&rtv});
    RSSetViewports(directX11.Context,
                   {{float(x), float(y), float(width), float(height), 0.0f, 1.0f}});
    PSSetShaderResources(directX11.Context, 0, sourceTexSRVs);
    directX11.Context->Draw(3, 0);
    PSSetShaderResources(directX11.Context, 0, {nullptr, nullptr});
}
