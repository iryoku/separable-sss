/**
 * Copyright (C) 2012 Jorge Jimenez (jorge@iryoku.com)
 * Copyright (C) 2012 Diego Gutierrez (diegog@unizar.es)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the following disclaimer
 *       in the documentation and/or other materials provided with the 
 *       distribution:
 *
 *       "Uses Separable SSS. Copyright (C) 2012 by Jorge Jimenez and Diego
 *        Gutierrez."
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


#include <sstream>
#include "SeparableSSS.h"
using namespace std;


#pragma region Useful Macros from DXUT (copy-pasted here as we prefer this to be as self-contained as possible)
#if defined(DEBUG) || defined(_DEBUG)
#ifndef V
#define V(x) { hr = (x); if (FAILED(hr)) { DXTrace(__FILE__, (DWORD)__LINE__, hr, L#x, true); } }
#endif
#ifndef V_RETURN
#define V_RETURN(x) { hr = (x); if (FAILED(hr)) { return DXTrace(__FILE__, (DWORD)__LINE__, hr, L#x, true); } }
#endif
#else
#ifndef V
#define V(x) { hr = (x); }
#endif
#ifndef V_RETURN
#define V_RETURN(x) { hr = (x); if( FAILED(hr) ) { return hr; } }
#endif
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p) { if (p) { delete (p); (p) = NULL; } }
#endif
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if (p) { delete[] (p); (p) = NULL; } }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = NULL; } }
#endif
#pragma endregion

#pragma region This stuff is for loading headers from resources
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
#pragma endregion


SeparableSSS::SeparableSSS(ID3D10Device *device,
                           int width,
                           int height,
                           float fovy,
                           float sssWidth,
                           int nSamples,
                           bool stencilInitialized,
                           bool followSurface,
                           bool separateStrengthSource,
                           DXGI_FORMAT format) : device(device),
                           sssWidth(sssWidth),
                           nSamples(nSamples),
                           stencilInitialized(stencilInitialized),
                           strength(D3DXVECTOR3(0.48f, 0.41f, 0.28f)),
                           falloff(D3DXVECTOR3(1.0f, 0.37f, 0.3f)) {
    HRESULT hr;

    // Setup the defines for compiling the effect:
    vector<D3D10_SHADER_MACRO> defines;

    stringstream s;

    s << fovy;
    string fovyText = s.str();
    D3D10_SHADER_MACRO fovyMacro = { "SSSS_FOVY", fovyText.c_str() };
    defines.push_back(fovyMacro);

    s.str("");
    s << nSamples;
    string nSamplesText = s.str();
    D3D10_SHADER_MACRO nSamplesMacro = { "SSSS_N_SAMPLES", nSamplesText.c_str() };
    defines.push_back(nSamplesMacro);

    D3D10_SHADER_MACRO followSurfaceMacro = { "SSSS_FOLLOW_SURFACE", followSurface? "1" : "0" };
    defines.push_back(followSurfaceMacro);

    D3D10_SHADER_MACRO stencilInitializedMacro = { "STENCIL_INITIALIZED", stencilInitialized? "1" : "0" };
    defines.push_back(stencilInitializedMacro);

    if (separateStrengthSource) {
        D3D10_SHADER_MACRO strengthSourceMacro = { "SSSS_STREGTH_SOURCE", "(SSSSSample(strengthTex, texcoord).a)"};
        defines.push_back(strengthSourceMacro);
    }

    D3D10_SHADER_MACRO null = { NULL, NULL };
    defines.push_back(null);

    // Now, let's load the effect:
    ID3D10IncludeResource includeResource;
    V(D3DX10CreateEffectFromResource(GetModuleHandle(NULL), L"SeparableSSS.fx", NULL, &defines.front(), &includeResource, "fx_4_0", D3D10_SHADER_ENABLE_STRICTNESS | D3D10_SHADER_OPTIMIZATION_LEVEL3, 0, device, NULL, NULL, &effect, NULL, NULL));

    // Setup the fullscreen quad:
    D3D10_PASS_DESC desc;
    V(effect->GetTechniqueByName("SSS")->GetPassByName("SSSSBlurX")->GetDesc(&desc));
    quad = new Quad(device, desc);

    // Create the temporal render target:
    tmpRT = new RenderTarget(device, width, height, format);

    // Create some handles for techniques and variables:
    technique = effect->GetTechniqueByName("SSS");
    idVariable = effect->GetVariableByName("id")->AsScalar();
    sssWidthVariable = effect->GetVariableByName("sssWidth")->AsScalar();
    kernelVariable = effect->GetVariableByName("kernel")->AsVector();
    colorTexVariable = effect->GetVariableByName("colorTex")->AsShaderResource();
    depthTexVariable = effect->GetVariableByName("depthTex")->AsShaderResource();
    strengthTexVariable = effect->GetVariableByName("strengthTex")->AsShaderResource();

    // And finally, calculate the sample positions and weights:
    calculateKernel();
}


SeparableSSS::~SeparableSSS() {
    SAFE_DELETE(tmpRT);
    SAFE_RELEASE(effect);
    SAFE_DELETE(quad);
}


D3DXVECTOR3 SeparableSSS::gaussian(float variance, float r) {
    /**
     * We use a falloff to modulate the shape of the profile. Big falloffs
     * spreads the shape making it wider, while small falloffs make it
     * narrower.
     */
    D3DXVECTOR3 g;
    for (int i = 0; i < 3; i++) {
        float rr = r / (0.001f + falloff[i]);
        g[i] = exp((-(rr * rr)) / (2.0f * variance)) / (2.0f * 3.14f * variance);
    }
    return g;
}


D3DXVECTOR3 SeparableSSS::profile(float r) {
    /**
     * We used the red channel of the original skin profile defined in
     * [d'Eon07] for all three channels. We noticed it can be used for green
     * and blue channels (scaled using the falloff parameter) without
     * introducing noticeable differences and allowing for total control over
     * the profile. For example, it allows to create blue SSS gradients, which
     * could be useful in case of rendering blue creatures.
     */
    return  // 0.233f * gaussian(0.0064f, r) + /* We consider this one to be directly bounced light, accounted by the strength parameter (see @STRENGTH) */
               0.100f * gaussian(0.0484f, r) +
               0.118f * gaussian( 0.187f, r) +
               0.113f * gaussian( 0.567f, r) +
               0.358f * gaussian(  1.99f, r) +
               0.078f * gaussian(  7.41f, r);
} 


void SeparableSSS::calculateKernel() {
    HRESULT hr;

    const float RANGE = nSamples > 20? 3.0f : 2.0f;
    const float EXPONENT = 2.0f;

    kernel.resize(nSamples);

    // Calculate the offsets:
    float step = 2.0f * RANGE / (nSamples - 1);
    for (int i = 0; i < nSamples; i++) {
        float o = -RANGE + float(i) * step;
        float sign = o < 0.0f? -1.0f : 1.0f;
        kernel[i].w = RANGE * sign * abs(pow(o, EXPONENT)) / pow(RANGE, EXPONENT);
    }

    // Calculate the weights:
    for (int i = 0; i < nSamples; i++) {
        float w0 = i > 0? abs(kernel[i].w - kernel[i - 1].w) : 0.0f;
        float w1 = i < nSamples - 1? abs(kernel[i].w - kernel[i + 1].w) : 0.0f;
        float area = (w0 + w1) / 2.0f;
        D3DXVECTOR3 t = area * profile(kernel[i].w);
        kernel[i].x = t.x;
        kernel[i].y = t.y;
        kernel[i].z = t.z;
    }

    // We want the offset 0.0 to come first:
    D3DXVECTOR4 t = kernel[nSamples / 2];
    for (int i = nSamples / 2; i > 0; i--)
        kernel[i] = kernel[i - 1];
    kernel[0] = t;
    
    // Calculate the sum of the weights, we will need to normalize them below:
    D3DXVECTOR3 sum = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < nSamples; i++)
        sum += D3DXVECTOR3(kernel[i]);

    // Normalize the weights:
    for (int i = 0; i < nSamples; i++) {
        kernel[i].x /= sum.x;
        kernel[i].y /= sum.y;
        kernel[i].z /= sum.z;
    }

    // Tweak them using the desired strength. The first one is:
    //     lerp(1.0, kernel[0].rgb, strength)
    kernel[0].x = (1.0f - strength.x) * 1.0f + strength.x * kernel[0].x;
    kernel[0].y = (1.0f - strength.y) * 1.0f + strength.y * kernel[0].y;
    kernel[0].z = (1.0f - strength.z) * 1.0f + strength.z * kernel[0].z;

    // The others:
    //     lerp(0.0, kernel[0].rgb, strength)
    for (int i = 1; i < nSamples; i++) {
        kernel[i].x *= strength.x;
        kernel[i].y *= strength.y;
        kernel[i].z *= strength.z;
    }

    // Finally, set 'em!
    V(kernelVariable->SetFloatVectorArray((float *) &kernel.front(), 0, nSamples));
}


void SeparableSSS::go(ID3D10RenderTargetView *mainRTV,
                      ID3D10ShaderResourceView *mainSRV,
                      ID3D10ShaderResourceView *depthSRV,
                      ID3D10DepthStencilView *depthDSV,
                      ID3D10ShaderResourceView *stregthSRV,
                      int id) {
    HRESULT hr;

    // Save the state:
    SaveViewportsScope saveViewport(device);
    SaveRenderTargetsScope saveRenderTargets(device);
    SaveInputLayoutScope saveInputLayout(device);

    // Clear the temporal render target:
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    device->ClearRenderTargetView(*tmpRT, clearColor);

    // Clear the stencil buffer if it was not available, and thus one must be 
    // initialized on the fly:
    if (!stencilInitialized)
        device->ClearDepthStencilView(depthDSV, D3D10_CLEAR_STENCIL, 1.0, 0);

    // Set variables:
    V(idVariable->SetInt(id));
    V(sssWidthVariable->SetFloat(sssWidth));
    V(depthTexVariable->SetResource(depthSRV));
    V(strengthTexVariable->SetResource(stregthSRV));

    // Set input layout and viewport:
    quad->setInputLayout();
    tmpRT->setViewport();

    // Run the horizontal pass:
    V(colorTexVariable->SetResource(mainSRV));
    technique->GetPassByName("SSSSBlurX")->Apply(0);
    device->OMSetRenderTargets(1, *tmpRT, depthDSV);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);

    // And finish with the vertical one:
    V(colorTexVariable->SetResource(*tmpRT));
    technique->GetPassByName("SSSSBlurY")->Apply(0);
    device->OMSetRenderTargets(1, &mainRTV, depthDSV);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);
}


string SeparableSSS::getKernelCode() const {
    stringstream s;
    s << "float4 kernel[] = {" << "\r\n";
    for (int i = 0; i < nSamples; i++)
        s << "    float4(" << kernel[i].x << ", "
                           << kernel[i].y << ", "
                           << kernel[i].z << ", "
                           << kernel[i].w << ")," << "\r\n";
    s << "};" << "\r\n";
    return s.str();
}
