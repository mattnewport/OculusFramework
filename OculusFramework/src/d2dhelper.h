#pragma once

#include "d3dhelper.h"

#include <d2d1_1.h>
#include <d2d1_1helper.h>
#include <dwrite.h>

#include <functional>

using ID2D1Factory1Ptr = Microsoft::WRL::ComPtr<ID2D1Factory1>;
using ID2D1GeometrySinkPtr = Microsoft::WRL::ComPtr<ID2D1GeometrySink>;
using ID2D1PathGeometryPtr = Microsoft::WRL::ComPtr<ID2D1PathGeometry>;
using ID2D1RenderTargetPtr = Microsoft::WRL::ComPtr<ID2D1RenderTarget>;
using ID2D1SolidColorBrushPtr = Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>;
using IDWriteFactoryPtr = Microsoft::WRL::ComPtr<IDWriteFactory>;
using IDWriteTextFormatPtr = Microsoft::WRL::ComPtr<IDWriteTextFormat>;
using IDWriteTextLayoutPtr = Microsoft::WRL::ComPtr<IDWriteTextLayout>;
using ID2D1DevicePtr = Microsoft::WRL::ComPtr<ID2D1Device>;
using ID2D1DeviceContextPtr = Microsoft::WRL::ComPtr<ID2D1DeviceContext>;
using ID2D1Bitmap1Ptr = Microsoft::WRL::ComPtr<ID2D1Bitmap1>;

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

inline auto CreateD2D1Factory1() {
    ID2D1Factory1Ptr res;
    ThrowOnFailure(
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, res.ReleaseAndGetAddressOf()));
    return res;
}

inline auto CreateD2D1DeviceAndDeviceContext(ID3D11Device* d3dDevice, ID2D1Factory1* d2d1Factory) {
    auto d3dDevicePtr = ID3D11DevicePtr{d3dDevice};
    IDXGIDevice1Ptr dxgiDevice1;
    ThrowOnFailure(d3dDevicePtr.As(&dxgiDevice1));
    ID2D1DevicePtr d2d1Device;
    d2d1Factory->CreateDevice(dxgiDevice1.Get(), d2d1Device.ReleaseAndGetAddressOf());
    ID2D1DeviceContextPtr d2d1DeviceContext;
    d2d1Device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2d1DeviceContext.ReleaseAndGetAddressOf());
    return std::make_tuple(d2d1Device, d2d1DeviceContext);
}

inline void DrawToRenderTargetTexture(
    ID2D1Factory1* d2d1Factory1, ID2D1DeviceContext* d2d1Context,
    ID3D11Texture2DPtr renderTargetTex,
    std::function<void(ID2D1Factory1*, ID2D1DeviceContext*)> drawFunction) {
    // Create a DXGI surface from the renderTargetTex that we can pass to D2D
    IDXGIResource1Ptr dxgiResource1;
    ThrowOnFailure(renderTargetTex.As(&dxgiResource1));
    IDXGISurface2Ptr dxgiSurface2;
    ThrowOnFailure(
        dxgiResource1->CreateSubresourceSurface(0, dxgiSurface2.ReleaseAndGetAddressOf()));

    ID2D1Bitmap1Ptr d2d1Bitmap1;
    const auto props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);
    ThrowOnFailure(d2d1Context->CreateBitmapFromDxgiSurface(dxgiSurface2.Get(), &props,
                                                            d2d1Bitmap1.ReleaseAndGetAddressOf()));

    // Call BeginDraw() on the D2D render target and then call the user provided draw function
    d2d1Context->SetTarget(d2d1Bitmap1.Get());
    d2d1Context->BeginDraw();
    drawFunction(d2d1Factory1, d2d1Context);
    d2d1Context->EndDraw();
    d2d1Context->SetTarget(nullptr);
}
