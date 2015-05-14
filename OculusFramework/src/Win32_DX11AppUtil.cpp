#include "Win32_DX11AppUtil.h"

#include "scene.h"

#include "DDSTextureLoader.h"

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
                         Sizei size, int mipLevels)
    : name(name_), Size(size) {
    CD3D11_TEXTURE2D_DESC dsDesc(depth ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
                                 size.w, size.h, 1, mipLevels);

    if (rendertarget) {
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
                                        ID3D11DepthStencilView* dsv, Recti vp) {
    float black[] = {0, 0, 0, 1};
    Context->OMSetRenderTargets(1, &rendertarget, dsv);
    Context->ClearRenderTargetView(rendertarget, black);
    Context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
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

bool DirectX11::InitWindowAndDevice(HINSTANCE hinst, Recti vp, bool windowed) {
    Window = [hinst, vp, windowed, this] {
        const auto className = L"OVRAppWindow";
        WNDCLASSW wc{};
        wc.lpszClassName = className;
        wc.lpfnWndProc = SystemWindowProc;
        RegisterClassW(&wc);

        const DWORD wsStyle = windowed ? (WS_POPUP | WS_OVERLAPPEDWINDOW) : WS_POPUP;
        const auto sizeDivisor = windowed ? 2 : 1;
        RECT winSize = {0, 0, vp.w / sizeDivisor, vp.h / sizeDivisor};
        AdjustWindowRect(&winSize, wsStyle, false);
        return CreateWindowW(className, L"OculusRoomTiny", wsStyle | WS_VISIBLE, vp.x, vp.y,
                             winSize.right - winSize.left, winSize.bottom - winSize.top, nullptr,
                             nullptr, hinst, this);
    }();
    if (!Window) return false;

    if (windowed) {
        RenderTargetSize = vp.GetSize();
    } else {
        RECT rc;
        GetClientRect(Window, &rc);
        RenderTargetSize = Sizei(rc.right - rc.left, rc.bottom - rc.top);
    }

    [this, windowed] {
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
        scDesc.BufferCount = 2;
        scDesc.BufferDesc.Width = RenderTargetSize.w;
        scDesc.BufferDesc.Height = RenderTargetSize.h;
        scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scDesc.OutputWindow = Window;
        scDesc.SampleDesc.Count = 1;
        scDesc.SampleDesc.Quality = 0;
        scDesc.Windowed = windowed;
        scDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        ThrowOnFailure(D3D11CreateDeviceAndSwapChain(
            Adapter, Adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, nullptr,
            creationFlags, nullptr, 0, D3D11_SDK_VERSION, &scDesc, &SwapChain, &Device, nullptr,
            &Context));

        SetDebugObjectName(SwapChain, "Direct3D11::SwapChain");
    }();

    ThrowOnFailure(
        SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&BackBuffer)));
    SetDebugObjectName(BackBuffer, "Direct3D11::BackBuffer");

    ThrowOnFailure(Device->CreateRenderTargetView(BackBuffer, nullptr, &BackBufferRT));
    SetDebugObjectName(BackBufferRT, "Direct3D11::BackBufferRT");

    /*
    MainDepthBuffer = std::make_unique<ImageBuffer>("MainDepthBuffer", Device, Context, true, true,
                                                    Sizei(RenderTargetSize.w, RenderTargetSize.h));
                                                    */
    if (!windowed) ThrowOnFailure(SwapChain->SetFullscreenState(1, nullptr));

    [this] {
        CD3D11_DEPTH_STENCIL_DESC dss{D3D11_DEFAULT};
        ID3D11DepthStencilStatePtr DepthState;
        ThrowOnFailure(Device->CreateDepthStencilState(&dss, &DepthState));
        SetDebugObjectName(DepthState, "Direct3D11::DepthState");
        Context->OMSetDepthStencilState(DepthState, 0);
    }();

    shaderDatabase.SetDevice(Device);
    rasterizerStateManager.setDevice(Device);
    texture2DManager.setDevice(Device);

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

VertexShader::VertexShader(ID3D11Device* device, ID3D10Blob* s) : byteCode{s}, numUniformInfo(0) {
    device->CreateVertexShader(s->GetBufferPointer(), s->GetBufferSize(), NULL, &D3DVert);

    ID3D11ShaderReflectionPtr ref;
    D3DReflect(s->GetBufferPointer(), s->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)&ref);
    ID3D11ShaderReflectionConstantBuffer* buf = ref->GetConstantBufferByIndex(0);
    D3D11_SHADER_BUFFER_DESC bufd;
    if (FAILED(buf->GetDesc(&bufd))) return;

    for (unsigned i = 0; i < bufd.Variables; i++) {
        ID3D11ShaderReflectionVariable* var = buf->GetVariableByIndex(i);
        D3D11_SHADER_VARIABLE_DESC vd;
        var->GetDesc(&vd);
        Uniform u;
        strcpy_s(u.Name, (const char*)vd.Name);
        ;
        u.Offset = vd.StartOffset;
        u.Size = vd.Size;
        UniformInfo[numUniformInfo++] = u;
    }
    UniformData.resize(bufd.Size);
}

void VertexShader::SetUniform(const char* name, int n, const float* v) {
    for (int i = 0; i < numUniformInfo; ++i) {
        if (!strcmp(UniformInfo[i].Name, name)) {
            memcpy(UniformData.data() + UniformInfo[i].Offset, v, n * sizeof(float));
            return;
        }
    }
}

ID3D11InputLayout* VertexShader::GetInputLayout(ID3D11Device* device,
                                                const InputLayoutKey& inputLayoutKey) {
    auto findIt = inputLayoutMap.find(inputLayoutKey);
    if (findIt != end(inputLayoutMap)) return findIt->second;
    ID3D11InputLayoutPtr inputLayout;
    device->CreateInputLayout(&inputLayoutKey[0], inputLayoutKey.size(),
                              byteCode->GetBufferPointer(), byteCode->GetBufferSize(),
                              &inputLayout);
    inputLayoutMap[inputLayoutKey] = inputLayout;
    return inputLayout;
}

PixelShader::PixelShader(ID3D11Device* device, ID3D10Blob* s) : numUniformInfo(0) {
    device->CreatePixelShader(s->GetBufferPointer(), s->GetBufferSize(), NULL, &D3DPix);

    ID3D11ShaderReflectionPtr ref;
    D3DReflect(s->GetBufferPointer(), s->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)&ref);
    ID3D11ShaderReflectionConstantBuffer* buf = ref->GetConstantBufferByIndex(0);
    D3D11_SHADER_BUFFER_DESC bufd;
    if (FAILED(buf->GetDesc(&bufd))) return;

    for (unsigned i = 0; i < bufd.Variables; i++) {
        ID3D11ShaderReflectionVariable* var = buf->GetVariableByIndex(i);
        D3D11_SHADER_VARIABLE_DESC vd;
        var->GetDesc(&vd);
        Uniform u;
        strcpy_s(u.Name, (const char*)vd.Name);
        ;
        u.Offset = vd.StartOffset;
        u.Size = vd.Size;
        UniformInfo[numUniformInfo++] = u;
    }
    UniformData.resize(bufd.Size);
}

void PixelShader::SetUniform(const char* name, int n, const float* v) {
    for (int i = 0; i < numUniformInfo; ++i) {
        if (!strcmp(UniformInfo[i].Name, name)) {
            memcpy(UniformData.data() + UniformInfo[i].Offset, v, n * sizeof(float));
            return;
        }
    }
}

RasterizerStateManager::ResourceType* RasterizerStateManager::createResource(
    const RasterizerStateManager::KeyType& key) {
    ID3D11RasterizerState* rs;
    device->CreateRasterizerState(&key, &rs);
    return rs;
}

Texture2DManager::ResourceType* Texture2DManager::createResource(
    const Texture2DManager::KeyType& key) {
    auto tex = ID3D11ResourcePtr{};
    ID3D11ShaderResourceView* srv = nullptr;
    ThrowOnFailure(DirectX::CreateDDSTextureFromFileEx(
        device, wstring(begin(key), end(key)).c_str(), 0, D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE, 0, 0, true, &tex, &srv));
    return srv;
}

class ShaderIncludeHandler : public ID3DInclude {
public:
    STDMETHOD(Open)(D3D_INCLUDE_TYPE /*IncludeType*/, LPCSTR pFileName, LPCVOID /*pParentData*/,
                    LPCVOID* ppData, UINT* pBytes) override {
        ifstream headerFile{pFileName};
        if (!headerFile) return E_INVALIDARG;
        auto buf = stringstream{};
        buf << headerFile.rdbuf();
        auto str = buf.str();
        *ppData = str.c_str();
        *pBytes = str.size();
        bufs_[*ppData] = move(str);
        return S_OK;
    }
    STDMETHOD(Close)(LPCVOID pData) override {
        auto findIt = bufs_.find(pData);
        if (findIt == bufs_.end()) return E_INVALIDARG;
        bufs_.erase(findIt);
        return S_OK;
    }

private:
    std::unordered_map<LPCVOID, string> bufs_;
};

template <typename ShaderType>
ShaderType* loadShader(ID3D11Device& device, const std::string& filename, const char* target) {
    ifstream shaderSourceFile{ filename };
    stringstream buf;
    buf << shaderSourceFile.rdbuf();
    ID3DBlobPtr compiledShader;
    ID3DBlobPtr errorMessages;
    ShaderIncludeHandler shaderIncludeHandler;
    if (SUCCEEDED(D3DCompile(buf.str().c_str(), buf.str().size(), filename.c_str(), nullptr,
        &shaderIncludeHandler, "main", target, 0, 0, &compiledShader,
        &errorMessages))) {
        return new ShaderType{&device, compiledShader};
    }
    else {
        OutputDebugStringA(static_cast<const char*>(errorMessages->GetBufferPointer()));
        return nullptr;
    }
}

VertexShaderManager::ResourceType* VertexShaderManager::createResource(const KeyType& key) {
    return loadShader<VertexShader>(device, key, "vs_5_0");
}

PixelShaderManager::ResourceType* PixelShaderManager::createResource(const KeyType& key) {
    return loadShader<PixelShader>(device, key, "ps_5_0");
}

VertexShaderManager::ResourceHandle ShaderDatabase::GetVertexShader(const char* filename) {
    return vertexShaderManager.get(filename);
}

PixelShaderManager::ResourceHandle ShaderDatabase::GetPixelShader(const char* filename) {
    return pixelShaderManager.get(filename);
}

void ShaderDatabase::ReloadShaders() {
    vertexShaderManager.recreateAll();
    pixelShaderManager.recreateAll();
}
