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
#include "FilmGrain.h"
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


FilmGrain::FilmGrain(ID3D10Device *device, int width, int height, float noiseIntensity, float exposure)
        : device(device),
          width(width), 
          height(height),
          noiseIntensity(noiseIntensity),
          exposure(exposure) {

    HRESULT hr;

    V(D3DX10CreateEffectFromResource(GetModuleHandle(NULL), L"FilmGrain.fx", NULL, NULL, NULL, "fx_4_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, device, NULL, NULL, &effect, NULL, NULL));

    D3D10_PASS_DESC desc;
    V(effect->GetTechniqueByName("FilmGrain")->GetPassByIndex(0)->GetDesc(&desc));
    quad = new Quad(device, desc);

    D3DX10_IMAGE_LOAD_INFO loadInfo;
    ZeroMemory(&loadInfo, sizeof(D3DX10_IMAGE_LOAD_INFO));
    loadInfo.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    loadInfo.MipLevels = 1;
    loadInfo.Format = DXGI_FORMAT_R8_UNORM;
    V(D3DX10CreateShaderResourceViewFromResource(device, GetModuleHandle(NULL), L"Noise.dds", &loadInfo, NULL, &noiseSRV, NULL));
}


FilmGrain::~FilmGrain() {
    SAFE_RELEASE(effect);
    SAFE_DELETE(quad);
    SAFE_RELEASE(noiseSRV);
}


void FilmGrain::go(ID3D10ShaderResourceView *src, ID3D10RenderTargetView *dst, float t) {
    HRESULT hr;

    SaveViewportsScope saveViewport(device);
    SaveRenderTargetsScope saveRenderTargets(device);
    SaveInputLayoutScope saveInputLayout(device);

    quad->setInputLayout();

    D3DXVECTOR2 pixelSize = D3DXVECTOR2(1.0f / width, 1.0f / height);
    V(effect->GetVariableByName("pixelSize")->AsVector()->SetFloatVector(pixelSize));
    V(effect->GetVariableByName("noiseIntensity")->AsScalar()->SetFloat(noiseIntensity));
    V(effect->GetVariableByName("exposure")->AsScalar()->SetFloat(exposure));
    V(effect->GetVariableByName("t")->AsScalar()->SetFloat(t));
    V(effect->GetVariableByName("srcTex")->AsShaderResource()->SetResource(src));
    V(effect->GetVariableByName("noiseTex")->AsShaderResource()->SetResource(noiseSRV));

    D3D10_VIEWPORT viewport = Utils::viewportFromView(dst);
    device->RSSetViewports(1, &viewport);

    V(effect->GetTechniqueByName("FilmGrain")->GetPassByIndex(0)->Apply(0));

    device->OMSetRenderTargets(1, &dst, NULL);
    quad->draw();
    device->OMSetRenderTargets(0, NULL, NULL);
}
