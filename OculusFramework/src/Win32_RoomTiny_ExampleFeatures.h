/************************************************************************************
Filename    :   Win32_RoomTiny_ExampleFeatures.h
Content     :   First-person view test application for Oculus Rift
Created     :   October 20th, 2014
Author      :   Tom Heath
Copyright   :   Copyright 2014 Oculus, Inc. All Rights reserved.
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

// Note, these options may not work in combination, 
//       and may not apply to both SDK-rendered and APP-rendered

int appClock = 0; 

//-----------------------------------------------------------------------------------------------------
void ExampleFeatures1(const DirectX11& DX11, ovrHmd HMD, const float * pSpeed, int * pTimesToRenderScene, ovrVector3f * useHmdToEyeViewOffset)
{
    // Update the appClock, used by some of the features
    appClock++;

	// Recenter the Rift by pressing 'R'
    if (DX11.Key['R'])
        ovrHmd_RecenterPose(HMD);

    // Toggle to monoscopic by holding the 'I' key,
    // to recognise the pitfalls of no stereoscopic viewing, how easy it
    // is to get this wrong, and displaying the method to manually adjust.
    if (DX11.Key['I'])
	{
		useHmdToEyeViewOffset[0].x = 0; // This value would normally be half the IPD,
		useHmdToEyeViewOffset[1].x = 0; //  received from the loaded profile. 
	}

    OVR_UNUSED(pSpeed);
    OVR_UNUSED(pTimesToRenderScene);

    // Dismiss the Health and Safety message by pressing any key
    if (DX11.IsAnyKeyPressed())
        ovrHmd_DismissHSWDisplay(HMD);
}

//---------------------------------------------------------------------------------------------
void ExampleFeatures2(const DirectX11& DX11, int eye, ImageBuffer ** pUseBuffer, ovrPosef ** pUseEyePose, float ** pUseYaw,         
                      bool * pClearEyeImage,bool * pUpdateEyeImage, ovrRecti* EyeRenderViewport, const ImageBuffer& EyeRenderTexture)
{
    // A debug function that allows the pressing of 'F' to freeze/cease the generation of any new eye
    // buffers, therefore showing the independent operation of timewarp. 
    // Recommended for your applications
    if (DX11.Key['F'])
        *pClearEyeImage = *pUpdateEyeImage = false;                        

    // This illustrates how the SDK allows the developer to vary the eye buffer resolution 
    // in realtime.  Adjust with the '9' key.
    if (DX11.Key['9']) 
        EyeRenderViewport->Size.h = (int)(EyeRenderTexture.Size.h*(2+sin(0.1f*appClock))/3.0f);

     // Press 'N' to simulate if, instead of rendering frames, exhibit blank frames
    // in order to guarantee frame rate.   Not recommended at all, but useful to see,
    // just in case some might consider it a viable alternative to juddering frames.
    const int BLANK_FREQUENCY = 10;
    if ((DX11.Key['N']) && ((appClock % (BLANK_FREQUENCY*2)) == (eye*BLANK_FREQUENCY)))
        *pUpdateEyeImage = false;

    OVR_UNUSED3(pUseYaw, pUseEyePose, pUseBuffer);
}
