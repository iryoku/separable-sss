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


#include <sstream>
#include "Bloom.h"
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


Bloom::Bloom(ID3D10Device *device, int width, int height, DXGI_FORMAT format, ToneMapOperator toneMapOperator, float exposure, float bloomThreshold, float bloomWidth, float bloomIntensity, float defocus)
        : device(device),
          width(width), 
          height(height),
          toneMapOperator(toneMapOperator),
          exposure(exposure),
          burnout(numeric_limits<float>::infinity()),
          bloomThreshold(bloomThreshold),
          bloomWidth(bloomWidth),
          bloomIntensity(bloomIntensity),
          defocus(defocus) {

    HRESULT hr;

    vector<D3D10_SHADER_MACRO> defines;

    stringstream s;
    s << int(toneMapOperator);
    string toneMapOperatorString = s.str();
    D3D10_SHADER_MACRO toneMapOperatorMacro = { "TONEMAP_OPERATOR", toneMapOperatorString.c_str() };
    defines.push_back(toneMapOperatorMacro);

    D3D10_SHADER_MACRO null = { NULL, NULL };
    defines.push_back(null);

    V(D3DX10CreateEffectFromResource(GetModuleHandle(NULL), L"Bloom.fx", NULL, &defines.front(), NULL, "fx_4_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, device, NULL, NULL, &effect, NULL, NULL));

    D3D10_PASS_DESC desc;
    V(effect->GetTechniqueByName("GlareDetection")->GetPassByIndex(0)->GetDesc(&desc));
    quad = new Quad(device, desc);

    glareRT = new RenderTarget(device, width / 2, height / 2, format);

    int base = 2;
    for (int i = 0; i < N_PASSES; i++) {
        tmpRT[i][0] = new RenderTarget(device, max(width / base, 1), max(height / base, 1), format);
        tmpRT[i][1] = new RenderTarget(device, max(width / base, 1), max(height / base, 1), format);
        base *= 2;
    }
}


Bloom::~Bloom() {
    SAFE_RELEASE(effect);
    SAFE_DELETE(quad);
    SAFE_DELETE(glareRT);
    for (int i = 0; i < N_PASSES; i++) {
        SAFE_DELETE(tmpRT[i][0]);
        SAFE_DELETE(tmpRT[i][1]);
    }
}


void Bloom::go(ID3D10ShaderResourceView *src, ID3D10RenderTargetView *dst) {
    HRESULT hr;

    SaveViewportsScope saveViewport(device);
    SaveRenderTargetsScope saveRenderTargets(device);
    SaveInputLayoutScope saveInputLayout(device);

    quad->setInputLayout();

    V(effect->GetVariableByName("exposure")->AsScalar()->SetFloat(exposure));
    V(effect->GetVariableByName("burnout")->AsScalar()->SetFloat(burnout));
    V(effect->GetVariableByName("bloomThreshold")->AsScalar()->SetFloat(bloomThreshold));
    V(effect->GetVariableByName("bloomWidth")->AsScalar()->SetFloat(bloomWidth));
    V(effect->GetVariableByName("bloomIntensity")->AsScalar()->SetFloat(bloomIntensity));
    V(effect->GetVariableByName("defocus")->AsScalar()->SetFloat(defocus));
    V(effect->GetVariableByName("finalTex")->AsShaderResource()->SetResource(src));

    if (bloomIntensity > 0.0f) {
        glareDetection();

        ID3D10ShaderResourceView *current = *glareRT;
        for (int i = 0; i < N_PASSES; i++) {
            D3DXVECTOR2 pixelSize = D3DXVECTOR2(1.0f / tmpRT[i][0]->getWidth(), 
                                                1.0f / tmpRT[i][0]->getHeight());
            V(effect->GetVariableByName("pixelSize")->AsVector()->SetFloatVector(pixelSize));

            tmpRT[i][0]->setViewport();

            horizontalBlur(current, *tmpRT[i][0]);
            verticalBlur(*tmpRT[i][0], *tmpRT[i][1]);

            current = *tmpRT[i][1];
        }

        combine(dst);
    } else {
        toneMap(dst);
    }
}


void Bloom::glareDetection() {
    HRESULT hr;

    glareRT->setViewport();

    D3DXVECTOR2 pixelSize = D3DXVECTOR2(1.0f / glareRT->getWidth(), 1.0f / glareRT->getHeight());
    V(effect->GetVariableByName("pixelSize")->AsVector()->SetFloatVector(pixelSize));
    V(effect->GetTechniqueByName("GlareDetection")->GetPassByIndex(0)->Apply(0));

    device->OMSetRenderTargets(1, *glareRT, NULL);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);
}


void Bloom::horizontalBlur(ID3D10ShaderResourceView *src, ID3D10RenderTargetView *dst) {
    HRESULT hr;

    V(effect->GetVariableByName("srcTex")->GetElement(0)->AsShaderResource()->SetResource(src));
    V(effect->GetVariableByName("direction")->AsVector()->SetFloatVector(D3DXVECTOR2(1.0f, 0.0f)));
    V(effect->GetTechniqueByName("Blur")->GetPassByIndex(0)->Apply(0));

    device->OMSetRenderTargets(1, &dst, NULL);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);
}


void Bloom::verticalBlur(ID3D10ShaderResourceView *src, ID3D10RenderTargetView *dst) {
    HRESULT hr;

    V(effect->GetVariableByName("srcTex")->GetElement(0)->AsShaderResource()->SetResource(src));
    V(effect->GetVariableByName("direction")->AsVector()->SetFloatVector(D3DXVECTOR2(0.0f, 1.0f)));
    V(effect->GetTechniqueByName("Blur")->GetPassByIndex(0)->Apply(0));

    device->OMSetRenderTargets(1, &dst, NULL);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);
}


void Bloom::combine(ID3D10RenderTargetView *dst) {
    HRESULT hr;

    D3D10_VIEWPORT viewport = Utils::viewportFromView(dst);
    device->RSSetViewports(1, &viewport);
    
    D3DXVECTOR2 pixelSize = D3DXVECTOR2(1.0f / viewport.Width, 1.0f / viewport.Height);

    V(effect->GetVariableByName("pixelSize")->AsVector()->SetFloatVector(pixelSize));
    for (int i = 0; i < N_PASSES; i++)
        V(effect->GetVariableByName("srcTex")->GetElement(i)->AsShaderResource()->SetResource(*tmpRT[i][1]));
    V(effect->GetTechniqueByName("Combine")->GetPassByIndex(0)->Apply(0));

    device->OMSetRenderTargets(1, &dst, NULL);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);
}


void Bloom::toneMap(ID3D10RenderTargetView *dst) {
    HRESULT hr;

    D3D10_VIEWPORT viewport = Utils::viewportFromView(dst);
    device->RSSetViewports(1, &viewport);

    V(effect->GetTechniqueByName("ToneMap")->GetPassByIndex(0)->Apply(0));

    device->OMSetRenderTargets(1, &dst, NULL);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);
}
