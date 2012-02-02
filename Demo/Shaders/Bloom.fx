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


#define N_PASSES 6

// Can be 13, 11, 9, 7 or 5
#define N_SAMPLES 13

#ifndef TONEMAP_OPERATOR
#define TONEMAP_OPERATOR 0
#endif

#define TONEMAP_LINEAR 0
#define TONEMAP_EXPONENTIAL 1
#define TONEMAP_EXPONENTIAL_HSV 2
#define TONEMAP_REINHARD 3
#define TONEMAP_FILMIC 4


cbuffer UpdatedOncePerFrame {
    float exposure;
    float burnout;
    float bloomWidth;
    float bloomIntensity;
    float bloomThreshold;
    float defocus;
}

cbuffer UpdatedPerBlurPass {
    float2 pixelSize;
    float2 direction;
}

Texture2D finalTex;
Texture2D srcTex[N_PASSES];


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


float3 rgb2hsv(float3 rgb) {
    float minValue = min(min(rgb.r, rgb.g), rgb.b);
    float maxValue = max(max(rgb.r, rgb.g), rgb.b);
    float d = maxValue - minValue;

    float3 hsv = 0.0;
    hsv.b = maxValue;
    if (d != 0) {
        hsv.g = d / maxValue;

        float3 delrgb = (((maxValue.xxx - rgb) / 6.0) + d / 2.0) / d;
        if      (maxValue == rgb.r) { hsv.r = delrgb.b - delrgb.g; }
        else if (maxValue == rgb.g) { hsv.r = 1.0 / 3.0 + delrgb.r - delrgb.b; }
        else if (maxValue == rgb.b) { hsv.r = 2.0 / 3.0 + delrgb.g - delrgb.r; }

        if (hsv.r < 0.0) { hsv.r += 1.0; }
        if (hsv.r > 1.0) { hsv.r -= 1.0; }
    }
    return hsv;
}

float3 rgb2xyz(float3 rgb) {
    const float3x3 m = { 0.5141364, 0.3238786,  0.16036376,
                         0.265068,  0.67023428, 0.06409157,
                         0.0241188, 0.1228178,  0.84442666 };
    return mul(m, rgb);
}

float3 xyz2Yxy(float3 xyz) {
    float w = xyz.r + xyz.g + xyz.b;
    if (w > 0.0) {
        float3 Yxy;
        Yxy.r = xyz.g;
        Yxy.g = xyz.r / w;
        Yxy.b = xyz.g / w;
        return Yxy;
    } else {
        return 0.0;
    }
}

float3 Yxy2xyz(float3 Yxy) {
    float3 xyz;
    xyz.g = Yxy.r;
    if (Yxy.b > 0.0) {
        xyz.r = Yxy.r * Yxy.g / Yxy.b;
        xyz.b = Yxy.r * (1 - Yxy.g - Yxy.b) / Yxy.b;
    } else {
        xyz.rb = 0.0;
    }
    return xyz;
}

float3 xyz2rgb(float3 xyz) {
    const float3x3 m  = {  2.5651, -1.1665, -0.3986,
                          -1.0217,  1.9777,  0.0439, 
                           0.0753, -0.2543,  1.1892 };
    return mul(m, xyz);
}

float3 hsv2rgb(float3 hsv) {
    const float h = hsv.r, s = hsv.g, v = hsv.b;

    float3 rgb = v;
    if (hsv.g != 0.0) {
        float h_i = floor(6 * h);
        float f = 6 * h - h_i;

        float p = v * (1.0 - s);
        float q = v * (1.0 - f * s);
        float t = v * (1.0 - (1.0 - f) * s);

        if      (h_i == 0) { rgb = float3(v, t, p); }
        else if (h_i == 1) { rgb = float3(q, v, p); }
        else if (h_i == 2) { rgb = float3(p, v, t); }
        else if (h_i == 3) { rgb = float3(p, q, v); }
        else if (h_i == 4) { rgb = float3(t, p, v); }
        else               { rgb = float3(v, p, q); }
    }
    return rgb;
}


float3 FilmicTonemap(float3 x) {
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    float W = 11.2;
    return ((x*(A*x+C*B)+D*E) / (x*(A*x+B)+D*F))- E / F;
}

float3 DoToneMap(float3 color) {
    #if TONEMAP_OPERATOR == TONEMAP_LINEAR
    return exposure * color;
    #elif TONEMAP_OPERATOR == TONEMAP_EXPONENTIAL
    color = 1.0 - exp2(-exposure * color);
    return color;
    #elif TONEMAP_OPERATOR == TONEMAP_EXPONENTIAL_HSV
    color = rgb2hsv(color);
    color.b = 1.0 - exp2(-exposure * color.b);
    color = hsv2rgb(color);
    return color;
    #elif TONEMAP_OPERATOR == TONEMAP_REINHARD
    color = xyz2Yxy(rgb2xyz(color));
    float L = color.r;
    L *= exposure;
    float LL = 1 + L / (burnout * burnout);
    float L_d = L * LL / (1 + L);
    color.r = L_d;
    color = xyz2rgb(Yxy2xyz(color));
    return color;
    #else // TONEMAP_FILMIC
    color = 2.0f * FilmicTonemap(exposure * color);
    float3 whiteScale = 1.0f / FilmicTonemap(11.2);
    color *= whiteScale;
    return color;
    #endif
}


void PassVS(float4 position : POSITION,
            out float4 svposition : SV_POSITION,
            inout float2 texcoord : TEXCOORD0) {
    svposition = position;
}


float4 GlareDetectionPS(float4 position : SV_POSITION,
                        float2 texcoord : TEXCOORD0) : SV_TARGET {
    float2 offsets[] = { 
        float2( 0.0,  0.0), 
        float2(-1.0,  0.0), 
        float2( 1.0,  0.0), 
        float2( 0.0, -1.0),
        float2( 0.0,  1.0),
    };

    float4 color = 1e100;
    for (int i = 0; i < 5; i++) 
        color = min(finalTex.Sample(LinearSampler,  texcoord + offsets[i] * pixelSize), color);
    color.rgb *= exposure;

    return float4(max(color.rgb - bloomThreshold / (1.0 - bloomThreshold), 0.0), color.a);
}


float4 BlurPS(float4 position : SV_POSITION,
              float2 texcoord : TEXCOORD0,
              uniform float2 step) : SV_TARGET {
    #if N_SAMPLES == 13
    float offsets[] = { -1.7688, -1.1984, -0.8694, -0.6151, -0.3957, -0.1940, 0, 0.1940, 0.3957, 0.6151, 0.8694, 1.1984, 1.7688 };
    const float n = 13.0;
    #elif N_SAMPLES == 11
    float offsets[] = { -1.6906, -1.0968, -0.7479, -0.4728, -0.2299, 0, 0.2299, 0.4728, 0.7479, 1.0968, 1.6906 };
    const float n = 11.0;
    #elif N_SAMPLES == 9
    float offsets[] = { -1.5932, -0.9674, -0.5895, -0.2822, 0, 0.2822, 0.5895, 0.9674, 1.5932 };
    const float n = 9.0;
    #elif N_SAMPLES == 7
    float offsets[] = { -1.4652, -0.7916, -0.3661, 0, 0.3661, 0.7916, 1.4652 };
    const float n = 7.0;
    #else
    float offsets[] = { -1.282, -0.524, 0.0, 0.524, 1.282 };
    const float n = 5.0;
    #endif

    float4 color = float4(0.0, 0.0, 0.0, 0.0);
    for (int i = 0; i < int(n); i++)
        color += srcTex[0].Sample(LinearSampler, texcoord + step * offsets[i]);
    return color / n;
}


float4 PyramidFilter(Texture2D tex, float2 texcoord, float2 width) {
    float4 color = tex.Sample(LinearSampler, texcoord + float2(0.5, 0.5) * width);
    color += tex.Sample(LinearSampler, texcoord + float2(-0.5,  0.5) * width);
    color += tex.Sample(LinearSampler, texcoord + float2( 0.5, -0.5) * width);
    color += tex.Sample(LinearSampler, texcoord + float2(-0.5, -0.5) * width);
    return 0.25 * color;
}


float4 CombinePS(float4 position : SV_POSITION,
                 float2 texcoord : TEXCOORD0) : SV_TARGET {
    float w[] = {64.0, 32.0, 16.0, 8.0, 4.0, 2.0, 1.0};

    float4 color = PyramidFilter(finalTex, texcoord, pixelSize * defocus);
    [unroll]
    for (int i = 0; i < N_PASSES; i++) {
        float4 sample = srcTex[i].Sample(LinearSampler, texcoord);
        color.rgb += bloomIntensity * w[i] * sample.rgb / 127.0;
        color.a += sample.a / N_PASSES;
    }

    color.rgb = DoToneMap(color.rgb);
    return color;
}


float4 ToneMapPS(float4 position : SV_POSITION,
                 float2 texcoord : TEXCOORD0) : SV_TARGET {
    float4 color = finalTex.Sample(PointSampler, texcoord);
    color.rgb = DoToneMap(color.rgb);
    return color;
}


DepthStencilState DisableDepthStencil {
    DepthEnable = FALSE;
    StencilEnable = FALSE;
};

BlendState NoBlending {
    AlphaToCoverageEnable = FALSE;
    BlendEnable[0] = FALSE;
};


technique10 GlareDetection {
    pass GlareDetection {
        SetVertexShader(CompileShader(vs_4_0, PassVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, GlareDetectionPS()));
        
        SetDepthStencilState(DisableDepthStencil, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}

technique10 Blur {
    pass Blur {
        SetVertexShader(CompileShader(vs_4_0, PassVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, BlurPS(pixelSize * bloomWidth * direction)));
        
        SetDepthStencilState(DisableDepthStencil, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}

technique10 Combine {
    pass Combine {
        SetVertexShader(CompileShader(vs_4_0, PassVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, CombinePS()));
        
        SetDepthStencilState(DisableDepthStencil, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}

technique10 ToneMap {
    pass ToneMap {
        SetVertexShader(CompileShader(vs_4_0, PassVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, ToneMapPS()));
        
        SetDepthStencilState(DisableDepthStencil, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}
