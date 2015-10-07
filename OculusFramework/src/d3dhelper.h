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

inline void SetDebugObjectName(IUnknown* resource, const char* name, size_t nameLen) {
#if !defined(NO_D3D11_DEBUG_NAME) && (defined(_DEBUG) || defined(PROFILE))
    ID3D11DeviceChildPtr deviceChild;
    resource->QueryInterface(__uuidof(ID3D11DeviceChild),
                             reinterpret_cast<void**>(deviceChild.ReleaseAndGetAddressOf()));
    if (deviceChild) deviceChild->SetPrivateData(WKPDID_D3DDebugObjectName, nameLen, name);
#else
    UNREFERENCED_PARAMETER(resource);
    UNREFERENCED_PARAMETER(name);
    UNREFERENCED_PARAMETER(nameLen);
#endif
}

template <size_t N>
inline void SetDebugObjectName(IUnknown* resource, const char(&name)[N]) {
    SetDebugObjectName(resource, name, N - 1);
}

inline void SetDebugObjectName(IUnknown* resource, const std::string& name) {
    SetDebugObjectName(resource, name.c_str(), name.size());
}

// Helpers to convert common types to equivalent DXGI_FORMAT

template <typename T>
DXGI_FORMAT getDXGIFormat();

template <>
inline DXGI_FORMAT getDXGIFormat<float>() {
    return DXGI_FORMAT_R32_FLOAT;
}

template <>
inline DXGI_FORMAT getDXGIFormat<mathlib::Vec2f>() {
    return DXGI_FORMAT_R32G32_FLOAT;
}

template <>
inline DXGI_FORMAT getDXGIFormat<mathlib::Vec3f>() {
    return DXGI_FORMAT_R32G32B32_FLOAT;
}

// Helpers to build input layouts

inline auto makeInputElementDescHelper(
    DXGI_FORMAT dxgiFormat, unsigned alignedByteOffset, const char* memberName,
    const char* semanticName = nullptr, unsigned semanticIndex = 0, unsigned inputSlot = 0,
    D3D11_INPUT_CLASSIFICATION inputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
    unsigned instanceDataStepRate = 0) {
    semanticName = semanticName ? semanticName : memberName;
    return D3D11_INPUT_ELEMENT_DESC{semanticName,        semanticIndex,     dxgiFormat,
                                    inputSlot,           alignedByteOffset, inputSlotClass,
                                    instanceDataStepRate};
}

#define MAKE_INPUT_ELEMENT_DESC(type, member, ...)                                              \
    makeInputElementDescHelper(getDXGIFormat<decltype(type::member)>(), offsetof(type, member), \
                               #member, __VA_ARGS__)

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

inline auto D3D11CreateDeviceAndSwapChain(IDXGIAdapter* adapter,
                                          const DXGI_SWAP_CHAIN_DESC& scDesc) {
    std::tuple<IDXGISwapChainPtr, ID3D11DevicePtr, ID3D11DeviceContextPtr> res;
    const auto driverType = adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;
    ThrowOnFailure(D3D11CreateDeviceAndSwapChain(
        adapter, driverType, nullptr, IsDebugBuild() ? D3D11_CREATE_DEVICE_DEBUG : 0u, nullptr, 0,
        D3D11_SDK_VERSION, &scDesc, &std::get<0>(res), &std::get<1>(res), nullptr,
        &std::get<2>(res)));
    return res;
}

inline auto CreateRenderTargetView(ID3D11Device* device, ID3D11Resource* resource) {
    ID3D11RenderTargetViewPtr res;
    ThrowOnFailure(device->CreateRenderTargetView(resource, nullptr, &res));
    return res;
}

inline auto CreateRenderTargetView(ID3D11Device* device, ID3D11Resource* resource,
                                   const D3D11_RENDER_TARGET_VIEW_DESC& desc) {
    ID3D11RenderTargetViewPtr res;
    ThrowOnFailure(device->CreateRenderTargetView(resource, &desc, &res));
    return res;
}

// Helpers for calling ID3D11DeviceContext Set functions with containers (auto deduce size)
template <typename Context, typename Buffers = std::initializer_list<ID3D11Buffer*>,
          typename Strides = std::initializer_list<UINT>,
          typename Offsets = std::initializer_list<UINT>>
void IASetVertexBuffers(const Context& context, unsigned startSlot, const Buffers& buffers,
                        const Strides& strides, const Offsets& offsets) {
    context->IASetVertexBuffers(UINT(startSlot), UINT(std::size(buffers)), std::begin(buffers),
                                std::begin(strides), std::begin(offsets));
};

template <typename Context, typename Buffers = std::initializer_list<ID3D11Buffer*>,
          typename Strides = std::initializer_list<UINT>>
void IASetVertexBuffers(const Context& context, unsigned startSlot, const Buffers& buffers,
                        const Strides& strides) {
    UINT offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
    IASetVertexBuffers(context, startSlot, buffers, strides, offsets);
};

template <typename Context, typename Buffers = std::initializer_list<ID3D11Buffer*>>
void PSSetConstantBuffers(const Context& context, unsigned startSlot, const Buffers& buffers) {
    context->PSSetConstantBuffers(startSlot, std::size(buffers), std::begin(buffers));
}

template <typename Context, typename Buffers = std::initializer_list<ID3D11Buffer*>>
void VSSetConstantBuffers(const Context& context, unsigned startSlot, const Buffers& buffers) {
    context->VSSetConstantBuffers(startSlot, std::size(buffers), std::begin(buffers));
}

template <typename Context, typename Srvs = std::initializer_list<ID3D11ShaderResourceView*>>
void PSSetShaderResources(const Context& context, unsigned startSlot, const Srvs& srvs) {
    context->PSSetShaderResources(UINT(startSlot), std::size(srvs), std::begin(srvs));
}

template <typename Context, typename Srvs = std::initializer_list<ID3D11ShaderResourceView*>>
void VSSetShaderResources(const Context& context, unsigned startSlot, const Srvs& srvs) {
    context->VSSetShaderResources(UINT(startSlot), std::size(srvs), std::begin(srvs));
}

template <typename Context, typename Samplers = std::initializer_list<ID3D11SamplerState*>>
void PSSetSamplers(const Context& context, unsigned startSlot, const Samplers& samplers) {
    context->PSSetSamplers(UINT(startSlot), std::size(samplers), std::begin(samplers));
}

template <typename Context, typename Rtvs = std::initializer_list<ID3D11RenderTargetView*>>
void OMSetRenderTargets(const Context& context, const Rtvs& rtvs,
                        ID3D11DepthStencilView* dsv = nullptr) {
    context->OMSetRenderTargets(std::size(rtvs), std::begin(rtvs), dsv);
}

template <typename Context, typename Vps = std::initializer_list<D3D11_VIEWPORT>>
void RSSetViewports(const Context& context, const Vps& vps) {
    context->RSSetViewports(std::size(vps), std::begin(vps));
}

// Misc helpers

inline auto GetBuffer(IDXGISwapChain* swapChain, unsigned buffer) {
    ID3D11Texture2DPtr res;
    swapChain->GetBuffer(buffer, __uuidof(ID3D11Texture2D),
                         reinterpret_cast<void**>(res.ReleaseAndGetAddressOf()));
    return res;
}

inline auto roundUpConstantBufferSize(size_t size) { return (1 + (size / 16)) * 16; };

