#pragma once

#include "hashhelpers.h"
#include "vector.h"

#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

#include <comdef.h>
#include <comip.h>

#include <d3d11_1.h>
#include <d3dcompiler.h>

_COM_SMARTPTR_TYPEDEF(IDXGIFactory, __uuidof(IDXGIFactory));
_COM_SMARTPTR_TYPEDEF(IDXGIAdapter, __uuidof(IDXGIAdapter));
_COM_SMARTPTR_TYPEDEF(IDXGIDevice1, __uuidof(IDXGIDevice1));
_COM_SMARTPTR_TYPEDEF(IDXGISwapChain, __uuidof(IDXGISwapChain));
_COM_SMARTPTR_TYPEDEF(ID3D11Device, __uuidof(ID3D11Device));
_COM_SMARTPTR_TYPEDEF(ID3D11Debug, __uuidof(ID3D11Debug));
_COM_SMARTPTR_TYPEDEF(ID3D11DeviceContext, __uuidof(ID3D11DeviceContext));
_COM_SMARTPTR_TYPEDEF(ID3D11DeviceChild, __uuidof(ID3D11DeviceChild));
_COM_SMARTPTR_TYPEDEF(ID3D11Texture2D, __uuidof(ID3D11Texture2D));
_COM_SMARTPTR_TYPEDEF(ID3D11RenderTargetView, __uuidof(ID3D11RenderTargetView));
_COM_SMARTPTR_TYPEDEF(ID3D11ShaderResourceView, __uuidof(ID3D11ShaderResourceView));
_COM_SMARTPTR_TYPEDEF(ID3D11DepthStencilView, __uuidof(ID3D11DepthStencilView));
_COM_SMARTPTR_TYPEDEF(ID3D11Buffer, __uuidof(ID3D11Buffer));
_COM_SMARTPTR_TYPEDEF(ID3D11RasterizerState, __uuidof(ID3D11RasterizerState));
_COM_SMARTPTR_TYPEDEF(ID3D11DepthStencilState, __uuidof(ID3D11DepthStencilState));
_COM_SMARTPTR_TYPEDEF(ID3D11VertexShader, __uuidof(ID3D11VertexShader));
_COM_SMARTPTR_TYPEDEF(ID3D11PixelShader, __uuidof(ID3D11PixelShader));
_COM_SMARTPTR_TYPEDEF(ID3D11ShaderReflection, __uuidof(ID3D11ShaderReflection));
_COM_SMARTPTR_TYPEDEF(ID3D11InputLayout, __uuidof(ID3D11InputLayout));
_COM_SMARTPTR_TYPEDEF(ID3D11SamplerState, __uuidof(ID3D11SamplerState));
_COM_SMARTPTR_TYPEDEF(ID3DBlob, __uuidof(ID3DBlob));
_COM_SMARTPTR_TYPEDEF(ID3D11Resource, __uuidof(ID3D11Resource));

void ThrowOnFailure(HRESULT hr);

// Helper sets a D3D resource name string (used by PIX and debug layer leak reporting).

template <typename T>
inline void SetDebugObjectName(_In_ const T& resource, _In_z_ const char* name, size_t nameLen) {
    ID3D11DeviceChildPtr deviceChild;
    resource->QueryInterface(__uuidof(ID3D11DeviceChild), reinterpret_cast<void**>(&deviceChild));
#if !defined(NO_D3D11_DEBUG_NAME) && (defined(_DEBUG) || defined(PROFILE))
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
    DXGI_FORMAT dxgiFormat, unsigned alignedByteOffset, const char* semanticName,
    unsigned semanticIndex = 0, unsigned inputSlot = 0,
    D3D11_INPUT_CLASSIFICATION inputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
    unsigned instanceDataStepRate = 0) {
    return D3D11_INPUT_ELEMENT_DESC{semanticName,        semanticIndex,     dxgiFormat,
                                    inputSlot,           alignedByteOffset, inputSlotClass,
                                    instanceDataStepRate};
}

#define MAKE_INPUT_ELEMENT_DESC(type, member, ...)                                              \
    makeInputElementDescHelper(getDXGIFormat<decltype(type::member)>(), offsetof(type, member), \
                               __VA_ARGS__)

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

// Misc helpers

inline auto roundUpConstantBufferSize(size_t size) { return (1 + (size / 16)) * 16; };

