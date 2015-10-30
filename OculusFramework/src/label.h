#pragma once

#include "vector.h"

#include "d3dhelper.h"
#include "d2dhelper.h"

#include <utility>

class Label {
public:
    Label(ID3D11Device* device, ID3D11DeviceContext* context, const char* labelText);
    ID3D11ShaderResourceView* srv() { return texSrv.Get(); }
    auto getWidth() const { return width; }
    auto getHeight() const { return height; }
    auto getUvs() const {
        return std::pair<mathlib::Vec2f, mathlib::Vec2f>{{0.0f, 0.0f}, {1.0f, 1.0f}};
    }

private:
    // MNTODO: move this somewhere more appropriate
    ID2D1FactoryPtr d2d1Factory;

    ID3D11Texture2DPtr tex;
    ID3D11ShaderResourceViewPtr texSrv;
    DWRITE_TEXT_METRICS textMetrics = {};
    float width = 0.0f, height = 0.0f;
};
