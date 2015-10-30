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

class SwapTextureSet {
    struct TextureSetBase {
        virtual ~TextureSetBase() {}
        virtual ID3D11Texture2D* d3dTexture() const = 0;
        virtual ovrSwapTextureSet* swapTextureSet() const = 0;
        virtual void advanceToNextTexture() = 0;
    };

    struct DummyTextureSet : TextureSetBase {
        std::unique_ptr<ovrSwapTextureSet> TextureSet;
        ID3D11Texture2DPtr tex;
        ID3D11ShaderResourceViewPtr srv;
        ovrD3D11Texture dummyTexture;

        DummyTextureSet(ID3D11Device* device, const D3D11_TEXTURE2D_DESC& texDesc) : dummyTexture{} {
            TextureSet =
                std::make_unique<ovrSwapTextureSet>(ovrSwapTextureSet{&dummyTexture.Texture, 1, 0});
            std::tie(tex, srv) = CreateTexture2DAndShaderResourceView(device, texDesc);
            dummyTexture.D3D11.pTexture = tex.Get();
            dummyTexture.D3D11.pSRView = srv.Get();
        }

        ID3D11Texture2D* d3dTexture() const override { return tex.Get(); }
        ovrSwapTextureSet* swapTextureSet() const override { return TextureSet.get(); }
        void advanceToNextTexture() override {}
    };

    struct OvrTextureSet : TextureSetBase {
        std::unique_ptr<ovrSwapTextureSet, std::function<void(ovrSwapTextureSet*)>> TextureSet;

        OvrTextureSet(ovrHmd hmd, ID3D11Device* device, const D3D11_TEXTURE2D_DESC& texDesc) {
            ovrSwapTextureSet* ts{};
            if (!OVR_SUCCESS(ovr_CreateSwapTextureSetD3D11(hmd, device, &texDesc, 0, &ts)))
                throwOvrError("ovrHmd_CreateSwapTextureSetD3D11() failed!", hmd);
            TextureSet = {ts, [hmd](ovrSwapTextureSet* ts) { ovr_DestroySwapTextureSet(hmd, ts); }};
        }

        ID3D11Texture2D* d3dTexture() const override {
            return reinterpret_cast<ovrD3D11Texture&>(
                       TextureSet->Textures[TextureSet->CurrentIndex])
                .D3D11.pTexture;
        }

        ovrSwapTextureSet* swapTextureSet() const override { return TextureSet.get(); }
        void advanceToNextTexture() override {
            TextureSet->CurrentIndex = (TextureSet->CurrentIndex + 1) % TextureSet->TextureCount;
        }
    };

public:
    SwapTextureSet(ovrHmd hmd, ovrSizei size, ID3D11Device* device) {
        const auto texDesc = Texture2DDesc(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, size.w, size.h)
                                 .mipLevels(1)
                                 .bindFlags(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

        if (hmd) {
            tex = std::make_unique<OvrTextureSet>(hmd, device, texDesc);
        } else {
            tex = std::make_unique<DummyTextureSet>(device, texDesc);
        }
    }

    auto d3dTexture() const { return tex->d3dTexture(); }
    auto swapTextureSet() const { return tex->swapTextureSet(); }
    void advanceToNextTexture() { tex->advanceToNextTexture(); }

private:
    std::unique_ptr<TextureSetBase> tex;
};

class MirrorTexture {
    struct MirrorTextureBase {
        virtual ~MirrorTextureBase() {}
        virtual ID3D11Texture2D* d3dTexture() const = 0;
        virtual ID3D11RenderTargetView* d3dRenderTargetView() const = 0;
    };

    struct OvrMirrorTexture : public MirrorTextureBase {
        std::unique_ptr<ovrTexture, std::function<void(ovrTexture*)>> tex;
        OvrMirrorTexture(ovrHmd hmd, ID3D11Device* device, const D3D11_TEXTURE2D_DESC& desc) {
            ovrTexture* mirrorTexture = nullptr;
            if (!OVR_SUCCESS(ovr_CreateMirrorTextureD3D11(hmd, device, &desc, 0, &mirrorTexture)))
                throwOvrError("ovrHmd_CreateMirrorTextureD3D11() failed!");
            tex = {mirrorTexture, [hmd](ovrTexture* t) { ovr_DestroyMirrorTexture(hmd, t); }};
        }

        ID3D11Texture2D* d3dTexture() const override {
            return reinterpret_cast<ovrD3D11Texture*>(tex.get())->D3D11.pTexture;
        }
        ID3D11RenderTargetView* d3dRenderTargetView() const override { return nullptr; }
    };

    struct DummyMirrorTexture : public MirrorTextureBase {
        ID3D11Texture2DPtr tex;
        ID3D11RenderTargetViewPtr rtv;
        DummyMirrorTexture(ID3D11Device* device, const D3D11_TEXTURE2D_DESC& desc) {
            auto newDesc = desc;
            newDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
            std::tie(tex, rtv) = CreateTexture2DAndRenderTargetView(device, newDesc);
        }

        ID3D11Texture2D* d3dTexture() const override { return tex.Get(); }
        ID3D11RenderTargetView* d3dRenderTargetView() const override { return rtv.Get(); }
    };

public:
    MirrorTexture(ovrHmd hmd, ID3D11Device* device, const D3D11_TEXTURE2D_DESC& desc) {
        if (hmd) {
            tex = std::make_unique<OvrMirrorTexture>(hmd, device, desc);
        }
        else {
            tex = std::make_unique<DummyMirrorTexture>(device, desc);
        }
    }

    ID3D11Texture2D* d3dTexture() const { return tex->d3dTexture(); }
    ID3D11RenderTargetView* d3dRenderTargetView() const { return tex->d3dRenderTargetView(); }

private:
    std::unique_ptr<MirrorTextureBase> tex;
};

// Interface let's us easily run without SDK which avoids problems doing PIX captures with HMD

class IHmd {
public:
    IHmd() = default;
    IHmd(const IHmd&) = delete;
    IHmd(IHmd&& x) = default;
    virtual ~IHmd() = 0;

    virtual double getTimeInSeconds() const = 0;

    virtual ovrVector2i getWindowsPos() const = 0;
    virtual ovrSizei getResolution() const = 0;
    virtual ovrFovPort getDefaultEyeFov(ovrEyeType eye) const = 0;

    virtual bool testCap(ovrHmdCaps cap) const = 0;
    virtual void setCap(ovrHmdCaps cap) = 0;

    virtual SwapTextureSet createSwapTextureSetD3D11(ovrSizei size, ID3D11Device* device) = 0;
    virtual MirrorTexture createMirrorTextureD3D11(ID3D11Device* device,
                                                   const D3D11_TEXTURE2D_DESC& desc) = 0;

    virtual void configureTracking(unsigned int supportedTrackingCaps,
                                   unsigned int requiredTrackingCaps = 0) = 0;

    virtual std::array<ovrEyeRenderDesc, 2> getRenderDesc() = 0;

    virtual bool submitFrame(unsigned int frameIndex, const ovrViewScaleDesc* viewScaleDesc,
                             ovrLayerHeader const* const* layerPtrList,
                             unsigned int layerCount) = 0;

    virtual ovrSizei getFovTextureSize(ovrEyeType eye) = 0;

    virtual ovrSizei getFovTextureSize(ovrEyeType eye, ovrFovPort fov,
                                       float pixelsPerDisplayPixel = 1.0f) = 0;

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
    virtual double getTimeInSeconds() const override;
    virtual ovrVector2i getWindowsPos() const override;
    virtual ovrSizei getResolution() const override;
    virtual ovrFovPort getDefaultEyeFov(ovrEyeType eye) const override;
    virtual bool testCap(ovrHmdCaps cap) const override;
    virtual void setCap(ovrHmdCaps cap) override;
    virtual SwapTextureSet createSwapTextureSetD3D11(ovrSizei size, ID3D11Device * device) override;
    virtual MirrorTexture createMirrorTextureD3D11(ID3D11Device * device, const D3D11_TEXTURE2D_DESC & desc) override;
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
    void setDirectX11(DirectX11& directX11_) { directX11 = &directX11_; }

private:
    DirectX11* directX11 = nullptr;
    ID3D11RenderTargetView* mirrorTextureRtv = nullptr;
    std::array<ovrEyeRenderDesc, 2> eyeRenderDescs;
};

class OvrHmd : public IHmd {
public:
    OvrHmd();
    OvrHmd(const OvrHmd&) = delete;
    ~OvrHmd();

    double getTimeInSeconds() const override { return ovr_GetTimeInSeconds(); }
    ovrVector2i getWindowsPos() const override { return {0, 0}; }
    ovrSizei getResolution() const override { return getHmdDesc().Resolution; }
    ovrFovPort getDefaultEyeFov(ovrEyeType eye) const override {
        return getHmdDesc().DefaultEyeFov[eye];
    }

    bool testCap(ovrHmdCaps cap) const override {
        return static_cast<ovrHmdCaps>(ovr_GetEnabledCaps(hmd_) & cap) == cap;
    }
    void setCap(ovrHmdCaps cap) override {
        ovr_SetEnabledCaps(hmd_, ovr_GetEnabledCaps(hmd_) | cap);
    }

    SwapTextureSet createSwapTextureSetD3D11(ovrSizei size, ID3D11Device* device) override {
        return {hmd_, size, device};
    }

    MirrorTexture createMirrorTextureD3D11(ID3D11Device* device,
                                           const D3D11_TEXTURE2D_DESC& desc) override {
        return {hmd_, device, desc};
    }

    void configureTracking(unsigned int supportedTrackingCaps,
                           unsigned int requiredTrackingCaps) override {
        if (!OVR_SUCCESS(ovr_ConfigureTracking(hmd_, supportedTrackingCaps, requiredTrackingCaps)))
            throwOvrError("ovrHmd_ConfigureTracking() returned false!", hmd_);
    }

    std::array<ovrEyeRenderDesc, 2> getRenderDesc() override {
        return {ovr_GetRenderDesc(hmd_, ovrEye_Left, getHmdDesc().DefaultEyeFov[ovrEye_Left]),
                ovr_GetRenderDesc(hmd_, ovrEye_Right, getHmdDesc().DefaultEyeFov[ovrEye_Right])};
    }

    bool submitFrame(unsigned int frameIndex, const ovrViewScaleDesc* viewScaleDesc,
                     ovrLayerHeader const* const* layerPtrList, unsigned int layerCount) {
        return ovr_SubmitFrame(hmd_, frameIndex, viewScaleDesc, layerPtrList, layerCount) ==
               ovrSuccess;
    }

    ovrSizei getFovTextureSize(ovrEyeType eye) override {
        return getFovTextureSize(eye, getHmdDesc().DefaultEyeFov[eye]);
    }

    ovrSizei getFovTextureSize(ovrEyeType eye, ovrFovPort fov,
                               float pixelsPerDisplayPixel = 1.0f) override {
        return ovr_GetFovTextureSize(hmd_, eye, fov, pixelsPerDisplayPixel);
    }

    float getProperty(const char* propertyName, float defaultValue) const override {
        return ovr_GetFloat(hmd_, propertyName, defaultValue);
    }

    std::pair<std::array<ovrPosef, 2>, ovrTrackingState> getEyePoses(
        unsigned int frameIndex, ovrVector3f hmdToEyeViewOffset[2]) const override {
        std::pair<std::array<ovrPosef, 2>, ovrTrackingState> res;
        ovr_GetEyePoses(hmd_, frameIndex, true, hmdToEyeViewOffset, res.first.data(), &res.second);
        return res;
    }

    void recenterPose() override { ovr_RecenterPose(hmd_); }

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
