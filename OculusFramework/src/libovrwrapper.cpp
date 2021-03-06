#include "libovrwrapper.h"

#include "d3dhelper.h"
#include "pipelinestateobject.h"

#pragma warning(push)
#pragma warning(disable : 4244 4127)
#define OVR_D3D_VERSION 11
#include "OVR_CAPI_D3D.h"  // Include SDK-rendered code for the D3D version
#pragma warning(pop)

#include "Win32_DX11AppUtil.h"

#include <Windows.h>

using namespace std;

namespace libovrwrapper {

DummyHmd::DummyHmd() {}

DummyHmd::~DummyHmd() {}

double DummyHmd::getTimeInSeconds() const
{
    return ovr_GetTimeInSeconds();
}

ovrVector2i DummyHmd::getWindowsPos() const { return {0, 0}; }

ovrSizei DummyHmd::getResolution() const { return {1920, 1080}; }

ovrFovPort DummyHmd::getDefaultEyeFov(ovrEyeType /*eye*/) const { return{}; }

bool DummyHmd::testCap(ovrHmdCaps /*cap*/) const { return false; }

void DummyHmd::setCap(ovrHmdCaps /*cap*/) {}

void DummyHmd::configureTracking(unsigned int /*supportedTrackingCaps*/,
                                 unsigned int /*requiredTrackingCaps*/) {}

std::array<ovrEyeRenderDesc, 2> DummyHmd::getRenderDesc() {
    // Fake out default DK2 values
    // Left eye
    eyeRenderDescs[0].Eye = ovrEye_Left;
    eyeRenderDescs[0].Fov.UpTan = 1.32928634f;
    eyeRenderDescs[0].Fov.DownTan = 1.32928634f;
    eyeRenderDescs[0].Fov.LeftTan = 1.05865765f;
    eyeRenderDescs[0].Fov.RightTan = 1.09236801f;
    eyeRenderDescs[0].DistortedViewport.Pos = ovrVector2i{0, 0};
    eyeRenderDescs[0].DistortedViewport.Size = ovrSizei{960, 1080};
    eyeRenderDescs[0].PixelsPerTanAngleAtCenter = ovrVector2f{549.618286f, 549.618286f};
    eyeRenderDescs[0].HmdToEyeViewOffset = ovrVector3f{0.0320000015f, 0.0f, 0.0f};
    // Right eye
    eyeRenderDescs[1].Eye = ovrEye_Right;
    eyeRenderDescs[1].Fov.UpTan = 1.32928634f;
    eyeRenderDescs[1].Fov.DownTan = 1.32928634f;
    eyeRenderDescs[1].Fov.LeftTan = 1.09236801f;
    eyeRenderDescs[1].Fov.RightTan = 1.05865765f;
    eyeRenderDescs[1].DistortedViewport.Pos = ovrVector2i{960, 0};
    eyeRenderDescs[1].DistortedViewport.Size = ovrSizei{960, 1080};
    eyeRenderDescs[1].PixelsPerTanAngleAtCenter = ovrVector2f{549.618286f, 549.618286f};
    eyeRenderDescs[1].HmdToEyeViewOffset = ovrVector3f{-0.0320000015f, 0.0f, 0.0f};

    return eyeRenderDescs;
}

bool DummyHmd::submitFrame(unsigned int /*frameIndex*/, const ovrViewScaleDesc* /*viewScaleDesc*/,
                           ovrLayerHeader const* const* layerPtrList, unsigned int /*layerCount*/) {
    auto layer = reinterpret_cast<const ovrLayerEyeFov*>(layerPtrList[0]);
    auto textureSet = layer->ColorTexture[0];
    auto d3dEyeTexture = reinterpret_cast<const ovrD3D11Texture*>(&textureSet->Textures[0]);
    const auto& leftVp = eyeRenderDescs[ovrEye_Left].DistortedViewport;
    const auto& rightVp = eyeRenderDescs[ovrEye_Right].DistortedViewport;
    auto quadRenderer = QuadRenderer{*directX11, "dummyhmdps.hlsl"};
    quadRenderer.render(*mirrorTextureRtv, {d3dEyeTexture[ovrEye_Left].D3D11.pSRView}, leftVp.Pos.x,
                        leftVp.Pos.y, leftVp.Size.w + rightVp.Size.w,
                        max(leftVp.Size.h, rightVp.Size.h));
    return false;
}

ovrSizei DummyHmd::getFovTextureSize(ovrEyeType eye) {
    return getFovTextureSize(eye, ovrFovPort{});
}

ovrSizei DummyHmd::getFovTextureSize(ovrEyeType /*eye*/, ovrFovPort /*fov*/,
                                     float /*pixelsPerDisplayPixel*/) {
    return {1182, 1464};
}

float DummyHmd::getProperty(const char* propertyName, const float defaultValue) const {
    if (std::string(propertyName) == OVR_KEY_EYE_HEIGHT) return 1.58577204f;
    return defaultValue;
}

std::pair<std::array<ovrPosef, 2>, ovrTrackingState> DummyHmd::getEyePoses(
    unsigned int /*frameIndex*/, ovrVector3f hmdToEyeViewOffset[2]) const {
    auto res = std::pair<std::array<ovrPosef, 2>, ovrTrackingState>{};
    res.first[0].Orientation = ovrQuatf{0.0f, 0.0f, 0.0f, 1.0f};
    res.first[0].Position = hmdToEyeViewOffset[0];
    res.first[1].Orientation = ovrQuatf{0.0f, 0.0f, 0.0f, 1.0f};
    res.first[1].Position = hmdToEyeViewOffset[1];
    return res;
}

SwapTextureSet DummyHmd::createSwapTextureSetD3D11(ovrSizei size, ID3D11Device* device) {
    return {nullptr, size, device};
}

MirrorTexture DummyHmd::createMirrorTextureD3D11(ID3D11Device* device,
                                                 const D3D11_TEXTURE2D_DESC& desc) {
    auto res = MirrorTexture{nullptr, device, desc};
    assert(!mirrorTextureRtv && "DummyHmd doesn't support multiple mirror textures.");
    mirrorTextureRtv = res.d3dRenderTargetView();
    return res;
}

IHmd::~IHmd() {}

void throwOvrError(const char* msg, ovrHmd /*hmd*/) {
    ovrErrorInfo errorInfo{};
    ovr_GetLastErrorInfo(&errorInfo);
#ifdef _DEBUG
    OutputDebugStringA(errorInfo.ErrorString);
#endif
    throw std::runtime_error(std::string{msg} + errorInfo.ErrorString);
}

OvrHmd::OvrHmd() {
    if (!OVR_SUCCESS(ovr_Initialize(nullptr))) throwOvrError("ovr_Initialize() returned false!");
    if (!OVR_SUCCESS(ovr_Create(&hmd_, &luid_))) {
        MessageBoxA(NULL, "Oculus Rift not detected.\n", "", MB_OK);
        throwOvrError("Failed to create HMD!");
    }
}

OvrHmd::~OvrHmd() {
    if (hmd_) ovr_Destroy(hmd_);
}

}  // namespace libovrwrapper
