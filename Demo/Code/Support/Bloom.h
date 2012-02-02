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


#ifndef BLOOMSHADER_H
#define BLOOMSHADER_H

#include "RenderTarget.h"
#include <dxgi.h>
#include <d3d10.h>
#include <d3dx10.h>
#include <dxerr.h>

class Bloom {
    public:
        enum ToneMapOperator { TONEMAP_LINEAR = 0, 
                               TONEMAP_EXPONENTIAL = 1,
                               TONEMAP_EXPONENTIAL_HSV = 2,
                               TONEMAP_REINHARD = 3,
                               TONEMAP_FILMIC = 4 };

        Bloom(ID3D10Device *device, int width, int height, DXGI_FORMAT format,
              ToneMapOperator toneMapOperator, float exposure,
              float bloomThreshold, float bloomWidth, float bloomIntensity,
              float defocus);
        ~Bloom();

        ToneMapOperator getToneMapOperator() const { return toneMapOperator; }

        void setExposure(float exposure) { this->exposure = exposure; }
        float getExposure() const { return exposure; }

        void setBurnout(float burnout) { this->burnout = burnout; }
        float getBurnout() const { return burnout; }

        void setBloomThreshold(float bloomThreshold) { this->bloomThreshold = bloomThreshold; }
        float getBloomThreshold() const { return bloomThreshold; }

        void setBloomWidth(float bloomWidth) { this->bloomWidth = bloomWidth; }
        float getBloomWidth() const { return bloomWidth; }

        void setBlooomIntensity(float bloomIntensity) { this->bloomIntensity = bloomIntensity; }
        float getBloomIntensity() const { return bloomIntensity; }

        void setDefocus(float defocus) { this->defocus = defocus; }
        float getDefocus() const { return defocus; }

        void go(ID3D10ShaderResourceView *src, ID3D10RenderTargetView *dst);

    private:        
        static const int N_PASSES = 6;

        void glareDetection();
        void horizontalBlur(ID3D10ShaderResourceView *src, ID3D10RenderTargetView *dst);
        void verticalBlur(ID3D10ShaderResourceView *src, ID3D10RenderTargetView *dst);
        void toneMap(ID3D10RenderTargetView *dst);
        void combine(ID3D10RenderTargetView *dst);

        ID3D10Device *device;
        int width, height;
        ToneMapOperator toneMapOperator;
        float exposure, burnout;
        float bloomThreshold, bloomWidth, bloomIntensity;
        float defocus;

        ID3D10Effect *effect;
        Quad *quad;
        RenderTarget *glareRT;
        RenderTarget *tmpRT[N_PASSES][2];
};

#endif
