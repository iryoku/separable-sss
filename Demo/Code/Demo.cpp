/**
 * Copyright (C) 2012 Jorge Jimenez (jorge@iryoku.com). All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are 
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the copyright holders.
 */


#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
#include "SDKwavefile.h"
#include <xaudio2.h>

#include <fstream>
#include <sstream>
#include <iomanip>
#include <limits>

#include "Timer.h"
#include "Camera.h"
#include "RenderTarget.h"
#include "ShadowMap.h"
#include "Animation.h"
#include "Fade.h"
#include "SplashScreen.h"
#include "SeparableSSS.h"
#include "Bloom.h"
#include "FilmGrain.h"
#include "DepthOfField.h"
#include "SkyDome.h"
#include "SMAA.h"

using namespace std;

/**
 * This allows to enable/disable the splash screen and the intro.
 */
#define INTRO_BUILD 1

#pragma region Global Stuff
CDXUTDialogResourceManager dialogResourceManager;
CDXUTDialog mainHud, secondaryHud, helpHud;

IXAudio2 *xaudio;
IXAudio2MasteringVoice *masteringVoice;
IXAudio2SourceVoice *sourceVoice;
vector<BYTE> waveData;
vector<UINT32> dpdsData;
bool audioEnabled = true;
bool audioStarted = false;

Timer *timer;
ID3DX10Font *font;
ID3DX10Sprite *sprite;
CDXUTTextHelper *txtHelper;

CDXUTSDKMesh mesh;
ID3D10ShaderResourceView *specularAOSRV;
ID3D10ShaderResourceView *beckmannSRV;
ID3D10ShaderResourceView *irradianceSRV[3];
ID3D10Effect *mainEffect;
ID3D10InputLayout *vertexLayout;

Camera camera;
SplashScreen *splashScreen;
SkyDome *skyDome[3];

int scene = 0;
float sceneTime = 0.0f;
int currentSkyDome = 0;

float lastTimeMeasured = 0.0f;
int nFrames = 0;
int nFramesInverval = 0;
int minFrames = numeric_limits<int>::max(), maxFrames = numeric_limits<int>::min();
float introTime = 0.0f;

BackbufferRenderTarget *backbufferRT;
RenderTarget *mainRTMS;
RenderTarget *mainRT;
RenderTarget *tmpRT_SRGB;
RenderTarget *tmpRT;
RenderTarget *finalRT[2];
RenderTarget *specularsRTMS;
RenderTarget *specularsRT;
RenderTarget *depthRTMS;
RenderTarget *depthRT;
RenderTarget *velocityRTMS;
RenderTarget *velocityRT;
DepthStencil *depthStencilMS;
DepthStencil *depthStencil;

SeparableSSS *separableSSS;
SMAA *smaa;
Bloom *bloom;
FilmGrain *filmGrain;
DepthOfField *dof;
Quad *quad;


D3DXMATRIX prevViewProj, currViewProj;
int subsampleIndex = 0;

struct MSAAMode {
    wstring name;
    DXGI_SAMPLE_DESC desc;
};
MSAAMode msaaModes[] = {
    {L"MSAA 1x",   {1,  0}},
    {L"MSAA 2x",   {2,  0}},
    {L"MSAA 4x",   {4,  0}},
    {L"CSAA 8x",   {4,  8}},
    {L"CSAA 8xQ ", {8,  8}},
    {L"CSAA 16x",  {4, 16}},
    {L"CSAA 16xQ", {8, 16}}
};
vector<MSAAMode> supportedMsaaModes;
int antialiasingMode = int(SMAA::MODE_SMAA_T2X);


double t0 = numeric_limits<double>::min();
double tFade = numeric_limits<double>::max();
bool skipIntro = false;

bool showHud = true;
bool showHelp = false;
bool loaded = false;


const int N_LIGHTS = 5;
const int N_HEADS = 1;
const int SHADOW_MAP_SIZE = 2048;
const int HUD_WIDTH = 140;
const int HUD_WIDTH2 = 100;
const float CAMERA_FOV = 20.0f;


struct Light {
    Camera camera;
    float fov;
    float falloffWidth;
    D3DXVECTOR3 color;
    float attenuation;
    float farPlane;
    float bias;
    ShadowMap *shadowMap;
};
Light lights[N_LIGHTS];


enum Object { OBJECT_CAMERA, OBJECT_LIGHT1, OBJECT_LIGHT2, OBJECT_LIGHT3, OBJECT_LIGHT4, OBJECT_LIGHT5 };
Object object = OBJECT_CAMERA;


enum State { STATE_SPLASH_INTRO, STATE_INTRO, STATE_SPLASH_OUTRO, STATE_RUNNING };
#if INTRO_BUILD == 1
State state = STATE_SPLASH_INTRO;
#else
State state = STATE_RUNNING;
#endif
#pragma endregion

#pragma region IDC Macros
#define IDC_SSS_HELP              1
#define IDC_HELP_TEXT             2
#define IDC_HELP_CLOSE_BUTTON     3
#define IDC_TOGGLEFULLSCREEN      4
#define IDC_MSAA                  5
#define IDC_HDR                   6
#define IDC_SEPARATE_SPECULARS    7
#define IDC_PROFILE               8
#define IDC_SSS                   9
#define IDC_FOLLOW_SURFACE       10
#define IDC_NSAMPLES_LABEL       11
#define IDC_NSAMPLES             12
#define IDC_SSSWIDTH_LABEL       13
#define IDC_SSSWIDTH             14
#define IDC_STRENGTH_LABEL       15
#define IDC_STRENGTH_R_LABEL     16
#define IDC_STRENGTH_R           17
#define IDC_STRENGTH_G_LABEL     18
#define IDC_STRENGTH_G           19
#define IDC_STRENGTH_B_LABEL     20
#define IDC_STRENGTH_B           21
#define IDC_FALLOFF_LABEL        22
#define IDC_FALLOFF_R_LABEL      23
#define IDC_FALLOFF_R            24
#define IDC_FALLOFF_G_LABEL      25
#define IDC_FALLOFF_G            26
#define IDC_FALLOFF_B_LABEL      27
#define IDC_FALLOFF_B            28
#define IDC_TRANSLUCENCY_LABEL   29
#define IDC_TRANSLUCENCY         30
#define IDC_SPEC_INTENSITY_LABEL 31
#define IDC_SPEC_INTENSITY       32
#define IDC_SPEC_ROUGHNESS_LABEL 33
#define IDC_SPEC_ROUGHNESS       34
#define IDC_SPEC_FRESNEL_LABEL   35
#define IDC_SPEC_FRESNEL         36
#define IDC_BUMPINESS_LABEL      37
#define IDC_BUMPINESS            38
#define IDC_EXPOSURE_LABEL       39
#define IDC_EXPOSURE             40
#define IDC_ENVMAP_LABEL         41
#define IDC_ENVMAP               42
#define IDC_AMBIENT              43
#define IDC_AMBIENT_LABEL        44
#define IDC_FOCUS_DISTANCE       45
#define IDC_FOCUS_DISTANCE_LABEL 46
#define IDC_FOCUS_RANGE          47
#define IDC_FOCUS_RANGE_LABEL    48
#define IDC_FOCUS_FALLOFF        49
#define IDC_FOCUS_FALLOFF_LABEL  50
#define IDC_CAMERA_BUTTON        51
#define IDC_LIGHT1_BUTTON        52
#define IDC_LIGHT1_LABEL         (53 + 5)
#define IDC_LIGHT1               (54 + 5 * 2)
#pragma endregion


class ID3D10IncludeResource : public ID3D10Include {
    public:
        STDMETHOD(Open)(THIS_ D3D_INCLUDE_TYPE, LPCSTR pFileName, LPCVOID, LPCVOID *ppData, UINT *pBytes)  {
            wstringstream s;
            s << pFileName;
            HRSRC src = FindResource(GetModuleHandle(NULL), s.str().c_str(), RT_RCDATA);
            HGLOBAL res = LoadResource(GetModuleHandle(NULL), src);

            *pBytes = SizeofResource(GetModuleHandle(NULL), src);
            *ppData = (LPCVOID) LockResource(res);

            return S_OK;
        }

        STDMETHOD(Close)(THIS_ LPCVOID)  {
            return S_OK;
        }
};


void shadowPass(ID3D10Device *device) {
    for (int i = 0; i < N_LIGHTS; i++) {
        if (D3DXVec3Length(&lights[i].color) > 0.0f) {
            lights[i].shadowMap->begin(lights[i].camera.getViewMatrix(), lights[i].camera.getProjectionMatrix());
            for (int j = 0; j < N_HEADS; j++) {
                D3DXMATRIX world;
                D3DXMatrixTranslation(&world, j - (N_HEADS - 1) / 2.0f, 0.0f, 0.0f);

                lights[i].shadowMap->setWorldMatrix((float*) world);

                mesh.Render(device, lights[i].shadowMap->getTechnique());
            }
            lights[i].shadowMap->end();
        }
    }
}


MSAAMode currentMode() {
    if (antialiasingMode < 10)
        return supportedMsaaModes[0];
    else
        return supportedMsaaModes[antialiasingMode - 10];
}


bool isMSAA() {
    if (antialiasingMode < 10)
        return false;
    else
        return currentMode().desc.Count > 1;
}


void mainPass(ID3D10Device *device) {
    HRESULT hr;

    // Jitter setup:
    D3DXVECTOR2 jitters[] = {
        D3DXVECTOR2( 0.25f, -0.25f),
        D3DXVECTOR2(-0.25f,  0.25f)
    };
    const DXGI_SURFACE_DESC *desc = DXUTGetDXGIBackBufferSurfaceDesc();
    D3DXVECTOR2 jitterProjectionSpace = 2.0f * jitters[subsampleIndex];
    jitterProjectionSpace.x /= float(desc->Width); jitterProjectionSpace.y /= float(desc->Height);
    if (SMAA::Mode(antialiasingMode) == SMAA::MODE_SMAA_T2X) {
        V(mainEffect->GetVariableByName("jitter")->AsVector()->SetFloatVector((float *) jitterProjectionSpace));
    } else {
        V(mainEffect->GetVariableByName("jitter")->AsVector()->SetFloatVector((float *) D3DXVECTOR2(0.0f, 0.0f)));
    }

    // Calculate current view-projection matrix:
    currViewProj = camera.getViewMatrix() * camera.getProjectionMatrix();

    // Variables setup:
    V(mainEffect->GetVariableByName("cameraPosition")->AsVector()->SetFloatVector((float *) (D3DXVECTOR3) camera.getEyePosition()));

    for (int i = 0; i < N_LIGHTS; i++) {
        D3DXVECTOR3 t = lights[i].camera.getLookAtPosition() - lights[i].camera.getEyePosition();
        D3DXVECTOR3 dir;
        D3DXVec3Normalize(&dir, &t);

        D3DXVECTOR3 pos = lights[i].camera.getEyePosition();

        V(mainEffect->GetVariableByName("lights")->GetElement(i)->GetMemberByName("position")->AsVector()->SetFloatVector((float *) pos));
        V(mainEffect->GetVariableByName("lights")->GetElement(i)->GetMemberByName("direction")->AsVector()->SetFloatVector((float *) dir));
        V(mainEffect->GetVariableByName("lights")->GetElement(i)->GetMemberByName("falloffStart")->AsScalar()->SetFloat(cos(0.5f * lights[i].fov)));
        V(mainEffect->GetVariableByName("lights")->GetElement(i)->GetMemberByName("falloffWidth")->AsScalar()->SetFloat(lights[i].falloffWidth));
        V(mainEffect->GetVariableByName("lights")->GetElement(i)->GetMemberByName("color")->AsVector()->SetFloatVector((float *) lights[i].color));
        V(mainEffect->GetVariableByName("lights")->GetElement(i)->GetMemberByName("attenuation")->AsScalar()->SetFloat(lights[i].attenuation));
        V(mainEffect->GetVariableByName("lights")->GetElement(i)->GetMemberByName("farPlane")->AsScalar()->SetFloat(lights[i].farPlane));
        V(mainEffect->GetVariableByName("lights")->GetElement(i)->GetMemberByName("bias")->AsScalar()->SetFloat(lights[i].bias));
        V(mainEffect->GetVariableByName("lights")->GetElement(i)->GetMemberByName("viewProjection")->AsMatrix()->SetMatrix(ShadowMap::getViewProjectionTextureMatrix(lights[i].camera.getViewMatrix(), lights[i].camera.getProjectionMatrix()))); 
        V(mainEffect->GetVariableByName("shadowMaps")->GetElement(i)->AsShaderResource()->SetResource(*lights[i].shadowMap));
    }
    mainEffect->GetTechniqueByName("Render")->GetPassByIndex(0)->Apply(0); // Just to clear some runtime warnings.

     // Render target setup:
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (isMSAA()) {
        device->ClearDepthStencilView(*depthStencilMS, D3D10_CLEAR_DEPTH | D3D10_CLEAR_STENCIL, 1.0, 0);
        device->ClearRenderTargetView(*mainRTMS, clearColor);
        device->ClearRenderTargetView(*depthRTMS, clearColor);
        device->ClearRenderTargetView(*specularsRTMS, clearColor);

        ID3D10RenderTargetView *rt[] = { *mainRTMS, *depthRTMS, *velocityRTMS, *specularsRTMS };
        device->OMSetRenderTargets(4, rt, *depthStencilMS);
    } else {
        device->ClearDepthStencilView(*depthStencil, D3D10_CLEAR_DEPTH | D3D10_CLEAR_STENCIL, 1.0, 0);
        device->ClearRenderTargetView(*mainRT, clearColor);
        device->ClearRenderTargetView(*depthRT, clearColor);
        device->ClearRenderTargetView(*specularsRT, clearColor);
        device->ClearRenderTargetView(*velocityRT, clearColor);

        ID3D10RenderTargetView *rt[] = { *mainRT, *depthRT, *velocityRT, *specularsRT };
        device->OMSetRenderTargets(4, rt, *depthStencil);
    }

    // Heads rendering:
    device->IASetInputLayout(vertexLayout);

    for (int i = 0; i < N_HEADS; i++) {
        D3DXMATRIX world;
        D3DXMatrixTranslation(&world, i - (N_HEADS - 1) / 2.0f, 0.0f, 0.0f);

        D3DXMATRIX currWorldViewProj = world * currViewProj;
        D3DXMATRIX prevWorldViewProj = world * prevViewProj;

        V(mainEffect->GetVariableByName("currWorldViewProj")->AsMatrix()->SetMatrix((float *) currWorldViewProj));
        V(mainEffect->GetVariableByName("prevWorldViewProj")->AsMatrix()->SetMatrix((float *) prevWorldViewProj));
        V(mainEffect->GetVariableByName("world")->AsMatrix()->SetMatrix((float *) world));
        V(mainEffect->GetVariableByName("worldInverseTranspose")->AsMatrix()->SetMatrixTranspose((float *) world));
        V(mainEffect->GetVariableByName("id")->AsScalar()->SetInt(1));

        mesh.Render(device, mainEffect->GetTechniqueByName("Render"), mainEffect->GetVariableByName("diffuseTex")->AsShaderResource(), mainEffect->GetVariableByName("normalTex")->AsShaderResource());
    }

    // Update previous view-projection matrix:
    prevViewProj = currViewProj;

    // Sky dome rendering:
    if (isMSAA())
        device->OMSetRenderTargets(1, *mainRTMS, *depthStencilMS);
    else
        device->OMSetRenderTargets(1, *mainRT, *depthStencil);
    skyDome[currentSkyDome]->render(camera.getViewMatrix(), camera.getProjectionMatrix(), 10.0f);
    device->OMSetRenderTargets(0, NULL, NULL);

    // Multisample buffers resolve:
    if (isMSAA()) {
        DXGI_FORMAT format = mainHud.GetCheckBox(IDC_HDR)->GetChecked()? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        device->ResolveSubresource(*mainRT, 0, *mainRTMS, 0, format);
        device->ResolveSubresource(*depthRT, 0, *depthRTMS, 0, DXGI_FORMAT_R32_FLOAT);
        device->ResolveSubresource(*specularsRT, 0, *specularsRTMS, 0, format);
    }
}


void addSpecular(ID3D10Device *device, RenderTarget *dst) {
    HRESULT hr;
    quad->setInputLayout();
    V(mainEffect->GetVariableByName("specularsTex")->AsShaderResource()->SetResource(*specularsRT));
    V(mainEffect->GetTechniqueByName("AddSpecular")->GetPassByIndex(0)->Apply(0));
    device->OMSetRenderTargets(1, *dst, NULL);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);
}


void smaaPass(ID3D10Device *device) {
    // Clear the stencil buffer:
    device->ClearDepthStencilView(*depthStencil, D3D10_CLEAR_STENCIL, 1.0, 0);

    // Calculate next subpixel index:
    int previousIndex = subsampleIndex;
    int currentIndex = (subsampleIndex + 1) % 2;

    // Run SMAA:
    smaa->go(*tmpRT, *tmpRT_SRGB, *depthRT, *finalRT[currentIndex], *depthStencil, SMAA::INPUT_LUMA,  SMAA::MODE_SMAA_T2X, subsampleIndex);
    smaa->reproject(*finalRT[currentIndex], *finalRT[previousIndex], *velocityRT, *tmpRT_SRGB);

    // Update subpixel index:
    subsampleIndex = currentIndex;
}


void renderText() {
    txtHelper->Begin();
    txtHelper->SetInsertionPos(7, 5);
    txtHelper->SetForegroundColor(D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f));

    wstringstream s;

    if (nFrames > 0) {
        s << setprecision(2) << std::fixed;
        s << "Min FPS: " << minFrames << endl;
        txtHelper->DrawTextLine(s.str().c_str());

        s.str(L"");
        s << "Max FPS: " << maxFrames << endl;
        txtHelper->DrawTextLine(s.str().c_str());

        s.str(L"");
        s << "Average FPS: " << nFrames / introTime << endl;
        txtHelper->DrawTextLine(s.str().c_str());

        txtHelper->DrawTextLine(L"");
    }

    if (timer->isEnabled()) {
        s.str(L"");
        s << *timer;
        txtHelper->DrawTextLine(s.str().c_str());
    }

    txtHelper->End();
}


void renderScene(ID3D10Device *device, double time, float elapsedTime) {
    // Shadow Pass
    timer->start();
    shadowPass(device);
    timer->clock(L"Shadows");

    // Main Pass
    mainPass(device);
    timer->clock(L"Main Pass");

    // Subsurface Scattering Pass
    if (mainHud.GetCheckBox(IDC_SSS)->GetChecked())
        separableSSS->go(*mainRT, *mainRT, *depthRT, *depthStencil, *specularsRT, 1);
    timer->clock(L"Skin");

    if (mainHud.GetCheckBox(IDC_SEPARATE_SPECULARS)->GetChecked())
        addSpecular(device, mainRT);
    timer->clock(L"Separate Speculars");

    // Bloom Pass
    bloom->go(*mainRT, *tmpRT_SRGB);
    timer->clock(L"Bloom");
    
    // Depth of Field Pass
    dof->go(*tmpRT_SRGB, *tmpRT_SRGB, *depthRT);
    timer->clock(L"DoF");

    // SMAA Pass
    if (SMAA::Mode(antialiasingMode) == SMAA::MODE_SMAA_T2X) {
        smaaPass(device);
        timer->clock(L"SMAA");
    }

    // Film Grain Pass
    filmGrain->go(*tmpRT_SRGB, *backbufferRT, 2.5f * float(time));
    timer->clock(L"Film Grain");
}


void updateLightControls(int i) {
    wstringstream s;
    s << lights[i].color.x;
    secondaryHud.GetStatic(IDC_LIGHT1_LABEL + i)->SetText(s.str().c_str());
    secondaryHud.GetSlider(IDC_LIGHT1 + i)->SetValue(int((lights[i].color.x / 5.0f) * 100.0f));
}


float updateSlider(CDXUTDialog &hud, UINT slider, UINT label, float scale, wstring text) {
    int min, max;
    hud.GetSlider(slider)->GetRange(min, max);
    float value = scale * float(hud.GetSlider(slider)->GetValue()) / (max - min);

    wstringstream s;
    s << text << value;
    hud.GetStatic(label)->SetText(s.str().c_str());

    return value;
}


void loadShot(istream &f) {
    HRESULT hr;

    f >> camera;

    for (int i = 0; i < N_LIGHTS; i++) {
        f >> lights[i].camera;
        f >> lights[i].color.x;
        f >> lights[i].color.y;
        f >> lights[i].color.z;
        updateLightControls(i);
    }

    float ambient;
    f >> ambient;
    secondaryHud.GetSlider(IDC_AMBIENT)->SetValue(int(ambient * 100.0f));
    updateSlider(secondaryHud, IDC_AMBIENT, IDC_AMBIENT_LABEL, 1.0f, L"Ambient: ");
    V(mainEffect->GetVariableByName("ambient")->AsScalar()->SetFloat(ambient));

    float environment;
    f >> environment;
    secondaryHud.GetSlider(IDC_ENVMAP)->SetValue(int(environment * 100.0f));
    updateSlider(secondaryHud, IDC_ENVMAP, IDC_ENVMAP_LABEL, 1.0f, L"Enviroment: ");
    for (int i = 0; i < 3; i++)
        skyDome[i]->setIntensity(environment);

    float value;
    f >> value;
    secondaryHud.GetSlider(IDC_FOCUS_DISTANCE)->SetValue(int((value / 2.0f) * 100.0f));
    updateSlider(secondaryHud, IDC_FOCUS_DISTANCE, IDC_FOCUS_DISTANCE_LABEL, 2.0f, L"Focus dist.: "); 
    dof->setFocusDistance(value);

    f >> value;
    secondaryHud.GetSlider(IDC_FOCUS_RANGE)->SetValue(int(value * 100.0f));
    updateSlider(secondaryHud, IDC_FOCUS_RANGE, IDC_FOCUS_RANGE_LABEL, 1.0f, L"Focus Range: "); 
    dof->setFocusRange(pow(value, 5.0f));

    f >> value;
    secondaryHud.GetSlider(IDC_FOCUS_FALLOFF)->SetValue(int((value / 20.0f)* 100.0f));
    updateSlider(secondaryHud, IDC_FOCUS_FALLOFF, IDC_FOCUS_FALLOFF_LABEL, 20.0f, L"Focus Falloff: "); 
    dof->setFocusFalloff(D3DXVECTOR2(value, value));
}


void loadPreset(int i) {
    wstringstream s;
    s << L"Preset" << i << L".txt";
    HRSRC src = FindResource(GetModuleHandle(NULL), s.str().c_str(), RT_RCDATA);
    HGLOBAL res = LoadResource(GetModuleHandle(NULL), src);
    LPBYTE data = (LPBYTE) LockResource(res);

    stringstream f((char *) data);
    loadShot(f);
}


void setExposure(float value) {
    bloom->setExposure(value);
    filmGrain->setExposure(value);
}


void correct(float amount) {
    camera.setDistanceVelocity(amount * camera.getDistanceVelocity());
    camera.setPanVelocity(amount * camera.getPanVelocity());
    camera.setAngularVelocity(amount * camera.getAngularVelocity());
}


bool setupIntro(float t) {
    HRESULT hr;

    switch (scene) {
        case 0:
            setExposure(0.0f);

            loadPreset(scene);
            camera.setDistanceVelocity(-25.1f);
            camera.setPanVelocity(D3DXVECTOR2(0.0f, -1.0f));
            correct(0.63f);
            scene++;
            break;
        case 1: {
            t = t - sceneTime;
            setExposure(2.0f * Animation::smoothFade(t, 0.0f, 39.0f, 15.0f, 5.0f));
            float focusFalloff = Animation::smooth2(t, 13.0f, 39.0f, 0.0f, 15.0f, 0.0f);
            dof->setFocusFalloff(D3DXVECTOR2(focusFalloff, focusFalloff));

            if (t > 40.5f) {
                sceneTime += 40.5f;
                loadPreset(scene);
                camera.setDistanceVelocity(0.0f);
                camera.setPanVelocity(D3DXVECTOR2(0.0f, 0.0f));
                scene++;
            }
            break;
        }   
        case 2:
            t = t - sceneTime;
            setExposure(0.5f * Animation::smoothFade(t, 0.0f, 16.0f, 3.0f, 3.0f));
            lights[1].color = Animation::smooth2(t, 5.0, 16.0f, D3DXVECTOR3(0.0f, 0.0f, 0.0f), D3DXVECTOR3(1.6f, 1.6f, 1.6f), D3DXVECTOR3(0.0f, 0.0f, 0.0f));

            if (t > 16.5f) {
                sceneTime += 16.5f;
                loadPreset(scene);
                correct(0.85f);
                camera.setDistanceVelocity(0.0f);
                camera.setPanVelocity(D3DXVECTOR2(0.0f, 0.0f));
                scene++;
            }
            break;
        case 3:
            t = t - sceneTime;
            setExposure(1.75f * Animation::smoothFade(t, 0.0f, 17.0f, 3.0f, 3.0f));

            if (t > 17.5f) {
                sceneTime += 17.5f;
                loadPreset(scene);
                camera.setDistanceVelocity(0.15f);
                camera.setPanVelocity(D3DXVECTOR2(0.0f, 0.0f));
                scene++;
            }
            break;
        case 4:
            t = t - sceneTime;
            setExposure(1.2f * Animation::smoothFade(t, 0.0f, 12.0f, 3.0f, 3.0f));
            lights[1].color = Animation::smooth2(t, 0.0, 10.0f, D3DXVECTOR3(0.5f, 0.5f, 0.5f), D3DXVECTOR3(4.0f, 4.0f, 4.0f), D3DXVECTOR3(0.0f, 0.0f, 0.0f));

            if (t > 12.5f) {
                sceneTime += 12.5f;
                loadPreset(scene);
                camera.setDistanceVelocity(0.0f);
                camera.setPanVelocity(D3DXVECTOR2(0.0f, 0.0f));
                scene++;
            }
            break;
        case 5: {
            t = t - sceneTime;
            float distance = camera.getDistance();
            setExposure(1.2f * Animation::smoothFade(t, 0.0f, 16.0f, 3.0f, 3.0f));
            dof->setFocusDistance(distance - 1.0f + Animation::smooth2(t, 4.5, 8.0f, 0.82f, 0.4f, 0.0f));

            if (t > 16.5f) {
                sceneTime += 16.5f;
                loadPreset(scene);
                camera.setDistanceVelocity(0.0f);
                camera.setPanVelocity(D3DXVECTOR2(0.0f, 0.0f));
                scene++;
            }
            break;
        }
        case 6:
            t = t - sceneTime;
            setExposure(2.3f * Animation::smoothFade(t, 0.0f, 16.0f, 8.0f, 3.0f));
            dof->setFocusRange(Animation::smooth2(t, 0.0f, 7.0f, 0.0f, 0.18f, 0.0f));

            if (t > 16.5f) {
                sceneTime += 16.5f;
                loadPreset(scene);
                camera.setDistanceVelocity(0.0f);
                camera.setPanVelocity(D3DXVECTOR2(0.0f, 0.0f));
                lights[0].color.x *= 0.4f;
                lights[0].color.y *= 0.7f;
                lights[0].color.z *= 0.6f;
                lights[0].color *= 1.5f;
                scene++;
            }
            break;
        case 7:
            t = t - sceneTime;
            setExposure(1.5f * Animation::smoothFade(t, 0.0f, 16.0f, 3.0f, 3.0f));

            if (t > 16.5f) {
                sceneTime += 16.5f;
                loadPreset(scene);
                camera.setDistanceVelocity(0.0f);
                camera.setPanVelocity(D3DXVECTOR2(0.0f, 0.0f));
                V(mainEffect->GetVariableByName("specularRoughness")->AsScalar()->SetFloat(0.2f));
                V(mainEffect->GetVariableByName("specularIntensity")->AsScalar()->SetFloat(1.2f));
                scene++;
            }
            break;
        case 8:
            t = t - sceneTime;
            setExposure(2.0f * Animation::smoothFade(t, 0.0f, 16.0f, 3.0f, 3.0f));

            if (t > 16.5f) {
                sceneTime += 16.5f;
                loadPreset(scene);
                camera.setDistanceVelocity(20.0f);
                camera.setPanVelocity(D3DXVECTOR2(0.0f, 1.4f));
                V(mainEffect->GetVariableByName("specularRoughness")->AsScalar()->SetFloat(0.3f));
                V(mainEffect->GetVariableByName("specularIntensity")->AsScalar()->SetFloat(1.88f));
                scene++;
            }
            break;
        case 9:
            t = t - sceneTime;
            setExposure(2.5f * Animation::smoothFade(t, 0.0f, 30.0f, 27.0f, 3.0f));
            float focusFalloff = Animation::smooth2(t, 0.0f, 10.0f, 15.0f, 0.0f, 0.0f);
            dof->setFocusFalloff(D3DXVECTOR2(focusFalloff, focusFalloff));

            if (t > 30.0f)
                return true;
            break;
    }
    return false;
}


void reset() {
    HRESULT hr;

    loadPreset(9);
    camera.setDistanceVelocity(0.0f);
    camera.setPanVelocity(D3DXVECTOR2(0.0f, 0.0f));
    setExposure(2.0f);
    V(mainEffect->GetVariableByName("specularRoughness")->AsScalar()->SetFloat(0.3f));
    V(mainEffect->GetVariableByName("specularIntensity")->AsScalar()->SetFloat(1.88f));
}


void CALLBACK onFrameRender(ID3D10Device *device, double time, float elapsedTime, void *context) {
    if (t0 == numeric_limits<double>::min())
        t0 = time;
    if (tFade == numeric_limits<double>::max() && skipIntro)
        tFade = time;

    #if INTRO_BUILD == 1
    if (float(time - t0) > 1.0f && audioEnabled && !audioStarted) {
        sourceVoice->Start(0);
        audioStarted = true;
        ShowCursor(FALSE);
    }
    #endif

    switch (state) {
        case STATE_SPLASH_INTRO:
            // Run the intro:
            splashScreen->intro(float(time - t0));
            Fade::go(Animation::linear(float(time - tFade), 0.0f, 1.0f, 1.0f, 0.0f));

            // If the scene or the fade have finished:
            if (splashScreen->introHasFinished(float(time - t0)) || float(time - tFade) > 1.0f) {
               // Setup next state:
               state = STATE_INTRO;
               t0 = time;
            }
            break;
        case STATE_INTRO: {
            // Setup the intro:
            bool finished = setupIntro(float(time - t0));

            // Render the scene:
            renderScene(device, time, elapsedTime);
            device->OMSetRenderTargets(1, *backbufferRT, NULL);
            Fade::go(Animation::linear(float(time - tFade), 0.0f, 1.0f, 1.0f, 0.0f));

            // Update counters:
            nFramesInverval += 1;
            if (lastTimeMeasured + 1.0f < float(time - t0)) {
                nFrames += nFramesInverval;
                minFrames = min(minFrames, nFramesInverval);
                maxFrames = max(maxFrames, nFramesInverval);
                lastTimeMeasured = float(time - t0);
                nFramesInverval = 0;
            }

            // If the scene or the fade have finished:
            if (finished || float(time - tFade) > 1.0f) {
               // Setup next state:
               state = STATE_SPLASH_OUTRO;
               introTime = float(time - t0);
               t0 = time;

               // Reset the scene:
               reset();
               break;
            }
            break;
        }
        case STATE_SPLASH_OUTRO:
            // Run the outro:
            splashScreen->outro(float(time - t0));
            Fade::go(Animation::linear(float(time - tFade), 0.0f, 1.0f, 1.0f, 0.0f));

            // If the scene or the fade have finished:
            if (splashScreen->outroHasFinished(float(time - t0)) || float(time - tFade) > 1.0f) {
               // Setup next state:
               state = STATE_RUNNING;
               t0 = time;
               ShowCursor(TRUE);
            }
            break;
        case STATE_RUNNING: {
            // Render the scene:
            renderScene(device, time, elapsedTime);

            // Render the HUD:
            device->OMSetRenderTargets(1, *backbufferRT, NULL);
            if (showHud) {
                mainHud.OnRender(elapsedTime);
                secondaryHud.OnRender(elapsedTime);
                renderText();
            }

            // Show the help:
            if (showHelp) {
                Fade::go(0.25);
                helpHud.OnRender(elapsedTime);
            }

            // Fade out the audio:
            #if INTRO_BUILD == 1
            if (audioEnabled)
                sourceVoice->SetVolume(Animation::smooth2(log(float(time - t0)), 0.0f, 2.0f, 1.0f, 0.0f, 0.0f));
            Fade::go(Animation::linear(float(time - t0), 1.0f, 4.0f, 0.0f, 1.0f));
            #endif
            break;
        }
    }
}


void setupSSS(ID3D10Device *device, const DXGI_SURFACE_DESC *desc) {
    int min, max;
    mainHud.GetSlider(IDC_SSSWIDTH)->GetRange(min, max);
    float sssLevel = 0.025f * float(mainHud.GetSlider(IDC_SSSWIDTH)->GetValue()) / (max - min);
    float value = 33.0f * float(mainHud.GetSlider(IDC_NSAMPLES)->GetValue()) / (max - min);
    int nSamples = std::max(3, 2 * (int(value) / 2) + 1);
    bool stencilInitialized = currentMode().desc.Count == 1;
    bool followSurface = mainHud.GetCheckBox(IDC_FOLLOW_SURFACE)->GetChecked();
    DXGI_FORMAT format = mainHud.GetCheckBox(IDC_HDR)->GetChecked()? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    SAFE_DELETE(separableSSS);
    separableSSS = new SeparableSSS(device, desc->Width, desc->Height, CAMERA_FOV, sssLevel, nSamples, stencilInitialized, followSurface, true, format);

    smaa = new SMAA(device, desc->Width, desc->Height, SMAA::PRESET_HIGH, true, true);

    D3DXVECTOR3 strength;
    mainHud.GetSlider(IDC_STRENGTH_R)->GetRange(min, max);
    strength.x = float(mainHud.GetSlider(IDC_STRENGTH_R)->GetValue()) / (max - min);
    strength.y = float(mainHud.GetSlider(IDC_STRENGTH_G)->GetValue()) / (max - min);
    strength.z = float(mainHud.GetSlider(IDC_STRENGTH_B)->GetValue()) / (max - min);
    separableSSS->setStrength(strength);
    
    D3DXVECTOR3 falloff;
    mainHud.GetSlider(IDC_FALLOFF_R)->GetRange(min, max);
    falloff.x = float(mainHud.GetSlider(IDC_FALLOFF_R)->GetValue()) / (max - min);
    falloff.y = float(mainHud.GetSlider(IDC_FALLOFF_G)->GetValue()) / (max - min);
    falloff.z = float(mainHud.GetSlider(IDC_FALLOFF_B)->GetValue()) / (max - min);
    separableSSS->setFalloff(falloff);
}


Camera *currentObject() { 
    switch (object) {
        case OBJECT_CAMERA:
            return &camera;
        default:
            return &lights[object - OBJECT_LIGHT1].camera;
    }
}


void CALLBACK onFrameMove(double time, float elapsedTime, void *context) {
    camera.frameMove(elapsedTime);
    for (int i = 0; i < N_LIGHTS; i++)
        lights[i].camera.frameMove(elapsedTime);

    int min, max;
    secondaryHud.GetSlider(IDC_FOCUS_DISTANCE)->GetRange(min, max);
    float focusDistance = 2.0f * float(secondaryHud.GetSlider(IDC_FOCUS_DISTANCE)->GetValue()) / (max - min);

    float distance = camera.getDistance();
    dof->setFocusDistance(distance - 1.0f + focusDistance);
}


void CALLBACK keyboardProc(UINT nChar, bool keydown, bool altdown, void *context) {
    HRESULT hr;

    if (keydown)
    switch (nChar) {
        case VK_TAB:
            showHud = !showHud;
            break;
        case 'O': {
            fstream f("Config.txt", fstream::out);
            f << camera;
            for (int i = 0; i < N_LIGHTS; i++) {
                f << lights[i].camera;
                f << lights[i].color.x << endl;
                f << lights[i].color.y << endl;
                f << lights[i].color.z << endl;
            }
            float ambient;
            V(mainEffect->GetVariableByName("ambient")->AsScalar()->GetFloat(&ambient));
            f << ambient << endl;
            f << skyDome[currentSkyDome]->getIntensity() << endl;
            f << (dof->getFocusDistance() - camera.getDistance() + 1) << endl;
            f << pow(dof->getFocusRange(), 1.0f / 5.0f) << endl;
            f << dof->getFocusFalloff().x << endl;
            break;
        }
        case 'P': {
            fstream f("Config.txt", fstream::in);
            loadShot(f);
            break;
        }
        case 'S': {
            bool sssEnabled = mainHud.GetCheckBox(IDC_SSS)->GetChecked();
            mainHud.GetCheckBox(IDC_SSS)->SetChecked(!sssEnabled);
            V(mainEffect->GetVariableByName("sssEnabled")->AsScalar()->SetBool(!sssEnabled));
            break;
        }
        case 'X':
            mainHud.GetCheckBox(IDC_PROFILE)->SetChecked(!mainHud.GetCheckBox(IDC_PROFILE)->GetChecked());
            timer->setEnabled(mainHud.GetCheckBox(IDC_PROFILE)->GetChecked());
            break;
        case '1':
            object = OBJECT_CAMERA;
            for (int i = 0; i < N_LIGHTS + 1; i++)
                secondaryHud.GetButton(IDC_CAMERA_BUTTON + i)->SetEnabled(i != int(object));
            break;
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
            object = Object(OBJECT_LIGHT1 + nChar - '2');
            for (int i = 0; i < N_LIGHTS + 1; i++)
                secondaryHud.GetButton(IDC_CAMERA_BUTTON + i)->SetEnabled(i != int(object));
            break;
        case 'C': {
            if (DXUTIsKeyDown(VK_LCONTROL) || DXUTIsKeyDown(VK_RCONTROL)) {
                string code = separableSSS->getKernelCode();
                if (OpenClipboard(DXUTGetHWND())) {
                    EmptyClipboard();
                    HGLOBAL data;
                    data = GlobalAlloc(GMEM_DDESHARE, code.size() + 1);
                    char *buffer = (char*) GlobalLock(data);
                    strcpy_s(buffer, code.size() + 1, code.c_str());
                    GlobalUnlock(data);
                    SetClipboardData(CF_TEXT, data);
                    CloseClipboard();
                }
            }
            break;
        }
        case ' ':
            if (state == STATE_SPLASH_INTRO || state == STATE_INTRO)
                skipIntro = true;
            break;
    }
}


LRESULT CALLBACK msgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, bool *pbNoFurtherProcessing, void *context) {
    switch (state) {
        case STATE_RUNNING:
            *pbNoFurtherProcessing = dialogResourceManager.MsgProc(hwnd, msg, wparam, lparam);
            if (*pbNoFurtherProcessing)
                return 0;
            
            if (showHelp) {
                *pbNoFurtherProcessing = helpHud.MsgProc(hwnd, msg, wparam, lparam);
                if (*pbNoFurtherProcessing)
                    return 0;
            } else if (showHud) {
                *pbNoFurtherProcessing = mainHud.MsgProc(hwnd, msg, wparam, lparam);
                if (*pbNoFurtherProcessing)
                    return 0;
            
                *pbNoFurtherProcessing = secondaryHud.MsgProc(hwnd, msg, wparam, lparam);
                if (*pbNoFurtherProcessing)
                    return 0;
            }

            currentObject()->handleMessages(hwnd, msg, wparam, lparam);

            break;
    }

    return 0;
}


void setupMainEffect() {
    HRESULT hr;

    SAFE_RELEASE(mainEffect);

    vector<D3D10_SHADER_MACRO> defines;

    D3D10_SHADER_MACRO separateSpecularsMacro = { "SEPARATE_SPECULARS", NULL };
    if (mainHud.GetCheckBox(IDC_SEPARATE_SPECULARS)->GetChecked())
        defines.push_back(separateSpecularsMacro);

    D3D10_SHADER_MACRO null = { NULL, NULL };
    defines.push_back(null);
    
    ID3D10IncludeResource includeResource;
    V(D3DX10CreateEffectFromResource(GetModuleHandle(NULL), L"Main.fx", NULL, &defines.front(), &includeResource, "fx_4_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, DXUTGetD3D10Device(), NULL, NULL, &mainEffect, NULL, NULL));

    V(mainEffect->GetVariableByName("specularAOTex")->AsShaderResource()->SetResource(specularAOSRV));
    V(mainEffect->GetVariableByName("beckmannTex")->AsShaderResource()->SetResource(beckmannSRV));
    V(mainEffect->GetVariableByName("irradianceTex")->AsShaderResource()->SetResource(irradianceSRV[currentSkyDome]));

    int min, max;
    mainHud.GetSlider(IDC_TRANSLUCENCY)->GetRange(min, max);
    float scale = float(mainHud.GetSlider(IDC_TRANSLUCENCY)->GetValue()) / (max - min);
    V(mainEffect->GetVariableByName("translucency")->AsScalar()->SetFloat(scale));

    bool sssEnabled = mainHud.GetCheckBox(IDC_SSS)->GetChecked();
    V(mainEffect->GetVariableByName("sssEnabled")->AsScalar()->SetBool(sssEnabled));

    mainHud.GetSlider(IDC_TRANSLUCENCY)->GetRange(min, max);
    float sssWidth = 0.025f * float(mainHud.GetSlider(IDC_SSSWIDTH)->GetValue()) / (max - min);
    V(mainEffect->GetVariableByName("sssWidth")->AsScalar()->SetFloat(sssWidth));

    secondaryHud.GetSlider(IDC_SPEC_INTENSITY)->GetRange(min, max);
    float specularIntensity = float(secondaryHud.GetSlider(IDC_SPEC_INTENSITY)->GetValue()) / (max - min);
    V(mainEffect->GetVariableByName("specularIntensity")->AsScalar()->SetFloat(4.0f * specularIntensity));

    secondaryHud.GetSlider(IDC_SPEC_ROUGHNESS)->GetRange(min, max);
    float specularRoughness = float(secondaryHud.GetSlider(IDC_SPEC_ROUGHNESS)->GetValue()) / (max - min);
    V(mainEffect->GetVariableByName("specularRoughness")->AsScalar()->SetFloat(specularRoughness));

    secondaryHud.GetSlider(IDC_SPEC_FRESNEL)->GetRange(min, max);
    float specularFresnel = float(secondaryHud.GetSlider(IDC_SPEC_FRESNEL)->GetValue()) / (max - min);
    V(mainEffect->GetVariableByName("specularFresnel")->AsScalar()->SetFloat(specularFresnel));

    secondaryHud.GetSlider(IDC_BUMPINESS)->GetRange(min, max);
    float bumpiness = float(secondaryHud.GetSlider(IDC_BUMPINESS)->GetValue()) / (max - min);
    V(mainEffect->GetVariableByName("bumpiness")->AsScalar()->SetFloat(bumpiness));

    secondaryHud.GetSlider(IDC_AMBIENT)->GetRange(min, max);
    float ambient = float(secondaryHud.GetSlider(IDC_AMBIENT)->GetValue()) / (max - min);
    V(mainEffect->GetVariableByName("ambient")->AsScalar()->SetFloat(ambient));
}


HRESULT CALLBACK onResizedSwapChain(ID3D10Device *device, IDXGISwapChain *swapChain, const DXGI_SURFACE_DESC *desc, void *context) {
    HRESULT hr;

    V_RETURN(dialogResourceManager.OnD3D10ResizedSwapChain(device, desc));

    DXGI_FORMAT format = mainHud.GetCheckBox(IDC_HDR)->GetChecked()? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    mainHud.GetComboBox(IDC_MSAA)->RemoveAllItems();
    mainHud.GetComboBox(IDC_MSAA)->AddItem(L"SMAA T2x", (LPVOID) SMAA::MODE_SMAA_T2X);

    supportedMsaaModes.clear();
    for(char i = 0; i < sizeof(msaaModes) / sizeof(MSAAMode); i++){
        UINT quality;
        device->CheckMultisampleQualityLevels(format, msaaModes[i].desc.Count, &quality);
        if (quality > msaaModes[i].desc.Quality) {
            mainHud.GetComboBox(IDC_MSAA)->AddItem(msaaModes[i].name.c_str(), (void*) (i + 10));
            supportedMsaaModes.push_back(msaaModes[i]);
        }
    }
    mainHud.GetComboBox(IDC_MSAA)->SetSelectedByData((void *) antialiasingMode);

    if (isMSAA()) {
        MSAAMode mode = currentMode();
        mainRTMS = new RenderTarget(device, desc->Width, desc->Height, format, mode.desc);
        depthRTMS = new RenderTarget(device, desc->Width, desc->Height, DXGI_FORMAT_R32_FLOAT, mode.desc);
        specularsRTMS = new RenderTarget(device, desc->Width, desc->Height, format, mode.desc);
        velocityRTMS = new RenderTarget(device, desc->Width, desc->Height, DXGI_FORMAT_R16G16_FLOAT, mode.desc);
        depthStencilMS = new DepthStencil(device, desc->Width, desc->Height,  DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, mode.desc);
    }

    backbufferRT = new BackbufferRenderTarget(device, DXUTGetDXGISwapChain());
    mainRT = new RenderTarget(device, desc->Width, desc->Height, format);
    tmpRT_SRGB = new RenderTarget(device, desc->Width, desc->Height, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    tmpRT = new RenderTarget(device, *tmpRT_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM);
    for (int i = 0; i < 2; i++)
        finalRT[i] = new RenderTarget(device, desc->Width, desc->Height, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

    depthRT = new RenderTarget(device, desc->Width, desc->Height, DXGI_FORMAT_R32_FLOAT);
    specularsRT = new RenderTarget(device, desc->Width, desc->Height, format);
    velocityRT = new RenderTarget(device, desc->Width, desc->Height, DXGI_FORMAT_R16G16_FLOAT);
    depthStencil = new DepthStencil(device, desc->Width, desc->Height, DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS);

    float aspect = (float) desc->Width / desc->Height;
    camera.setProjection(CAMERA_FOV * D3DX_PI / 180.f, aspect, 0.1f, 100.0f);

    setupSSS(device, desc);

    int min, max;
    secondaryHud.GetSlider(IDC_EXPOSURE)->GetRange(min, max);
    float exposure = 5.0f * float(secondaryHud.GetSlider(IDC_EXPOSURE)->GetValue()) / (max - min);
    bloom = new Bloom(device, desc->Width, desc->Height, format, Bloom::TONEMAP_FILMIC, exposure, 0.63f, 1.0f, 1.0f, 0.2f);

    filmGrain = new FilmGrain(device, desc->Width, desc->Height, 1.0f, exposure);

    secondaryHud.GetSlider(IDC_FOCUS_DISTANCE)->GetRange(min, max);
    float focusDistance = 2.0f * float(secondaryHud.GetSlider(IDC_FOCUS_DISTANCE)->GetValue()) / (max - min);
    float focusRange = pow(float(secondaryHud.GetSlider(IDC_FOCUS_RANGE)->GetValue()) / (max - min), 5.0f);
    float focusFalloff = 20.0f * float(secondaryHud.GetSlider(IDC_FOCUS_FALLOFF)->GetValue()) / (max - min);
    dof = new DepthOfField(device, desc->Width, desc->Height, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, focusDistance, focusRange, D3DXVECTOR2(focusFalloff, focusFalloff), 2.5f);

    camera.setViewportSize(D3DXVECTOR2((float) desc->Width, (float) desc->Height));
    for (int i = 0; i < N_LIGHTS; i++)
        lights[i].camera.setViewportSize(D3DXVECTOR2((float) desc->Width, (float) desc->Height));

    mainHud.SetLocation(desc->Width - (45 + HUD_WIDTH), 0);
    secondaryHud.SetLocation(0, desc->Height - 520);
    helpHud.SetLocation(0, 0);
    helpHud.GetStatic(IDC_HELP_TEXT)->SetSize(desc->Width, desc->Height);
    helpHud.GetButton(IDC_HELP_CLOSE_BUTTON)->SetLocation(desc->Width - (45 + HUD_WIDTH) + 35, 10);

    if (!loaded) {
        #if INTRO_BUILD == 1
        loadPreset(0);
        #else
        loadPreset(9);
        #endif
        loaded = true;
    }

    return S_OK;
}


void CALLBACK onReleasingSwapChain(void *context) {
    dialogResourceManager.OnD3D10ReleasingSwapChain();

    SAFE_DELETE(mainRTMS);
    SAFE_DELETE(depthStencilMS);
    SAFE_DELETE(depthRTMS);
    SAFE_DELETE(specularsRTMS);
    SAFE_DELETE(velocityRTMS);

    SAFE_DELETE(backbufferRT);
    SAFE_DELETE(mainRT);
    SAFE_DELETE(tmpRT_SRGB);
    SAFE_DELETE(tmpRT);
    for (int i = 0; i < 2; i++)
        SAFE_DELETE(finalRT[i]);

    SAFE_DELETE(depthRT);
    SAFE_DELETE(specularsRT);
    SAFE_DELETE(velocityRT);
    SAFE_DELETE(depthStencil);

    SAFE_DELETE(separableSSS);
    SAFE_DELETE(smaa);
    SAFE_DELETE(bloom);
    SAFE_DELETE(filmGrain);
    SAFE_DELETE(dof);
}


void CALLBACK onGUIEvent(UINT event, int id, CDXUTControl *control, void *context) {
    HRESULT hr;

    switch (id) {
        case IDC_SSS_HELP:
            showHelp = true;
            mainHud.GetButton(IDC_SSS_HELP)->SetVisible(false);
            break;
        case IDC_HELP_CLOSE_BUTTON:
            showHelp = false;
            mainHud.GetButton(IDC_SSS_HELP)->SetVisible(true);
            break;
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen();
            break;
        case IDC_MSAA: {
            CDXUTComboBox *box = (CDXUTComboBox *) control;
            antialiasingMode = int(box->GetSelectedData());
            onReleasingSwapChain(NULL);
            onResizedSwapChain(DXUTGetD3D10Device(), DXUTGetDXGISwapChain(), DXUTGetDXGIBackBufferSurfaceDesc(), NULL);
            break;
        }
        case IDC_HDR:
            if (event == EVENT_CHECKBOX_CHANGED) {
                onReleasingSwapChain(NULL);
                onResizedSwapChain(DXUTGetD3D10Device(), DXUTGetDXGISwapChain(), DXUTGetDXGIBackBufferSurfaceDesc(), NULL);
            }
            break;
        case IDC_SEPARATE_SPECULARS:
            if (event == EVENT_CHECKBOX_CHANGED)
                setupMainEffect();
            break;
        case IDC_PROFILE:
            if (event == EVENT_CHECKBOX_CHANGED)
                timer->setEnabled(mainHud.GetCheckBox(IDC_PROFILE)->GetChecked());
            break;
        case IDC_SSS: {
            bool sssEnabled = mainHud.GetCheckBox(IDC_SSS)->GetChecked();
            V(mainEffect->GetVariableByName("sssEnabled")->AsScalar()->SetBool(sssEnabled));
            break;
        }
        case IDC_NSAMPLES: {
            int min, max;
            mainHud.GetSlider(IDC_NSAMPLES)->GetRange(min, max);
            float value = 33.0f * float(mainHud.GetSlider(IDC_NSAMPLES)->GetValue()) / (max - min);

            wstringstream s;
            s << L"Samples: " << std::max(3, 2 * (int(value) / 2) + 1);
            mainHud.GetStatic(IDC_NSAMPLES_LABEL)->SetText(s.str().c_str());

            setupSSS(DXUTGetD3D10Device(), DXUTGetDXGIBackBufferSurfaceDesc());
            break;
        }
        case IDC_FOLLOW_SURFACE:
            if (event == EVENT_CHECKBOX_CHANGED) {
                onReleasingSwapChain(NULL);
                onResizedSwapChain(DXUTGetD3D10Device(), DXUTGetDXGISwapChain(), DXUTGetDXGIBackBufferSurfaceDesc(), NULL);
            }
            break;
        case IDC_SSSWIDTH: {
            float value = updateSlider(mainHud, IDC_SSSWIDTH, IDC_SSSWIDTH_LABEL, 0.025f, L"SSS Width: "); 
            separableSSS->setWidth(value);
            V(mainEffect->GetVariableByName("sssWidth")->AsScalar()->SetFloat(value));
            break;
        }
        case IDC_STRENGTH_R:
        case IDC_STRENGTH_G:
        case IDC_STRENGTH_B: {
            D3DXVECTOR3 strength; 
            strength.x = updateSlider(mainHud, IDC_STRENGTH_R, IDC_STRENGTH_R_LABEL, 1.0f, L"R: ");
            strength.y = updateSlider(mainHud, IDC_STRENGTH_G, IDC_STRENGTH_G_LABEL, 1.0f, L"G: "); 
            strength.z = updateSlider(mainHud, IDC_STRENGTH_B, IDC_STRENGTH_B_LABEL, 1.0f, L"B: ");  
            separableSSS->setStrength(strength);
            break;
        }
        case IDC_FALLOFF_R:
        case IDC_FALLOFF_G:
        case IDC_FALLOFF_B: {
            D3DXVECTOR3 falloff; 
            falloff.x = updateSlider(mainHud, IDC_FALLOFF_R, IDC_FALLOFF_R_LABEL, 1.0f, L"R: ");
            falloff.y = updateSlider(mainHud, IDC_FALLOFF_G, IDC_FALLOFF_G_LABEL, 1.0f, L"G: "); 
            falloff.z = updateSlider(mainHud, IDC_FALLOFF_B, IDC_FALLOFF_B_LABEL, 1.0f, L"B: ");  
            separableSSS->setFalloff(falloff);
            break;
        }
        case IDC_TRANSLUCENCY: {
            float value = updateSlider(mainHud, IDC_TRANSLUCENCY, IDC_TRANSLUCENCY_LABEL, 1.0f, L"Translucency: "); 
            V(mainEffect->GetVariableByName("translucency")->AsScalar()->SetFloat(value));
            break;
        }
        case IDC_SPEC_INTENSITY: {
            float value = updateSlider(secondaryHud, IDC_SPEC_INTENSITY, IDC_SPEC_INTENSITY_LABEL, 4.0f, L"Spec. Intensity: "); 
            V(mainEffect->GetVariableByName("specularIntensity")->AsScalar()->SetFloat(value));
            break;
        }
        case IDC_SPEC_ROUGHNESS: {
            float value = updateSlider(secondaryHud, IDC_SPEC_ROUGHNESS, IDC_SPEC_ROUGHNESS_LABEL, 1.0f, L"Spec. Rough.: "); 
            V(mainEffect->GetVariableByName("specularRoughness")->AsScalar()->SetFloat(value));
            break;
        }
        case IDC_SPEC_FRESNEL: {
            float value = updateSlider(secondaryHud, IDC_SPEC_FRESNEL, IDC_SPEC_FRESNEL_LABEL, 1.0f, L"Spec. Fresnel: "); 
            V(mainEffect->GetVariableByName("specularFresnel")->AsScalar()->SetFloat(value));
            break;
        }
        case IDC_BUMPINESS: {
            float value = updateSlider(secondaryHud, IDC_BUMPINESS, IDC_BUMPINESS_LABEL, 1.0f, L"Bumpiness: "); 
            V(mainEffect->GetVariableByName("bumpiness")->AsScalar()->SetFloat(value));
            break;
        }
        case IDC_EXPOSURE: {
            float value = updateSlider(secondaryHud, IDC_EXPOSURE, IDC_EXPOSURE_LABEL, 5.0f, L"Exposure: "); 
            bloom->setExposure(value);
            filmGrain->setExposure(value);
            break;
        }
        case IDC_ENVMAP: {
            float value = updateSlider(secondaryHud, IDC_ENVMAP, IDC_ENVMAP_LABEL, 1.0f, L"Enviroment: "); 
            for (int i = 0; i < 3; i++)
                skyDome[i]->setIntensity(value);
            break;
        }
        case IDC_AMBIENT: {
            float value = updateSlider(secondaryHud, IDC_AMBIENT, IDC_AMBIENT_LABEL, 1.0f, L"Ambient: ");
            V(mainEffect->GetVariableByName("ambient")->AsScalar()->SetFloat(value));
            break;
        }
        case IDC_FOCUS_DISTANCE: {
            updateSlider(secondaryHud, IDC_FOCUS_DISTANCE, IDC_FOCUS_DISTANCE_LABEL, 2.0f, L"Focus dist.: ");
            break;
        }
        case IDC_FOCUS_RANGE: {
            float value = updateSlider(secondaryHud, IDC_FOCUS_RANGE, IDC_FOCUS_RANGE_LABEL, 1.0f, L"Focus Range: ");
            dof->setFocusRange(pow(value, 5.0f));
            break;
        }
        case IDC_FOCUS_FALLOFF: {
            float value = updateSlider(secondaryHud, IDC_FOCUS_FALLOFF, IDC_FOCUS_FALLOFF_LABEL, 20.0f, L"Focus Falloff: ");
            dof->setFocusFalloff(D3DXVECTOR2(value, value));
            break;
        }
        case IDC_CAMERA_BUTTON:
        case IDC_LIGHT1_BUTTON:
        case IDC_LIGHT1_BUTTON + 1:
        case IDC_LIGHT1_BUTTON + 2:
        case IDC_LIGHT1_BUTTON + 3:
        case IDC_LIGHT1_BUTTON + 4:
            object = Object(id - IDC_CAMERA_BUTTON);
            for (int i = 0; i < N_LIGHTS + 1; i++)
                secondaryHud.GetButton(IDC_CAMERA_BUTTON + i)->SetEnabled(i != int(object));
            break;
        case IDC_LIGHT1:
        case IDC_LIGHT1 + 1:
        case IDC_LIGHT1 + 2:
        case IDC_LIGHT1 + 3:
        case IDC_LIGHT1 + 4: {
            float value = updateSlider(secondaryHud, id, IDC_LIGHT1_LABEL + id - IDC_LIGHT1, 5.0f, L""); 
            lights[id - IDC_LIGHT1].color = value * D3DXVECTOR3(1.0, 1.0, 1.0);
            break;
        }
    }
}


void CALLBACK createTextureFromFile(ID3D10Device *device, char *filename, ID3D10ShaderResourceView **shaderResourceView, void *context, bool srgb) {
    HRESULT hr;

    wstringstream s;
    s << ((wchar_t *) context) << "\\" << filename;

    D3DX10_IMAGE_LOAD_INFO loadInfo;
    loadInfo.BindFlags = D3D10_BIND_SHADER_RESOURCE;

    if (srgb) {
        loadInfo.Filter = D3DX10_FILTER_POINT | D3DX10_FILTER_SRGB_IN;
        
        D3DX10_IMAGE_INFO info;
        V(D3DX10GetImageInfoFromResource(GetModuleHandle(NULL), s.str().c_str(), NULL, &info, NULL));
        loadInfo.Format = MAKE_SRGB(info.Format);
    }

    V(D3DX10CreateShaderResourceViewFromResource(device, GetModuleHandle(NULL), s.str().c_str(), &loadInfo, NULL, shaderResourceView, NULL));
}


void loadMesh(CDXUTSDKMesh &mesh, ID3D10Device *device, const wstring &name, const wstring &path) {
    HRESULT hr;

    HRSRC src = FindResource(GetModuleHandle(NULL), name.c_str(), RT_RCDATA);
    HGLOBAL res = LoadResource(GetModuleHandle(NULL), src);
    UINT size = SizeofResource(GetModuleHandle(NULL), src);
    LPBYTE data = (LPBYTE) LockResource(res);

    SDKMESH_CALLBACKS10 callbacks;
    ZeroMemory(&callbacks, sizeof(SDKMESH_CALLBACKS10));  
    callbacks.pCreateTextureFromFile = &createTextureFromFile;
    callbacks.pContext = (void *) path.c_str();

    V(mesh.Create(device, data, size, true, true, &callbacks));
}


HRESULT CALLBACK onCreateDevice(ID3D10Device *device, const DXGI_SURFACE_DESC *desc, void *context) {
    HRESULT hr;

    V_RETURN(dialogResourceManager.OnD3D10CreateDevice(device));

    timer = new Timer(device);
    timer->setEnabled(mainHud.GetCheckBox(IDC_PROFILE)->GetChecked());

    V_RETURN(D3DX10CreateFont(device, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &font));
    V_RETURN(D3DX10CreateSprite(device, 512, &sprite));
    txtHelper = new CDXUTTextHelper(NULL, NULL, font, sprite, 15);

    loadMesh(mesh, device, L"Head\\Head.sdkmesh", L"Head");

    D3DX10_IMAGE_LOAD_INFO loadInfo;
    ZeroMemory(&loadInfo, sizeof(D3DX10_IMAGE_LOAD_INFO));
    loadInfo.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    V_RETURN(D3DX10CreateShaderResourceViewFromResource(device, GetModuleHandle(NULL), L"Head\\SpecularAOMap.dds", &loadInfo, NULL, &specularAOSRV, NULL));

    ZeroMemory(&loadInfo, sizeof(D3DX10_IMAGE_LOAD_INFO));
    loadInfo.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    loadInfo.MipLevels = 1;
    loadInfo.Format = DXGI_FORMAT_R8_UNORM;
    V_RETURN(D3DX10CreateShaderResourceViewFromResource(device, GetModuleHandle(NULL), L"BeckmannMap.dds", &loadInfo, NULL, &beckmannSRV, NULL));

    ZeroMemory(&loadInfo, sizeof(D3DX10_IMAGE_LOAD_INFO));
    loadInfo.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    V_RETURN(D3DX10CreateShaderResourceViewFromResource(device, GetModuleHandle(NULL), L"Enviroment\\StPeters\\IrradianceMap.dds", &loadInfo, NULL, &irradianceSRV[0], NULL));
    V_RETURN(D3DX10CreateShaderResourceViewFromResource(device, GetModuleHandle(NULL), L"Enviroment\\Grace\\IrradianceMap.dds", &loadInfo, NULL, &irradianceSRV[1], NULL));
    V_RETURN(D3DX10CreateShaderResourceViewFromResource(device, GetModuleHandle(NULL), L"Enviroment\\Eucalyptus\\IrradianceMap.dds", &loadInfo, NULL, &irradianceSRV[2], NULL));

    setupMainEffect();
    
    const D3D10_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 }
    };
    UINT numElements = sizeof(layout) / sizeof(D3D10_INPUT_ELEMENT_DESC);

    D3D10_PASS_DESC passDesc;
    V_RETURN(mainEffect->GetTechniqueByName("Render")->GetPassByIndex(0)->GetDesc(&passDesc));
    V_RETURN(device->CreateInputLayout(layout, numElements, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &vertexLayout));

    device->IASetInputLayout(vertexLayout);

    for (int i = 0; i < N_LIGHTS; i++) {
        lights[i].fov = 45.0f * D3DX_PI / 180.f;
        lights[i].falloffWidth = 0.05f;
        lights[i].attenuation = 1.0f / 128.0f;
        lights[i].farPlane = 10.0f;
        lights[i].bias = -0.01f;

        lights[i].camera.setDistance(2.0);
        lights[i].camera.setProjection(lights[i].fov, 1.0f, 0.1f, lights[i].farPlane);
        lights[i].color = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
        lights[i].shadowMap = new ShadowMap(device, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
    }

    splashScreen = new SplashScreen(device, sprite);
    skyDome[0] = new SkyDome(device, L"Enviroment\\StPeters", 0.0f);
    skyDome[1] = new SkyDome(device, L"Enviroment\\Grace", 0.0f);
    skyDome[2] = new SkyDome(device, L"Enviroment\\Eucalyptus", 0.0f);

    V(mainEffect->GetTechniqueByName("AddSpecular")->GetPassByIndex(0)->GetDesc(&passDesc));
    quad = new Quad(device, passDesc);

    ShadowMap::init(device);
    Fade::init(device);

    return S_OK;
}


void CALLBACK onDestroyDevice(void *context) {
    dialogResourceManager.OnD3D10DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();

    SAFE_DELETE(timer);
    SAFE_RELEASE(font);
    SAFE_RELEASE(sprite);
    SAFE_DELETE(txtHelper);

    mesh.Destroy();
    SAFE_RELEASE(specularAOSRV);
    SAFE_RELEASE(beckmannSRV);
    SAFE_RELEASE(mainEffect);
    SAFE_RELEASE(vertexLayout);

    for (int i = 0; i < N_LIGHTS; i++)
        SAFE_DELETE(lights[i].shadowMap);

    SAFE_DELETE(splashScreen);

    for (int i = 0; i < 3; i++) {
        SAFE_DELETE(skyDome[i]);
        SAFE_RELEASE(irradianceSRV[i]);
    }

    SAFE_DELETE(quad);

    ShadowMap::release();
    Fade::release();
}


HRESULT initAudio() {
    HRESULT hr;

    UINT32 flags = 0;
    #if defined(DEBUG) | defined(_DEBUG)
    flags |= XAUDIO2_DEBUG_ENGINE;
    #endif

    if (FAILED(hr = XAudio2Create(&xaudio, flags)))
        return hr;

    if (FAILED(hr = xaudio->CreateMasteringVoice(&masteringVoice)))
        return hr;

    CWaveFile wav;
    // I modified CWaveFile::Open so that it can open xWMA files
    V_RETURN(wav.Open(L"Music.xwm", NULL, WAVEFILE_READ));
    WAVEFORMATEX *format = wav.GetFormat();
    DWORD size = wav.GetSize();

    waveData.resize(size);
    V_RETURN(wav.Read(&waveData.front(), size, &size));
    V_RETURN(xaudio->CreateSourceVoice(&sourceVoice, format));

    XAUDIO2_BUFFER buffer = {0};
    buffer.pAudioData = &waveData.front();
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.AudioBytes = size;


    if (mmioSeek(wav.m_hmmio, 0, SEEK_SET) == -1)
        return E_FAIL;

    MMCKINFO ckinfo;
    if (mmioDescend(wav.m_hmmio, &ckinfo, NULL, 0) != 0)
        return E_FAIL;

    ckinfo.ckid = mmioFOURCC( 'd', 'p', 'd', 's' );
    if (mmioDescend(wav.m_hmmio, &ckinfo, NULL, MMIO_FINDCHUNK) != 0)
        return E_FAIL;

    XAUDIO2_BUFFER_WMA packetWMA;
    packetWMA.PacketCount = ckinfo.cksize / sizeof(UINT32);
    dpdsData.resize(packetWMA.PacketCount);
    packetWMA.pDecodedPacketCumulativeBytes = &dpdsData.front();

    if (unsigned(mmioRead(wav.m_hmmio, (HPSTR) packetWMA.pDecodedPacketCumulativeBytes, ckinfo.cksize)) != ckinfo.cksize)
        return E_FAIL;

    V_RETURN(sourceVoice->SubmitSourceBuffer(&buffer, &packetWMA))

    return S_OK;
}


void releaseAudio() {
    if (sourceVoice) sourceVoice->DestroyVoice(); sourceVoice = NULL;
    if (masteringVoice) masteringVoice->DestroyVoice(); masteringVoice = NULL;
    SAFE_RELEASE(xaudio);
    waveData.clear();
    dpdsData.clear();
}


void initApp() {
    mainHud.Init(&dialogResourceManager);
    mainHud.SetCallback(onGUIEvent);

    secondaryHud.Init(&dialogResourceManager);
    secondaryHud.SetCallback(onGUIEvent);

    helpHud.Init(&dialogResourceManager);
    helpHud.SetCallback(onGUIEvent);

    /**
     * Create main hud (the one on the right)
     */
    int iY = 10;
    mainHud.AddButton(IDC_SSS_HELP, L"Help", 35, iY, HUD_WIDTH, 22, VK_F1);
    iY += 15;

    mainHud.AddButton(IDC_TOGGLEFULLSCREEN, L"Toggle Fullscreen", 35, iY += 24, HUD_WIDTH, 22);
    mainHud.AddComboBox(IDC_MSAA, 35, iY += 24, HUD_WIDTH, 22);

    iY += 15;
    mainHud.AddCheckBox(IDC_HDR, L"HDR", 35, iY += 24, HUD_WIDTH, 22, true);
    mainHud.AddCheckBox(IDC_SEPARATE_SPECULARS, L"Separate Speculars", 35, iY += 24, HUD_WIDTH, 22, true);
    mainHud.AddCheckBox(IDC_PROFILE, L"Profile", 35, iY += 24, HUD_WIDTH, 22, false);

    iY += 15;
    mainHud.AddCheckBox(IDC_SSS, L"SSS Rendering", 35, iY += 24, HUD_WIDTH, 22, true);
    mainHud.AddCheckBox(IDC_FOLLOW_SURFACE, L"SSS Follow Surface", 35, iY += 24, HUD_WIDTH, 22, true);

    iY += 15;
    mainHud.AddStatic(IDC_NSAMPLES_LABEL, L"Samples: 17", 35, iY += 24, HUD_WIDTH, 22);
    mainHud.AddSlider(IDC_NSAMPLES, 35, iY += 24, HUD_WIDTH, 22, 0, 100, int((17.0f / 33.0f) * 100.0f));
    mainHud.AddStatic(IDC_SSSWIDTH_LABEL, L"SSS Width: 0.012", 35, iY += 24, HUD_WIDTH, 22);
    mainHud.AddSlider(IDC_SSSWIDTH, 35, iY += 24, HUD_WIDTH, 22, 0, 100, int((0.012f / 0.025f) * 100.0f));

    iY += 15;
    mainHud.AddStatic(IDC_STRENGTH_LABEL, L"SSS Strength", 35, iY += 24, HUD_WIDTH, 22);
    mainHud.AddStatic(IDC_STRENGTH_R_LABEL, L"R: 0.48", 35, iY += 24, 50, 22);
    mainHud.AddSlider(IDC_STRENGTH_R, 80, iY, HUD_WIDTH - 45, 22, 0, 100, int(0.48f * 100.0f));
    mainHud.AddStatic(IDC_STRENGTH_G_LABEL, L"G: 0.41", 35, iY += 24, 50, 22);
    mainHud.AddSlider(IDC_STRENGTH_G, 80, iY, HUD_WIDTH - 45, 22, 0, 100, int(0.41f * 100.0f));
    mainHud.AddStatic(IDC_STRENGTH_B_LABEL, L"B: 0.28", 35, iY += 24, 50, 22);
    mainHud.AddSlider(IDC_STRENGTH_B, 80, iY, HUD_WIDTH - 45, 22, 0, 100, int(0.28f * 100.0f));

    iY += 15;
    mainHud.AddStatic(IDC_FALLOFF_LABEL, L"SSS Falloff", 35, iY += 24, HUD_WIDTH, 22);
    mainHud.AddStatic(IDC_FALLOFF_R_LABEL, L"R: 1.0", 35, iY += 24, 50, 22);
    mainHud.AddSlider(IDC_FALLOFF_R, 80, iY, HUD_WIDTH - 45, 22, 0, 100, int(1.0f * 100.0f));
    mainHud.AddStatic(IDC_FALLOFF_G_LABEL, L"G: 0.37", 35, iY += 24, 50, 22);
    mainHud.AddSlider(IDC_FALLOFF_G, 80, iY, HUD_WIDTH - 45, 22, 0, 100, int(0.37f * 100.0f));
    mainHud.AddStatic(IDC_FALLOFF_B_LABEL, L"B: 0.3", 35, iY += 24, 50, 22);
    mainHud.AddSlider(IDC_FALLOFF_B, 80, iY, HUD_WIDTH - 45, 22, 0, 100, int(0.3f * 100.0f));

    iY += 15;
    mainHud.AddStatic(IDC_TRANSLUCENCY_LABEL, L"Translucency: 0.83", 35, iY += 24, HUD_WIDTH, 22);
    mainHud.AddSlider(IDC_TRANSLUCENCY, 35, iY += 24, HUD_WIDTH, 22, 0, 100, int(0.83f * 100.0f));

    /**
     * Create the speculars and light step hud (the one on the left)
     */
    iY = 0;
    int iX = 10;

    secondaryHud.AddStatic(IDC_SPEC_INTENSITY_LABEL, L"Spec. Intensity: 1.88", iX, iY, HUD_WIDTH2, 22);
    secondaryHud.AddSlider(IDC_SPEC_INTENSITY, iX, iY += 24, HUD_WIDTH2, 22, 0, 100, int((1.88f / 4.0f) * 100.0f));
    secondaryHud.AddStatic(IDC_SPEC_ROUGHNESS_LABEL, L"Spec. Rough.: 0.3", iX, iY += 24, HUD_WIDTH2, 22);
    secondaryHud.AddSlider(IDC_SPEC_ROUGHNESS, iX, iY += 24, HUD_WIDTH2, 22, 0, 100, int(0.3f * 100.0f));
    secondaryHud.AddStatic(IDC_SPEC_FRESNEL_LABEL, L"Spec. Fresnel: 0.82", iX, iY += 24, HUD_WIDTH2, 22);
    secondaryHud.AddSlider(IDC_SPEC_FRESNEL, iX, iY += 24, HUD_WIDTH2, 22, 0, 100, int(0.82f * 100.0f));
    iY += 15;

    secondaryHud.AddStatic(IDC_BUMPINESS_LABEL, L"Bumpiness: 0.9", iX, iY += 24, HUD_WIDTH2, 22);
    secondaryHud.AddSlider(IDC_BUMPINESS, iX, iY += 24, HUD_WIDTH2, 22, 0, 100, int(0.9f * 100.0f));
    iY += 15;

    secondaryHud.AddStatic(IDC_EXPOSURE_LABEL, L"Exposure: 2.0", iX, iY += 24, HUD_WIDTH2, 22);
    secondaryHud.AddSlider(IDC_EXPOSURE, iX, iY += 24, HUD_WIDTH2, 22, 0, 100, int((2.0f / 5.0f) * 100.0f));

    secondaryHud.AddStatic(IDC_ENVMAP_LABEL, L"Env. Map: 0", iX, iY += 24, HUD_WIDTH2, 22);
    secondaryHud.AddSlider(IDC_ENVMAP, iX, iY += 24, HUD_WIDTH2, 22, 0, 100, int(0.0f * 100.0f));

    secondaryHud.AddStatic(IDC_AMBIENT_LABEL, L"Ambient: 0", iX, iY += 24, HUD_WIDTH2, 22);
    secondaryHud.AddSlider(IDC_AMBIENT, iX, iY += 24, HUD_WIDTH2, 22, 0, 100, int(0.0f * 100.0f));

    secondaryHud.AddStatic(IDC_FOCUS_DISTANCE_LABEL, L"Focus Dist.: 0.55", iX, iY += 24, HUD_WIDTH2, 22);
    secondaryHud.AddSlider(IDC_FOCUS_DISTANCE, iX, iY += 24, HUD_WIDTH2, 22, 0, 100, int((0.55f / 2.0f) * 100.0f));

    secondaryHud.AddStatic(IDC_FOCUS_RANGE_LABEL, L"Focus Range: 0.86", iX, iY += 24, HUD_WIDTH2, 22);
    secondaryHud.AddSlider(IDC_FOCUS_RANGE, iX, iY += 24, HUD_WIDTH2, 22, 0, 100, int((0.86f / 1.0f) * 100.0f));

    secondaryHud.AddStatic(IDC_FOCUS_FALLOFF_LABEL, L"Focus Falloff: 10.0", iX, iY += 24, HUD_WIDTH2, 22);
    secondaryHud.AddSlider(IDC_FOCUS_FALLOFF, iX, iY += 24, HUD_WIDTH2, 22, 0, 100, int((10.0f / 20.0f) * 100.0f));

    int top = iY - 24 * 3;
    iX += HUD_WIDTH2 + 20;

    iY = top;
    secondaryHud.AddButton(IDC_CAMERA_BUTTON, L"Camera", iX, iY += 24, 45, 22);
    secondaryHud.GetButton(IDC_CAMERA_BUTTON)->SetEnabled(false);
    iX += 45 + 20;

    for (int i = 0; i < N_LIGHTS; i++) {
        iY = top;

        wstringstream s;
        s << L"Light " << (i + 1); 
        secondaryHud.AddButton(IDC_LIGHT1_BUTTON + i, s.str().c_str(), iX, iY += 24, 45, 22);
        secondaryHud.AddStatic(IDC_LIGHT1_LABEL + i, L"0", iX, iY += 24, 45, 22);
        secondaryHud.AddSlider(IDC_LIGHT1 + i, iX, iY += 24, 45, 22, 0, 100, 0);

        iX += 45 + 20;
    }

    /**
     * Create the help page
     */
    HRSRC src = FindResource(GetModuleHandle(NULL), L"Help.txt", RT_RCDATA);
    HGLOBAL res = LoadResource(GetModuleHandle(NULL), src);
    UINT size = SizeofResource(GetModuleHandle(NULL), src);
    CHAR *data = (CHAR *) LockResource(res);

    int n = MultiByteToWideChar(CP_ACP, 0, data, size, NULL, 0);
    WCHAR *wdata = new WCHAR[n + 1];
    MultiByteToWideChar(CP_ACP, 0, data, size, wdata, n);
    wdata[n] = 0;
    helpHud.AddStatic(IDC_HELP_TEXT, wdata, 0, 0, 1280, 720);
    delete [] wdata;

    helpHud.AddButton(IDC_HELP_CLOSE_BUTTON, L"Close", 0, 0, HUD_WIDTH, 22, VK_F1);
}


bool CALLBACK modifyDeviceSettings(DXUTDeviceSettings *settings, void *context) {
    settings->d3d10.AutoCreateDepthStencil = false;
    settings->d3d10.sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
    return true;
}


bool CALLBACK isDeviceAcceptable(UINT adapter, UINT output, D3D10_DRIVER_TYPE deviceType, DXGI_FORMAT format, bool windowed, void *context) {
    return true;
}


INT WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    // Enable run-time memory check for debug builds.
    #if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    #endif

    DXUTSetCallbackD3D10DeviceAcceptable(isDeviceAcceptable);
    DXUTSetCallbackD3D10DeviceCreated(onCreateDevice);
    DXUTSetCallbackD3D10DeviceDestroyed(onDestroyDevice);
    DXUTSetCallbackD3D10SwapChainResized(onResizedSwapChain);
    DXUTSetCallbackD3D10SwapChainReleasing(onReleasingSwapChain);
    DXUTSetCallbackD3D10FrameRender(onFrameRender);
    
    DXUTSetCallbackDeviceChanging(modifyDeviceSettings);
    DXUTSetCallbackMsgProc(msgProc);
    DXUTSetCallbackKeyboard(keyboardProc);
    DXUTSetCallbackFrameMove(onFrameMove);

    #if INTRO_BUILD == 1
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(initAudio())) {
        audioEnabled = false;
        releaseAudio();
    }
    #endif

    initApp();
    DXUTInit(true, true, L"-forcevsync:0");
    DXUTSetCursorSettings(true, true);
    DXUTCreateWindow(L"Separable SSS Demo");
    DXUTCreateDevice(true, 1280, 720);
    ShowWindow(DXUTGetHWNDDeviceWindowed(), SW_MAXIMIZE);
    DXUTMainLoop();

    #if INTRO_BUILD == 1
    releaseAudio();
    CoUninitialize();
    #endif

    return DXUTGetExitCode();
}
