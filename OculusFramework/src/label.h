#pragma once

#include "d3dhelper.h"

#include <d2d1.h>
#include <dwrite.h>

using ID2D1FactoryPtr = Microsoft::WRL::ComPtr<ID2D1Factory>;
using ID2D1RenderTargetPtr = Microsoft::WRL::ComPtr<ID2D1RenderTarget>;
using IDXGISurface1Ptr = Microsoft::WRL::ComPtr<IDXGISurface1>;
using IDWriteFactoryPtr = Microsoft::WRL::ComPtr<IDWriteFactory>;
using IDWriteTextFormatPtr = Microsoft::WRL::ComPtr<IDWriteTextFormat>;
using ID2D1SolidColorBrushPtr = Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>;

class Label {
public:
    Label(ID3D11Device* device, const char* labelText);
    ID3D11ShaderResourceView* srv() { return texSrv.Get(); }
private:
    // MNTODO: move this somewhere more appropriate
    ID2D1FactoryPtr d2d1Factory;

    ID3D11Texture2DPtr tex;
    ID3D11ShaderResourceViewPtr texSrv;
};
