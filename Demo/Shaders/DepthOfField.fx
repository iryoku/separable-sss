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


// Can be 13, 11, 9, 7 or 5
#define N_SAMPLES 13


float2 pixelSize;
float2 direction;
float blurWidth;
float focusDistance;
float focusRange;
float2 focusFalloff;

Texture2D blurredTex;
Texture2D depthTex;
Texture2D cocTex;


SamplerState PointSampler {
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

SamplerState LinearSampler {
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};


void PassVS(float4 position : POSITION,
            out float4 svposition : SV_POSITION,
            inout float2 texcoord : TEXCOORD0) {
    svposition = position;
}


float4 CoCPS(float4 position : SV_POSITION,
             float2 texcoord : TEXCOORD0) : SV_TARGET {
    float depth = depthTex.Sample(PointSampler, texcoord).r;
    if (abs(depth - focusDistance) > focusRange / 2.0) {
        if (depth - focusDistance > 0.0) {
            float t = saturate(abs(depth - focusDistance) - focusRange / 2.0);
            return saturate(t * focusFalloff.x); 
        } else {
            float t = saturate(abs(depth - focusDistance) - focusRange / 2.0);
            return saturate(t * focusFalloff.y);
        }
    } else
        return 0.0;
}


float4 BlurPS(float4 position : SV_POSITION,
              float2 texcoord : TEXCOORD0,
              uniform float2 step) : SV_TARGET {
    #if N_SAMPLES == 13
    float offsets[] = { -1.7688, -1.1984, -0.8694, -0.6151, -0.3957, -0.1940, 0.1940, 0.3957, 0.6151, 0.8694, 1.1984, 1.7688 };
    const float n = 12.0;
    #elif N_SAMPLES == 11
    float offsets[] = { -1.6906, -1.0968, -0.7479, -0.4728, -0.2299, 0.2299, 0.4728, 0.7479, 1.0968, 1.6906 };
    const float n = 10.0;
    #elif N_SAMPLES == 9
    float offsets[] = { -1.5932, -0.9674, -0.5895, -0.2822, 0.2822, 0.5895, 0.9674, 1.5932 };
    const float n = 8.0;
    #elif N_SAMPLES == 7
    float offsets[] = { -1.4652, -0.7916, -0.3661, 0.3661, 0.7916, 1.4652 };
    const float n = 6.0;
    #else
    float offsets[] = { -1.282, -0.524, 0.524, 1.282 };
    const float n = 4.0;
    #endif

    float CoC = cocTex.Sample(LinearSampler, texcoord).r;

    float4 color = blurredTex.Sample(LinearSampler, texcoord);
    float sum = 1.0;
    for (int i = 0; i < int(n); i++) {
        float tapCoC = cocTex.Sample(LinearSampler, texcoord + step * offsets[i] * CoC).r;
        float4 tap = blurredTex.Sample(LinearSampler, texcoord + step * offsets[i] * CoC);

        float contribution = tapCoC > CoC? 1.0f : tapCoC;
        color += contribution * tap;
        sum += contribution;
    }
    return color / sum;
}


DepthStencilState DisableDepthStencil {
    DepthEnable = FALSE;
    StencilEnable = FALSE;
};

BlendState NoBlending {
    AlphaToCoverageEnable = FALSE;
    BlendEnable[0] = FALSE;
};


technique10 CoC {
    pass CoC {
        SetVertexShader(CompileShader(vs_4_0, PassVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, CoCPS()));
        
        SetDepthStencilState(DisableDepthStencil, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}

technique10 Blur {
    pass Blur {
        SetVertexShader(CompileShader(vs_4_0, PassVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, BlurPS(pixelSize * blurWidth * direction)));
        
        SetDepthStencilState(DisableDepthStencil, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}
