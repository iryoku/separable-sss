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


// Set the HLSL version:
#ifndef SMAA_HLSL_4
#define SSSS_HLSL_4 1
#endif

// And include our header!
#include "SeparableSSS.h"


#pragma warning(disable: 3571) 
#define N_LIGHTS 5 


struct Light {
    float3 position;
    float3 direction;
    float falloffStart;
    float falloffWidth;
    float3 color;
    float attenuation;
    float farPlane;
    float bias;
    float4x4 viewProjection;
};

cbuffer UpdatedPerFrame {
    float3 cameraPosition;
    Light lights[N_LIGHTS];

    matrix currWorldViewProj;
    matrix prevWorldViewProj;
    float2 jitter;
}

cbuffer UpdatedPerObject {
    matrix world;
    matrix worldInverseTranspose;
    int id;
    float bumpiness;
    float specularIntensity;
    float specularRoughness;
    float specularFresnel;
    float translucency;
    float sssEnabled;
    float sssWidth;
    float ambient;
}

Texture2D shadowMaps[N_LIGHTS];
Texture2D diffuseTex;
Texture2D normalTex;
Texture2D specularAOTex;
Texture2D beckmannTex;
TextureCube irradianceTex;

Texture2D specularsTex;


SamplerState AnisotropicSampler {
    Filter = ANISOTROPIC;
    AddressU = Clamp;
    AddressV = Clamp;
    MaxAnisotropy = 16;
};

SamplerComparisonState ShadowSampler {
    ComparisonFunc = Less;
    Filter = COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
};


struct RenderV2P {
    // Position and texcoord:
    float4 svPosition : SV_POSITION;
    float2 texcoord : TEXCOORD0;

    // For reprojection:
    float3 currPosition : TEXCOORD1;
    float3 prevPosition : TEXCOORD2;

    // For shading:
    centroid float3 worldPosition : TEXCOORD3;
    centroid float3 view : TEXCOORD4;
    centroid float3 normal : TEXCOORD5;
    centroid float3 tangent : TEXCOORD6;
};

RenderV2P RenderVS(float4 position : POSITION0,
                   float3 normal : NORMAL,
                   float3 tangent : TANGENT,
                   float2 texcoord : TEXCOORD0) {
    RenderV2P output;

    // Transform to homogeneous projection space:
    output.svPosition = mul(position, currWorldViewProj);
    output.currPosition = output.svPosition.xyw;
    output.prevPosition = mul(position, prevWorldViewProj).xyw;

    // Covert the jitter from non-homogeneous coordiantes to homogeneous
    // coordinates and add it:
    // (note that for providing the jitter in non-homogeneous projection space,
    //  pixel coordinates (screen space) need to multiplied by two in the C++
    //  code)
    output.svPosition.xy -= jitter * output.svPosition.w;

    // Positions in projection space are in [-1, 1] range, while texture
    // coordinates are in [0, 1] range. So, we divide by 2 to get velocities in
    // the scale:
    output.currPosition.xy /= 2.0;
    output.prevPosition.xy /= 2.0;

    // Texture coordinates have a top-to-bottom y axis, so flip this axis:
    output.currPosition.y = -output.currPosition.y;
    output.prevPosition.y = -output.prevPosition.y;

    // Output texture coordinates:
    output.texcoord = texcoord;

    // Build the vectors required for shading:
    output.worldPosition = mul(position, world).xyz;
    output.view = cameraPosition - output.worldPosition;
    output.normal = mul(normal, (float3x3) worldInverseTranspose);
    output.tangent = mul(tangent, (float3x3) worldInverseTranspose);

    return output;
}


float3 BumpMap(Texture2D normalTex, float2 texcoord) {
    float3 bump;
    bump.xy = -1.0 + 2.0 * normalTex.Sample(AnisotropicSampler, texcoord).gr;
    bump.z = sqrt(1.0 - bump.x * bump.x - bump.y * bump.y);
    return normalize(bump);
}

float Fresnel(float3 half, float3 view, float f0) {
    float base = 1.0 - dot(view, half);
    float exponential = pow(base, 5.0);
    return exponential + f0 * (1.0 - exponential);
}

float SpecularKSK(Texture2D beckmannTex, float3 normal, float3 light, float3 view, float roughness) {
    float3 half = view + light;
    float3 halfn = normalize(half);

    float ndotl = max(dot(normal, light), 0.0);
    float ndoth = max(dot(normal, halfn), 0.0);

    float ph = pow(2.0 * beckmannTex.SampleLevel(LinearSampler, float2(ndoth, roughness), 0).r, 10.0);
    float f = lerp(0.25, Fresnel(halfn, view, 0.028), specularFresnel);
    float ksk = max(ph * f / dot(half, half), 0.0);

    return ndotl * ksk;   
}

float Shadow(float3 worldPosition, int i) {
    float4 shadowPosition = mul(float4(worldPosition, 1.0), lights[i].viewProjection);
    shadowPosition.xy /= shadowPosition.w;
    shadowPosition.z += lights[i].bias;
    return shadowMaps[i].SampleCmpLevelZero(ShadowSampler, shadowPosition.xy, shadowPosition.z / lights[i].farPlane).r;
}

float ShadowPCF(float3 worldPosition, int i, int samples, float width) {
    float4 shadowPosition = mul(float4(worldPosition, 1.0), lights[i].viewProjection);
    shadowPosition.xy /= shadowPosition.w;
    shadowPosition.z += lights[i].bias;
    
    float w, h;
    shadowMaps[i].GetDimensions(w, h);

    float shadow = 0.0;
    float offset = (samples - 1.0) / 2.0;
    [unroll]
    for (float x = -offset; x <= offset; x += 1.0) {
        [unroll]
        for (float y = -offset; y <= offset; y += 1.0) {
            float2 pos = shadowPosition.xy + width * float2(x, y) / w;
            shadow += shadowMaps[i].SampleCmpLevelZero(ShadowSampler, pos, shadowPosition.z / lights[i].farPlane).r;
        }
    }
    shadow /= samples * samples;
    return shadow;
}


float4 RenderPS(RenderV2P input,
                out float depth : SV_TARGET1,
                out float2 velocity : SV_TARGET2,
                out float4 specularColor : SV_TARGET3) : SV_TARGET0 {
    // We build the TBN frame here in order to be able to use the bump map for IBL:
    input.normal = normalize(input.normal);
    input.tangent = normalize(input.tangent);
    float3 bitangent = cross(input.normal, input.tangent);
    float3x3 tbn = transpose(float3x3(input.tangent, bitangent, input.normal));

    // Transform bumped normal to world space, in order to use IBL for ambient lighting:
    float3 tangentNormal = lerp(float3(0.0, 0.0, 1.0), BumpMap(normalTex, input.texcoord), bumpiness);
    float3 normal = mul(tbn, tangentNormal);
    input.view = normalize(input.view);

    // Fetch albedo, specular parameters and static ambient occlusion:
    float4 albedo = diffuseTex.Sample(AnisotropicSampler, input.texcoord);
    float3 specularAO = specularAOTex.Sample(LinearSampler, input.texcoord).rgb;

    float occlusion = specularAO.b;
    float intensity = specularAO.r * specularIntensity;
    float roughness = (specularAO.g / 0.3) * specularRoughness;

    // Initialize the output:
    float4 color = float4(0.0, 0.0, 0.0, 0.0);
    specularColor = float4(0.0, 0.0, 0.0, 0.0);

    [unroll]
    for (int i = 0; i < N_LIGHTS; i++) {
        float3 light = lights[i].position - input.worldPosition;
        float dist = length(light);
        light /= dist;

        float spot = dot(lights[i].direction, -light);
        [flatten]
        if (spot > lights[i].falloffStart) {
            // Calculate attenuation:
            float curve = min(pow(dist / lights[i].farPlane, 6.0), 1.0);
            float attenuation = lerp(1.0 / (1.0 + lights[i].attenuation * dist * dist), 0.0, curve);

            // And the spot light falloff:
            spot = saturate((spot - lights[i].falloffStart) / lights[i].falloffWidth);

            // Calculate some terms we will use later on:
            float3 f1 = lights[i].color * attenuation * spot;
            float3 f2 = albedo.rgb * f1;

            // Calculate the diffuse and specular lighting:
            float3 diffuse = saturate(dot(light, normal));
            float specular = intensity * SpecularKSK(beckmannTex, normal, light, input.view, roughness);

            // And also the shadowing:
            float shadow = ShadowPCF(input.worldPosition, i, 3, 1.0);

            // Add the diffuse and specular components:
            #ifdef SEPARATE_SPECULARS
            color.rgb += shadow * f2 * diffuse;
            specularColor.rgb += shadow * f1 * specular;
            #else
            color.rgb += shadow * (f2 * diffuse + f1 * specular);
            #endif

            // Add the transmittance component:
            if (sssEnabled)
                color.rgb += f2 * SSSSTransmittance(translucency, sssWidth, input.worldPosition, input.normal, light, shadowMaps[i], lights[i].viewProjection, lights[i].farPlane);
        }
    }

    // Add the ambient component:
    color.rgb += occlusion * ambient * albedo.rgb * irradianceTex.Sample(LinearSampler, normal).rgb;

    // Store the SSS strength:
    specularColor.a = albedo.a;

    // Store the depth value:
    depth = input.svPosition.w;

    // Convert to non-homogeneous points by dividing by w:
    input.currPosition.xy /= input.currPosition.z; // w is stored in z
    input.prevPosition.xy /= input.prevPosition.z;

    // Calculate velocity in non-homogeneous projection space:
    velocity = input.currPosition.xy - input.prevPosition.xy;

    // Compress the velocity for storing it in a 8-bit render target:
    color.a = sqrt(5.0 * length(velocity));

    return color;
}


void PassVS(float4 position : POSITION,
            out float4 svposition : SV_POSITION,
            inout float2 texcoord : TEXCOORD0) {
    svposition = position;
}

float4 AddSpecularPS(float4 position : SV_POSITION,
                      float2 texcoord : TEXCOORD) : SV_TARGET {
    return specularsTex.Sample(PointSampler, texcoord);
}


DepthStencilState DisableDepthStencil {
    DepthEnable = FALSE;
    StencilEnable = FALSE;
};

DepthStencilState EnableDepthStencil {
    DepthEnable = TRUE;
    StencilEnable = TRUE;
    FrontFaceStencilPass = REPLACE;
};

BlendState NoBlending {
    AlphaToCoverageEnable = FALSE;
    BlendEnable[0] = FALSE;
    BlendEnable[1] = FALSE;
};

BlendState AddSpecularBlending {
    AlphaToCoverageEnable = FALSE;
    BlendEnable[0] = TRUE;
    BlendEnable[1] = FALSE;
    SrcBlend = ONE;
    DestBlend = ONE;
    RenderTargetWriteMask[0] = 0x7;
};

RasterizerState EnableMultisampling {
    MultisampleEnable = TRUE;
};


technique10 Render {
    pass Render {
        SetVertexShader(CompileShader(vs_4_0, RenderVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, RenderPS()));
        
        SetDepthStencilState(EnableDepthStencil, id);
        SetBlendState(NoBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
        SetRasterizerState(EnableMultisampling);
    }
}

technique10 AddSpecular {
    pass AddSpecular {
        SetVertexShader(CompileShader(vs_4_0, PassVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, AddSpecularPS()));
        
        SetDepthStencilState(DisableDepthStencil, 0);
        SetBlendState(AddSpecularBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}
