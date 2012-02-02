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


#ifndef DEPTHOFFIELD_H
#define DEPTHOFFIELD_H

#include "RenderTarget.h"
#include <dxgi.h>
#include <d3d10.h>
#include <d3dx10.h>
#include <dxerr.h>

class DepthOfField {
    public:
        DepthOfField(ID3D10Device *device, int width, int height, DXGI_FORMAT format, float focusDistance, float focusRange, const D3DXVECTOR2 &focusFalloff, float blurWidth);
        ~DepthOfField();

        void setBlurWidth(float blurWidth) { this->blurWidth = blurWidth; }
        float getBlurWidth() const { return blurWidth; }

        void setFocusDistance(float focusDistance) { this->focusDistance = focusDistance; }
        float getFocusDistance() const { return focusDistance; }
        
        void setFocusRange(float focusRange) { this->focusRange = focusRange; }
        float getFocusRange() const { return focusRange; }

        void setFocusFalloff(const D3DXVECTOR2 &focusFalloff) { this->focusFalloff = focusFalloff; }
        D3DXVECTOR2 getFocusFalloff() const { return focusFalloff; }

        void go(ID3D10ShaderResourceView *src, ID3D10RenderTargetView *dst, ID3D10ShaderResourceView *depth);

    private:
        void horizontalBlur(ID3D10ShaderResourceView *src, ID3D10RenderTargetView *dst, float width);
        void verticalBlur(ID3D10ShaderResourceView *src, ID3D10RenderTargetView *dst, float width);
        void coc(ID3D10ShaderResourceView *src, ID3D10RenderTargetView *dst);

        ID3D10Device *device;
        int width, height;
        float blurWidth;
        float focusDistance;
        float focusRange;
        D3DXVECTOR2 focusFalloff;

        ID3D10Effect *effect;
        Quad *quad;
        RenderTarget *tmpRT;
        RenderTarget *cocRT;
};

#endif
