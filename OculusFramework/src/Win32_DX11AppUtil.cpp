#include "Win32_DX11AppUtil.h"

#include <algorithm>
#include <stdexcept>
#include <string>

using namespace std;

void ThrowOnFailure(HRESULT hr) {
    if (FAILED(hr)) {
        _com_error err{hr};
        OutputDebugString(err.ErrorMessage());
        throw std::runtime_error{"Failed HRESULT"};
    }
}

DataBuffer::DataBuffer(ID3D11Device* device, D3D11_BIND_FLAG use, const void* buffer,
                              size_t size)
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

void DataBuffer::Refresh(ID3D11DeviceContext* deviceContext, const void* buffer,
                                size_t size) {
    D3D11_MAPPED_SUBRESOURCE map;
    deviceContext->Map(D3DBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
    memcpy((void*)map.pData, buffer, size);
    deviceContext->Unmap(D3DBuffer, 0);
}

ImageBuffer::ImageBuffer(const char* name_, ID3D11Device* device, ID3D11DeviceContext* deviceContext,
                                bool rendertarget, bool depth, Sizei size, int mipLevels,
                                unsigned char* data)
    : name(name_), Size(size) {
    CD3D11_TEXTURE2D_DESC dsDesc(depth ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM, size.w,
                                 size.h, 1, mipLevels);

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
        }
        else {
            device->CreateRenderTargetView(Tex, nullptr, &TexRtv);
            SetDebugObjectName(TexRtv, string("ImageBuffer::TexRtv - ") + name);
        }
    }

    if (data)  // Note data is trashed, as is width and height
    {
        for (int level = 0; level < mipLevels; level++) {
            deviceContext->UpdateSubresource(Tex, level, NULL, data, size.w * 4, size.h * 4);
            for (int j = 0; j < (size.h & ~1); j += 2) {
                const uint8_t* psrc = data + (size.w * j * 4);
                uint8_t* pdest = data + ((size.w >> 1) * (j >> 1) * 4);
                for (int i = 0; i<size.w>> 1; i++, psrc += 8, pdest += 4) {
                    pdest[0] =
                        (((int)psrc[0]) + psrc[4] + psrc[size.w * 4 + 0] + psrc[size.w * 4 + 4]) >>
                        2;
                    pdest[1] =
                        (((int)psrc[1]) + psrc[5] + psrc[size.w * 4 + 1] + psrc[size.w * 4 + 5]) >>
                        2;
                    pdest[2] =
                        (((int)psrc[2]) + psrc[6] + psrc[size.w * 4 + 2] + psrc[size.w * 4 + 6]) >>
                        2;
                    pdest[3] =
                        (((int)psrc[3]) + psrc[7] + psrc[size.w * 4 + 3] + psrc[size.w * 4 + 7]) >>
                        2;
                }
            }
            size.w >>= 1;
            size.h >>= 1;
        }
    }
}

DirectX11::DirectX11() { fill(begin(Key), end(Key), false); }

DirectX11::~DirectX11() {
    Context->ClearState();
    Context->Flush();
    secondWindow = nullptr;
    UniformBufferGen = nullptr;
    BackBufferRT = nullptr;
    BackBuffer = nullptr;
    SwapChain = nullptr;
    Context = nullptr;

    ID3D11DebugPtr d3dDebugDevice;
    if (SUCCEEDED(Device->QueryInterface(__uuidof(ID3D11Debug),
                                         reinterpret_cast<void**>(&d3dDebugDevice)))) {
        Device = nullptr;
        d3dDebugDevice->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY | D3D11_RLDO_DETAIL);
    }
}

void DirectX11::ClearAndSetRenderTarget(ID3D11RenderTargetView* rendertarget,
                                        ImageBuffer* depthbuffer, Recti vp) {
    float black[] = {0, 0, 0, 1};
    Context->OMSetRenderTargets(1, &rendertarget, depthbuffer->TexDsv);
    Context->ClearRenderTargetView(rendertarget, black);
    Context->ClearDepthStencilView(depthbuffer->TexDsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1,
                                   0);
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
    Window = [hinst, vp, windowed, this]{
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

    UniformBufferGen = std::make_unique<DataBuffer>(Device, D3D11_BIND_CONSTANT_BUFFER, nullptr,
                                                    2000);  // make sure big enough

    [this]{
        CD3D11_RASTERIZER_DESC rs{D3D11_DEFAULT};
        ID3D11RasterizerStatePtr Rasterizer;
        ThrowOnFailure(Device->CreateRasterizerState(&rs, &Rasterizer));
        SetDebugObjectName(Rasterizer, "Direct3D11::Rasterizer");
        Context->RSSetState(Rasterizer);
    }();

    [this]{
        CD3D11_DEPTH_STENCIL_DESC dss{D3D11_DEFAULT};
        ID3D11DepthStencilStatePtr DepthState;
        ThrowOnFailure(Device->CreateDepthStencilState(&dss, &DepthState));
        SetDebugObjectName(DepthState, "Direct3D11::DepthState");
        Context->OMSetDepthStencilState(DepthState, 0);
    }();

    return true;
}

SecondWindow::~SecondWindow() {
    DestroyWindow(Window);
    UnregisterClassW(className, hinst);
}

void SecondWindow::Init(HINSTANCE hinst_, ID3D11Device* device, ID3D11DeviceContext* Context) {
    hinst = hinst_;
    width = 640;
    height = 360;

    Window = [this]{
        WNDCLASSW wc{};
        wc.lpszClassName = className;
        wc.lpfnWndProc = SystemWindowProc;
        RegisterClassW(&wc);

        const DWORD wsStyle = WS_POPUP | WS_OVERLAPPEDWINDOW;
        RECT winSize = { 0, 0, width, height };
        AdjustWindowRect(&winSize, wsStyle, false);
        return CreateWindowW(className, L"OculusRoomTiny", wsStyle | WS_VISIBLE, 100, 100,
            winSize.right - winSize.left, winSize.bottom - winSize.top, nullptr,
            nullptr, hinst, nullptr);
    }();
    if (!Window) throw runtime_error{ "Failed to create second window!" };

    [this, device] {
        IDXGIDevice1Ptr DXGIDevice;
        ThrowOnFailure(device->QueryInterface(__uuidof(IDXGIDevice1), reinterpret_cast<void**>(&DXGIDevice)));

        IDXGIAdapterPtr Adapter;
        ThrowOnFailure(DXGIDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&Adapter)));

        IDXGIFactoryPtr DXGIFactory;
        ThrowOnFailure(Adapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&DXGIFactory)));

        DXGI_SWAP_CHAIN_DESC scDesc{};
        scDesc.BufferCount = 2;
        scDesc.BufferDesc.Width = width;
        scDesc.BufferDesc.Height = height;
        scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scDesc.OutputWindow = Window;
        scDesc.SampleDesc.Count = 1;
        scDesc.SampleDesc.Quality = 0;
        scDesc.Windowed = TRUE;
        scDesc.Flags = 0;

        ThrowOnFailure(DXGIFactory->CreateSwapChain(device, &scDesc, &SwapChain));
        SetDebugObjectName(SwapChain, "SecondWindow::SwapChain");
    }();

    ThrowOnFailure(
        SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&BackBuffer)));
    SetDebugObjectName(BackBuffer, "SecondWindow::BackBuffer");

    ThrowOnFailure(device->CreateRenderTargetView(BackBuffer, nullptr, &BackBufferRT));
    SetDebugObjectName(BackBufferRT, "SecondWindow::BackBufferRT");

    DepthBuffer = std::make_unique<ImageBuffer>("SecondWindow::DepthBuffer", device, Context, true, true,
        Sizei(width, height));
}

void DirectX11::InitSecondWindow(HINSTANCE hinst) {
    secondWindow = make_unique<SecondWindow>();
    secondWindow->Init(hinst, Device, Context);
}

void DirectX11::Render(ShaderFill* fill, DataBuffer* vertices, DataBuffer* indices, UINT stride,
                       int count) {
    Context->IASetInputLayout(fill->InputLayout);
    Context->IASetIndexBuffer(indices->D3DBuffer, DXGI_FORMAT_R16_UINT, 0);

    UINT offset = 0;
    ID3D11Buffer* vertexBuffers[] = {vertices->D3DBuffer};
    Context->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);

    UniformBufferGen->Refresh(Context, fill->VShader->UniformData.data(), fill->VShader->UniformData.size());
    ID3D11Buffer* vsConstantBuffers[] = {UniformBufferGen->D3DBuffer};
    Context->VSSetConstantBuffers(0, 1, vsConstantBuffers);

    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context->VSSetShader(fill->VShader->D3DVert, NULL, 0);
    Context->PSSetShader(fill->PShader->D3DPix, NULL, 0);
    ID3D11SamplerState* samplerStates[] = {fill->SamplerState};
    Context->PSSetSamplers(0, 1, samplerStates);

    if (fill->OneTexture) {
        ID3D11ShaderResourceView* srvs[] = {fill->OneTexture->TexSv};
        Context->PSSetShaderResources(0, 1, srvs);
    }
    Context->DrawIndexed(count, 0, 0);
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

ShaderFill::ShaderFill(ID3D11Device* device, D3D11_INPUT_ELEMENT_DESC* VertexDesc,
                       int numVertexDesc, char* vertexShader, char* pixelShader,
                       std::unique_ptr<ImageBuffer>&& t, bool wrap)
    : OneTexture(std::move(t)) {
    ID3D10BlobPtr blobData;
    D3DCompile(vertexShader, strlen(vertexShader), NULL, NULL, NULL, "main", "vs_4_0", 0, 0,
               &blobData, NULL);
    VShader = std::make_unique<Shader>(device, blobData, 0);
    device->CreateInputLayout(VertexDesc, numVertexDesc, blobData->GetBufferPointer(),
                              blobData->GetBufferSize(), &InputLayout);
    SetDebugObjectName(InputLayout, "ShaderFill::InputLayout");

    D3DCompile(pixelShader, strlen(pixelShader), NULL, NULL, NULL, "main", "ps_4_0", 0, 0,
               &blobData, NULL);
    PShader = std::make_unique<Shader>(device, blobData, 1);

    CD3D11_SAMPLER_DESC ss{D3D11_DEFAULT};
    ss.AddressU = ss.AddressV = ss.AddressW =
        wrap ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_BORDER;
    ss.Filter = D3D11_FILTER_ANISOTROPIC;
    ss.MaxAnisotropy = 8;
    device->CreateSamplerState(&ss, &SamplerState);
}

Shader::Shader(ID3D11Device* device, ID3D10Blob* s, int which_type) : numUniformInfo(0) {
    if (which_type == 0)
        device->CreateVertexShader(s->GetBufferPointer(), s->GetBufferSize(), NULL, &D3DVert);
    else
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

void Shader::SetUniform(const char* name, int n, const float* v) {
    for (int i = 0; i < numUniformInfo; ++i) {
        if (!strcmp(UniformInfo[i].Name, name)) {
            memcpy(UniformData.data() + UniformInfo[i].Offset, v, n * sizeof(float));
            return;
        }
    }
}

void Model::AllocateBuffers(ID3D11Device* device) {
    VertexBuffer = std::make_unique<DataBuffer>(device, D3D11_BIND_VERTEX_BUFFER, &Vertices[0],
                                                numVertices * sizeof(Vertex));
    IndexBuffer =
        std::make_unique<DataBuffer>(device, D3D11_BIND_INDEX_BUFFER, &Indices[0], numIndices * 2);
}

void Model::AddSolidColorBox(float x1, float y1, float z1, float x2, float y2, float z2, Color c) {
    Vector3f Vert[][2] = {
        Vector3f(x1, y2, z1), Vector3f(z1, x1), Vector3f(x2, y2, z1), Vector3f(z1, x2),
        Vector3f(x2, y2, z2), Vector3f(z2, x2), Vector3f(x1, y2, z2), Vector3f(z2, x1),
        Vector3f(x1, y1, z1), Vector3f(z1, x1), Vector3f(x2, y1, z1), Vector3f(z1, x2),
        Vector3f(x2, y1, z2), Vector3f(z2, x2), Vector3f(x1, y1, z2), Vector3f(z2, x1),
        Vector3f(x1, y1, z2), Vector3f(z2, y1), Vector3f(x1, y1, z1), Vector3f(z1, y1),
        Vector3f(x1, y2, z1), Vector3f(z1, y2), Vector3f(x1, y2, z2), Vector3f(z2, y2),
        Vector3f(x2, y1, z2), Vector3f(z2, y1), Vector3f(x2, y1, z1), Vector3f(z1, y1),
        Vector3f(x2, y2, z1), Vector3f(z1, y2), Vector3f(x2, y2, z2), Vector3f(z2, y2),
        Vector3f(x1, y1, z1), Vector3f(x1, y1), Vector3f(x2, y1, z1), Vector3f(x2, y1),
        Vector3f(x2, y2, z1), Vector3f(x2, y2), Vector3f(x1, y2, z1), Vector3f(x1, y2),
        Vector3f(x1, y1, z2), Vector3f(x1, y1), Vector3f(x2, y1, z2), Vector3f(x2, y1),
        Vector3f(x2, y2, z2), Vector3f(x2, y2), Vector3f(x1, y2, z2), Vector3f(x1, y2),
    };

    uint16_t CubeIndices[] = {0,  1,  3,  3,  1,  2,  5,  4,  6,  6,  4,  7,
                              8,  9,  11, 11, 9,  10, 13, 12, 14, 14, 12, 15,
                              16, 17, 19, 19, 17, 18, 21, 20, 22, 22, 20, 23};

    for (int i = 0; i < 36; i++) AddIndex(CubeIndices[i] + (uint16_t)numVertices);

    for (int v = 0; v < 24; v++) {
        Vertex vvv;
        vvv.Pos = Vert[v][0];
        vvv.U = Vert[v][1].x;
        vvv.V = Vert[v][1].y;
        float dist1 = (vvv.Pos - Vector3f(-2, 4, -2)).Length();
        float dist2 = (vvv.Pos - Vector3f(3, 4, -3)).Length();
        float dist3 = (vvv.Pos - Vector3f(-4, 3, 25)).Length();
        int bri = rand() % 160;
        float RRR = c.R * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
        float GGG = c.G * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
        float BBB = c.B * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
        vvv.C.R = RRR > 255 ? 255 : (unsigned char)RRR;
        vvv.C.G = GGG > 255 ? 255 : (unsigned char)GGG;
        vvv.C.B = BBB > 255 ? 255 : (unsigned char)BBB;
        AddVertex(vvv);
    }
}

// Simple latency box (keep similar vertex format and shader params same, for ease of code)
Scene::Scene(ID3D11Device* device, ID3D11DeviceContext* deviceContext, int reducedVersion)
    : num_models(0) {
    D3D11_INPUT_ELEMENT_DESC ModelVertexDesc[] = {
        {"Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Model::Vertex, Pos),
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"Color", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(Model::Vertex, C),
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Model::Vertex, U),
         D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    char* VertexShaderSrc =
        "float4x4 Proj, View;"
        "float4 NewCol;"
        "void main(in  float4 Position  : POSITION,    in  float4 Color : COLOR0, in  float2 "
        "TexCoord  : TEXCOORD0,"
        "          out float4 oPosition : SV_Position, out float4 oColor: COLOR0, out float2 "
        "oTexCoord : TEXCOORD0)"
        "{   oPosition = mul(Proj, mul(View, Position)); oTexCoord = TexCoord; oColor = Color; "
        "}";
    char* PixelShaderSrc =
        "Texture2D Texture   : register(t0); SamplerState Linear : register(s0); "
        "float4 main(in float4 Position : SV_Position, in float4 Color: COLOR0, in float2 "
        "TexCoord : TEXCOORD0) : SV_Target"
        "{   return Color * Texture.Sample(Linear, TexCoord); }";

    // Construct textures
    const auto texWidthHeight = 256;
    const auto texCount = 5;
    static Model::Color tex_pixels[texCount][texWidthHeight * texWidthHeight];
    std::unique_ptr<ShaderFill> generated_texture[texCount];

    for (int k = 0; k < texCount; ++k) {
        for (int j = 0; j < texWidthHeight; ++j)
            for (int i = 0; i < texWidthHeight; ++i) {
                if (k == 0)
                    tex_pixels[0][j * texWidthHeight + i] =
                        (((i >> 7) ^ (j >> 7)) & 1) ? Model::Color(180, 180, 180, 255)
                                                    : Model::Color(80, 80, 80, 255);  // floor
                if (k == 1)
                    tex_pixels[1][j * texWidthHeight + i] =
                        (((j / 4 & 15) == 0) ||
                         (((i / 4 & 15) == 0) && ((((i / 4 & 31) == 0) ^ ((j / 4 >> 4) & 1)) == 0)))
                            ? Model::Color(60, 60, 60, 255)
                            : Model::Color(180, 180, 180, 255);  // wall
                if (k == 2 || k == 4)
                    tex_pixels[k][j * texWidthHeight + i] =
                        (i / 4 == 0 || j / 4 == 0) ? Model::Color(80, 80, 80, 255)
                                                   : Model::Color(180, 180, 180, 255);  // ceiling
                if (k == 3)
                    tex_pixels[3][j * texWidthHeight + i] =
                        Model::Color(128, 128, 128, 255);  // blank
            }
        std::unique_ptr<ImageBuffer> t = std::make_unique<ImageBuffer>("generated_texture",
            device, deviceContext, false, false, Sizei(texWidthHeight, texWidthHeight), 8,
            (unsigned char*)tex_pixels[k]);
        generated_texture[k] = std::make_unique<ShaderFill>(
            device, ModelVertexDesc, 3, VertexShaderSrc, PixelShaderSrc, std::move(t));
    }

    // Construct geometry
    Model* m = new Model(Vector3f(0, 0, 0), std::move(generated_texture[2]));  // Moving box
    m->AddSolidColorBox(0, 0, 0, +1.0f, +1.0f, 1.0f, Model::Color(64, 64, 64));
    m->AllocateBuffers(device);
    Add(m);

    m = new Model(Vector3f(0, 0, 0), std::move(generated_texture[1]));  // Walls
    m->AddSolidColorBox(-10.1f, 0.0f, -20.0f, -10.0f, 4.0f, 20.0f,
                        Model::Color(128, 128, 128));  // Left Wall
    m->AddSolidColorBox(-10.0f, -0.1f, -20.1f, 10.0f, 4.0f, -20.0f,
                        Model::Color(128, 128, 128));  // Back Wall
    m->AddSolidColorBox(10.0f, -0.1f, -20.0f, 10.1f, 4.0f, 20.0f,
                        Model::Color(128, 128, 128));  // Right Wall
    m->AllocateBuffers(device);
    Add(m);

    m = new Model(Vector3f(0, 0, 0), std::move(generated_texture[0]));  // Floors
    m->AddSolidColorBox(-10.0f, -0.1f, -20.0f, 10.0f, 0.0f, 20.1f,
                        Model::Color(128, 128, 128));  // Main floor
    m->AddSolidColorBox(-15.0f, -6.1f, 18.0f, 15.0f, -6.0f, 30.0f,
                        Model::Color(128, 128, 128));  // Bottom floor
    m->AllocateBuffers(device);
    Add(m);

    if (reducedVersion) return;

    m = new Model(Vector3f(0, 0, 0), std::move(generated_texture[4]));  // Ceiling
    m->AddSolidColorBox(-10.0f, 4.0f, -20.0f, 10.0f, 4.1f, 20.1f, Model::Color(128, 128, 128));
    m->AllocateBuffers(device);
    Add(m);

    m = new Model(Vector3f(0, 0, 0), std::move(generated_texture[3]));  // Fixtures & furniture
    m->AddSolidColorBox(9.5f, 0.75f, 3.0f, 10.1f, 2.5f, 3.1f,
                        Model::Color(96, 96, 96));  // Right side shelf// Verticals
    m->AddSolidColorBox(9.5f, 0.95f, 3.7f, 10.1f, 2.75f, 3.8f,
                        Model::Color(96, 96, 96));  // Right side shelf
    m->AddSolidColorBox(9.55f, 1.20f, 2.5f, 10.1f, 1.30f, 3.75f,
                        Model::Color(96, 96, 96));  // Right side shelf// Horizontals
    m->AddSolidColorBox(9.55f, 2.00f, 3.05f, 10.1f, 2.10f, 4.2f,
                        Model::Color(96, 96, 96));  // Right side shelf
    m->AddSolidColorBox(5.0f, 1.1f, 20.0f, 10.0f, 1.2f, 20.1f,
                        Model::Color(96, 96, 96));  // Right railing
    m->AddSolidColorBox(-10.0f, 1.1f, 20.0f, -5.0f, 1.2f, 20.1f,
                        Model::Color(96, 96, 96));  // Left railing
    for (float f = 5.0f; f <= 9.0f; f += 1.0f) {
        m->AddSolidColorBox(f, 0.0f, 20.0f, f + 0.1f, 1.1f, 20.1f,
                            Model::Color(128, 128, 128));  // Left Bars
        m->AddSolidColorBox(-f, 1.1f, 20.0f, -f - 0.1f, 0.0f, 20.1f,
                            Model::Color(128, 128, 128));  // Right Bars
    }
    m->AddSolidColorBox(-1.8f, 0.8f, 1.0f, 0.0f, 0.7f, 0.0f, Model::Color(128, 128, 0));  // Table
    m->AddSolidColorBox(-1.8f, 0.0f, 0.0f, -1.7f, 0.7f, 0.1f,
                        Model::Color(128, 128, 0));  // Table Leg
    m->AddSolidColorBox(-1.8f, 0.7f, 1.0f, -1.7f, 0.0f, 0.9f,
                        Model::Color(128, 128, 0));  // Table Leg
    m->AddSolidColorBox(0.0f, 0.0f, 1.0f, -0.1f, 0.7f, 0.9f,
                        Model::Color(128, 128, 0));  // Table Leg
    m->AddSolidColorBox(0.0f, 0.7f, 0.0f, -0.1f, 0.0f, 0.1f,
                        Model::Color(128, 128, 0));  // Table Leg
    m->AddSolidColorBox(-1.4f, 0.5f, -1.1f, -0.8f, 0.55f, -0.5f,
                        Model::Color(44, 44, 128));  // Chair Set
    m->AddSolidColorBox(-1.4f, 0.0f, -1.1f, -1.34f, 1.0f, -1.04f,
                        Model::Color(44, 44, 128));  // Chair Leg 1
    m->AddSolidColorBox(-1.4f, 0.5f, -0.5f, -1.34f, 0.0f, -0.56f,
                        Model::Color(44, 44, 128));  // Chair Leg 2
    m->AddSolidColorBox(-0.8f, 0.0f, -0.5f, -0.86f, 0.5f, -0.56f,
                        Model::Color(44, 44, 128));  // Chair Leg 2
    m->AddSolidColorBox(-0.8f, 1.0f, -1.1f, -0.86f, 0.0f, -1.04f,
                        Model::Color(44, 44, 128));  // Chair Leg 2
    m->AddSolidColorBox(-1.4f, 0.97f, -1.05f, -0.8f, 0.92f, -1.10f,
                        Model::Color(44, 44, 128));  // Chair Back high bar

    for (float f = 3.0f; f <= 6.6f; f += 0.4f)
        m->AddSolidColorBox(-3, 0.0f, f, -2.9f, 1.3f, f + 0.1f, Model::Color(64, 64, 64));  // Posts

    m->AllocateBuffers(device);
    Add(m);
}

// Simple latency box (keep similar vertex format and shader params same, for ease of code)
Scene::Scene(ID3D11Device* device, int renderTargetWidth, int renderTargetHeight) : num_models(0) {
    D3D11_INPUT_ELEMENT_DESC ModelVertexDesc[] = {
        {"Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Model::Vertex, Pos),
         D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    char* VertexShaderSrc =
        "float4x4 Proj, View;"
        "float4 NewCol;"
        "void main(in float4 Position : POSITION, out float4 oPosition : SV_Position, out "
        "float4 oColor: COLOR0)"
        "{   oPosition = mul(Proj, Position); oColor = NewCol; }";
    char* PixelShaderSrc =
        "float4 main(in float4 Position : SV_Position, in float4 Color: COLOR0) : SV_Target"
        "{   return Color ; }";

    Model* m = new Model(Vector3f(0, 0, 0),
                         std::make_unique<ShaderFill>(device, ModelVertexDesc, 3, VertexShaderSrc,
                                                      PixelShaderSrc, nullptr));
    float scale = 0.04f;
    float extra_y = ((float)renderTargetWidth / (float)renderTargetHeight);
    m->AddSolidColorBox(1 - scale, 1 - (scale * extra_y), -1, 1 + scale, 1 + (scale * extra_y), -1,
                        Model::Color(0, 128, 0));
    m->AllocateBuffers(device);
    Add(m);
}

void Scene::Render(DirectX11& dx11, Matrix4f view, Matrix4f proj) {
    for (int i = 0; i < num_models; i++) {
        Matrix4f modelmat = Models[i]->GetMatrix();
        Matrix4f mat = (view * modelmat).Transposed();

        Models[i]->Fill->VShader->SetUniform("View", 16, (float*)&mat);
        Models[i]->Fill->VShader->SetUniform("Proj", 16, (float*)&proj);

        dx11.Render(Models[i]->Fill.get(), Models[i]->VertexBuffer.get(),
                    Models[i]->IndexBuffer.get(), sizeof(Model::Vertex), Models[i]->numIndices);
    }
}
