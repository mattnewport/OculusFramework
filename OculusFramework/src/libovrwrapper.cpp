#include "libovrwrapper.h"

#include "d3dhelper.h"
#include "pipelinestateobject.h"

#define OVR_D3D_VERSION 11
#include "../Src/OVR_CAPI_D3D.h"  // Include SDK-rendered code for the D3D version

#include "Win32_DX11AppUtil.h"

#include <Windows.h>

#pragma warning(push)
#pragma warning(disable : 4100)  // unreferenced formal parameter - temp until these are implemented

using namespace std;

namespace libovrwrapper {

struct DummyHmd::RenderHelper {
    RenderHelper(DummyHmd& dummyHmd_, DirectX11& directX11_)
        : dummyHmd{dummyHmd_}, directX11{directX11_} {
        PipelineStateObjectDesc desc;
        desc.vertexShader = "dummyhmdvs.hlsl";
        desc.pixelShader = "dummyhmdps.hlsl";
        desc.depthStencilState = [] {
            CD3D11_DEPTH_STENCIL_DESC desc{ D3D11_DEFAULT };
            desc.DepthEnable = FALSE;
            return desc;
        }();
        pipelineStateObject = directX11.pipelineStateObjectManager->get(desc);

        [this] {
            CD3D11_SAMPLER_DESC desc{ D3D11_DEFAULT };
            ThrowOnFailure(directX11.Device->CreateSamplerState(&desc, &samplerState));
        }();
    }

    void render(const ovrTexture eyeTexture[2]);

    DummyHmd& dummyHmd;
    DirectX11& directX11;

    ID3D11RenderTargetViewPtr backBufferRTV;
    IDXGISwapChainPtr swapChain;

    PipelineStateObjectManager::ResourceHandle pipelineStateObject;
    ID3D11SamplerStatePtr samplerState;
};

DummyHmd::DummyHmd()
{
}

DummyHmd::~DummyHmd() {}

ovrVector2i DummyHmd::getWindowsPos() const { return {0, 0}; }

ovrSizei DummyHmd::getResolution() const { return {1920, 1080}; }

bool DummyHmd::testCap(ovrHmdCaps cap) const { return false; }

void DummyHmd::setCap(ovrHmdCaps cap) {}

bool DummyHmd::getCap(ovrHmdCaps cap) const { return false; }

void DummyHmd::attachToWindow(void* window) {}

void DummyHmd::configureTracking(unsigned int supportedTrackingCaps,
                                 unsigned int requiredTrackingCaps) {}

std::array<ovrEyeRenderDesc, 2> DummyHmd::configureRendering(const ovrRenderAPIConfig* apiConfig,
                                                             unsigned int distortionCaps) {
    // Grab D3D API objects
    auto d3d11Config = reinterpret_cast<const ovrD3D11Config*>(apiConfig);
    renderHelper->backBufferRTV = d3d11Config->D3D11.pBackBufferRT;
    renderHelper->swapChain = d3d11Config->D3D11.pSwapChain;

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

void DummyHmd::shutdownRendering() {
    renderHelper.reset();
}

ovrFrameTiming DummyHmd::beginFrame(unsigned int frameIndex) { return ovrFrameTiming(); }

void DummyHmd::endFrame(const ovrPosef renderPose[2], const ovrTexture eyeTexture[2]) {
    renderHelper->render(eyeTexture);
}

ovrSizei DummyHmd::getFovTextureSize(ovrEyeType eye) {
    return getFovTextureSize(eye, ovrFovPort{});
}

ovrSizei DummyHmd::getFovTextureSize(ovrEyeType eye, ovrFovPort fov, float pixelsPerDisplayPixel) {
    return {1182, 1461};
}

float DummyHmd::getProperty(const char* propertyName, const float defaultValue) const {
    if (std::string(propertyName) == OVR_KEY_EYE_HEIGHT) return 1.58577204f;
    return defaultValue;
}

std::pair<std::array<ovrPosef, 2>, ovrTrackingState> DummyHmd::getEyePoses(
    unsigned int frameIndex, ovrVector3f hmdToEyeViewOffset[2]) const {
    auto res = std::pair<std::array<ovrPosef, 2>, ovrTrackingState>{};
    res.first[0].Orientation = ovrQuatf{0.0f, 0.0f, 0.0f, 1.0f};
    res.first[0].Position = hmdToEyeViewOffset[0];
    res.first[1].Orientation = ovrQuatf{0.0f, 0.0f, 0.0f, 1.0f};
    res.first[1].Position = hmdToEyeViewOffset[1];
    return res;
}

void DummyHmd::setDirectX11(DirectX11& directX11_) {
    if (!renderHelper) {
        renderHelper = make_unique<RenderHelper>(*this, directX11_);
    }
}

void DummyHmd::RenderHelper::render(const ovrTexture eyeTexture[2]) {
    [this, eyeTexture] {
        directX11.applyState(*directX11.Context, *pipelineStateObject.get());
        ID3D11RenderTargetView* rtvs[] = {backBufferRTV};
        directX11.Context->OMSetRenderTargets(1, rtvs, nullptr);
        ID3D11SamplerState* samplers[] = {samplerState};
        directX11.Context->PSSetSamplers(0, 1, samplers);
        D3D11_VIEWPORT vp;
        vp.TopLeftX =
            static_cast<float>(dummyHmd.eyeRenderDescs[ovrEye_Left].DistortedViewport.Pos.x);
        vp.TopLeftY =
            static_cast<float>(dummyHmd.eyeRenderDescs[ovrEye_Left].DistortedViewport.Pos.y);
        vp.Width =
            static_cast<float>(dummyHmd.eyeRenderDescs[ovrEye_Left].DistortedViewport.Size.w +
                               dummyHmd.eyeRenderDescs[ovrEye_Right].DistortedViewport.Size.w);
        vp.Height =
            static_cast<float>(max(dummyHmd.eyeRenderDescs[ovrEye_Left].DistortedViewport.Size.h,
                                   dummyHmd.eyeRenderDescs[ovrEye_Right].DistortedViewport.Size.h));
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        directX11.Context->RSSetViewports(1, &vp);
        auto d3dEyeTexture = reinterpret_cast<const ovrD3D11Texture*>(eyeTexture);
        ID3D11ShaderResourceView* srvs[] = {d3dEyeTexture[ovrEye_Left].D3D11.pSRView};
        directX11.Context->PSSetShaderResources(0, 1, srvs);
        directX11.Context->Draw(3, 0);
        ID3D11ShaderResourceView* clearSrvs[] = {nullptr};
        directX11.Context->PSSetShaderResources(0, 1, clearSrvs);
    }();
    swapChain->Present(0, 0);
}

IHmd::~IHmd()
{
}

Ovr::Ovr() {
    if (!ovr_Initialize()) throwOvrError("ovr_Initialize() returned false!");
}

Ovr::~Ovr() { ovr_Shutdown(); }

Hmd Ovr::CreateHmd(int index) {
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

void throwOvrError(const char * msg, ovrHmd hmd) {
    const char* errString = ovrHmd_GetLastError(hmd);
#ifdef _DEBUG
    OutputDebugStringA(errString);
#endif
    throw std::runtime_error(std::string{ msg } +errString);
}

}  // namespace libovrwrapper

#pragma warning(pop)
