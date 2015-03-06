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
#include "OVR_CAPI.h"           // Include the OculusVR SDK

#include "Win32_RoomTiny_ExampleFeatures.h"  // Include extra options to show some simple operations

#define OVR_D3D_VERSION 11
#include "OVR_CAPI_D3D.h"  // Include SDK-rendered code for the D3D version

#include "libovrwrapper.h"

#include <algorithm>
#include <cassert>
#include <regex>
#include <string>
#include <unordered_map>

using namespace libovrwrapper;

using namespace std;

unordered_map<string, string> parseArgs(const char* args) {
    unordered_map<string, string> argMap;
    regex re{ R"(-((\?)|(\w+))(\s+(\w+))?)" };
    for_each(cregex_iterator{args, args + strlen(args), re}, cregex_iterator{}, [&](const cmatch m) {
        argMap[string{ m[1].first, m[1].second }] = string{ m[5].first, m[5].second };
    });
    if (argMap.find(string{ "?" }) != end(argMap)) {
        // Print command line args
    }
    return argMap;
}

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR args, int) {
    auto argMap = parseArgs(args);

    unique_ptr<Ovr> ovr;
    unique_ptr<Hmd> hmd;

    if (argMap[string{ "oculus" }] != "off") {
        // Initializes LibOVR, and the Rift
        ovr = make_unique<Ovr>();
        hmd = make_unique<Hmd>(ovr->CreateHmd());
    }

    // MNTODO: handle startup with HMD disabled ("-oculus off" on command line) - hmd will be null
    // This currently seems preferable to switching oculus on / off as there's not an obvious way to make that play nicely with SDK distortion
    // and it also gives us the option of running on systems that don't have the Rift SDK installed.

    DirectX11 DX11{};

    // Setup Window and Graphics - use window frame if relying on Oculus driver
    bool windowed = !hmd || !hmd->testCap(ovrHmdCap_ExtendDesktop);
    Recti windowRect = hmd ? Recti(hmd->getWindowsPos(), hmd->getResolution()) : Recti(0, 0, 1280, 720);
    if (!DX11.InitWindowAndDevice(hinst, windowRect, windowed))
        return 0;

    DX11.InitSecondWindow(hinst);

    //DX11.SetMaxFrameLatency(1);

    if (hmd) {
        hmd->attachToWindow(DX11.Window);
        hmd->setCap(ovrHmdCap_LowPersistence);
        hmd->setCap(ovrHmdCap_DynamicPrediction);

        // Start the sensor which informs of the Rift's pose and motion
        hmd->configureTracking(ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection | ovrTrackingCap_Position);
    }

    // Make the eye render buffers (caution if actual size < requested due to HW limits).
    ovrRecti EyeRenderViewport[2];    // Useful to remember when varying resolution
    ImageBuffer EyeRenderTexture[2];  // Where the eye buffers will be rendered
    ImageBuffer EyeDepthBuffer[2];    // For the eye buffers to use when rendered

    for (int eye = 0; eye < 2; ++eye) {
        auto idealSize = hmd ? hmd->getFovTextureSize(ovrEyeType(eye)) : ovrSizei{ 1024, 1024 };
        EyeRenderTexture[eye] = ImageBuffer("EyeRenderTexture", DX11.Device, DX11.Context, true, false, idealSize);
        EyeDepthBuffer[eye] = ImageBuffer("EyeDepthBuffer", DX11.Device, DX11.Context, true, true, EyeRenderTexture[eye].Size);
        EyeRenderViewport[eye].Pos = Vector2i(0, 0);
        EyeRenderViewport[eye].Size = EyeRenderTexture[eye].Size;
    }

    // Setup VR components
    std::array<ovrEyeRenderDesc, 2> EyeRenderDesc;
    if (hmd) {
        ovrD3D11Config d3d11cfg;
        d3d11cfg.D3D11.Header.API = ovrRenderAPI_D3D11;
        d3d11cfg.D3D11.Header.BackBufferSize = hmd->getResolution();
        d3d11cfg.D3D11.Header.Multisample = 1;
        d3d11cfg.D3D11.pDevice = DX11.Device;
        d3d11cfg.D3D11.pDeviceContext = DX11.Context;
        d3d11cfg.D3D11.pBackBufferRT = DX11.BackBufferRT;
        d3d11cfg.D3D11.pSwapChain = DX11.SwapChain;

        EyeRenderDesc = hmd->configureRendering(
            &d3d11cfg.Config, ovrDistortionCap_Chromatic | ovrDistortionCap_Vignette |
                                  ovrDistortionCap_TimeWarp | ovrDistortionCap_Overdrive);
    }

    {
        // Create the room model
        Scene roomScene(DX11.Device, DX11.Context, false);  // Can simplify scene further with parameter if required.

        float Yaw(3.141592f);             // Horizontal rotation of the player
        Vector3f Pos(0.0f, 1.6f, -5.0f);  // Position of player

        // MAIN LOOP
        // =========
        while (!(DX11.Key['Q'] && DX11.Key[VK_CONTROL]) && !DX11.Key[VK_ESCAPE]) {
            DX11.HandleMessages();

            const float speed = 1.0f;    // Can adjust the movement speed.
            int timesToRenderScene = 1;  // Can adjust the render burden on the app.
            ovrVector3f useHmdToEyeViewOffset[2] = {EyeRenderDesc[0].HmdToEyeViewOffset,
                                                    EyeRenderDesc[1].HmdToEyeViewOffset};

            if (hmd) hmd->beginFrame(0);

            // Handle key toggles for re-centering, meshes, FOV, etc.
            if (hmd) ExampleFeatures1(DX11, hmd->getHmd(), &speed, &timesToRenderScene, useHmdToEyeViewOffset);

            // Keyboard inputs to adjust player orientation
            if (DX11.Key[VK_LEFT]) Yaw += 0.02f;
            if (DX11.Key[VK_RIGHT]) Yaw -= 0.02f;

            // Keyboard inputs to adjust player position
            if (DX11.Key['W'] || DX11.Key[VK_UP])
                Pos += Matrix4f::RotationY(Yaw).Transform(Vector3f(0, 0, -speed * 0.05f));
            if (DX11.Key['S'] || DX11.Key[VK_DOWN])
                Pos += Matrix4f::RotationY(Yaw).Transform(Vector3f(0, 0, +speed * 0.05f));
            if (DX11.Key['D'])
                Pos += Matrix4f::RotationY(Yaw).Transform(Vector3f(+speed * 0.05f, 0, 0));
            if (DX11.Key['A'])
                Pos += Matrix4f::RotationY(Yaw).Transform(Vector3f(-speed * 0.05f, 0, 0));
            Pos.y = hmd ? hmd->getProperty(OVR_KEY_EYE_HEIGHT, Pos.y) : Pos.y;

            // Animate the cube
            if (speed)
                roomScene.Models[0]->Pos = Vector3f(9 * sin(0.01f * appClock), 3, 9 * cos(0.01f * appClock));

            // Get both eye poses simultaneously, with IPD offset already included.
            auto tempEyePoses = hmd ? hmd->getEyePoses(0, useHmdToEyeViewOffset)
                                    : make_pair(array<ovrPosef, 2>{}, ovrTrackingState{});

            ovrPosef EyeRenderPose[2];  // Useful to remember where the rendered eye originated
            float YawAtRender[2];       // Useful to remember where the rendered eye originated

            // Render the two undistorted eye views into their render buffers.
            for (int eye = 0; eye < 2; eye++) {
                ImageBuffer* useBuffer = &EyeRenderTexture[eye];
                ovrPosef* useEyePose = &EyeRenderPose[eye];
                float* useYaw = &YawAtRender[eye];
                bool clearEyeImage = true;
                bool updateEyeImage = true;

                // Handle key toggles for half-frame rendering, buffer resolution, etc.
                ExampleFeatures2(DX11, eye, &useBuffer, &useEyePose, &useYaw, &clearEyeImage, &updateEyeImage,
                    &EyeRenderViewport[eye], EyeRenderTexture[eye]);

                if (clearEyeImage)
                    DX11.ClearAndSetRenderTarget(useBuffer->TexRtv, &EyeDepthBuffer[eye],
                    Recti(EyeRenderViewport[eye]));
                if (updateEyeImage) {
                    // Write in values actually used (becomes significant in Example features)
                    *useEyePose = tempEyePoses.first[eye];
                    *useYaw = Yaw;

                    // Get view and projection matrices (note near Z to reduce eye strain)
                    Matrix4f rollPitchYaw = Matrix4f::RotationY(Yaw);
                    Matrix4f finalRollPitchYaw = rollPitchYaw * Matrix4f(useEyePose->Orientation);
                    Vector3f finalUp = finalRollPitchYaw.Transform(Vector3f(0, 1, 0));
                    Vector3f finalForward = finalRollPitchYaw.Transform(Vector3f(0, 0, -1));
                    Vector3f shiftedEyePos = Pos + rollPitchYaw.Transform(useEyePose->Position);

                    Matrix4f view =
                        Matrix4f::LookAtRH(shiftedEyePos, shiftedEyePos + finalForward, finalUp);
                    Matrix4f proj = ovrMatrix4f_Projection(EyeRenderDesc[eye].Fov, 0.2f, 1000.0f, true);

                    // Render the scene
                    for (int t = 0; t < timesToRenderScene; t++)
                        roomScene.Render(DX11, view, proj.Transposed());

                    // Render undistorted to second window
                    if (eye == ovrEye_Left) {
                        // MNTODO: clean this up (hard coded width / height, assumption EyeDepthBuffer is big enough...
                        DX11.ClearAndSetRenderTarget(DX11.secondWindow->BackBufferRT, DX11.secondWindow->DepthBuffer.get(), Recti{ 0, 0, DX11.secondWindow->width, DX11.secondWindow->height });
                        roomScene.Render(DX11, view, proj.Transposed());
                        DX11.secondWindow->SwapChain->Present(0, 0);
                    }
                }
            }

            // Do distortion rendering, Present and flush/sync
            if (hmd) {
                ovrD3D11Texture eyeTexture[2];  // Gather data for eye textures
                for (int eye = 0; eye < 2; eye++) {
                    eyeTexture[eye].D3D11.Header.API = ovrRenderAPI_D3D11;
                    eyeTexture[eye].D3D11.Header.TextureSize = EyeRenderTexture[eye].Size;
                    eyeTexture[eye].D3D11.Header.RenderViewport = EyeRenderViewport[eye];
                    eyeTexture[eye].D3D11.pTexture = EyeRenderTexture[eye].Tex;
                    eyeTexture[eye].D3D11.pSRView = EyeRenderTexture[eye].TexSv;
                }
                hmd->endFrame(EyeRenderPose, &eyeTexture[0].Texture);
            }
        }
    }
    
    // Release and close down
    if (hmd) hmd->shutdownRendering();
    DX11.ReleaseWindow(hinst);

    return 0;
}
