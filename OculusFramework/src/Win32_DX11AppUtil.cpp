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

ImageBuffer::ImageBuffer(const char* name, ID3D11Device* device, ovrSizei size) : Size{size} {
    Tex = CreateTexture2D(device,
                          Texture2DDesc(DXGI_FORMAT_R16G16B16A16_FLOAT, size.w, size.h)
                              .mipLevels(1)
                              .sampleDesc({4, 0})
                              .bindFlags(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET),
                          "ImageBuffer::Tex - "s + name);

    TexSv = CreateShaderResourceView(device, Tex.Get(), "ImageBuffer::TexSv - "s + name);
    TexRtv = CreateRenderTargetView(device, Tex.Get(), "ImageBuffer::TexRtv - "s + name);
}

DepthBuffer::DepthBuffer(const char* name, ID3D11Device* device, ovrSizei size) {
    ID3D11Texture2DPtr Tex =
        CreateTexture2D(device, Texture2DDesc(DXGI_FORMAT_D24_UNORM_S8_UINT, size.w, size.h)
                                    .mipLevels(1)
                                    .sampleDesc({4, 0})
                                    .bindFlags(D3D11_BIND_DEPTH_STENCIL),
                        "DepthBuffer::Tex - "s + name);
    TexDsv = CreateDepthStencilView(device, Tex.Get(), "DepthBuffer::TexDsv - "s + name);
}

DirectX11::~DirectX11() {
    Context->ClearState();
    Context->Flush();
    DestroyWindow(Window);
    UnregisterClassW(L"OVRAppWindow", hinst);
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

bool DirectX11::InitWindowAndDevice(ovrRecti vp, const LUID* pLuid) {
    Window = [vp, this] {
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
        auto DXGIFactory = CreateDXGIFactory1();
        auto Adapter = [&DXGIFactory, pLuid] {
            if (!pLuid) return IDXGIAdapter1Ptr{};
            auto adapterEnumerator = EnumIDXGIAdapters{ DXGIFactory };
            auto findIt = std::find_if(std::begin(adapterEnumerator), std::end(adapterEnumerator), [pLuid](const auto& x) {
                return GetDesc1(x.Get()).AdapterLuid == *pLuid;
            });
            return findIt == std::end(adapterEnumerator) ? IDXGIAdapter1Ptr{} : *findIt;
        }();

        std::tie(SwapChain, Device, Context) =
            D3D11CreateDeviceAndSwapChain(Adapter.Get(), {
                                                             {UINT(RenderTargetSize.w),
                                                              UINT(RenderTargetSize.h),
                                                              {},  // Refresh rate
                                                              DXGI_FORMAT_R8G8B8A8_UNORM_SRGB},
                                                             {1u, 0u},  // SampleDesc Count, Quality
                                                             DXGI_USAGE_RENDER_TARGET_OUTPUT,
                                                             2u,  // BufferCount
                                                             Window,
                                                             TRUE,  // Windowed
                                                             DXGI_SWAP_EFFECT_SEQUENTIAL,
                                                             0u  // Flags
                                                         });

        SetDebugObjectName(Device.Get(), "DirectX11::Device");
        SetDebugObjectName(Context.Get(), "DirectX11::Context");
        SetDebugObjectName(SwapChain.Get(), "DirectX11::SwapChain");
    }();

    stateManagers = make_unique<StateManagers>(Device.Get());
    pipelineStateObjectManager = make_unique<PipelineStateObjectManager>(*stateManagers);
    texture2DManager.setDevice(Device.Get());

    BackBuffer = GetBuffer(SwapChain.Get(), 0);
    SetDebugObjectName(BackBuffer.Get(), "DirectX11::BackBuffer");

    BackBufferRT =
        CreateRenderTargetView(Device.Get(), BackBuffer.Get(), "DirectX11::BackBufferRT");

    [this] {
        IDXGIDevice1Ptr DXGIDevice1;
        if (SUCCEEDED(Device.As<IDXGIDevice1>(&DXGIDevice1)))
            DXGIDevice1->SetMaximumFrameLatency(1);
    }();

    return true;
}

void DirectX11::HandleMessages() {
    MSG msg;
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
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

void QuadRenderer::render(ID3D11RenderTargetView& rtv,
                          gsl::array_view<ID3D11ShaderResourceView* const> sourceTexSRVs, int x,
                          int y, int width, int height) {
    directX11.applyState(*directX11.Context.Get(), *pipelineStateObject.get());
    OMSetRenderTargets(directX11.Context, {&rtv});
    RSSetViewports(directX11.Context,
                   {{float(x), float(y), float(width), float(height), 0.0f, 1.0f}});
    PSSetShaderResources(directX11.Context, 0, sourceTexSRVs);
    directX11.Context->Draw(3, 0);
    PSSetShaderResources(directX11.Context, 0, {nullptr, nullptr});
}
