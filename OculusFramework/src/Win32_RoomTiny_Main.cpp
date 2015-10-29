/************************************************************************************
Filename    :   Win32_RoomTiny_Main.cpp
Content     :   First-person view test application for Oculus Rift
Created     :   October 4, 2012
Authors     :   Tom Heath, Michael Antonov, Andrew Reisse, Volga Aksoy
Copyright   :   Copyright 2012 Oculus, Inc. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*************************************************************************************/

// This app renders a simple room, with right handed coord system :  Y->Up, Z->Back, X->Right
// 'W','A','S','D' and arrow keys to navigate. (Other keys in Win32_RoomTiny_ExamplesFeatures.h)
// 1.  SDK-rendered is the simplest path (this file)
// 2.  APP-rendered involves other functions, in Win32_RoomTiny_AppRendered.h
// 3.  Further options are illustrated in Win32_RoomTiny_ExampleFeatures.h
// 4.  Supporting D3D11 and utility code is in Win32_DX11AppUtil.h

#include "Win32_DX11AppUtil.h"  // Include Non-SDK supporting utilities
#pragma warning(push)
#pragma warning(disable : 4244 4127)
#define OVR_D3D_VERSION 11
#include "OVR_CAPI_D3D.h"  // Include SDK-rendered code for the D3D version
#pragma warning(pop)

#include "pipelinestateobject.h"
#include "scene.h"
#include "libovrwrapper.h"
#include "util.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"

#include <vector.h>
#include <matrix.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iterator>
#include <regex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>

#include <Xinput.h>

using namespace util;
using namespace mathlib;
using namespace libovrwrapper;
using namespace std;

auto parseArgs(const char* args) {
    using ArgMap = unordered_map<string, string>;
    auto argMap = ArgMap{};
    regex re{R"(-((\?)|(\w+))(\s+(\w+))?)"};
    transform(cregex_iterator{args, args + strlen(args), re}, cregex_iterator{},
              inserter(argMap, begin(argMap)), [](const auto& m) {
                  return ArgMap::value_type{{m[1].first, m[1].second}, {m[5].first, m[5].second}};
              });
    if (argMap.find("?") != end(argMap)) {
        // Print command line args
    }
    return argMap;
}

int appClock = 0;

void ExampleFeatures1(const DirectX11& DX11, IHmd& hmd, ovrVector3f* useHmdToEyeViewOffset) {
    // Update the appClock, used by some of the features
    appClock++;

    // Recenter the Rift by pressing 'R'
    if (DX11.Key['R']) hmd.recenterPose();

    // Toggle to monoscopic by holding the 'I' key,
    // to recognise the pitfalls of no stereoscopic viewing, how easy it
    // is to get this wrong, and displaying the method to manually adjust.
    if (DX11.Key['I']) {
        useHmdToEyeViewOffset[0].x = 0;  // This value would normally be half the IPD,
        useHmdToEyeViewOffset[1].x = 0;  //  received from the loaded profile.
    }
}

void ExampleFeatures2(const DirectX11& DX11, int eye, ImageBuffer** pUseBuffer,
                      ovrPosef** pUseEyePose, float** pUseYaw, bool* pClearEyeImage,
                      bool* pUpdateEyeImage, ovrRecti* EyeRenderViewport,
                      const ImageBuffer& EyeRenderTexture) {
    // A debug function that allows the pressing of 'F' to freeze/cease the generation of any new
    // eye
    // buffers, therefore showing the independent operation of timewarp.
    // Recommended for your applications
    if (DX11.Key['F']) *pClearEyeImage = *pUpdateEyeImage = false;

    // This illustrates how the SDK allows the developer to vary the eye buffer resolution
    // in realtime.  Adjust with the '9' key.
    if (DX11.Key['9'])
        EyeRenderViewport->Size.h =
            (int)(EyeRenderTexture.Size.h * (2 + sin(0.1f * appClock)) / 3.0f);

    // Press 'N' to simulate if, instead of rendering frames, exhibit blank frames
    // in order to guarantee frame rate.   Not recommended at all, but useful to see,
    // just in case some might consider it a viable alternative to juddering frames.
    const int BLANK_FREQUENCY = 10;
    if ((DX11.Key['N']) && ((appClock % (BLANK_FREQUENCY * 2)) == (eye * BLANK_FREQUENCY)))
        *pUpdateEyeImage = false;

    (void)(pUseYaw, pUseEyePose, pUseBuffer);
}

struct ToneMapper {
    ToneMapper(DirectX11& directX11, int width_, int height_)
        : width{static_cast<unsigned>(width_)},
          height{static_cast<unsigned>(height_)},
          quadRenderer{directX11, "tonemapps.hlsl"},
          sourceTex{CreateTexture2D(
              directX11.Device.Get(),
              Texture2DDesc{DXGI_FORMAT_R16G16B16A16_FLOAT, width, height}.mipLevels(1),
              "ToneMapper::sourceTex")},
          sourceTexSrv{CreateShaderResourceView(directX11.Device.Get(), sourceTex.Get(),
                                                "ToneMapper::sourceTexSRV")},
          renderTargetTex{CreateTexture2D(
              directX11.Device.Get(),
              Texture2DDesc{DXGI_FORMAT_R8G8B8A8_TYPELESS, width, height}.mipLevels(1).bindFlags(
                  D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET),
              "ToneMapper::renderTargetTex")},
          renderTargetView{
              CreateRenderTargetView(directX11.Device.Get(), renderTargetTex.Get(),
                                     CD3D11_RENDER_TARGET_VIEW_DESC{D3D11_RTV_DIMENSION_TEXTURE2D,
                                                                    DXGI_FORMAT_R8G8B8A8_UNORM},
                                     "ToneMapper::renderTargetView")} {}

    void render(ID3D11ShaderResourceView* avgLogLuminance);

    unsigned width = 0;
    unsigned height = 0;

    QuadRenderer quadRenderer;
    ID3D11Texture2DPtr sourceTex;
    ID3D11ShaderResourceViewPtr sourceTexSrv;
    ID3D11Texture2DPtr renderTargetTex;
    ID3D11RenderTargetViewPtr renderTargetView;
};

void ToneMapper::render(ID3D11ShaderResourceView* avgLogLuminance) {
    quadRenderer.render(*renderTargetView.Get(), {sourceTexSrv.Get(), avgLogLuminance}, 0, 0, width,
                        height);
}

struct LuminanceRangeFinder {
    LuminanceRangeFinder(DirectX11& directX11, int width_, int height_)
        : width{static_cast<unsigned>(width_)},
          height{static_cast<unsigned>(height_)},
          logLumRenderer{directX11, "loglumps.hlsl"},
          logAverageRenderer{directX11, "luminancerangeps.hlsl"},
          luminanceBlender{directX11, "loglumblendps.hlsl"} {
        auto makeLumTex = [&directX11](unsigned texWidth, unsigned texHeight) {
            auto tex = CreateTexture2D(
                directX11.Device.Get(),
                Texture2DDesc{DXGI_FORMAT_R16_FLOAT, texWidth, max(1u, texHeight)}
                    .mipLevels(1)
                    .bindFlags(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET),
                "LuminanceRangeFinder::tex");
            auto srv = CreateShaderResourceView(directX11.Device.Get(), tex.Get(),
                                                "LuminanceRangeFinder::srv");
            auto rtv = CreateRenderTargetView(directX11.Device.Get(), tex.Get(),
                                              "LuminanceRangeFinder::rtv");
            return make_tuple(tex, srv, rtv);
        };
        for (auto texWidth = 1024u, texHeight = 1024u; texWidth > 0u;
             texWidth = texWidth >> 1, texHeight = texHeight >> 1) {
            textureChain.emplace_back(makeLumTex(texWidth, texHeight));
        }
        previousFrame = makeLumTex(1, 1);
    }

    void render(ID3D11ShaderResourceView* sourceSRV);

    unsigned width = 0;
    unsigned height = 0;

    tuple<ID3D11Texture2DPtr, ID3D11ShaderResourceViewPtr, ID3D11RenderTargetViewPtr> previousFrame;
    vector<decltype(previousFrame)> textureChain;
    QuadRenderer logLumRenderer;
    QuadRenderer logAverageRenderer;
    QuadRenderer luminanceBlender;
};

void LuminanceRangeFinder::render(ID3D11ShaderResourceView* sourceSRV) {
    for (auto i = 0u; i < textureChain.size() - 1; ++i) {
        QuadRenderer& qr = i == 0 ? logLumRenderer : logAverageRenderer;
        auto& target = textureChain[i];
        qr.render(*get<2>(target).Get(), {sourceSRV}, 0, 0, 1024 >> i, 1024 >> i);
        sourceSRV = get<1>(target).Get();
    }
    luminanceBlender.render(
        *get<2>(textureChain.back()).Get(),
        {get<1>(textureChain[textureChain.size() - 2]).Get(), get<1>(previousFrame).Get()}, 0, 0, 1,
        1);
    swap(previousFrame, textureChain.back());
}

struct ImGuiHelper {
    int width, height;
    ID3D11ShaderResourceViewPtr srv;
    ID3D11RenderTargetViewPtr rtv;
    QuadRenderer renderer;
    bool active = false;

    ImGuiHelper(DirectX11& DX11, int width_, int height_)
        : width{width_}, height{height_}, renderer{DX11, "imguips.hlsl", true} {
        ImGui_ImplDX11_Init(DX11.Window, DX11.Device.Get(), DX11.Context.Get());
        ImGui::GetStyle().Colors[ImGuiCol_Text] = ImVec4{0.0f, 0.9f, 0.0f, 1.0f};

        // Create a render target for IMGUI
        auto tex =
            CreateTexture2D(DX11.Device.Get(),
                            Texture2DDesc{DXGI_FORMAT_R8G8B8A8_UNORM, to<UINT>(width), to<UINT>(height)}
                                .mipLevels(1)
                                .bindFlags(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET),
                            "ImGuiHelper::tex");
        srv = CreateShaderResourceView(DX11.Device.Get(), tex.Get(), "ImGuiHelper::srv");
        rtv = CreateRenderTargetView(DX11.Device.Get(), tex.Get(), "ImGuiHelper::rtv");
    }

    ~ImGuiHelper() { ImGui_ImplDX11_Shutdown(); }

    void render(DirectX11& DX11, Scene& scene, ID3D11RenderTargetView* toneMapperRtv,
                ovrVector2i rightEyePos) {
        if (active) {
            float clearColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
            DX11.Context->ClearRenderTargetView(rtv.Get(), clearColor);
            OMSetRenderTargets(DX11.Context, {rtv.Get()});
            DX11.setViewport({{0, 0}, {width, height}});
            ImVec2 displaySize{static_cast<float>(width), static_cast<float>(height)};
            ImGui_ImplDX11_NewFrame(displaySize);
            ImGui::GetIO().MouseDrawCursor = true;
            ImGui::SetNextWindowPos(
                ImVec2(static_cast<float>(width / 4), static_cast<float>(height / 4)),
                ImGuiSetCond_FirstUseEver);
            showGui(scene);
            // ImGui::ShowTestWindow();
            ImGui::Render();
            renderer.render(*toneMapperRtv, {srv.Get(), nullptr}, 0, 0, width, height);
            renderer.render(*toneMapperRtv, {srv.Get(), nullptr}, rightEyePos.x, rightEyePos.y,
                            width, height);

            if (!ImGui::IsAnyItemActive() && DX11.keyPressed[VK_ESCAPE]) {
                DX11.keyPressed[VK_ESCAPE] = false;
                active = false;
            }
        } else {
            if (DX11.keyPressed[VK_ESCAPE]) {
                DX11.keyPressed[VK_ESCAPE] = false;
                active = true;
            }
        }
    }

    void showGui(Scene& scene) {
        if (!ImGui::Begin()) {
            // Early out if the window is collapsed, as an optimization.
            ImGui::End();
            return;
        }

        ImGui::PushItemWidth(-140);  // Right align, keep 140 pixels for labels

        scene.showGui();

        ImGui::End();
    }
};

int WINAPI WinMain(_In_ HINSTANCE hinst, _In_opt_ HINSTANCE, _In_ LPSTR args, _In_ int) {
    auto argMap = parseArgs(args);

    unique_ptr<IHmd> hmd;

    if (argMap[string{"oculus"}] == "off") {
        // Use a dummy hmd and don't initialize the OVR SDK
        hmd = make_unique<DummyHmd>();
    } else {
        // Initializes LibOVR, and the Rift
        // Note: this must happen before initializing D3D
        hmd = make_unique<OvrHmd>();
    }

    // Setup Window and D3D
    auto windowRect = ovrRecti{hmd->getWindowsPos(), hmd->getResolution()};
    DirectX11 DX11{hinst, windowRect, hmd->getAdapterLuid()};

    // Bit of a hack: if we're using a DummyHmd, set its DirectX11 pointer
    if (const auto dummyHmd = dynamic_cast<DummyHmd*>(hmd.get())) dummyHmd->setDirectX11(DX11);

    // Make the eye render buffers (caution if actual size < requested due to HW limits).
    const auto idealSizeL = hmd->getFovTextureSize(ovrEyeType(ovrEye_Left));
    const auto idealSizeR = hmd->getFovTextureSize(ovrEyeType(ovrEye_Right));

    // leave a gap between the rendered areas as suggested by Oculus
    const auto eyeBufferpadding = 16; 
    auto eyeBufferSize =
        ovrSizei{idealSizeL.w + idealSizeR.w + eyeBufferpadding, max(idealSizeL.h, idealSizeR.h)};

    // Setup VR swap texture set
    auto eyeResolveTexture = hmd->createSwapTextureSetD3D11(eyeBufferSize, DX11.Device.Get());

    // Setup individual eye render targets
    auto EyeRenderTexture =
        ImageBuffer{"EyeRenderTexture", DX11.Device.Get(), eyeBufferSize};
    auto EyeDepthBuffer =
        DepthBuffer{"EyeDepthBuffer", DX11.Device.Get(), eyeBufferSize};
    ovrRecti EyeRenderViewport[2];  // Useful to remember when varying resolution
    EyeRenderViewport[ovrEye_Left].Pos = ovrVector2i{0, 0};
    EyeRenderViewport[ovrEye_Left].Size = idealSizeL;
    EyeRenderViewport[ovrEye_Right].Pos = ovrVector2i{idealSizeL.w + eyeBufferpadding, 0};
    EyeRenderViewport[ovrEye_Right].Size = idealSizeR;

    // ToneMapper
    ToneMapper toneMapper{DX11, eyeBufferSize.w, eyeBufferSize.h};
    LuminanceRangeFinder luminanceRangeFinder{DX11, eyeBufferSize.w, eyeBufferSize.h};

    // Create a mirror texture to see on the monitor.
    auto mirrorTexture = hmd->createMirrorTextureD3D11(
        DX11.Device.Get(), Texture2DDesc{DXGI_FORMAT_R8G8B8A8_UNORM, to<UINT>(windowRect.Size.w),
                                         to<UINT>(windowRect.Size.h)}
                               .mipLevels(1));

    // Create a render target for IMGUI
    ImGuiHelper imguiHelper{
        DX11, max(EyeRenderViewport[ovrEye_Left].Size.w, EyeRenderViewport[ovrEye_Right].Size.w),
        max(EyeRenderViewport[ovrEye_Left].Size.h, EyeRenderViewport[ovrEye_Right].Size.h)};

    const auto EyeRenderDesc = hmd->getRenderDesc();

    // Create the room model
    Scene roomScene(DX11, DX11.Device.Get(), DX11.Context.Get(), *DX11.pipelineStateObjectManager,
                    DX11.texture2DManager);

    float Yaw(3.141592f);  // Horizontal rotation of the player
    Vec4f pos{0.0f, 1.6f, -5.0f, 1.0f};

    // MAIN LOOP
    // =========
    while (!(DX11.Key['Q'] && DX11.Key[VK_CONTROL])) {
        DX11.HandleMessages();

        ovrVector3f useHmdToEyeViewOffset[2] = {EyeRenderDesc[ovrEye_Left].HmdToEyeViewOffset,
                                                EyeRenderDesc[ovrEye_Right].HmdToEyeViewOffset};

        // Handle key toggles for re-centering, meshes, FOV, etc.
        ExampleFeatures1(DX11, *hmd, useHmdToEyeViewOffset);

        // Reload shaders
        if (DX11.Key['H']) {
            DX11.stateManagers->vertexShaderManager.recreateAll();
            DX11.stateManagers->pixelShaderManager.recreateAll();
        }

        // Keyboard inputs to adjust player orientation
        if (DX11.Key[VK_LEFT]) Yaw += 0.02f;
        if (DX11.Key[VK_RIGHT]) Yaw -= 0.02f;

        // Keyboard inputs to adjust player position
        const auto speed = 1.0f;    // Can adjust the movement speed.
        const auto rotationMat = rotationYMat4f(Yaw);
        if (DX11.Key['W'] || DX11.Key[VK_UP])
            pos += Vec4f{0.0f, 0.0f, -0.05f, 0.0f} * speed * rotationMat;
        if (DX11.Key['S'] || DX11.Key[VK_DOWN])
            pos += Vec4f{0.0f, 0.0f, 0.05f, 0.0f} * speed * rotationMat;
        if (DX11.Key['D']) pos += Vec4f{0.05f, 0.0f, 0.0f, 0.0f} * speed * rotationMat;
        if (DX11.Key['A']) pos += Vec4f{-0.05f, 0.0f, 0.0f, 0.0f} * speed * rotationMat;

        // gamepad inputs
        for (auto i = 0; i < XUSER_MAX_COUNT; ++i) {
            XINPUT_STATE state{};
            if (SUCCEEDED(XInputGetState(i, &state))) {
                auto handleDeadzone = [](float x, float y, float deadzone) {
                    const auto v = Vec2f{x, y};
                    const auto mag = magnitude(v);
                    if (mag > deadzone) {
                        const auto n = v * 1.0f / mag;
                        const auto s = saturate(
                            (mag - deadzone) /
                            (numeric_limits<decltype(XINPUT_GAMEPAD::sThumbLX)>::max() - deadzone));
                        return n * s;
                    }
                    return Vec2f{0.0f};
                };
                const auto& gp = state.Gamepad;
                const auto ls =
                    handleDeadzone(gp.sThumbLX, gp.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                const auto rs =
                    handleDeadzone(gp.sThumbRX, gp.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);

                Yaw += -0.04f * rs.x();
                pos += Vec4f{0.05f * ls.x(), 0.0f, -0.05f * ls.y(), 0.0f} * rotationMat;
            }
        }

        pos.y() = hmd->getProperty(OVR_KEY_EYE_HEIGHT, pos.y());

        // Animate the cube
        roomScene.Models[0]->Pos =
            mathlib::Vec3f(9.0f * sin(0.01f * appClock), 3.0f, 9.0f * cos(0.01f * appClock));

        // Get both eye poses simultaneously, with IPD offset already included.
        const auto sensorSampleTime = hmd->getTimeInSeconds();
        const auto eyePoses = hmd->getEyePoses(0, useHmdToEyeViewOffset);

        DX11.ClearAndSetRenderTarget(EyeRenderTexture.TexRtv.Get(), EyeDepthBuffer.TexDsv.Get());

        // Render the two undistorted eye views into their render buffers.
        for (auto eye : {ovrEye_Left, ovrEye_Right}) {
            DX11.setViewport(EyeRenderViewport[eye]);
            const auto useEyePose = &eyePoses.first[eye];

            // Get view and projection matrices (note near Z to reduce eye strain)
            const auto rollPitchYaw = rotationYMat4f(Yaw);
            const auto finalRollPitchYaw =
                Mat4FromQuat(Quatf{&useEyePose->Orientation.x}) * rollPitchYaw;
            const auto finalUp = basisVector<Vec4f>(Y) * finalRollPitchYaw;
            const auto finalForward = -basisVector<Vec4f>(Z) * finalRollPitchYaw;
            const auto finalEye = pos + Vec4f{Vec3f{&useEyePose->Position.x}, 0.0f} * rollPitchYaw;
            const auto finalAt = finalEye + finalForward;
            const auto view = lookAtRhMat4f(finalEye.xyz(), finalAt.xyz(), finalUp.xyz());
            const auto projTemp =
                ovrMatrix4f_Projection(EyeRenderDesc[eye].Fov, 0.1f, 1000.0f, true);
            const auto proj = transpose(MatrixFromDataPointer<float, 4, 4>(&projTemp.M[0][0]));

            // Render the scene
            roomScene.Render(DX11, finalEye.xyz(), view, proj);
        }

        DX11.Context->ResolveSubresource(toneMapper.sourceTex.Get(), 0, EyeRenderTexture.Tex.Get(),
                                         0, DXGI_FORMAT_R16G16B16A16_FLOAT);
        luminanceRangeFinder.render(toneMapper.sourceTexSrv.Get());
        toneMapper.render(get<1>(luminanceRangeFinder.textureChain.back()).Get());

        // IMGUI update/rendering
        imguiHelper.render(DX11, roomScene, toneMapper.renderTargetView.Get(),
                           EyeRenderViewport[ovrEye_Right].Pos);

        eyeResolveTexture.advanceToNextTexture();
        DX11.Context->CopyResource(eyeResolveTexture.d3dTexture(),
                                   toneMapper.renderTargetTex.Get());

        // Initialize our single full screen Fov layer.
        ovrLayerEyeFov ld;
        ld.Header.Type = ovrLayerType_EyeFov;
        ld.Header.Flags = 0;

        for (auto eye : {ovrEye_Left, ovrEye_Right}) {
            ld.ColorTexture[eye] = eyeResolveTexture.swapTextureSet();
            ld.Viewport[eye] = EyeRenderViewport[eye];
            ld.Fov[eye] = hmd->getDefaultEyeFov(ovrEyeType(eye));
            ld.RenderPose[eye] = eyePoses.first[eye];
            ld.SensorSampleTime = sensorSampleTime;
        }

        ovrLayerHeader* layers = &ld.Header;
        hmd->submitFrame(0, nullptr, &layers, 1);

        // Copy mirror texture to back buffer
        DX11.Context->CopyResource(DX11.BackBuffer.Get(), mirrorTexture.d3dTexture());
        DX11.SwapChain->Present(0, 0);
    }

    return 0;
}
