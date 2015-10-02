#pragma once

#include "d3dhelper.h"

#pragma warning(push)
#pragma warning(disable: 4244 4127)
#include "OVR_CAPI_D3D.h"
#pragma warning(pop)

#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

struct DirectX11;

namespace libovrwrapper {

void throwOvrError(const char* msg, ovrHmd hmd = nullptr);

// Temp
struct OculusTexture {
    ovrSwapTextureSet* TextureSet;
    ovrD3D11Texture dummyTexture;
    ID3D11RenderTargetView* TexRtv[3];

    OculusTexture(ovrHmd hmd, ovrSizei size, ID3D11Device* device) : dummyTexture{} {
        D3D11_TEXTURE2D_DESC dsDesc;
        dsDesc.Width = size.w;
        dsDesc.Height = size.h;
        dsDesc.MipLevels = 1;
        dsDesc.ArraySize = 1;
        dsDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        dsDesc.SampleDesc.Count = 1;  // No multi-sampling allowed
        dsDesc.SampleDesc.Quality = 0;
        dsDesc.Usage = D3D11_USAGE_DEFAULT;
        dsDesc.CPUAccessFlags = 0;
        dsDesc.MiscFlags = 0;
        dsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

        if (!hmd) {
            TextureSet = new ovrSwapTextureSet{};
            TextureSet->Textures = &dummyTexture.Texture;
            TextureSet->TextureCount = 1;
            TextureSet->CurrentIndex = 0;
            device->CreateTexture2D(&dsDesc, nullptr, &dummyTexture.D3D11.pTexture);
            device->CreateShaderResourceView(dummyTexture.D3D11.pTexture, nullptr, &dummyTexture.D3D11.pSRView);
        } else {
            if (!OVR_SUCCESS(ovr_CreateSwapTextureSetD3D11(hmd, device, &dsDesc, 0, &TextureSet)))
                throwOvrError("ovrHmd_CreateSwapTextureSetD3D11() failed!", hmd);
        }
    }

    void AdvanceToNextTexture() {
        TextureSet->CurrentIndex = (TextureSet->CurrentIndex + 1) % TextureSet->TextureCount;
    }
    void Release(ovrHmd hmd) { 
        if (hmd) {
            ovr_DestroySwapTextureSet(hmd, TextureSet);
        } else {
            dummyTexture.D3D11.pSRView->Release();
            dummyTexture.D3D11.pTexture->Release();
            delete TextureSet;
        }
    }
};

// Interface let's us easily run without SDK which avoids problems doing PIX captures with HMD

class IHmd {
public:
    IHmd() = default;
    IHmd(const IHmd&) = delete;
    IHmd(IHmd&& x) = default;
    virtual ~IHmd() = 0;

    virtual ovrVector2i getWindowsPos() const = 0;
    virtual ovrSizei getResolution() const = 0;
    virtual ovrFovPort getDefaultEyeFov(ovrEyeType eye) const = 0;

    virtual bool testCap(ovrHmdCaps cap) const = 0;
    virtual void setCap(ovrHmdCaps cap) = 0;

    virtual OculusTexture* createSwapTextureSetD3D11(ovrSizei size, ID3D11Device* device) = 0;
    virtual void destroySwapTextureSetD3D11(OculusTexture* tex) = 0;
    virtual ovrTexture* createMirrorTextureD3D11(ID3D11Device* device, const D3D11_TEXTURE2D_DESC& desc) = 0;
    virtual void destroyMirrorTextureD3D11(ovrTexture* tex) = 0;

    virtual void configureTracking(unsigned int supportedTrackingCaps,
        unsigned int requiredTrackingCaps = 0) = 0;

    virtual std::array<ovrEyeRenderDesc, 2> getRenderDesc() = 0;

    virtual bool submitFrame(unsigned int frameIndex, const ovrViewScaleDesc* viewScaleDesc,
        ovrLayerHeader const* const* layerPtrList, unsigned int layerCount) = 0;

    virtual ovrSizei getFovTextureSize(ovrEyeType eye) = 0;

    virtual ovrSizei getFovTextureSize(ovrEyeType eye, ovrFovPort fov, float pixelsPerDisplayPixel = 1.0f) = 0;

    virtual float getProperty(const char* propertyName, float defaultValue) const = 0;

    virtual std::pair<std::array<ovrPosef, 2>, ovrTrackingState> getEyePoses(
        unsigned int frameIndex, ovrVector3f hmdToEyeViewOffset[2]) const = 0;

    virtual void recenterPose() = 0;

    virtual const LUID* getAdapterLuid() const = 0;
};

class DummyHmd : public IHmd {
public:
    DummyHmd();
    ~DummyHmd();

    // Inherited via IHmd
    virtual ovrVector2i getWindowsPos() const override;
    virtual ovrSizei getResolution() const override;
    virtual ovrFovPort getDefaultEyeFov(ovrEyeType eye) const override;
    virtual bool testCap(ovrHmdCaps cap) const override;
    virtual void setCap(ovrHmdCaps cap) override;
    virtual OculusTexture * createSwapTextureSetD3D11(ovrSizei size, ID3D11Device * device) override;
    virtual void destroySwapTextureSetD3D11(OculusTexture * tex) override;
    virtual ovrTexture * createMirrorTextureD3D11(ID3D11Device * device, const D3D11_TEXTURE2D_DESC & desc) override;
    virtual void destroyMirrorTextureD3D11(ovrTexture* tex) override;
    virtual void configureTracking(unsigned int supportedTrackingCaps, unsigned int requiredTrackingCaps) override;
    virtual std::array<ovrEyeRenderDesc, 2> getRenderDesc() override;
    virtual bool submitFrame(unsigned int frameIndex, const ovrViewScaleDesc* viewScaleDesc,
        ovrLayerHeader const* const* layerPtrList, unsigned int layerCount) override;
    virtual ovrSizei getFovTextureSize(ovrEyeType eye) override;
    virtual ovrSizei getFovTextureSize(ovrEyeType eye, ovrFovPort fov, float pixelsPerDisplayPixel = 1.0f) override;
    virtual float getProperty(const char * propertyName, float defaultValue) const override;
    virtual std::pair<std::array<ovrPosef, 2>, ovrTrackingState> getEyePoses(unsigned int frameIndex, ovrVector3f hmdToEyeViewOffset[2]) const override;
    virtual void recenterPose() override {}
    virtual const LUID* getAdapterLuid() const { return nullptr; }

    // Non inherited
    void setDirectX11(DirectX11& directX11_);
private:
    struct RenderHelper;
    friend struct RenderHelper;
    std::unique_ptr<RenderHelper> renderHelper;
    std::array<ovrEyeRenderDesc, 2> eyeRenderDescs;
};

class Hmd : public IHmd {
    friend class Ovr;

public:
    Hmd();
    Hmd(const Hmd&) = delete;
    ~Hmd();

    ovrVector2i getWindowsPos() const override { return{ 0, 0 }; }
    ovrSizei getResolution() const override { return getHmdDesc().Resolution; }
    ovrFovPort getDefaultEyeFov(ovrEyeType eye) const override { return getHmdDesc().DefaultEyeFov[eye]; }

    bool testCap(ovrHmdCaps cap) const override { return static_cast<ovrHmdCaps>(ovr_GetEnabledCaps(hmd_) & cap) == cap; }
    void setCap(ovrHmdCaps cap) override {
        int enabledCaps = ovr_GetEnabledCaps(hmd_);
        enabledCaps |= cap;
        ovr_SetEnabledCaps(hmd_, enabledCaps);
    }

    OculusTexture* createSwapTextureSetD3D11(ovrSizei size, ID3D11Device* device) override {
        return new OculusTexture{ hmd_, size, device };
    }

    void destroySwapTextureSetD3D11(OculusTexture* tex) override {
        tex->Release(hmd_);
    }

    ovrTexture* createMirrorTextureD3D11(ID3D11Device* device,
        const D3D11_TEXTURE2D_DESC& desc) override {
        ovrTexture* mirrorTexture = nullptr;
        if (!OVR_SUCCESS(ovr_CreateMirrorTextureD3D11(hmd_, device, &desc, 0, &mirrorTexture)))
            throwOvrError("ovrHmd_CreateMirrorTextureD3D11() failed!");
        return mirrorTexture;
    }

    void destroyMirrorTextureD3D11(ovrTexture* tex) override {
        ovr_DestroyMirrorTexture(hmd_, tex);
    }

    void configureTracking(unsigned int supportedTrackingCaps,
        unsigned int requiredTrackingCaps) override {
        if (!OVR_SUCCESS(ovr_ConfigureTracking(hmd_, supportedTrackingCaps, requiredTrackingCaps)))
            throwOvrError("ovrHmd_ConfigureTracking() returned false!", hmd_);
    }

    std::array<ovrEyeRenderDesc, 2> getRenderDesc() override {
        std::array<ovrEyeRenderDesc, 2> res;
        res[0] = ovr_GetRenderDesc(hmd_, ovrEye_Left, getHmdDesc().DefaultEyeFov[0]);
        res[1] = ovr_GetRenderDesc(hmd_, ovrEye_Right, getHmdDesc().DefaultEyeFov[1]);
        return res;
    }

    bool submitFrame(unsigned int frameIndex, const ovrViewScaleDesc* viewScaleDesc,
        ovrLayerHeader const* const* layerPtrList, unsigned int layerCount) {
        return ovr_SubmitFrame(hmd_, frameIndex, viewScaleDesc, layerPtrList, layerCount) ==
            ovrSuccess;
    }

    ovrSizei getFovTextureSize(ovrEyeType eye) override {
        return getFovTextureSize(eye, getHmdDesc().DefaultEyeFov[eye]);
    }

    ovrSizei getFovTextureSize(ovrEyeType eye, ovrFovPort fov, float pixelsPerDisplayPixel = 1.0f) override {
        return ovr_GetFovTextureSize(hmd_, eye, fov, pixelsPerDisplayPixel);
    }

    float getProperty(const char* propertyName, float defaultValue) const override {
        return ovr_GetFloat(hmd_, propertyName, defaultValue);
    }

    std::pair<std::array<ovrPosef, 2>, ovrTrackingState> getEyePoses(
        unsigned int frameIndex, ovrVector3f hmdToEyeViewOffset[2]) const override {
        std::pair<std::array<ovrPosef, 2>, ovrTrackingState> res;
        ovr_GetEyePoses(hmd_, frameIndex, hmdToEyeViewOffset, res.first.data(), &res.second);
        return res;
    }

    void recenterPose() override {
        ovr_RecenterPose(hmd_);
    }

    const LUID* getAdapterLuid() const { return reinterpret_cast<const LUID*>(&luid_); }

private:
    ovrHmdDesc getHmdDesc() const { 
        auto desc = ovr_GetHmdDesc(nullptr);
        assert(desc.Type != ovrHmd_None);
        return desc;
    }

    ovrHmd hmd_;
    ovrGraphicsLuid luid_;
};

}
