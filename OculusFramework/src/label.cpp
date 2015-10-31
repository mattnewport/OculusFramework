#include "label.h"

#include "util.h"

#include <cmath>

using namespace std;
using namespace util;

Label::Label(ID3D11Device* device, ID3D11DeviceContext* context, ID2D1Factory1* d2d1Factory1,
             ID2D1DeviceContext* d2d1DeviceContext, const char* labelText) {
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

    DrawToRenderTargetTexture(
        d2d1Factory1, d2d1DeviceContext, tex.Get(),
        [this, textLayout](ID2D1Factory*, ID2D1RenderTarget* d2d1Rt) {
            auto brush = CreateSolidColorBrush(d2d1Rt, D2D1::ColorF(D2D1::ColorF::Black));
            d2d1Rt->SetTransform(D2D1::IdentityMatrix());
            d2d1Rt->Clear(D2D1::ColorF{D2D1::ColorF::White});
            d2d1Rt->DrawTextLayout({0.0f, 0.0f}, textLayout.Get(), brush.Get());
        });

    context->GenerateMips(texSrv.Get());
}
