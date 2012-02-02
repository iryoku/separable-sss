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


#include <string>
#include "Animation.h"
#include "Fade.h"
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


ID3D10Device *Fade::device;
ID3D10BlendState *Fade::spriteBlendState;
ID3D10Effect *Fade::effect;
Quad *Fade::quad;


void Fade::init(ID3D10Device *device) {
    Fade::device = device;

    HRESULT hr;

    D3D10_BLEND_DESC blendDesc;
    ZeroMemory(&blendDesc, sizeof(D3D10_BLEND_DESC));
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.BlendEnable[0] = TRUE;
    blendDesc.SrcBlend = D3D10_BLEND_ZERO;
    blendDesc.DestBlend = D3D10_BLEND_SRC_ALPHA;
    blendDesc.BlendOp = D3D10_BLEND_OP_ADD;
    blendDesc.SrcBlendAlpha = D3D10_BLEND_ZERO;
    blendDesc.DestBlendAlpha = D3D10_BLEND_ZERO;
    blendDesc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
    blendDesc.RenderTargetWriteMask[0] = 0xf;
    device->CreateBlendState(&blendDesc, &spriteBlendState);

    string s = "float alpha;"
               "float4 VS(float4 position: POSITION, float2 texcoord: TEXCOORD0) : SV_POSITION { return position; }"
               "float4 PS() : SV_TARGET { return float4(0.0, 0.0, 0.0, alpha); }"
               "DepthStencilState DisableDepthStencil { DepthEnable = FALSE; StencilEnable = FALSE; };"
               "technique10 Black { pass Black {"
               "SetVertexShader(CompileShader(vs_4_0, VS())); SetGeometryShader(NULL); SetPixelShader(CompileShader(ps_4_0, PS()));"
               "SetDepthStencilState(DisableDepthStencil, 0);"
               "}}";
    V(D3DX10CreateEffectFromMemory(s.c_str(), s.length(), NULL, NULL, NULL, "fx_4_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, device, NULL, NULL, &effect, NULL, NULL));

    D3D10_PASS_DESC desc;
    V(effect->GetTechniqueByName("Black")->GetPassByIndex(0)->GetDesc(&desc));
    quad = new Quad(device, desc);
}


void Fade::release() {
    SAFE_RELEASE(spriteBlendState);
    SAFE_RELEASE(effect);
    SAFE_DELETE(quad);
}


void Fade::linear(float t, float in, float out, float inLength, float outLength) {
    go(Animation::linearFade(t, in, out, inLength, outLength));
}


void Fade::smooth(float t, float in, float out, float inLength, float outLength) {
    go(Animation::smoothFade(t, in, out, inLength, outLength));
}


void Fade::go(float fade) {
    device->OMSetBlendState(spriteBlendState, 0, 0xffffffff);
    HRESULT hr;
    if (fade < 1.0f) {
        V(effect->GetVariableByName("alpha")->AsScalar()->SetFloat(pow(fade, 2.2f)));
        V(effect->GetTechniqueByName("Black")->GetPassByIndex(0)->Apply(0));
        quad->setInputLayout();
        quad->draw();
    }
}
