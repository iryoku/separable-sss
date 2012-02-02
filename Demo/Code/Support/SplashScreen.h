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


#ifndef SPLASHSCREEN_H
#define SPLASHSCREEN_H

#include "RenderTarget.h"

class SplashScreen {
    public:
        SplashScreen(ID3D10Device *device, ID3DX10Sprite *sprite);
        ~SplashScreen();

        void intro(float t);
        void outro(float t);
        bool introHasFinished(float t) const { return t > 28.0f; }
        bool outroHasFinished(float t) const { return t > 21.0f; }

    private:
        D3DX10_SPRITE newSprite(ID3D10ShaderResourceView *spriteSRV, const D3D10_VIEWPORT &viewport, 
                                int x, int y, int i, int n, int width, int height, float alpha);
        void renderGradient(float fade);

        ID3D10Device *device;

        ID3DX10Sprite *sprite;
        ID3D10ShaderResourceView *smallTitlesSRV;
        ID3D10ShaderResourceView *bigTitlesSRV;
        ID3D10ShaderResourceView *backgroundSRV;
        ID3D10BlendState *spriteBlendState;
};

#endif
