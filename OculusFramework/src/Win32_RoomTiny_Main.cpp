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

void ExampleFeatures1(const DirectX11& DX11, IHmd& hmd, const float* pSpeed,
                      int* pTimesToRenderScene, ovrVector3f* useHmdToEyeViewOffset) {
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

    (void)(pSpeed);
    (void)(pTimesToRenderScene);
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
    ToneMapper(DirectX11& directX11_, int width_, int height_)
        : directX11{directX11_},
          width{static_cast<unsigned>(width_)},
          height{static_cast<unsigned>(height_)},
          quadRenderer{directX11_, "tonemapps.hlsl"},
          sourceTex{CreateTexture2D(
              directX11.Device.Get(),
              Texture2DDesc{DXGI_FORMAT_R16G16B16A16_FLOAT, width, height}.mipLevels(1))},
          renderTargetTex{CreateTexture2D(
              directX11.Device.Get(),
              Texture2DDesc{DXGI_FORMAT_R8G8B8A8_TYPELESS, width, height}.mipLevels(1).bindFlags(
                  D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET))} {
        SetDebugObjectName(sourceTex.Get(), "ToneMapper::sourceTex");
        ThrowOnFailure(
            directX11.Device->CreateShaderResourceView(sourceTex.Get(), nullptr, &sourceTexSRV));
        SetDebugObjectName(sourceTexSRV.Get(), "ToneMapper::sourceTexSRV");

        SetDebugObjectName(renderTargetTex.Get(), "ToneMapper::renderTargetTex");
        renderTargetView =
            CreateRenderTargetView(directX11.Device.Get(), renderTargetTex.Get(),
                                   CD3D11_RENDER_TARGET_VIEW_DESC{D3D11_RTV_DIMENSION_TEXTURE2D,
                                                                  DXGI_FORMAT_R8G8B8A8_UNORM});
        SetDebugObjectName(renderTargetView.Get(), "ToneMapper::renderTargetView");
    }

    void render(ID3D11ShaderResourceView* avgLogLuminance);

    DirectX11& directX11;
    unsigned width = 0;
    unsigned height = 0;

    QuadRenderer quadRenderer;
    ID3D11Texture2DPtr sourceTex;
    ID3D11ShaderResourceViewPtr sourceTexSRV;
    ID3D11Texture2DPtr renderTargetTex;
    ID3D11RenderTargetViewPtr renderTargetView;
};

void ToneMapper::render(ID3D11ShaderResourceView* avgLogLuminance) {
    quadRenderer.render(*renderTargetView.Get(), {sourceTexSRV.Get(), avgLogLuminance}, 0, 0, width,
                        height);
}

struct LuminanceRangeFinder {
    LuminanceRangeFinder(DirectX11& directX11_, int width_, int height_)
        : directX11{directX11_},
          width{static_cast<unsigned>(width_)},
          height{static_cast<unsigned>(height_)},
          logLumRenderer{directX11_, "loglumps.hlsl"},
          logAverageRenderer{directX11_, "luminancerangeps.hlsl"},
          luminanceBlender{directX11_, "loglumblendps.hlsl"} {
        [this] {
            auto makeLumTex = [this](unsigned texWidth, unsigned texHeight) {
                auto tex = CreateTexture2D(
                    directX11.Device.Get(),
                    Texture2DDesc{DXGI_FORMAT_R16_FLOAT, texWidth, max(1u, texHeight)}
                        .mipLevels(1)
                        .bindFlags(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET));
                SetDebugObjectName(tex.Get(), "LuminanceRangeFinder::tex");
                ID3D11ShaderResourceViewPtr srv;
                ThrowOnFailure(directX11.Device->CreateShaderResourceView(tex.Get(), nullptr, &srv));
                SetDebugObjectName(srv.Get(), "LuminanceRangeFinder::srv");
                auto rtv = CreateRenderTargetView(directX11.Device.Get(), tex.Get());
                SetDebugObjectName(rtv.Get(), "LuminanceRangeFinder::rtv");
                return make_tuple(tex, srv, rtv);
            };
            for (auto texWidth = 1024u, texHeight = 1024u; texWidth > 0u;
                 texWidth = texWidth >> 1, texHeight = texHeight >> 1) {
                textureChain.emplace_back(makeLumTex(texWidth, texHeight));
            }
            previousFrame = makeLumTex(1, 1);
        }();
    }

    void render(ID3D11ShaderResourceView* sourceSRV);

    DirectX11& directX11;
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

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR args, int) {
    auto argMap = parseArgs(args);

    unique_ptr<IHmd> hmd;

    if (argMap[string{"oculus"}] == "off") {
        // Use a dummy hmd and don't initialize the OVR SDK
        hmd = make_unique<DummyHmd>();
    } else {
        // Initializes LibOVR, and the Rift
        // Note: this must happen before initializing D3D
        hmd = make_unique<Hmd>();
    }

    DirectX11 DX11{};

    // Setup Window and Graphics - use window frame if relying on Oculus driver
    ovrRecti windowRect{ hmd->getWindowsPos(), hmd->getResolution() };
    if (!DX11.InitWindowAndDevice(hinst, windowRect, hmd->getAdapterLuid())) return 0;

    [&hmd, &DX11] {
        const auto dummyHmd = dynamic_cast<DummyHmd*>(hmd.get());
        if (dummyHmd) {
            dummyHmd->setDirectX11(DX11);
        }
    }();

    // Start the sensor which informs of the Rift's pose and motion
    hmd->configureTracking(ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection |
                           ovrTrackingCap_Position);

    // Make the eye render buffers (caution if actual size < requested due to HW limits).
    const auto idealSizeL = hmd->getFovTextureSize(ovrEyeType(ovrEye_Left));
    const auto idealSizeR = hmd->getFovTextureSize(ovrEyeType(ovrEye_Right));

    // leave a gap between the rendered areas as suggested by Oculus
    const auto eyeBufferpadding = 16; 
    auto eyeBufferSize =
        ovrSizei{idealSizeL.w + idealSizeR.w + eyeBufferpadding, max(idealSizeL.h, idealSizeR.h)};

    // Setup VR swap texture set
    OculusTexture* eyeResolveTexture =
        hmd->createSwapTextureSetD3D11(eyeBufferSize, DX11.Device.Get());

    // Setup individual eye render targets
    auto EyeRenderTexture =
        ImageBuffer{"EyeRenderTexture", DX11.Device.Get(), true, false, eyeBufferSize, 1, true};
    auto EyeDepthBuffer =
        ImageBuffer{"EyeDepthBuffer", DX11.Device.Get(), true, true, eyeBufferSize, 1, true};
    ovrRecti EyeRenderViewport[2];  // Useful to remember when varying resolution
    EyeRenderViewport[ovrEye_Left].Pos = ovrVector2i{0, 0};
    EyeRenderViewport[ovrEye_Left].Size = idealSizeL;
    EyeRenderViewport[ovrEye_Right].Pos = ovrVector2i{idealSizeL.w + eyeBufferpadding, 0};
    EyeRenderViewport[ovrEye_Right].Size = idealSizeR;

    // ToneMapper
    ToneMapper toneMapper{DX11, eyeBufferSize.w, eyeBufferSize.h};
    LuminanceRangeFinder luminanceRangeFinder{DX11, eyeBufferSize.w, eyeBufferSize.h};

    // Create a mirror texture to see on the monitor.
    ovrTexture* mirrorTexture = hmd->createMirrorTextureD3D11(
        DX11.Device.Get(),
        Texture2DDesc{DXGI_FORMAT_R8G8B8A8_UNORM, UINT(windowRect.Size.w), UINT(windowRect.Size.h)}
            .mipLevels(1));

    ImGui_ImplDX11_Init(DX11.Window, DX11.Device.Get(), DX11.Context.Get());
    ImGui::GetStyle().Colors[ImGuiCol_Text] = ImVec4{0.0f, 0.9f, 0.0f, 1.0f};

    // Create a render target for IMGUI
    ID3D11Texture2DPtr imguiRenderTargetTex;
    ID3D11ShaderResourceViewPtr imguiRenderTargetSRV;
    ID3D11RenderTargetViewPtr imguiRTV;
    const auto imguiRTVWidth =
        max(EyeRenderViewport[ovrEye_Left].Size.w, EyeRenderViewport[ovrEye_Right].Size.w);
    const auto imguiRTVHeight =
        max(EyeRenderViewport[ovrEye_Left].Size.h, EyeRenderViewport[ovrEye_Right].Size.h);
    [&DX11, &EyeRenderViewport, &imguiRenderTargetTex, &imguiRenderTargetSRV, &imguiRTV,
     imguiRTVWidth, imguiRTVHeight] {
        imguiRenderTargetTex = CreateTexture2D(
            DX11.Device.Get(),
            Texture2DDesc{DXGI_FORMAT_R8G8B8A8_UNORM, UINT(imguiRTVWidth), UINT(imguiRTVHeight)}
                .mipLevels(1)
                .bindFlags(D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET));
        ThrowOnFailure(DX11.Device->CreateShaderResourceView(imguiRenderTargetTex.Get(), nullptr,
                                                             &imguiRenderTargetSRV));
        imguiRTV = CreateRenderTargetView(DX11.Device.Get(), imguiRenderTargetTex.Get());
    }();
    QuadRenderer imguiQuadRenderer{DX11, "imguips.hlsl", true};

    const auto EyeRenderDesc = hmd->getRenderDesc();

    {
        // Create the room model
        Scene roomScene(DX11.Device.Get(), DX11.Context.Get(), *DX11.pipelineStateObjectManager,
                        DX11.texture2DManager);

        float Yaw(3.141592f);  // Horizontal rotation of the player
        Vec4f pos{0.0f, 1.6f, -5.0f, 1.0f};

        // MAIN LOOP
        // =========
        while (!(DX11.Key['Q'] && DX11.Key[VK_CONTROL])) {
            DX11.HandleMessages();

            const float speed = 1.0f;    // Can adjust the movement speed.
            int timesToRenderScene = 1;  // Can adjust the render burden on the app.
            ovrVector3f useHmdToEyeViewOffset[2] = {EyeRenderDesc[0].HmdToEyeViewOffset,
                                                    EyeRenderDesc[1].HmdToEyeViewOffset};

            // Handle key toggles for re-centering, meshes, FOV, etc.
            ExampleFeatures1(DX11, *hmd, &speed, &timesToRenderScene, useHmdToEyeViewOffset);

            // Reload shaders
            if (DX11.Key['H']) {
                DX11.stateManagers->vertexShaderManager.recreateAll();
                DX11.stateManagers->pixelShaderManager.recreateAll();
            }

            // Keyboard inputs to adjust player orientation
            if (DX11.Key[VK_LEFT]) Yaw += 0.02f;
            if (DX11.Key[VK_RIGHT]) Yaw -= 0.02f;

            // Keyboard inputs to adjust player position
            const auto rotationMat = Mat4fRotationY(Yaw);
            if (DX11.Key['W'] || DX11.Key[VK_UP])
                pos += Vec4f{0.0f, 0.0f, -speed * 0.05f, 0.0f} * rotationMat;
            if (DX11.Key['S'] || DX11.Key[VK_DOWN])
                pos += Vec4f{0.0f, 0.0f, +speed * 0.05f, 0.0f} * rotationMat;
            if (DX11.Key['D']) pos += Vec4f{+speed * 0.05f, 0.0f, 0.0f, 0.0f} * rotationMat;
            if (DX11.Key['A']) pos += Vec4f{-speed * 0.05f, 0.0f, 0.0f, 0.0f} * rotationMat;

            // gamepad inputs
            for (auto i = 0; i < XUSER_MAX_COUNT; ++i) {
                XINPUT_STATE state{};
                auto res = XInputGetState(i, &state);
                if (SUCCEEDED(res)) {
                    const auto& gp = state.Gamepad;
                    auto handleDeadzone = [](float x, float y, float deadzone) {
                        const auto v = Vec2f{x, y};
                        const auto mag = magnitude(v);
                        if (mag > deadzone) {
                            const auto n = v * 1.0f / mag;
                            const auto s = saturate(
                                (magnitude(v) - deadzone) /
                                (numeric_limits<decltype(XINPUT_GAMEPAD::sThumbLX)>::max() -
                                 deadzone));
                            return n * s;
                        }
                        return Vec2f{0.0f, 0.0f};
                    };
                    const auto ls = handleDeadzone(gp.sThumbLX, gp.sThumbLY,
                                                   XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                    const auto rs = handleDeadzone(gp.sThumbRX, gp.sThumbRY,
                                                   XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);

                    Yaw += -0.04f * rs.x();
                    pos += Vec4f{0.05f * ls.x(), 0.0f, 0.0f, 0.0f} * rotationMat;
                    pos += Vec4f{0.0f, 0.0f, -0.05f * ls.y(), 0.0f} * rotationMat;
                }
            }

            pos.y() = hmd->getProperty(OVR_KEY_EYE_HEIGHT, pos.y());

            // Animate the cube
            roomScene.Models[0]->Pos =
                mathlib::Vec3f(9.0f * sin(0.01f * appClock), 3.0f, 9.0f * cos(0.01f * appClock)) *
                speed;

            // Get both eye poses simultaneously, with IPD offset already included.
            auto tempEyePoses = hmd->getEyePoses(0, useHmdToEyeViewOffset);

            ovrPosef EyeRenderPose[2];  // Useful to remember where the rendered eye originated
            float YawAtRender[2];       // Useful to remember where the rendered eye originated

            DX11.ClearAndSetRenderTarget(EyeRenderTexture.TexRtv.Get(), EyeDepthBuffer.TexDsv.Get());

            // Render the two undistorted eye views into their render buffers.
            for (int eye = 0; eye < 2; eye++) {
                DX11.setViewport(EyeRenderViewport[eye]);
                ovrPosef* useEyePose = &EyeRenderPose[eye];
                float* useYaw = &YawAtRender[eye];

                // Write in values actually used (becomes significant in Example features)
                *useEyePose = tempEyePoses.first[eye];
                *useYaw = Yaw;

                // Get view and projection matrices (note near Z to reduce eye strain)
                auto rollPitchYaw = Mat4fRotationY(Yaw);
                mathlib::Quatf ori;
                memcpy(&ori, &useEyePose->Orientation, sizeof(ori));
                auto fromOri = Mat4FromQuat(ori);
                Mat4f finalRollPitchYaw = fromOri * rollPitchYaw;
                Vec4f finalUp = Vec4f{0.0f, 1.0f, 0.0f, 0.0f} * finalRollPitchYaw;
                Vec4f finalForward = Vec4f{0.0f, 0.0f, -1.0f, 0.0f} * finalRollPitchYaw;
                Vec4f shiftedEyePos = pos +
                                      Vec4f{useEyePose->Position.x, useEyePose->Position.y,
                                            useEyePose->Position.z, 0.0f} *
                                          rollPitchYaw;

                Mat4f view = Mat4fLookAtRH(shiftedEyePos, shiftedEyePos + finalForward, finalUp);
                ovrMatrix4f projTemp =
                    ovrMatrix4f_Projection(EyeRenderDesc[eye].Fov, 0.1f, 1000.0f, true);
                Mat4f projT;
                memcpy(&projT, &projTemp, sizeof(projT));
                Mat4f proj{projT.column(0), projT.column(1), projT.column(2), projT.column(3)};

                // Render the scene
                for (int t = 0; t < timesToRenderScene; t++)
                    roomScene.Render(DX11,
                                     Vec3f{shiftedEyePos.x(), shiftedEyePos.y(), shiftedEyePos.z()},
                                     view, proj);
            }

            DX11.Context->ResolveSubresource(toneMapper.sourceTex.Get(), 0,
                                             EyeRenderTexture.Tex.Get(), 0,
                                             DXGI_FORMAT_R16G16B16A16_FLOAT);
            luminanceRangeFinder.render(toneMapper.sourceTexSRV.Get());
            toneMapper.render(get<1>(luminanceRangeFinder.textureChain.back()).Get());

            // IMGUI update/rendering
            if (DX11.imguiActive) {
                float clearColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
                DX11.Context->ClearRenderTargetView(imguiRTV.Get(), clearColor);
                OMSetRenderTargets(DX11.Context, {imguiRTV.Get()});
                DX11.setViewport(EyeRenderViewport[ovrEye_Left]);
                ImVec2 displaySize{static_cast<float>(imguiRTVWidth),
                    static_cast<float>(imguiRTVHeight)};
                ImGui_ImplDX11_NewFrame(displaySize);
                ImGui::GetIO().MouseDrawCursor = true;
                ImGui::SetNextWindowPos(ImVec2(static_cast<float>(imguiRTVWidth / 4),
                    static_cast<float>(imguiRTVHeight / 4)),
                    ImGuiSetCond_FirstUseEver);
                showGui(roomScene);
                //ImGui::ShowTestWindow();
                ImGui::Render();
                PSSetSamplers(DX11.Context, 0, {roomScene.linearSampler.Get(),
                                                roomScene.standardTextureSampler.Get()});
                imguiQuadRenderer.render(*toneMapper.renderTargetView.Get(),
                                         {imguiRenderTargetSRV.Get(), nullptr}, 0, 0, imguiRTVWidth,
                                         imguiRTVHeight);
                imguiQuadRenderer.render(
                    *toneMapper.renderTargetView.Get(), {imguiRenderTargetSRV.Get(), nullptr},
                    EyeRenderViewport[ovrEye_Right].Pos.x, EyeRenderViewport[ovrEye_Right].Pos.y,
                    imguiRTVWidth, imguiRTVHeight);

                if (!ImGui::IsAnyItemActive() && DX11.keyPressed[VK_ESCAPE]) {
                    DX11.keyPressed[VK_ESCAPE] = false;
                    DX11.imguiActive = false;
                }
            } else {
                if (DX11.keyPressed[VK_ESCAPE]) {
                    DX11.keyPressed[VK_ESCAPE] = false;
                    DX11.imguiActive = true;
                }
            }

            eyeResolveTexture->AdvanceToNextTexture();
            DX11.Context->CopyResource(
                reinterpret_cast<ovrD3D11Texture&>(
                    eyeResolveTexture->TextureSet
                        ->Textures[eyeResolveTexture->TextureSet->CurrentIndex])
                    .D3D11.pTexture,
                toneMapper.renderTargetTex.Get());

            // Initialize our single full screen Fov layer.
            ovrLayerEyeFov ld;
            ld.Header.Type = ovrLayerType_EyeFov;
            ld.Header.Flags = 0;

            for (int eye = 0; eye < 2; eye++) {
                ld.ColorTexture[eye] = eyeResolveTexture->TextureSet;
                ld.Viewport[eye] = EyeRenderViewport[eye];
                ld.Fov[eye] = hmd->getDefaultEyeFov(ovrEyeType(eye));
                ld.RenderPose[eye] = EyeRenderPose[eye];
            }

            ovrLayerHeader* layers = &ld.Header;
            hmd->submitFrame(0, nullptr, &layers, 1);

            // Copy mirror texture to back buffer
            ovrD3D11Texture* tex = (ovrD3D11Texture*)mirrorTexture;
            DX11.Context->CopyResource(DX11.BackBuffer.Get(), tex->D3D11.pTexture);
            DX11.SwapChain->Present(0, 0);
        }
    }

    // Release and close down
    ImGui_ImplDX11_Shutdown();
    hmd->destroyMirrorTextureD3D11(mirrorTexture);
    hmd->destroySwapTextureSetD3D11(eyeResolveTexture);
    hmd.reset();

    DX11.ReleaseWindow(hinst);

    return 0;
}
