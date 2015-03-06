#pragma once

#include "OVR_CAPI.h"

#include <Windows.h>

#include <array>
#include <stdexcept>
#include <string>
#include <utility>

namespace libovrwrapper {

inline void throwOvrError(const char* msg, ovrHmd hmd = nullptr) {
    const char* errString = ovrHmd_GetLastError(hmd);
#ifdef _DEBUG
    OutputDebugStringA(errString);
#endif
    throw std::runtime_error(std::string{msg} + errString);
}

class Hmd {
    friend class Ovr;

public:
    Hmd(const Hmd&) = delete;
    Hmd(Hmd&& x) : hmd_(std::move(x.hmd_)) { x.hmd_ = nullptr; }
    ~Hmd() {
        if (hmd_) ovrHmd_Destroy(hmd_);
    }

    ovrVector2i getWindowsPos() const { return hmd_->WindowsPos; }

    ovrSizei getResolution() const { return hmd_->Resolution; }

    bool testCap(ovrHmdCaps cap) const { return static_cast<ovrHmdCaps>(hmd_->HmdCaps & cap) == cap; }
    void setCap(ovrHmdCaps cap) {
        int enabledCaps = ovrHmd_GetEnabledCaps(hmd_);
        enabledCaps |= cap;
        ovrHmd_SetEnabledCaps(hmd_, enabledCaps);
    }
    // OVR SDK API is a bit confusing here - GetEnabledCaps() is different from HmdCaps even though
    // it uses the same enum - it only deals with caps that can be enabled where HmdCaps deals with
    // hardware caps for the specified HMD.
    bool getCap(ovrHmdCaps cap) { return static_cast<ovrHmdCaps>(ovrHmd_GetEnabledCaps(hmd_) & cap) == cap; }

    void attachToWindow(void* window) {
        if (!ovrHmd_AttachToWindow(hmd_, window, nullptr, nullptr))
            throwOvrError("ovrHmd_AttachToWindow() returned false!", hmd_);
    }

    void configureTracking(unsigned int supportedTrackingCaps,
                           unsigned int requiredTrackingCaps = 0) {
        if (!ovrHmd_ConfigureTracking(hmd_, supportedTrackingCaps, requiredTrackingCaps))
            throwOvrError("ovrHmd_ConfigureTracking() returned false!", hmd_);
    }

    std::array<ovrEyeRenderDesc, 2> configureRendering(const ovrRenderAPIConfig* apiConfig,
                                                       unsigned int distortionCaps) {
        std::array<ovrEyeRenderDesc, 2> res;
        if (!ovrHmd_ConfigureRendering(hmd_, apiConfig, distortionCaps, hmd_->DefaultEyeFov,
                                       res.data()))
            throwOvrError("ovrHmd_ConfigureRendering returned false!", hmd_);
        return res;
    }

    void shutdownRendering() {
        std::array<ovrEyeRenderDesc, 2> res;
        if (!ovrHmd_ConfigureRendering(hmd_, nullptr, 0, hmd_->DefaultEyeFov, res.data()))
            throwOvrError("ovrHmd_ConfigureRendering returned false!", hmd_);
    }

    ovrFrameTiming beginFrame(unsigned int frameIndex) {
        return ovrHmd_BeginFrame(hmd_, frameIndex);
    }

    void endFrame(const ovrPosef renderPose[2], const ovrTexture eyeTexture[2]) {
        ovrHmd_EndFrame(hmd_, renderPose, eyeTexture);
    }

    ovrSizei getFovTextureSize(ovrEyeType eye) {
        return getFovTextureSize(eye, hmd_->DefaultEyeFov[eye]);
    }

    ovrSizei getFovTextureSize(ovrEyeType eye, ovrFovPort fov, float pixelsPerDisplayPixel = 1.0f) {
        return ovrHmd_GetFovTextureSize(hmd_, eye, fov, pixelsPerDisplayPixel);
    }

    template <typename T>
    T getProperty(const char* propertyName, const T& defaultValue) const;

    template <>
    float getProperty(const char* propertyName, const float& defaultValue) const {
        return ovrHmd_GetFloat(hmd_, propertyName, defaultValue);
    }

    std::pair<std::array<ovrPosef, 2>, ovrTrackingState> getEyePoses(
        unsigned int frameIndex, ovrVector3f hmdToEyeViewOffset[2]) const {
        std::pair<std::array<ovrPosef, 2>, ovrTrackingState> res;
        ovrHmd_GetEyePoses(hmd_, frameIndex, hmdToEyeViewOffset, res.first.data(), &res.second);
        return res;
    }

    // Temporary
    ovrHmd getHmd() { return hmd_; }

private:
    Hmd(ovrHmd hmd) : hmd_(hmd) {}
    ovrHmd hmd_;
};

class Ovr {
public:
    Ovr() {
        if (!ovr_Initialize()) throwOvrError("ovr_Initialize() returned false!");
    }
    ~Ovr() { ovr_Shutdown(); }

    Hmd CreateHmd(int index = 0) {
        ovrHmd hmd = ovrHmd_Create(index);
        if (!hmd) {
            MessageBoxA(NULL, "Oculus Rift not detected.\nAttempting to create debug HMD.", "", MB_OK);

            // If we didn't detect an Hmd, create a simulated one for debugging.
            hmd = ovrHmd_CreateDebug(ovrHmd_DK2);
            if (!hmd) throwOvrError("Failed to create HMD!");
        }

        if (hmd->ProductName[0] == '\0')
            MessageBoxA(NULL, "Rift detected, display not enabled.", "", MB_OK);
        return Hmd{hmd};
    }
};
}
