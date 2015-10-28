#include "d3dhelper.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <tuple>

#include <comdef.h>
#include <comip.h>

using namespace std;

void ThrowOnFailure(HRESULT hr) {
    if (FAILED(hr)) {
        _com_error err{hr};
        OutputDebugString(err.ErrorMessage());
        OutputDebugString(L"\n");
        throw std::runtime_error{"Failed HRESULT"};
    }
}

struct OffsetSizePair {
    constexpr OffsetSizePair(size_t offset_, size_t size_) : offset(offset_), size(size_) {}
    size_t offset;
    size_t size;
};
#define OFFSETOF_NAME(type, member) type##_##member##_Offset
#define SIZEOF_NAME(type, member) type##_##member##_Size
#define OFFSETOF(type, member) constexpr auto OFFSETOF_NAME(type, member) = offsetof(type, member)
#define SIZEOF(type, member) constexpr auto SIZEOF_NAME(type, member) = sizeof(type::member)
#define OFFSETOF_SIZEOF(type, member) \
    OFFSETOF(type, member);           \
    SIZEOF(type, member)
#define FOLLOWS(type, member2, member1) \
    (OFFSETOF_NAME(type, member2) == OFFSETOF_NAME(type, member1) + SIZEOF_NAME(type, member1))

OFFSETOF_SIZEOF(D3D11_INPUT_ELEMENT_DESC, SemanticName);
OFFSETOF_SIZEOF(D3D11_INPUT_ELEMENT_DESC, SemanticIndex);
OFFSETOF_SIZEOF(D3D11_INPUT_ELEMENT_DESC, Format);
OFFSETOF_SIZEOF(D3D11_INPUT_ELEMENT_DESC, InputSlot);
OFFSETOF_SIZEOF(D3D11_INPUT_ELEMENT_DESC, AlignedByteOffset);
OFFSETOF_SIZEOF(D3D11_INPUT_ELEMENT_DESC, InputSlotClass);
OFFSETOF_SIZEOF(D3D11_INPUT_ELEMENT_DESC, InstanceDataStepRate);

static_assert(OFFSETOF_NAME(D3D11_INPUT_ELEMENT_DESC, SemanticName) == 0 &&
                  FOLLOWS(D3D11_INPUT_ELEMENT_DESC, SemanticIndex, SemanticName) &&
                  FOLLOWS(D3D11_INPUT_ELEMENT_DESC, Format, SemanticIndex) &&
                  FOLLOWS(D3D11_INPUT_ELEMENT_DESC, InputSlot, Format) &&
                  FOLLOWS(D3D11_INPUT_ELEMENT_DESC, AlignedByteOffset, InputSlot) &&
                  FOLLOWS(D3D11_INPUT_ELEMENT_DESC, InputSlotClass, AlignedByteOffset) &&
                  FOLLOWS(D3D11_INPUT_ELEMENT_DESC, InstanceDataStepRate, InputSlotClass),
              "Require no packing for memcmp to be valid operator==() implementation and for a "
              "hash of struct memory to be a valid hash.");

bool operator==(const D3D11_INPUT_ELEMENT_DESC& x, const D3D11_INPUT_ELEMENT_DESC& y) {
    return memcmp(&x, &y, sizeof(x)) == 0;
}

size_t hashHelper(const D3D11_INPUT_ELEMENT_DESC& x) {
    const size_t maxSemanticLength = 64;
    const auto semanticLength = strlen(x.SemanticName);
    assert(semanticLength < maxSemanticLength - 1);
    char lcSemantic[maxSemanticLength];
    transform(x.SemanticName, x.SemanticName + semanticLength + 1, begin(lcSemantic), tolower);
    return hashWithSeed(reinterpret_cast<const char*>(&x.SemanticIndex),
                        sizeof(x) - offsetof(D3D11_INPUT_ELEMENT_DESC, SemanticIndex),
                        hashWithSeed(lcSemantic, semanticLength, 0));
}

OFFSETOF_SIZEOF(D3D11_RENDER_TARGET_BLEND_DESC, BlendEnable);
OFFSETOF_SIZEOF(D3D11_RENDER_TARGET_BLEND_DESC, SrcBlend);
OFFSETOF_SIZEOF(D3D11_RENDER_TARGET_BLEND_DESC, DestBlend);
OFFSETOF_SIZEOF(D3D11_RENDER_TARGET_BLEND_DESC, BlendOp);
OFFSETOF_SIZEOF(D3D11_RENDER_TARGET_BLEND_DESC, SrcBlendAlpha);
OFFSETOF_SIZEOF(D3D11_RENDER_TARGET_BLEND_DESC, DestBlendAlpha);
OFFSETOF_SIZEOF(D3D11_RENDER_TARGET_BLEND_DESC, BlendOpAlpha);
OFFSETOF_SIZEOF(D3D11_RENDER_TARGET_BLEND_DESC, RenderTargetWriteMask);

static_assert(OFFSETOF_NAME(D3D11_RENDER_TARGET_BLEND_DESC, BlendEnable) == 0 &&
                  FOLLOWS(D3D11_RENDER_TARGET_BLEND_DESC, SrcBlend, BlendEnable) &&
                  FOLLOWS(D3D11_RENDER_TARGET_BLEND_DESC, DestBlend, SrcBlend) &&
                  FOLLOWS(D3D11_RENDER_TARGET_BLEND_DESC, BlendOp, DestBlend) &&
                  FOLLOWS(D3D11_RENDER_TARGET_BLEND_DESC, SrcBlendAlpha, BlendOp) &&
                  FOLLOWS(D3D11_RENDER_TARGET_BLEND_DESC, DestBlendAlpha, SrcBlendAlpha) &&
                  FOLLOWS(D3D11_RENDER_TARGET_BLEND_DESC, BlendOpAlpha, DestBlendAlpha) &&
                  FOLLOWS(D3D11_RENDER_TARGET_BLEND_DESC, RenderTargetWriteMask, BlendOpAlpha),
              "Require no packing for memcmp to be valid operator==() implementation and for a "
              "hash of struct memory to be a valid hash.");

bool operator==(const D3D11_RENDER_TARGET_BLEND_DESC& a, const D3D11_RENDER_TARGET_BLEND_DESC& b) {
    return memcmp(&a, &b, sizeof(a)) == 0;
}

OFFSETOF_SIZEOF(CD3D11_BLEND_DESC, AlphaToCoverageEnable);
OFFSETOF_SIZEOF(CD3D11_BLEND_DESC, IndependentBlendEnable);
OFFSETOF_SIZEOF(CD3D11_BLEND_DESC, RenderTarget);

static_assert(OFFSETOF_NAME(CD3D11_BLEND_DESC, AlphaToCoverageEnable) == 0 &&
                  FOLLOWS(CD3D11_BLEND_DESC, IndependentBlendEnable, AlphaToCoverageEnable) &&
                  FOLLOWS(CD3D11_BLEND_DESC, RenderTarget, IndependentBlendEnable),
              "Require no packing for memcmp to be valid operator==() implementation and for a "
              "hash of struct memory to be a valid hash.");

bool operator==(const CD3D11_BLEND_DESC& a, const CD3D11_BLEND_DESC& b) {
    return memcmp(&a, &b, sizeof(a)) == 0;
}

size_t hashHelper(const CD3D11_BLEND_DESC& x) {
    return hashWithSeed(reinterpret_cast<const char*>(&x), sizeof(x), 0);
}

OFFSETOF_SIZEOF(CD3D11_RASTERIZER_DESC, FillMode);
OFFSETOF_SIZEOF(CD3D11_RASTERIZER_DESC, CullMode);
OFFSETOF_SIZEOF(CD3D11_RASTERIZER_DESC, FrontCounterClockwise);
OFFSETOF_SIZEOF(CD3D11_RASTERIZER_DESC, DepthBias);
OFFSETOF_SIZEOF(CD3D11_RASTERIZER_DESC, DepthBiasClamp);
OFFSETOF_SIZEOF(CD3D11_RASTERIZER_DESC, SlopeScaledDepthBias);
OFFSETOF_SIZEOF(CD3D11_RASTERIZER_DESC, DepthClipEnable);
OFFSETOF_SIZEOF(CD3D11_RASTERIZER_DESC, ScissorEnable);
OFFSETOF_SIZEOF(CD3D11_RASTERIZER_DESC, MultisampleEnable);
OFFSETOF_SIZEOF(CD3D11_RASTERIZER_DESC, AntialiasedLineEnable);

static_assert(OFFSETOF_NAME(CD3D11_RASTERIZER_DESC, FillMode) == 0 &&
                  FOLLOWS(CD3D11_RASTERIZER_DESC, CullMode, FillMode) &&
                  FOLLOWS(CD3D11_RASTERIZER_DESC, FrontCounterClockwise, CullMode) &&
                  FOLLOWS(CD3D11_RASTERIZER_DESC, DepthBias, FrontCounterClockwise) &&
                  FOLLOWS(CD3D11_RASTERIZER_DESC, DepthBiasClamp, DepthBias) &&
                  FOLLOWS(CD3D11_RASTERIZER_DESC, SlopeScaledDepthBias, DepthBiasClamp) &&
                  FOLLOWS(CD3D11_RASTERIZER_DESC, DepthClipEnable, SlopeScaledDepthBias) &&
                  FOLLOWS(CD3D11_RASTERIZER_DESC, ScissorEnable, DepthClipEnable) &&
                  FOLLOWS(CD3D11_RASTERIZER_DESC, MultisampleEnable, ScissorEnable) &&
                  FOLLOWS(CD3D11_RASTERIZER_DESC, AntialiasedLineEnable, MultisampleEnable),
              "Require no packing for memcmp to be valid operator==() implementation and for a "
              "hash of struct memory to be a valid hash.");

bool operator==(const CD3D11_RASTERIZER_DESC& a, const CD3D11_RASTERIZER_DESC& b) {
    return memcmp(&a, &b, sizeof(a)) == 0;
}

size_t hashHelper(const CD3D11_RASTERIZER_DESC& x) {
    return hashWithSeed(reinterpret_cast<const char*>(&x), sizeof(x), 0);
}

OFFSETOF_SIZEOF(D3D11_DEPTH_STENCILOP_DESC, StencilFailOp);
OFFSETOF_SIZEOF(D3D11_DEPTH_STENCILOP_DESC, StencilDepthFailOp);
OFFSETOF_SIZEOF(D3D11_DEPTH_STENCILOP_DESC, StencilPassOp);
OFFSETOF_SIZEOF(D3D11_DEPTH_STENCILOP_DESC, StencilFunc);

static_assert(OFFSETOF_NAME(D3D11_DEPTH_STENCILOP_DESC, StencilFailOp) == 0 &&
                  FOLLOWS(D3D11_DEPTH_STENCILOP_DESC, StencilDepthFailOp, StencilFailOp) &&
                  FOLLOWS(D3D11_DEPTH_STENCILOP_DESC, StencilPassOp, StencilDepthFailOp) &&
                  FOLLOWS(D3D11_DEPTH_STENCILOP_DESC, StencilFunc, StencilPassOp),
              "Require no packing for memcmp to be valid operator==() implementation and for a "
              "hash of struct memory to be a valid hash.");

bool operator==(const D3D11_DEPTH_STENCILOP_DESC& a, const D3D11_DEPTH_STENCILOP_DESC& b) {
    return memcmp(&a, &b, sizeof(a)) == 0;
}

OFFSETOF_SIZEOF(CD3D11_DEPTH_STENCIL_DESC, DepthEnable);
OFFSETOF_SIZEOF(CD3D11_DEPTH_STENCIL_DESC, DepthWriteMask);
OFFSETOF_SIZEOF(CD3D11_DEPTH_STENCIL_DESC, DepthFunc);
OFFSETOF_SIZEOF(CD3D11_DEPTH_STENCIL_DESC, StencilEnable);
OFFSETOF_SIZEOF(CD3D11_DEPTH_STENCIL_DESC, StencilReadMask);
OFFSETOF_SIZEOF(CD3D11_DEPTH_STENCIL_DESC, StencilWriteMask);
OFFSETOF_SIZEOF(CD3D11_DEPTH_STENCIL_DESC, FrontFace);
OFFSETOF_SIZEOF(CD3D11_DEPTH_STENCIL_DESC, BackFace);

// Note: there is padding in this structure between StencilWriteMask and FrontFace, hence we have a
// slightly more complex compare than a straight memcmp below
static_assert(OFFSETOF_NAME(CD3D11_DEPTH_STENCIL_DESC, DepthEnable) == 0 &&
                  FOLLOWS(CD3D11_DEPTH_STENCIL_DESC, DepthWriteMask, DepthEnable) &&
                  FOLLOWS(CD3D11_DEPTH_STENCIL_DESC, DepthFunc, DepthWriteMask) &&
                  FOLLOWS(CD3D11_DEPTH_STENCIL_DESC, StencilEnable, DepthFunc) &&
                  FOLLOWS(CD3D11_DEPTH_STENCIL_DESC, StencilReadMask, StencilEnable) &&
                  FOLLOWS(CD3D11_DEPTH_STENCIL_DESC, StencilWriteMask, StencilReadMask) &&
                  FOLLOWS(CD3D11_DEPTH_STENCIL_DESC, BackFace, FrontFace),
              "Require expected packing for operator==() below to be valid.");

bool operator==(const CD3D11_DEPTH_STENCIL_DESC& a, const CD3D11_DEPTH_STENCIL_DESC& b) {
    return memcmp(&a, &b, OFFSETOF_NAME(CD3D11_DEPTH_STENCIL_DESC, StencilWriteMask) +
                              SIZEOF_NAME(CD3D11_DEPTH_STENCIL_DESC, StencilWriteMask)) == 0 &&
           a.FrontFace == b.FrontFace && a.BackFace == b.BackFace;
}

bool operator==(const LUID& a, const LUID& b) {
    return tie(a.HighPart, a.LowPart) == tie(b.HighPart, b.LowPart);
}

size_t hashHelper(const CD3D11_DEPTH_STENCIL_DESC& x) {
    return hashWithSeed(
        reinterpret_cast<const char*>(&x.FrontFace), sizeof(x.FrontFace) + sizeof(x.BackFace),
        hashWithSeed(
            reinterpret_cast<const char*>(&x),
            offsetof(CD3D11_DEPTH_STENCIL_DESC, StencilWriteMask) + sizeof(x.StencilWriteMask), 0));
}

// Helpers for calling various DXGI and D3D Create functions
IDXGIFactory1Ptr CreateDXGIFactory1() {
    IDXGIFactory1Ptr res;
    ThrowOnFailure(
        CreateDXGIFactory1(__uuidof(res), reinterpret_cast<void**>(res.ReleaseAndGetAddressOf())));
    return res;
}

bool EnumIDXGIAdapters::Iterator::operator==(const EnumIDXGIAdapters::Iterator& x) const {
    return tie(dxgiFactory_, adapter_, iAdapter_) == tie(x.dxgiFactory_, x.adapter_, x.iAdapter_);
}

void EnumIDXGIAdapters::Iterator::enumCurrentAdapter() {
    if (dxgiFactory_->EnumAdapters1(iAdapter_, &adapter_) == DXGI_ERROR_NOT_FOUND) {
        dxgiFactory_.Reset();
        adapter_.Reset();
        iAdapter_ = UINT(-1);
    }
}
