/**
 * Copyright (C) 2012 Jorge Jimenez (jorge@iryoku.com)
 * Copyright (C) 2012 Diego Gutierrez (diegog@unizar.es)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the following disclaimer
 *       in the documentation and/or other materials provided with the 
 *       distribution:
 *
 *       "Uses Separable SSS. Copyright (C) 2012 by Jorge Jimenez and Diego
 *        Gutierrez."
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


// Set the HLSL version:
#ifndef SMAA_HLSL_4
#define SSSS_HLSL_4 1
#endif

// Texture used to fecth the SSS strength:
Texture2D strengthTex;

// And include our header!
#include "SeparableSSS.h"


/**
 * If STENCIL_INITIALIZED is set to 1, the stencil buffer is expected to be
 * initialized with the pixels that need SSS processing, marked with some id
 * (see 'id' below).
 *
 * If set to 0, then the bound stencil buffer is expected to be cleared.
 * The stencil will be initialized in the first pass, to at least optimize
 * the second pass. When using MSAA, the stencil buffer will probably be
 * uninitialized.
 */
#ifndef STENCIL_INITIALIZED
#define STENCIL_INITIALIZED 0
#endif

/**
 * This specifies the stencil id we must apply subsurface scattering on.
 * If !STENCIL_INITIALIZED, it should be set to an unused value, as
 * pixels will be marked in the stencil buffer in the first pass to
 * optimize the second one.
 */
int id;

/**
 * See |SeparableSSS.h| for more details.
 */
float sssWidth;


/**
 * DepthStencilState's and company
 */
DepthStencilState BlurStencil {
    DepthEnable = FALSE;
    StencilEnable = TRUE;
    FrontFaceStencilFunc = EQUAL;
};

DepthStencilState InitStencil {
    DepthEnable = FALSE;
    StencilEnable = TRUE;
    FrontFaceStencilPass = REPLACE;
};

BlendState NoBlending {
    AlphaToCoverageEnable = FALSE;
    BlendEnable[0] = FALSE;
    BlendEnable[1] = FALSE;
};


/**
 * Input textures
 */
Texture2D colorTex;
Texture2D depthTex;


/**
 * Function wrappers
 */
void DX10_SSSSBlurVS(float4 position : POSITION,
                     out float4 svPosition : SV_POSITION,
                     inout float2 texcoord : TEXCOORD0) {
    SSSSBlurVS(position, svPosition, texcoord);
}

float4 DX10_SSSSBlurPS(float4 position : SV_POSITION,
                       float2 texcoord : TEXCOORD0,
                       uniform SSSSTexture2D colorTex,
                       uniform SSSSTexture2D depthTex,
                       uniform float sssWidth,
                       uniform float2 dir,
                       uniform bool initStencil) : SV_TARGET {
    return SSSSBlurPS(texcoord, colorTex, depthTex, sssWidth, dir, initStencil);
}


/**
 * Time for some techniques!
 */
technique10 SSS {
    pass SSSSBlurX {
        SetVertexShader(CompileShader(vs_4_0, DX10_SSSSBlurVS()));
        SetGeometryShader(NULL);

        #if STENCIL_INITIALIZED == 1
        SetPixelShader(CompileShader(ps_4_0, DX10_SSSSBlurPS(colorTex, depthTex, sssWidth, float2(1.0, 0.0), false)));
        SetDepthStencilState(BlurStencil, id);
        #else
        SetPixelShader(CompileShader(ps_4_0, DX10_SSSSBlurPS(colorTex, depthTex, sssWidth, float2(1.0, 0.0), true)));
        SetDepthStencilState(InitStencil, id);
        #endif

        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }

    pass SSSSBlurY {
        SetVertexShader(CompileShader(vs_4_0, DX10_SSSSBlurVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, DX10_SSSSBlurPS(colorTex, depthTex, sssWidth, float2(0.0, 1.0), false)));
        
        SetDepthStencilState(BlurStencil, id);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}
