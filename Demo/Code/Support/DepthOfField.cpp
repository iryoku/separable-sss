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
#include "DepthOfField.h"
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


DepthOfField::DepthOfField(ID3D10Device *device, int width, int height, DXGI_FORMAT format, float focusDistance, float focusRange, const D3DXVECTOR2 &focusFalloff, float blurWidth)
        : device(device),
          width(width), 
          height(height),
          focusDistance(focusDistance),
          focusRange(focusRange),
          focusFalloff(focusFalloff),
          blurWidth(blurWidth) {

    HRESULT hr;

    V(D3DX10CreateEffectFromResource(GetModuleHandle(NULL), L"DepthOfField.fx", NULL, NULL, NULL, "fx_4_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, device, NULL, NULL, &effect, NULL, NULL));

    D3D10_PASS_DESC desc;
    V(effect->GetTechniqueByName("Blur")->GetPassByIndex(0)->GetDesc(&desc));
    quad = new Quad(device, desc);

    tmpRT = new RenderTarget(device, width, height, format);
    cocRT = new RenderTarget(device, width, height, DXGI_FORMAT_R8_UNORM);
}


DepthOfField::~DepthOfField() {
    SAFE_RELEASE(effect);
    SAFE_DELETE(quad);
    SAFE_DELETE(tmpRT);
    SAFE_DELETE(cocRT);
}


void DepthOfField::go(ID3D10ShaderResourceView *src, ID3D10RenderTargetView *dst, ID3D10ShaderResourceView *depth) {
    SaveViewportsScope saveViewport(device);
    SaveRenderTargetsScope saveRenderTargets(device);
    SaveInputLayoutScope saveInputLayout(device);

    quad->setInputLayout();
    
    coc(depth, *cocRT);
    horizontalBlur(src, *tmpRT, blurWidth);
    verticalBlur(*tmpRT, dst, blurWidth);
}


void DepthOfField::coc(ID3D10ShaderResourceView *src, ID3D10RenderTargetView *dst) {
    HRESULT hr;

    D3D10_VIEWPORT viewport = Utils::viewportFromView(dst);
    device->RSSetViewports(1, &viewport);
    
    V(effect->GetVariableByName("focusDistance")->AsScalar()->SetFloat(focusDistance));
    V(effect->GetVariableByName("focusRange")->AsScalar()->SetFloat(focusRange));
    V(effect->GetVariableByName("focusFalloff")->AsVector()->SetFloatVector(focusFalloff));
    V(effect->GetVariableByName("depthTex")->AsShaderResource()->SetResource(src));
    V(effect->GetTechniqueByName("CoC")->GetPassByIndex(0)->Apply(0));

    device->OMSetRenderTargets(1, &dst, NULL);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);
}

void DepthOfField::horizontalBlur(ID3D10ShaderResourceView *src, ID3D10RenderTargetView *dst, float width) {
    HRESULT hr;

    D3D10_VIEWPORT viewport = Utils::viewportFromView(dst);
    device->RSSetViewports(1, &viewport);
    
    D3DXVECTOR2 pixelSize = D3DXVECTOR2(1.0f / viewport.Width, 1.0f / viewport.Height);
    V(effect->GetVariableByName("pixelSize")->AsVector()->SetFloatVector(pixelSize));
    V(effect->GetVariableByName("direction")->AsVector()->SetFloatVector(D3DXVECTOR2(1.0f, 0.0f)));
    V(effect->GetVariableByName("blurWidth")->AsScalar()->SetFloat(width));
    V(effect->GetVariableByName("blurredTex")->AsShaderResource()->SetResource(src));
    V(effect->GetVariableByName("cocTex")->AsShaderResource()->SetResource(*cocRT));
    V(effect->GetTechniqueByName("Blur")->GetPassByIndex(0)->Apply(0));

    device->OMSetRenderTargets(1, &dst, NULL);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);
}

void DepthOfField::verticalBlur(ID3D10ShaderResourceView *src, ID3D10RenderTargetView *dst, float width) {
    HRESULT hr;

    D3D10_VIEWPORT viewport = Utils::viewportFromView(dst);
    device->RSSetViewports(1, &viewport);
    
    D3DXVECTOR2 pixelSize = D3DXVECTOR2(1.0f / viewport.Width, 1.0f / viewport.Height);
    V(effect->GetVariableByName("pixelSize")->AsVector()->SetFloatVector(pixelSize));
    V(effect->GetVariableByName("direction")->AsVector()->SetFloatVector(D3DXVECTOR2(0.0f, 1.0f)));
    V(effect->GetVariableByName("blurWidth")->AsScalar()->SetFloat(width));
    V(effect->GetVariableByName("blurredTex")->AsShaderResource()->SetResource(src));
    V(effect->GetVariableByName("cocTex")->AsShaderResource()->SetResource(*cocRT));
    V(effect->GetTechniqueByName("Blur")->GetPassByIndex(0)->Apply(0));

    device->OMSetRenderTargets(1, &dst, NULL);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);
}
