#pragma once

#include "hashhelpers.h"
#include "vector.h"

#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <utility>

#include <wrl/client.h>

#include <d3d11_1.h>
#include <d3dcompiler.h>

#pragma warning(push)
#pragma warning(disable: 4245)
#include <array_view.h>
#include <string_view.h>
#pragma warning(pop)

using ID3D11BufferPtr = Microsoft::WRL::ComPtr<ID3D11Buffer>;
using ID3D11DebugPtr = Microsoft::WRL::ComPtr<ID3D11Debug>;
using ID3D11DepthStencilStatePtr = Microsoft::WRL::ComPtr<ID3D11DepthStencilState>;
using ID3D11DepthStencilViewPtr = Microsoft::WRL::ComPtr<ID3D11DepthStencilView>;
using ID3D11DevicePtr = Microsoft::WRL::ComPtr<ID3D11Device>;
using ID3D11DeviceChildPtr = Microsoft::WRL::ComPtr<ID3D11DeviceChild>;
using ID3D11DeviceContextPtr = Microsoft::WRL::ComPtr<ID3D11DeviceContext>;
using ID3D11InputLayoutPtr = Microsoft::WRL::ComPtr<ID3D11InputLayout>;
using ID3D11PixelShaderPtr = Microsoft::WRL::ComPtr<ID3D11PixelShader>;
using ID3D11RasterizerStatePtr = Microsoft::WRL::ComPtr<ID3D11RasterizerState>;
using ID3D11RenderTargetViewPtr = Microsoft::WRL::ComPtr<ID3D11RenderTargetView>;
using ID3D11ResourcePtr = Microsoft::WRL::ComPtr<ID3D11Resource>;
using ID3D11SamplerStatePtr = Microsoft::WRL::ComPtr<ID3D11SamplerState>;
using ID3D11ShaderReflectionPtr = Microsoft::WRL::ComPtr<ID3D11ShaderReflection>;
using ID3D11ShaderResourceViewPtr = Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>;
using ID3D11Texture2DPtr = Microsoft::WRL::ComPtr<ID3D11Texture2D>;
using ID3D11VertexShaderPtr = Microsoft::WRL::ComPtr<ID3D11VertexShader>;
using ID3DBlobPtr = Microsoft::WRL::ComPtr<ID3DBlob>;
using IDXGIAdapter1Ptr = Microsoft::WRL::ComPtr<IDXGIAdapter1>;
using IDXGIDevice1Ptr = Microsoft::WRL::ComPtr<IDXGIDevice1>;
using IDXGIFactory1Ptr = Microsoft::WRL::ComPtr<IDXGIFactory1>;
using IDXGISwapChainPtr = Microsoft::WRL::ComPtr<IDXGISwapChain>;

void ThrowOnFailure(HRESULT hr);

constexpr auto IsDebugBuild() {
#ifdef _DEBUG
    return true;
#else
    return false;
#endif
}

// Helper sets a D3D resource name string (used by PIX and debug layer leak reporting).

inline void SetDebugObjectName(IUnknown* resource, gsl::cstring_view<> name) {
#if !defined(NO_D3D11_DEBUG_NAME) && (defined(_DEBUG) || defined(PROFILE))
    ID3D11DeviceChildPtr deviceChild;
    resource->QueryInterface(__uuidof(deviceChild),
                             reinterpret_cast<void**>(deviceChild.ReleaseAndGetAddressOf()));
    if (deviceChild && name)
        deviceChild->SetPrivateData(WKPDID_D3DDebugObjectName, name.bytes(), name.data());
#else
    UNREFERENCED_PARAMETER(resource);
    UNREFERENCED_PARAMETER(name);
#endif
}

// Helpers to convert common types to equivalent DXGI_FORMAT

template <typename T>
constexpr DXGI_FORMAT getDXGIFormat();

template <>
constexpr DXGI_FORMAT getDXGIFormat<float>() {
    return DXGI_FORMAT_R32_FLOAT;
}

template <>
inline DXGI_FORMAT getDXGIFormat<std::uint32_t>() {
    return DXGI_FORMAT_R8G8B8A8_UNORM;
}
template <>
constexpr DXGI_FORMAT getDXGIFormat<mathlib::Vec2f>() {
    return DXGI_FORMAT_R32G32_FLOAT;
}

template <>
constexpr DXGI_FORMAT getDXGIFormat<mathlib::Vec3f>() {
    return DXGI_FORMAT_R32G32B32_FLOAT;
}

// Helpers to build input layouts

constexpr auto makeInputElementDescHelper(
    DXGI_FORMAT dxgiFormat, unsigned alignedByteOffset, const char* memberName,
    const char* semanticName = nullptr, unsigned semanticIndex = 0, unsigned inputSlot = 0,
    D3D11_INPUT_CLASSIFICATION inputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
    unsigned instanceDataStepRate = 0) {
    return D3D11_INPUT_ELEMENT_DESC{semanticName ? semanticName : memberName,
                                    semanticIndex,
                                    dxgiFormat,
                                    inputSlot,
                                    alignedByteOffset,
                                    inputSlotClass,
                                    instanceDataStepRate};
}

#define MAKE_INPUT_ELEMENT_DESC(type, member, ...)                                              \
    makeInputElementDescHelper(getDXGIFormat<decltype(type::member)>(), offsetof(type, member), \
                               #member, __VA_ARGS__)

// Fluent interface helpers for setting up various DESC structs
struct Texture2DDesc : public CD3D11_TEXTURE2D_DESC {
    using CD3D11_TEXTURE2D_DESC::CD3D11_TEXTURE2D_DESC;
    auto& width(UINT x) {
        Width = x;
        return *this;
    }
    auto& height(UINT x) {
        Height = x;
        return *this;
    }
    auto& mipLevels(UINT x) {
        MipLevels = x;
        return *this;
    }
    auto& arraySize(UINT x) {
        ArraySize = x;
        return *this;
    }
    auto& format(DXGI_FORMAT x) {
        Format = x;
        return *this;
    }
    auto& sampleDesc(DXGI_SAMPLE_DESC x) {
        SampleDesc = x;
        return *this;
    }
    auto& usage(D3D11_USAGE x) {
        Usage = x;
        return *this;
    }
    auto& bindFlags(UINT x) {
        BindFlags = x;
        return *this;
    }
    auto& cpuAccessFlags(UINT x) {
        CPUAccessFlags = x;
        return *this;
    }
    auto& miscFlags(UINT x) {
        MiscFlags = x;
        return *this;
    }
};

struct BufferDesc : public CD3D11_BUFFER_DESC {
    using CD3D11_BUFFER_DESC::CD3D11_BUFFER_DESC;
    auto& byteWidth(UINT x) {
        ByteWidth = x;
        return *this;
    }
    auto& usage(D3D11_USAGE x) {
        Usage = x;
        return *this;
    }
    auto& bindFlags(UINT x) {
        BindFlags = x;
        return *this;
    }
    auto& cpuAccessFlags(UINT x) {
        CPUAccessFlags = x;
        return *this;
    }
    auto& miscFlags(UINT x) {
        MiscFlags = x;
        return *this;
    }
    auto& structureByteStride(UINT x) {
        StructureByteStride = x;
        return *this;
    }
};

struct BlendDesc : public CD3D11_BLEND_DESC {
    using CD3D11_BLEND_DESC::CD3D11_BLEND_DESC;
    BlendDesc() : CD3D11_BLEND_DESC{D3D11_DEFAULT} {}
    auto& alphaToCoverageEnable(BOOL x) {
        AlphaToCoverageEnable = x;
        return *this;
    }
    auto& independentBlendEnable(BOOL x) {
        IndependentBlendEnable = x;
        return *this;
    }
    auto& blendEnable(BOOL x, size_t rtIdx = 0) {
        RenderTarget[rtIdx].BlendEnable = x;
        return *this;
    }
    auto& srcBlend(D3D11_BLEND x, size_t rtIdx = 0) {
        RenderTarget[rtIdx].SrcBlend = x;
        return *this;
    }
    auto& destBlend(D3D11_BLEND x, size_t rtIdx = 0) {
        RenderTarget[rtIdx].DestBlend = x;
        return *this;
    }
    auto& blendOp(D3D11_BLEND_OP x, size_t rtIdx = 0) {
        RenderTarget[rtIdx].BlendOp = x;
        return *this;
    }
    auto& srcBlendAlpha(D3D11_BLEND x, size_t rtIdx = 0) {
        RenderTarget[rtIdx].SrcBlendAlpha = x;
        return *this;
    }
    auto& destBlendAlpha(D3D11_BLEND x, size_t rtIdx = 0) {
        RenderTarget[rtIdx].DestBlendAlpha = x;
        return *this;
    }
    auto& blendOpAlpha(D3D11_BLEND_OP x, size_t rtIdx = 0) {
        RenderTarget[rtIdx].BlendOpAlpha = x;
        return *this;
    }
    auto& renderTargetWriteMask(UINT8 x, size_t rtIdx = 0) {
        RenderTarget[rtIdx].RenderTargetWriteMask = x;
        return *this;
    }
};

struct DepthStencilOpDesc : public D3D11_DEPTH_STENCILOP_DESC {
    DepthStencilOpDesc()
        : D3D11_DEPTH_STENCILOP_DESC{D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP,
                                     D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS} {}
    auto& stencilFailOp(D3D11_STENCIL_OP x) {
        StencilFailOp = x;
        return *this;
    }
    auto& stencilDepthFailOp(D3D11_STENCIL_OP x) {
        StencilDepthFailOp = x;
        return *this;
    }
    auto& stencilPassOp(D3D11_STENCIL_OP x) {
        StencilPassOp = x;
        return *this;
    }
    auto& stencilFunc(D3D11_COMPARISON_FUNC x) {
        StencilFunc = x;
        return *this;
    }
};

struct DepthStencilDesc : public CD3D11_DEPTH_STENCIL_DESC {
    using CD3D11_DEPTH_STENCIL_DESC::CD3D11_DEPTH_STENCIL_DESC;
    DepthStencilDesc() : CD3D11_DEPTH_STENCIL_DESC{D3D11_DEFAULT} {}
    auto& depthEnable(BOOL x) {
        DepthEnable = x;
        return *this;
    }
    auto& depthWriteMask(D3D11_DEPTH_WRITE_MASK x) {
        DepthWriteMask = x;
        return *this;
    }
    auto& depthFunc(D3D11_COMPARISON_FUNC x) {
        DepthFunc = x;
        return *this;
    }
    auto& stencilEnable(BOOL x) {
        StencilEnable = x;
        return *this;
    }
    auto& stencilReadMask(UINT8 x) {
        StencilReadMask = x;
        return *this;
    }
    auto& stencilWriteMask(UINT8 x) {
        StencilWriteMask = x;
        return *this;
    }
    auto& frontFace(D3D11_DEPTH_STENCILOP_DESC x) {
        FrontFace = x;
        return *this;
    }
    auto& backFace(D3D11_DEPTH_STENCILOP_DESC x) {
        BackFace = x;
        return *this;
    }
};

struct SamplerDesc : public CD3D11_SAMPLER_DESC {
    using CD3D11_SAMPLER_DESC::CD3D11_SAMPLER_DESC;
    SamplerDesc() : CD3D11_SAMPLER_DESC{D3D11_DEFAULT} {}
    auto& filter(D3D11_FILTER x) {
        Filter = x;
        return *this;
    }
    auto& address(D3D11_TEXTURE_ADDRESS_MODE x) {
        AddressU = AddressV = AddressW = x;
        return *this;
    }
    auto& addressU(D3D11_TEXTURE_ADDRESS_MODE x) {
        AddressU = x;
        return *this;
    }
    auto& addressV(D3D11_TEXTURE_ADDRESS_MODE x) {
        AddressV = x;
        return *this;
    }
    auto& addressW(D3D11_TEXTURE_ADDRESS_MODE x) {
        AddressW = x;
        return *this;
    }
    auto& mipLodBias(FLOAT x) {
        MipLODBias = x;
        return *this;
    }
    auto& maxAnisotropy(UINT x) {
        MaxAnisotropy = x;
        return *this;
    }
    auto& comparisonFunc(D3D11_COMPARISON_FUNC x) {
        ComparisonFunc = x;
        return *this;
    }
    auto& borderColor(FLOAT r, FLOAT g, FLOAT b, FLOAT a) {
        BorderColor[0] = r;
        BorderColor[1] = g;
        BorderColor[2] = b;
        BorderColor[3] = a;
        return *this;
    }
    auto& minLod(FLOAT x) {
        MinLOD = x;
        return *this;
    }
    auto& maxLod(FLOAT x) {
        MaxLOD = x;
        return *this;
    }
};

// Helper operators

bool operator==(const D3D11_INPUT_ELEMENT_DESC& x, const D3D11_INPUT_ELEMENT_DESC& y);
bool operator==(const D3D11_RENDER_TARGET_BLEND_DESC& a, const D3D11_RENDER_TARGET_BLEND_DESC& b);
bool operator==(const CD3D11_BLEND_DESC& a, const CD3D11_BLEND_DESC& b);
bool operator==(const CD3D11_RASTERIZER_DESC& a, const CD3D11_RASTERIZER_DESC& b);
bool operator==(const D3D11_DEPTH_STENCILOP_DESC& a, const D3D11_DEPTH_STENCILOP_DESC& b);
bool operator==(const CD3D11_DEPTH_STENCIL_DESC& a, const CD3D11_DEPTH_STENCIL_DESC& b);

bool operator==(const LUID& a, const LUID& b);

// Helper hashing

size_t hashHelper(const D3D11_INPUT_ELEMENT_DESC& x);
size_t hashHelper(const CD3D11_BLEND_DESC& x);
size_t hashHelper(const CD3D11_RASTERIZER_DESC& x);
size_t hashHelper(const D3D11_DEPTH_STENCILOP_DESC& x);
size_t hashHelper(const CD3D11_DEPTH_STENCIL_DESC& x);

namespace std {
template <>
struct hash<D3D11_INPUT_ELEMENT_DESC> {
    size_t operator()(const D3D11_INPUT_ELEMENT_DESC& x) const {
        return hashHelper(x);
    }
};

template <>
struct hash<CD3D11_BLEND_DESC> {
    size_t operator()(const CD3D11_BLEND_DESC& x) const {
        return hashHelper(x);
    }
};

template <>
struct hash<CD3D11_RASTERIZER_DESC> {
    size_t operator()(const CD3D11_RASTERIZER_DESC& x) const {
        return hashHelper(x);
    }
};

template <>
struct hash<CD3D11_DEPTH_STENCIL_DESC> {
    size_t operator()(const CD3D11_DEPTH_STENCIL_DESC& x) const {
        return hashHelper(x);
    }
};

}

// Helpers for calling various DXGI and D3D Create and Initialization functions
IDXGIFactory1Ptr CreateDXGIFactory1();

class EnumIDXGIAdapters {
public:
    class Iterator
        : public std::iterator<std::forward_iterator_tag, IDXGIAdapter1Ptr> {
    public:
        Iterator() = default;
        Iterator(IDXGIFactory1Ptr dxgiFactory)
            : dxgiFactory_{dxgiFactory}, iAdapter_{0} {
            enumCurrentAdapter();
        }

        bool operator==(const Iterator& x) const;
        bool operator!=(const Iterator& x) const { return !(*this == x); }
        Iterator& operator++() {
            ++iAdapter_;
            enumCurrentAdapter();
            return *this;
        }
        Iterator operator++(int) {
            Iterator res{*this};
            ++(*this);
            return res;
        }
        const IDXGIAdapter1Ptr& operator*() const { return adapter_; }
        const IDXGIAdapter1Ptr* operator->() const { return std::addressof(adapter_); }

    private:
        void enumCurrentAdapter();

        IDXGIFactory1Ptr dxgiFactory_;
        IDXGIAdapter1Ptr adapter_;
        UINT iAdapter_ = UINT(-1);
    };

    EnumIDXGIAdapters(IDXGIFactory1Ptr dxgiFactory) : dxgiFactory_{dxgiFactory} {}
    auto begin() const { return Iterator{dxgiFactory_}; }
    auto end() const { return Iterator{}; }

private:
    IDXGIFactory1Ptr dxgiFactory_;
};

inline auto GetDesc1(IDXGIAdapter1* a) {
    auto res = DXGI_ADAPTER_DESC1{};
    ThrowOnFailure(a->GetDesc1(&res));
    return res;
}

inline auto D3D11CreateDevice(IDXGIAdapter* adapter) {
    std::pair<ID3D11DevicePtr, ID3D11DeviceContextPtr> res;
    const auto driverType = adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;
    ThrowOnFailure(D3D11CreateDevice(adapter, driverType, nullptr,
                                     IsDebugBuild() ? D3D11_CREATE_DEVICE_DEBUG : 0u, nullptr, 0,
                                     D3D11_SDK_VERSION, &res.first, nullptr, &res.second));
    return res;
}

inline auto D3D11CreateDeviceAndSwapChain(IDXGIAdapter* adapter, const DXGI_SWAP_CHAIN_DESC& scDesc,
                                          UINT flags = 0) {
    std::tuple<IDXGISwapChainPtr, ID3D11DevicePtr, ID3D11DeviceContextPtr> res;
    const auto driverType = adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;
    ThrowOnFailure(D3D11CreateDeviceAndSwapChain(
        adapter, driverType, nullptr, (IsDebugBuild() ? D3D11_CREATE_DEVICE_DEBUG : 0u) | flags,
        nullptr, 0, D3D11_SDK_VERSION, &scDesc, &std::get<0>(res), &std::get<1>(res), nullptr,
        &std::get<2>(res)));
    return res;
}

inline auto CreateRenderTargetView(ID3D11Device* device, ID3D11Resource* resource,
                                   gsl::cstring_view<> name = {}) {
    ID3D11RenderTargetViewPtr res;
    ThrowOnFailure(device->CreateRenderTargetView(resource, nullptr, &res));
    SetDebugObjectName(res.Get(), name);
    return res;
}

inline auto CreateRenderTargetView(ID3D11Device* device, ID3D11Resource* resource,
                                   const D3D11_RENDER_TARGET_VIEW_DESC& desc,
                                   gsl::cstring_view<> name = {}) {
    ID3D11RenderTargetViewPtr res;
    ThrowOnFailure(device->CreateRenderTargetView(resource, &desc, &res));
    SetDebugObjectName(res.Get(), name);
    return res;
}

inline auto CreateTexture2D(ID3D11Device* device, const D3D11_TEXTURE2D_DESC& desc,
                            gsl::cstring_view<> name = {}) {
    ID3D11Texture2DPtr res;
    ThrowOnFailure(device->CreateTexture2D(&desc, nullptr, &res));
    SetDebugObjectName(res.Get(), name);
    return res;
}

inline auto CreateTexture2D(ID3D11Device* device, const D3D11_TEXTURE2D_DESC& desc,
                            const D3D11_SUBRESOURCE_DATA& data, gsl::cstring_view<> name = {}) {
    ID3D11Texture2DPtr res;
    ThrowOnFailure(device->CreateTexture2D(&desc, &data, &res));
    SetDebugObjectName(res.Get(), name);
    return res;
}

inline auto CreateBuffer(ID3D11Device* device, const D3D11_BUFFER_DESC& desc,
                         gsl::cstring_view<> name = {}) {
    ID3D11BufferPtr res;
    ThrowOnFailure(device->CreateBuffer(&desc, nullptr, &res));
    SetDebugObjectName(res.Get(), name);
    return res;
}

inline auto CreateBuffer(ID3D11Device* device, const D3D11_BUFFER_DESC& desc,
                         const D3D11_SUBRESOURCE_DATA& data, gsl::cstring_view<> name = {}) {
    ID3D11BufferPtr res;
    ThrowOnFailure(device->CreateBuffer(&desc, &data, &res));
    SetDebugObjectName(res.Get(), name);
    return res;
}

inline auto CreateShaderResourceView(ID3D11Device* device, ID3D11Resource* resource,
                                     const D3D11_SHADER_RESOURCE_VIEW_DESC& desc,
                                     gsl::cstring_view<> name = {}) {
    ID3D11ShaderResourceViewPtr res;
    ThrowOnFailure(device->CreateShaderResourceView(resource, &desc, &res));
    SetDebugObjectName(res.Get(), name);
    return res;
}

inline auto CreateShaderResourceView(ID3D11Device* device, ID3D11Resource* resource,
                                     gsl::cstring_view<> name = {}) {
    ID3D11ShaderResourceViewPtr res;
    ThrowOnFailure(device->CreateShaderResourceView(resource, nullptr, &res));
    SetDebugObjectName(res.Get(), name);
    return res;
}

inline auto CreateDepthStencilView(ID3D11Device* device, ID3D11Resource* resource,
                                   const D3D11_DEPTH_STENCIL_VIEW_DESC& desc,
                                   gsl::cstring_view<> name = {}) {
    ID3D11DepthStencilViewPtr res;
    ThrowOnFailure(device->CreateDepthStencilView(resource, &desc, &res));
    SetDebugObjectName(res.Get(), name);
    return res;
}

inline auto CreateDepthStencilView(ID3D11Device* device, ID3D11Resource* resource,
                                   gsl::cstring_view<> name = {}) {
    ID3D11DepthStencilViewPtr res;
    ThrowOnFailure(device->CreateDepthStencilView(resource, nullptr, &res));
    SetDebugObjectName(res.Get(), name);
    return res;
}

// Helpers for mapping a buffer
struct MapHandle {
    MapHandle(ID3D11DeviceContext* context, ID3D11Resource* resource, UINT subResource = 0,
              D3D11_MAP mapType = D3D11_MAP_WRITE_DISCARD, UINT mapFlags = 0)
        : context_{context}, resource_{resource}, subResource_{subResource} {
        ThrowOnFailure(context_->Map(resource_.Get(), subResource, mapType, mapFlags, &data_));
    }
    MapHandle(const MapHandle&) = delete;
    MapHandle(MapHandle&& x) {
        context_.Swap(x.context_);
        resource_.Swap(x.resource_);
        std::swap(subResource_, x.subResource_);
        std::swap(data_, x.data_);
    }
    ~MapHandle() {
        if (context_) context_->Unmap(resource_.Get(), subResource_);
    }
    MapHandle& operator=(const MapHandle&) = delete;
    MapHandle& operator=(MapHandle&& x) = default;
    const auto& mappedSubresource() { return data_; }

private:
    ID3D11DeviceContextPtr context_;
    ID3D11ResourcePtr resource_;
    UINT subResource_ = 0;
    D3D11_MAPPED_SUBRESOURCE data_ = {};
};

// Helpers for calling ID3D11DeviceContext Set functions with containers (auto deduce size)
template <typename Context>
void IASetVertexBuffers(const Context& context, unsigned startSlot,
                        gsl::array_view<ID3D11Buffer* const> buffers,
                        gsl::array_view<const UINT> strides, gsl::array_view<const UINT> offsets) {
    context->IASetVertexBuffers(UINT(startSlot), UINT(std::size(buffers)), std::data(buffers),
                                std::data(strides), std::data(offsets));
};

template <typename Context>
void IASetVertexBuffers(const Context& context, unsigned startSlot,
                        gsl::array_view<ID3D11Buffer* const> buffers,
                        gsl::array_view<const UINT> strides) {
    UINT offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
    context->IASetVertexBuffers(UINT(startSlot), UINT(std::size(buffers)), std::data(buffers),
                                std::data(strides), std::data(offsets));
};

template <typename Context>
void PSSetConstantBuffers(const Context& context, unsigned startSlot,
                          gsl::array_view<ID3D11Buffer* const> buffers) {
    context->PSSetConstantBuffers(startSlot, std::size(buffers), std::data(buffers));
}

template <typename Context>
void VSSetConstantBuffers(const Context& context, unsigned startSlot,
                          gsl::array_view<ID3D11Buffer* const> buffers) {
    context->VSSetConstantBuffers(startSlot, std::size(buffers), std::data(buffers));
}

template <typename Context>
void PSSetShaderResources(const Context& context, unsigned startSlot,
                          gsl::array_view<ID3D11ShaderResourceView* const> srvs) {
    context->PSSetShaderResources(UINT(startSlot), std::size(srvs), std::data(srvs));
}

template <typename Context>
void VSSetShaderResources(const Context& context, unsigned startSlot,
                          gsl::array_view<ID3D11ShaderResourceView* const> srvs) {
    context->VSSetShaderResources(UINT(startSlot), std::size(srvs), std::data(srvs));
}

template <typename Context>
void PSSetSamplers(const Context& context, unsigned startSlot,
                   gsl::array_view<ID3D11SamplerState* const> samplers) {
    context->PSSetSamplers(UINT(startSlot), std::size(samplers), std::data(samplers));
}

template <typename Context>
void OMSetRenderTargets(const Context& context, gsl::array_view<ID3D11RenderTargetView* const> rtvs,
                        ID3D11DepthStencilView* dsv = nullptr) {
    context->OMSetRenderTargets(std::size(rtvs), std::data(rtvs), dsv);
}

template <typename Context>
void RSSetViewports(const Context& context, gsl::array_view<const D3D11_VIEWPORT> vps) {
    context->RSSetViewports(std::size(vps), std::data(vps));
}

// Misc helpers

inline auto GetBuffer(IDXGISwapChain* swapChain, unsigned buffer) {
    ID3D11Texture2DPtr res;
    ThrowOnFailure(swapChain->GetBuffer(buffer, __uuidof(res),
                                        reinterpret_cast<void**>(res.ReleaseAndGetAddressOf())));
    return res;
}

constexpr auto roundUpConstantBufferSize(size_t size) { return (1 + (size / 16)) * 16; };

