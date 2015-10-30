#include "label.h"

#include "util.h"

#include <dxgi1_2.h>

#include <cmath>

using namespace std;
using namespace util;

Label::Label(ID3D11Device* device, ID3D11DeviceContext* context, const char* labelText) {
    IDWriteFactoryPtr dwriteFactory;
    ThrowOnFailure(
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(dwriteFactory),
                            reinterpret_cast<IUnknown**>(dwriteFactory.ReleaseAndGetAddressOf())));

    IDWriteTextFormatPtr textFormat;
    ThrowOnFailure(dwriteFactory->CreateTextFormat(
        L"Calibri", nullptr, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 36.0f, L"en-us", textFormat.ReleaseAndGetAddressOf()));
    const auto text = widen(gsl::ensure_z(labelText));
    IDWriteTextLayoutPtr textLayout;
    ThrowOnFailure(dwriteFactory->CreateTextLayout(text.c_str(), text.size(), textFormat.Get(),
                                                   1e6f, 1e6f,
                                                   textLayout.ReleaseAndGetAddressOf()));
    ThrowOnFailure(textLayout->GetMetrics(&textMetrics));
    width = std::ceil(textMetrics.width);
    height = std::ceil(textMetrics.height);

    tie(tex, texSrv) = CreateTexture2DAndShaderResourceView(
        device, Texture2DDesc{DXGI_FORMAT_B8G8R8A8_UNORM, to<UINT>(width), to<UINT>(height)}
                    .bindFlags(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE)
                    .miscFlags(D3D11_RESOURCE_MISC_GENERATE_MIPS),
        "Label::tex");
    IDXGIResource1Ptr dxgiResource1;
    ThrowOnFailure(tex.As(&dxgiResource1));
    IDXGISurface2Ptr dxgiSurface2;
    ThrowOnFailure(dxgiResource1->CreateSubresourceSurface(0, dxgiSurface2.ReleaseAndGetAddressOf()));
    IDXGISurface1Ptr dxgiSurface;
    ThrowOnFailure(dxgiSurface2.As(&dxgiSurface));

    const auto props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);

    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d1Factory.ReleaseAndGetAddressOf());
    ID2D1RenderTargetPtr d2d1Rt;
    ThrowOnFailure(d2d1Factory->CreateDxgiSurfaceRenderTarget(dxgiSurface.Get(), props,
        d2d1Rt.ReleaseAndGetAddressOf()));
    ID2D1SolidColorBrushPtr brush;
    ThrowOnFailure(d2d1Rt->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black),
                                                 brush.ReleaseAndGetAddressOf()));

    d2d1Rt->BeginDraw();
    d2d1Rt->SetTransform(D2D1::IdentityMatrix());
    d2d1Rt->Clear(D2D1::ColorF{D2D1::ColorF::White});
    d2d1Rt->DrawTextLayout({0.0f, 0.0f}, textLayout.Get(), brush.Get());
    ThrowOnFailure(d2d1Rt->EndDraw());
    context->GenerateMips(texSrv.Get());
}
