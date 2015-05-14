#pragma once

#include "OVR_CAPI.h"

#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

struct DirectX11;

namespace libovrwrapper {

void throwOvrError(const char* msg, ovrHmd hmd = nullptr);

// Interface let's us easily run without SDK which avoids problems doing PIX captures with HMD

class IHmd {
public:
    IHmd() = default;
    IHmd(const IHmd&) = delete;
    IHmd(IHmd&& x) = default;
    virtual ~IHmd() = 0;

    virtual ovrVector2i getWindowsPos() const = 0;

    virtual ovrSizei getResolution() const = 0;

    virtual bool testCap(ovrHmdCaps cap) const = 0;
    virtual void setCap(ovrHmdCaps cap) = 0;
    // OVR SDK API is a bit confusing here - GetEnabledCaps() is different from HmdCaps even though
    // it uses the same enum - it only deals with caps that can be enabled where HmdCaps deals with
    // hardware caps for the specified HMD.
    virtual bool getCap(ovrHmdCaps cap) const = 0;

    virtual void attachToWindow(void* window) = 0;

    virtual void configureTracking(unsigned int supportedTrackingCaps,
        unsigned int requiredTrackingCaps = 0) = 0;

    virtual std::array<ovrEyeRenderDesc, 2> configureRendering(const ovrRenderAPIConfig* apiConfig,
        unsigned int distortionCaps) = 0;

    virtual void shutdownRendering() = 0;

    virtual ovrFrameTiming beginFrame(unsigned int frameIndex) = 0;

    virtual void endFrame(const ovrPosef renderPose[2], const ovrTexture eyeTexture[2]) = 0;

    virtual ovrSizei getFovTextureSize(ovrEyeType eye) = 0;

    virtual ovrSizei getFovTextureSize(ovrEyeType eye, ovrFovPort fov, float pixelsPerDisplayPixel = 1.0f) = 0;

    virtual float getProperty(const char* propertyName, float defaultValue) const = 0;

    virtual std::pair<std::array<ovrPosef, 2>, ovrTrackingState> getEyePoses(
        unsigned int frameIndex, ovrVector3f hmdToEyeViewOffset[2]) const = 0;

    virtual void recenterPose() = 0;

    virtual void dismissHSWDisplay() = 0;
};

class DummyHmd : public IHmd {
public:
    DummyHmd();
    ~DummyHmd();

    // Inherited via IHmd
    virtual ovrVector2i getWindowsPos() const override;
    virtual ovrSizei getResolution() const override;
    virtual bool testCap(ovrHmdCaps cap) const override;
    virtual void setCap(ovrHmdCaps cap) override;
    virtual bool getCap(ovrHmdCaps cap) const override;
    virtual void attachToWindow(void * window) override;
    virtual void configureTracking(unsigned int supportedTrackingCaps, unsigned int requiredTrackingCaps = 0) override;
    virtual std::array<ovrEyeRenderDesc, 2> configureRendering(const ovrRenderAPIConfig * apiConfig, unsigned int distortionCaps) override;
    virtual void shutdownRendering() override;
    virtual ovrFrameTiming beginFrame(unsigned int frameIndex) override;
    virtual void endFrame(const ovrPosef renderPose[2], const ovrTexture eyeTexture[2]) override;
    virtual ovrSizei getFovTextureSize(ovrEyeType eye) override;
    virtual ovrSizei getFovTextureSize(ovrEyeType eye, ovrFovPort fov, float pixelsPerDisplayPixel = 1.0f) override;
    virtual float getProperty(const char * propertyName, float defaultValue) const override;
    virtual std::pair<std::array<ovrPosef, 2>, ovrTrackingState> getEyePoses(unsigned int frameIndex, ovrVector3f hmdToEyeViewOffset[2]) const override;
    virtual void recenterPose() override {}
    virtual void dismissHSWDisplay() override {}

    // Non inherited
    void setDirectX11(DirectX11& directX11_);
private:
    struct RenderHelper;
    friend struct RenderHelper;
    std::unique_ptr<RenderHelper> renderHelper;
};

class Hmd : public IHmd {
    friend class Ovr;

public:
    Hmd(const Hmd&) = delete;
    Hmd(Hmd&& x) : hmd_(std::move(x.hmd_)) { x.hmd_ = nullptr; }
    ~Hmd() {
        if (hmd_) ovrHmd_Destroy(hmd_);
    }

    ovrVector2i getWindowsPos() const override { return hmd_->WindowsPos; }

    ovrSizei getResolution() const override { return hmd_->Resolution; }

    bool testCap(ovrHmdCaps cap) const override { return static_cast<ovrHmdCaps>(hmd_->HmdCaps & cap) == cap; }
    void setCap(ovrHmdCaps cap) override {
        int enabledCaps = ovrHmd_GetEnabledCaps(hmd_);
        enabledCaps |= cap;
        ovrHmd_SetEnabledCaps(hmd_, enabledCaps);
    }
    // OVR SDK API is a bit confusing here - GetEnabledCaps() is different from HmdCaps even though
    // it uses the same enum - it only deals with caps that can be enabled where HmdCaps deals with
    // hardware caps for the specified HMD.
    bool getCap(ovrHmdCaps cap) const override { return static_cast<ovrHmdCaps>(ovrHmd_GetEnabledCaps(hmd_) & cap) == cap; }

    void attachToWindow(void* window) override {
        if (!ovrHmd_AttachToWindow(hmd_, window, nullptr, nullptr))
            throwOvrError("ovrHmd_AttachToWindow() returned false!", hmd_);
    }

    void configureTracking(unsigned int supportedTrackingCaps,
                           unsigned int requiredTrackingCaps = 0) override {
        if (!ovrHmd_ConfigureTracking(hmd_, supportedTrackingCaps, requiredTrackingCaps))
            throwOvrError("ovrHmd_ConfigureTracking() returned false!", hmd_);
    }

    std::array<ovrEyeRenderDesc, 2> configureRendering(const ovrRenderAPIConfig* apiConfig,
                                                       unsigned int distortionCaps) override {
        std::array<ovrEyeRenderDesc, 2> res;
        if (!ovrHmd_ConfigureRendering(hmd_, apiConfig, distortionCaps, hmd_->DefaultEyeFov,
                                       res.data()))
            throwOvrError("ovrHmd_ConfigureRendering returned false!", hmd_);
        return res;
    }

    void shutdownRendering() override {
        std::array<ovrEyeRenderDesc, 2> res;
        if (!ovrHmd_ConfigureRendering(hmd_, nullptr, 0, hmd_->DefaultEyeFov, res.data()))
            throwOvrError("ovrHmd_ConfigureRendering returned false!", hmd_);
    }

    ovrFrameTiming beginFrame(unsigned int frameIndex) override {
        return ovrHmd_BeginFrame(hmd_, frameIndex);
    }

    void endFrame(const ovrPosef renderPose[2], const ovrTexture eyeTexture[2]) override {
        ovrHmd_EndFrame(hmd_, renderPose, eyeTexture);
    }

    ovrSizei getFovTextureSize(ovrEyeType eye) override {
        return getFovTextureSize(eye, hmd_->DefaultEyeFov[eye]);
    }

    ovrSizei getFovTextureSize(ovrEyeType eye, ovrFovPort fov, float pixelsPerDisplayPixel = 1.0f) override {
        return ovrHmd_GetFovTextureSize(hmd_, eye, fov, pixelsPerDisplayPixel);
    }

    float getProperty(const char* propertyName, float defaultValue) const override {
        return ovrHmd_GetFloat(hmd_, propertyName, defaultValue);
    }

    std::pair<std::array<ovrPosef, 2>, ovrTrackingState> getEyePoses(
        unsigned int frameIndex, ovrVector3f hmdToEyeViewOffset[2]) const override {
        std::pair<std::array<ovrPosef, 2>, ovrTrackingState> res;
        ovrHmd_GetEyePoses(hmd_, frameIndex, hmdToEyeViewOffset, res.first.data(), &res.second);
        return res;
    }

    void recenterPose() override {
        ovrHmd_RecenterPose(hmd_);
    }

    void dismissHSWDisplay() override {
        ovrHmd_DismissHSWDisplay(hmd_);
    }

private:
    Hmd(ovrHmd hmd) : hmd_(hmd) {}
    ovrHmd hmd_;
};

class Ovr {
public:
    Ovr();
    ~Ovr();

    Hmd CreateHmd(int index = 0);
};
}
