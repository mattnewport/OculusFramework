#pragma once

#include "hashhelpers.h"
#include "vector.h"

#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <utility>

#include <comdef.h>
#include <comip.h>

#include <d3d11_1.h>
#include <d3dcompiler.h>

_COM_SMARTPTR_TYPEDEF(ID3D11Buffer, __uuidof(ID3D11Buffer));
_COM_SMARTPTR_TYPEDEF(ID3D11Debug, __uuidof(ID3D11Debug));
_COM_SMARTPTR_TYPEDEF(ID3D11DepthStencilState, __uuidof(ID3D11DepthStencilState));
_COM_SMARTPTR_TYPEDEF(ID3D11DepthStencilView, __uuidof(ID3D11DepthStencilView));
_COM_SMARTPTR_TYPEDEF(ID3D11Device, __uuidof(ID3D11Device));
_COM_SMARTPTR_TYPEDEF(ID3D11DeviceChild, __uuidof(ID3D11DeviceChild));
_COM_SMARTPTR_TYPEDEF(ID3D11DeviceContext, __uuidof(ID3D11DeviceContext));
_COM_SMARTPTR_TYPEDEF(ID3D11InputLayout, __uuidof(ID3D11InputLayout));
_COM_SMARTPTR_TYPEDEF(ID3D11PixelShader, __uuidof(ID3D11PixelShader));
_COM_SMARTPTR_TYPEDEF(ID3D11RasterizerState, __uuidof(ID3D11RasterizerState));
_COM_SMARTPTR_TYPEDEF(ID3D11RenderTargetView, __uuidof(ID3D11RenderTargetView));
_COM_SMARTPTR_TYPEDEF(ID3D11Resource, __uuidof(ID3D11Resource));
_COM_SMARTPTR_TYPEDEF(ID3D11SamplerState, __uuidof(ID3D11SamplerState));
_COM_SMARTPTR_TYPEDEF(ID3D11ShaderReflection, __uuidof(ID3D11ShaderReflection));
_COM_SMARTPTR_TYPEDEF(ID3D11ShaderResourceView, __uuidof(ID3D11ShaderResourceView));
_COM_SMARTPTR_TYPEDEF(ID3D11Texture2D, __uuidof(ID3D11Texture2D));
_COM_SMARTPTR_TYPEDEF(ID3D11VertexShader, __uuidof(ID3D11VertexShader));
_COM_SMARTPTR_TYPEDEF(ID3DBlob, __uuidof(ID3DBlob));
_COM_SMARTPTR_TYPEDEF(IDXGIAdapter, __uuidof(IDXGIAdapter));
_COM_SMARTPTR_TYPEDEF(IDXGIDevice1, __uuidof(IDXGIDevice1));
_COM_SMARTPTR_TYPEDEF(IDXGIFactory, __uuidof(IDXGIFactory));
_COM_SMARTPTR_TYPEDEF(IDXGISwapChain, __uuidof(IDXGISwapChain));

void ThrowOnFailure(HRESULT hr);

// Helper sets a D3D resource name string (used by PIX and debug layer leak reporting).

template <typename T>
inline void SetDebugObjectName(_In_ const T& resource, _In_z_ const char* name, size_t nameLen) {
#if !defined(NO_D3D11_DEBUG_NAME) && (defined(_DEBUG) || defined(PROFILE))
    ID3D11DeviceChildPtr deviceChild;
    resource->QueryInterface(__uuidof(ID3D11DeviceChild), reinterpret_cast<void**>(&deviceChild));
    if (deviceChild) deviceChild->SetPrivateData(WKPDID_D3DDebugObjectName, nameLen, name);
#else
    UNREFERENCED_PARAMETER(resource);
    UNREFERENCED_PARAMETER(name);
    UNREFERENCED_PARAMETER(nameLen);
#endif
}

template <typename T, size_t N>
inline void SetDebugObjectName(_In_ const T& resource, _In_z_ const char(&name)[N]) {
    SetDebugObjectName(resource, name, N - 1);
}

template <typename T>
inline void SetDebugObjectName(_In_ const T& resource, const std::string& name) {
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

// Helpers for calling ID3D11DeviceContext Set functions with containers (auto deduce size)
template <typename Buffers = std::initializer_list<ID3D11Buffer*>,
          typename Strides = std::initializer_list<UINT>,
          typename Offsets = std::initializer_list<UINT>>
void IASetVertexBuffers(ID3D11DeviceContext* context, unsigned startSlot, const Buffers& buffers,
                        const Strides& strides, const Offsets& offsets) {
    context->IASetVertexBuffers(UINT(startSlot), UINT(std::size(buffers)), std::begin(buffers),
                                std::begin(strides), std::begin(offsets));
};

template <typename Buffers = std::initializer_list<ID3D11Buffer*>,
          typename Strides = std::initializer_list<UINT>>
void IASetVertexBuffers(ID3D11DeviceContext* context, unsigned startSlot, const Buffers& buffers,
                        const Strides& strides) {
    UINT offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
    IASetVertexBuffers(context, startSlot, buffers, strides, offsets);
};

template <typename Buffers = std::initializer_list<ID3D11Buffer*>>
void PSSetConstantBuffers(ID3D11DeviceContext* context, unsigned startSlot,
    const Buffers& buffers) {
    context->PSSetConstantBuffers(startSlot, std::size(buffers), std::begin(buffers));
}

template <typename Buffers = std::initializer_list<ID3D11Buffer*>>
void VSSetConstantBuffers(ID3D11DeviceContext* context, unsigned startSlot,
                          const Buffers& buffers) {
    context->VSSetConstantBuffers(startSlot, std::size(buffers), std::begin(buffers));
}

template <typename Srvs = std::initializer_list<ID3D11ShaderResourceView*>>
void PSSetShaderResources(ID3D11DeviceContext* context, unsigned startSlot, const Srvs& srvs) {
    context->PSSetShaderResources(UINT(startSlot), std::size(srvs), std::begin(srvs));
}

template <typename Srvs = std::initializer_list<ID3D11ShaderResourceView*>>
void VSSetShaderResources(ID3D11DeviceContext* context, unsigned startSlot, const Srvs& srvs) {
    context->VSSetShaderResources(UINT(startSlot), std::size(srvs), std::begin(srvs));
}

template<typename Samplers = std::initializer_list<ID3D11SamplerState*>>
void PSSetSamplers(ID3D11DeviceContext* context, unsigned startSlot, const Samplers& samplers) {
    context->PSSetSamplers(UINT(startSlot), std::size(samplers), std::begin(samplers));
}

template <typename Rtvs = std::initializer_list<ID3D11RenderTargetView*>>
void OMSetRenderTargets(ID3D11DeviceContext* context, const Rtvs& rtvs,
                        ID3D11DepthStencilView* dsv = nullptr) {
    context->OMSetRenderTargets(std::size(rtvs), std::begin(rtvs), dsv);
}

template<typename Vps = std::initializer_list<D3D11_VIEWPORT>>
void RSSetViewports(ID3D11DeviceContext* context, const Vps& vps) {
    context->RSSetViewports(std::size(vps), std::begin(vps));
}

// Misc helpers

inline auto roundUpConstantBufferSize(size_t size) { return (1 + (size / 16)) * 16; };

