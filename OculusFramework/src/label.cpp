#include "label.h"

#include <string>

using namespace std;

Label::Label(ID3D11Device* device, const char* /*labelText*/) {
    const auto texFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    const auto width = 512, height = 512;
    tex = CreateTexture2D(device,
                          Texture2DDesc{texFormat, UINT(width), UINT(height)}
                              .bindFlags(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE)
                              .mipLevels(1),
                          "Label::tex");
    texSrv = CreateShaderResourceView(device, tex.Get(), "Label::texSrv");

    IDXGISurface1Ptr dxgiSurface;
    ThrowOnFailure(tex.As(&dxgiSurface));

    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d1Factory.ReleaseAndGetAddressOf());

    const auto props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED), 120.0f, 120.0f);

    ID2D1RenderTargetPtr d2d1Rt;
    ThrowOnFailure(d2d1Factory->CreateDxgiSurfaceRenderTarget(dxgiSurface.Get(), props,
                                                              d2d1Rt.ReleaseAndGetAddressOf()));

    IDWriteFactoryPtr dwriteFactory;
    ThrowOnFailure(
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(dwriteFactory),
                            reinterpret_cast<IUnknown**>(dwriteFactory.ReleaseAndGetAddressOf())));

    IDWriteTextFormatPtr textFormat;
    ThrowOnFailure(dwriteFactory->CreateTextFormat(
        L"Calibri", nullptr, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 24.0f, L"en-us", textFormat.ReleaseAndGetAddressOf()));

    ID2D1SolidColorBrushPtr brush;
    ThrowOnFailure(d2d1Rt->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black),
                                                 brush.ReleaseAndGetAddressOf()));

    d2d1Rt->BeginDraw();
    d2d1Rt->SetTransform(D2D1::IdentityMatrix());
    d2d1Rt->Clear(D2D1::ColorF{D2D1::ColorF::White});
    const auto layoutRect = D2D1_RECT_F{0.0f, 0.0f, float(width), float(height)};
    const auto text = L"Test String"s;
    d2d1Rt->DrawTextW(text.c_str(), text.size(), textFormat.Get(), &layoutRect, brush.Get());
    ThrowOnFailure(d2d1Rt->EndDraw());
}
