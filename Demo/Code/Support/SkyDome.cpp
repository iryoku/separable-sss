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


#include <DXUT.h>
#include <sstream>
#include "SkyDome.h"
using namespace std;


SkyDome::SkyDome(ID3D10Device *device, const std::wstring &dir, float intensity)
        : device(device),
          intensity(intensity),
          angle(0.0f, 0.0f) {
    HRESULT hr;
    V(D3DX10CreateEffectFromResource(GetModuleHandle(NULL), L"SkyDome.fx", NULL, NULL, NULL, "fx_4_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, device, NULL, NULL, &effect, NULL, NULL));
    createMesh(dir);
}


SkyDome::~SkyDome() {
    SAFE_RELEASE(effect);
    mesh.Destroy();
}


void SkyDome::render(const D3DXMATRIX &view, const D3DXMATRIX &projection, float scale) {
    HRESULT hr;

    D3DXMATRIX world;
    D3DXMatrixScaling(&world, scale, scale, scale);

    D3DXMATRIX t;
    D3DXMatrixRotationX(&t, angle.y);
    world = t * world;

    D3DXMatrixRotationY(&t, angle.x);
    world = t * world;

    V(effect->GetVariableByName("world")->AsMatrix()->SetMatrix((D3DXMATRIX) world));
    V(effect->GetVariableByName("view")->AsMatrix()->SetMatrix((D3DXMATRIX) view));
    V(effect->GetVariableByName("projection")->AsMatrix()->SetMatrix((D3DXMATRIX) projection));
    V(effect->GetVariableByName("intensity")->AsScalar()->SetFloat(intensity));

    mesh.Render(device, effect->GetTechniqueByName("RenderSkyDome"), effect->GetVariableByName("skyTex")->AsShaderResource());
}


void SkyDome::setDirectory(const std::wstring &dir) {
    mesh.Destroy();
    createMesh(dir);
}


void SkyDome::createMesh(const std::wstring &dir) {
    HRESULT hr;

    HRSRC src = FindResource(GetModuleHandle(NULL), (dir + L"\\SkyDome.sdkmesh").c_str(), RT_RCDATA);
    HGLOBAL res = LoadResource(GetModuleHandle(NULL), src);
    UINT size = SizeofResource(GetModuleHandle(NULL), src);
    LPBYTE data = (LPBYTE) LockResource(res);

    SDKMESH_CALLBACKS10 callbacks;
    ZeroMemory(&callbacks, sizeof(SDKMESH_CALLBACKS10));
    callbacks.pCreateTextureFromFile = &createTextureFromFile;
    callbacks.pContext = (void *) dir.c_str();

    V(mesh.Create(device, data, size, true, true, &callbacks));
}


void CALLBACK SkyDome::createTextureFromFile(ID3D10Device* device, 
                                             char *filename, ID3D10ShaderResourceView **shaderResourceView,
                                             void *context,
                                             bool srgb) {
    if (string(filename) != "default-normalmap.dds") {
        HRESULT hr;

        wstringstream s;
        s << ((wchar_t *) context) << "\\" << filename;

        V(D3DX10CreateShaderResourceViewFromResource(device, GetModuleHandle(NULL), s.str().c_str(), NULL, NULL, shaderResourceView, NULL));
    } else {
        *shaderResourceView = (ID3D10ShaderResourceView *) ERROR_RESOURCE_VALUE;
    }
}
