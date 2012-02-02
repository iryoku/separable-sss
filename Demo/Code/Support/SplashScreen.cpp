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
#include "SplashScreen.h"
#include "Animation.h"
using namespace std;


SplashScreen::SplashScreen(ID3D10Device *device, ID3DX10Sprite *sprite)
        : device(device), sprite(sprite) {
    HRESULT hr;

    D3DX10_IMAGE_LOAD_INFO loadInfo;
    ZeroMemory(&loadInfo, sizeof(D3DX10_IMAGE_LOAD_INFO));
    loadInfo.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    loadInfo.MipLevels = 1;
    loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    loadInfo.Filter = D3DX10_FILTER_POINT | D3DX10_FILTER_SRGB_IN;
    V(D3DX10CreateShaderResourceViewFromResource(device, GetModuleHandle(NULL), L"SmallTitles.dds", &loadInfo, NULL, &smallTitlesSRV, NULL));
    V(D3DX10CreateShaderResourceViewFromResource(device, GetModuleHandle(NULL), L"BigTitles.dds", &loadInfo, NULL, &bigTitlesSRV, NULL));
    V(D3DX10CreateShaderResourceViewFromResource(device, GetModuleHandle(NULL), L"Background.png", &loadInfo, NULL, &backgroundSRV, NULL));

    D3D10_BLEND_DESC blendDesc;
    ZeroMemory(&blendDesc, sizeof(D3D10_BLEND_DESC));
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.BlendEnable[0] = TRUE;
    blendDesc.SrcBlend = D3D10_BLEND_SRC_ALPHA;
    blendDesc.DestBlend = D3D10_BLEND_INV_SRC_ALPHA;
    blendDesc.BlendOp = D3D10_BLEND_OP_ADD;
    blendDesc.SrcBlendAlpha = D3D10_BLEND_ZERO;
    blendDesc.DestBlendAlpha = D3D10_BLEND_ZERO;
    blendDesc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
    blendDesc.RenderTargetWriteMask[0] = 0xf;
    V(device->CreateBlendState(&blendDesc, &spriteBlendState));
}


SplashScreen::~SplashScreen() {
    SAFE_RELEASE(smallTitlesSRV);
    SAFE_RELEASE(bigTitlesSRV);
    SAFE_RELEASE(backgroundSRV);
    SAFE_RELEASE(spriteBlendState);
}


void SplashScreen::intro(float t) {
    HRESULT hr;

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    device->ClearRenderTargetView(DXUTGetD3D10RenderTargetView(), clearColor);

    D3D10_VIEWPORT viewport;
    UINT numViewports = 1;
    device->RSGetViewports(&numViewports, &viewport);

    D3DX10_SPRITE s[7];
    s[0] = newSprite(backgroundSRV,  viewport,   0,    0, 0, 1, 1920,  1080, Animation::linearFade(t, 0.0f,  16.0f, 2.0f, 3.0f)); // Gradient
    s[1] = newSprite(smallTitlesSRV, viewport,   0,   45, 0, 7, 1024,    24, Animation::linearFade(t, 1.0f,  7.5f,  2.0f, 2.0f)); // Jorge Jimenez
    s[2] = newSprite(smallTitlesSRV, viewport,   0,  -15, 1, 7, 1024,    24, Animation::linearFade(t, 1.5f,  7.5f,  2.0f, 2.0f)); // Diego Gutierrez
    s[3] = newSprite(smallTitlesSRV, viewport,   0,  -95, 2, 7, 1024,    24, Animation::linearFade(t, 2.0f,  7.5f,  2.5f, 2.0f)); // Universidad de Zaragoza
    s[4] = newSprite(bigTitlesSRV,   viewport,   0,   20, 0, 1, 1174,   127, Animation::linearFade(t, 8.0f,  14.0f, 1.0f, 3.0f)); // SSSS
    s[5] = newSprite(smallTitlesSRV, viewport, -45,   30, 3, 7, 1024,    24, Animation::linearFade(t, 15.0f, 25.0f, 2.0f, 3.0f)); // It is our imperfections...
    s[6] = newSprite(smallTitlesSRV, viewport,  45,  -30, 4, 7, 1024,    24, Animation::linearFade(t, 18.0f, 25.0f, 2.0f, 3.0f)); // ...That makes us so perfect

    device->OMSetBlendState(spriteBlendState, 0, 0xffffffff);
    V(sprite->Begin(D3DX10_SPRITE_SAVE_STATE));
    V(sprite->DrawSpritesImmediate(s, 7, 0, 0));
    V(sprite->End());
}


void SplashScreen::outro(float t) {
    HRESULT hr;

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    device->ClearRenderTargetView(DXUTGetD3D10RenderTargetView(), clearColor);

    D3D10_VIEWPORT viewport;
    UINT numViewports = 1;
    device->RSGetViewports(&numViewports, &viewport);

    D3DX10_SPRITE s[3];
    s[0] = newSprite(smallTitlesSRV, viewport,   0,    0, 6, 7,  1024,  24, Animation::linearFade(t,  3.0f,  9.0f, 2.0f, 3.0f)); // To the memory of my Father
    s[1] = newSprite(bigTitlesSRV,   viewport,   0,   20, 0, 1,  1174, 127, Animation::linearFade(t, 10.0f, 19.0f, 1.0f, 3.0f)); // SSSS
    s[2] = newSprite(smallTitlesSRV, viewport,   0, -120, 5, 7,  1024,  24, Animation::linearFade(t, 10.0f, 17.0f, 1.0f, 3.0f)); // www.iryoku.com

    device->OMSetBlendState(spriteBlendState, 0, 0xffffffff);
    V(sprite->Begin(D3DX10_SPRITE_SAVE_STATE));
    V(sprite->DrawSpritesImmediate(s, 3, 0, 0));
    V(sprite->End());
}


D3DX10_SPRITE SplashScreen::newSprite(ID3D10ShaderResourceView *spriteSRV, const D3D10_VIEWPORT &viewport, 
                                      int x, int y, int i, int n, int width, int height, float alpha) {
    D3DXMATRIX scale, translation;
    D3DXMatrixScaling(&scale, 2.0f * width / viewport.Width, 2.0f * height / viewport.Height, 1.0f);
    D3DXMatrixTranslation(&translation, 2.0f * float(x) / viewport.Width, 2.0f * float(y) / viewport.Height, 0.0f);

    D3DX10_SPRITE s;
    s.ColorModulate = D3DXCOLOR(1.0f, 1.0f, 1.0f, pow(alpha, 2.2f));
    s.matWorld = scale * translation;
    s.pTexture = spriteSRV;
    s.TexCoord = D3DXVECTOR2(0.0f, float(i) / n);
    s.TexSize = D3DXVECTOR2(1.0f, 1.0f / n);
    s.TextureIndex = 0;
    return s;
}
