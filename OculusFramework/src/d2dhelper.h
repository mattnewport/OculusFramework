#pragma once

#include "d3dhelper.h"

#include <d2d1.h>
#include <dwrite.h>

#include <functional>

using ID2D1FactoryPtr = Microsoft::WRL::ComPtr<ID2D1Factory>;
using ID2D1GeometrySinkPtr = Microsoft::WRL::ComPtr<ID2D1GeometrySink>;
using ID2D1PathGeometryPtr = Microsoft::WRL::ComPtr<ID2D1PathGeometry>;
using ID2D1RenderTargetPtr = Microsoft::WRL::ComPtr<ID2D1RenderTarget>;
using ID2D1SolidColorBrushPtr = Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>;
using IDWriteFactoryPtr = Microsoft::WRL::ComPtr<IDWriteFactory>;
using IDWriteTextFormatPtr = Microsoft::WRL::ComPtr<IDWriteTextFormat>;
using IDWriteTextLayoutPtr = Microsoft::WRL::ComPtr<IDWriteTextLayout>;

inline auto CreateSolidColorBrush(ID2D1RenderTarget* d2d1Rt, const D2D1_COLOR_F& color) {
    ID2D1SolidColorBrushPtr res;
    ThrowOnFailure(d2d1Rt->CreateSolidColorBrush(color, res.ReleaseAndGetAddressOf()));
    return res;
}

inline auto CreatePathGeometry(ID2D1Factory* factory) {
    ID2D1PathGeometryPtr res;
    ThrowOnFailure(factory->CreatePathGeometry(res.ReleaseAndGetAddressOf()));
    return res;
}

inline auto Open(ID2D1PathGeometry* pathGeometry) {
    ID2D1GeometrySinkPtr res;
    ThrowOnFailure(pathGeometry->Open(&res));
    return res;
}

inline void DrawToRenderTargetTexture(
    ID2D1Factory* d2d1Factory, ID3D11Texture2DPtr renderTargetTex,
    std::function<void(ID2D1Factory*, ID2D1RenderTarget*)> drawFunction) {
    // Create a DXGI surface from the renderTargetTex that we can pass to D2D
    IDXGIResource1Ptr dxgiResource1;
    ThrowOnFailure(renderTargetTex.As(&dxgiResource1));
    IDXGISurface2Ptr dxgiSurface2;
    ThrowOnFailure(
        dxgiResource1->CreateSubresourceSurface(0, dxgiSurface2.ReleaseAndGetAddressOf()));
    IDXGISurface1Ptr dxgiSurface1;
    ThrowOnFailure(dxgiSurface2.As(&dxgiSurface1));
    const auto props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);

    ID2D1RenderTargetPtr d2d1Rt;
    ThrowOnFailure(d2d1Factory->CreateDxgiSurfaceRenderTarget(dxgiSurface1.Get(), props,
                                                              d2d1Rt.ReleaseAndGetAddressOf()));

    // Call BeginDraw() on the D2D render target and then call the user provided draw function
    d2d1Rt->BeginDraw();
    drawFunction(d2d1Factory, d2d1Rt.Get());
    ThrowOnFailure(d2d1Rt->EndDraw());
}
