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
#include "OVR.h"  // Include the OculusVR SDK
#define OVR_D3D_VERSION 11
#include "OVR_CAPI_D3D.h"  // Include SDK-rendered code for the D3D version
#pragma warning(pop)

#include "pipelinestateobject.h"
#include "scene.h"
#include "libovrwrapper.h"

#include <vector.h>
#include <matrix.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <regex>
#include <string>
#include <unordered_map>

#include <Xinput.h>

using namespace mathlib;

using namespace libovrwrapper;

using namespace std;

using namespace OVR;

unordered_map<string, string> parseArgs(const char* args) {
    unordered_map<string, string> argMap;
    regex re{R"(-((\?)|(\w+))(\s+(\w+))?)"};
    for_each(cregex_iterator{args, args + strlen(args), re}, cregex_iterator{},
             [&](const cmatch m) {
                 argMap[string{m[1].first, m[1].second}] = string{m[5].first, m[5].second};
             });
    if (argMap.find(string{"?"}) != end(argMap)) {
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

    OVR_UNUSED(pSpeed);
    OVR_UNUSED(pTimesToRenderScene);
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

    OVR_UNUSED3(pUseYaw, pUseEyePose, pUseBuffer);
}

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR args, int) {
    auto argMap = parseArgs(args);

    unique_ptr<Ovr> ovr;
    unique_ptr<IHmd> hmd;

    if (argMap[string{"oculus"}] == "off") {
        // Use a dummy hmd and don't initialize the OVR SDK
        hmd = make_unique<DummyHmd>();
    } else {
        // Initializes LibOVR, and the Rift
        // Note: this must happen before initializing D3D
        ovr = make_unique<Ovr>();
        hmd = make_unique<Hmd>(ovr->CreateHmd());
    }

    DirectX11 DX11{};

    // Setup Window and Graphics - use window frame if relying on Oculus driver
    Recti windowRect = Recti(hmd->getWindowsPos(), hmd->getResolution());
    if (!DX11.InitWindowAndDevice(hinst, windowRect)) return 0;

    [&hmd, &DX11] {
        const auto dummyHmd = dynamic_cast<DummyHmd*>(hmd.get());
        if (dummyHmd) {
            dummyHmd->setDirectX11(DX11);
        }
    }();

    hmd->setCap(ovrHmdCap_LowPersistence);
    hmd->setCap(ovrHmdCap_DynamicPrediction);

    // Start the sensor which informs of the Rift's pose and motion
    hmd->configureTracking(ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection |
                           ovrTrackingCap_Position);

    // Make the eye render buffers (caution if actual size < requested due to HW limits).
    const auto idealSizeL = hmd->getFovTextureSize(ovrEyeType(ovrEye_Left));
    const auto idealSizeR = hmd->getFovTextureSize(ovrEyeType(ovrEye_Right));
    const auto eyeBufferpadding =
        16;  // leave a gap between the rendered areas as suggested by Oculus
    auto eyeBufferSize =
        OVR::Sizei{idealSizeL.w + idealSizeR.w + eyeBufferpadding, max(idealSizeL.h, idealSizeR.h)};
    auto EyeRenderTexture =
        ImageBuffer{"EyeRenderTexture", DX11.Device, true, false, eyeBufferSize, 1, true};
    auto EyeDepthBuffer =
        ImageBuffer{"EyeDepthBuffer", DX11.Device, true, true, eyeBufferSize, 1, true};
    ovrRecti EyeRenderViewport[2];  // Useful to remember when varying resolution
    EyeRenderViewport[ovrEye_Left].Pos = Vector2i{0, 0};
    EyeRenderViewport[ovrEye_Left].Size = idealSizeL;
    EyeRenderViewport[ovrEye_Right].Pos = Vector2i{idealSizeL.w + eyeBufferpadding, 0};
    EyeRenderViewport[ovrEye_Right].Size = idealSizeR;

    // Setup VR components
    OculusTexture* eyeResolveTexture = hmd->createSwapTextureSetD3D11(eyeBufferSize, DX11.Device);

    // Create a mirror to see on the monitor.
    D3D11_TEXTURE2D_DESC td = {};
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    td.Width = windowRect.w;
    td.Height = windowRect.h;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.SampleDesc.Count = 1;
    td.MipLevels = 1;
    ovrTexture* mirrorTexture = hmd->createMirrorTextureD3D11(DX11.Device, td);

    const auto EyeRenderDesc = hmd->getRenderDesc();

    {
        // Create the room model
        Scene roomScene(DX11.Device, DX11.Context, *DX11.pipelineStateObjectManager,
                        DX11.texture2DManager);

        float Yaw(3.141592f);  // Horizontal rotation of the player
        Vec4f pos{0.0f, 1.6f, -5.0f, 1.0f};

        // MAIN LOOP
        // =========
        while (!(DX11.Key['Q'] && DX11.Key[VK_CONTROL]) && !DX11.Key[VK_ESCAPE]) {
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
                Vector3f(9 * sin(0.01f * appClock), 3, 9 * cos(0.01f * appClock)) * speed;

            // Get both eye poses simultaneously, with IPD offset already included.
            auto tempEyePoses = hmd->getEyePoses(0, useHmdToEyeViewOffset);

            ovrPosef EyeRenderPose[2];  // Useful to remember where the rendered eye originated
            float YawAtRender[2];       // Useful to remember where the rendered eye originated

            DX11.ClearAndSetRenderTarget(EyeRenderTexture.TexRtv, EyeDepthBuffer.TexDsv);

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
                Matrix4f projTemp =
                    ovrMatrix4f_Projection(EyeRenderDesc[eye].Fov, 0.1f, 1000.0f, true);
                projTemp.Transpose();
                Mat4f proj;
                memcpy(&proj, &projTemp, sizeof(proj));

                // Render the scene
                for (int t = 0; t < timesToRenderScene; t++)
                    roomScene.Render(DX11,
                                     Vec3f{shiftedEyePos.x(), shiftedEyePos.y(), shiftedEyePos.z()},
                                     view, proj);
            }

            eyeResolveTexture->AdvanceToNextTexture();
            DX11.Context->ResolveSubresource(
                reinterpret_cast<ovrD3D11Texture&>(
                    eyeResolveTexture->TextureSet
                        ->Textures[eyeResolveTexture->TextureSet->CurrentIndex])
                    .D3D11.pTexture,
                0, EyeRenderTexture.Tex, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);

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

            // Render mirror
            ovrD3D11Texture* tex = (ovrD3D11Texture*)mirrorTexture;
            DX11.Context->CopyResource(DX11.BackBuffer, tex->D3D11.pTexture);
            DX11.SwapChain->Present(0, 0);
        }
    }

    // Release and close down
    // Release
    hmd->destroyMirrorTextureD3D11(mirrorTexture);
    hmd->destroySwapTextureSetD3D11(eyeResolveTexture);
    hmd.reset();
    ovr.reset();

    DX11.ReleaseWindow(hinst);

    return 0;
}
