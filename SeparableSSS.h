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


/**
 *                  _______      _______      _______      _______
 *                 /       |    /       |    /       |    /       |
 *                |   (----    |   (----    |   (----    |   (----
 *                 \   \        \   \        \   \        \   \
 *              ----)   |    ----)   |    ----)   |    ----)   |
 *             |_______/    |_______/    |_______/    |_______/
 *
 *        S E P A R A B L E   S U B S U R F A C E   S C A T T E R I N G
 *
 *                           http://www.iryoku.com/
 *
 * Hi, thanks for your interest in Separable SSS!
 *
 * It's a simple shader composed of two components:
 *
 * 1) A transmittance function, 'SSSSTransmittance', which allows to calculate
 *    light transmission in thin slabs, useful for ears and nostrils. It should
 *    be applied during the main rendering pass as follows:
 *
 *        float3 t = albedo.rgb * lights[i].color * attenuation * spot;
 *        color.rgb += t * SSSSTransmittance(...)
 *
 *    (See 'Main.fx' for more details).
 *
 * 2) A simple two-pass reflectance post-processing shader, 'SSSSBlur*', which
 *    softens the skin appearance. It should be applied as a regular
 *    post-processing effect like bloom (the usual framebuffer ping-ponging):
 *
 *    a) The first pass (horizontal) must be invoked by taking the final color
 *       framebuffer as input, and storing the results into a temporal
 *       framebuffer.
 *    b) The second pass (vertical) must be invoked by taking the temporal
 *       framebuffer as input, and storing the results into the original final
 *       color framebuffer.
 *
 *    Note that This SSS filter should be applied *before* tonemapping.
 *
 * Before including SeparableSSS.h you'll have to setup the target. The
 * following targets are available:
 *         SMAA_HLSL_3
 *         SMAA_HLSL_4
 *         SMAA_GLSL_3
 *
 * For more information of what's under the hood, you can check the following
 * URLs (but take into account that the shader has evolved a little bit since
 * these publications):
 *
 * 1) Reflectance: http://www.iryoku.com/sssss/
 * 2) Transmittance: http://www.iryoku.com/translucency/
 *
 * If you've got any doubts, just contact us!
 */


//-----------------------------------------------------------------------------
// Configurable Defines

/**
 * SSSS_FOV must be set to the value used to render the scene.
 */
#ifndef SSSS_FOVY
#define SSSS_FOVY 20.0
#endif

/**
 * Light diffusion should occur on the surface of the object, not in a screen 
 * oriented plane. Setting SSSS_FOLLOW_SURFACE to 1 will ensure that diffusion
 * is more accurately calculated, at the expense of more memory accesses.
 */
#ifndef SSSS_FOLLOW_SURFACE
#define SSSS_FOLLOW_SURFACE 0
#endif

/**
 * This define allows to specify a different source for the SSS strength
 * (instead of using the alpha channel of the color framebuffer). This is
 * useful when the alpha channel of the mian color buffer is used for something
 * else.
 */
#ifndef SSSS_STREGTH_SOURCE
#define SSSS_STREGTH_SOURCE (colorM.a)
#endif

/**
 * If SSSS_N_SAMPLES is defined at this point, a custom filter kernel must be
 * set by the runtime.
 */
#ifdef SSSS_N_SAMPLES
/**
 * Filter kernel layout is as follows:
 *   - Weights in the RGB channels.
 *   - Offsets in the A channel.
 */
float4 kernel[SSSS_N_SAMPLES]; 
#else
/**
 * Here you have ready-to-use kernels for quickstarters. Three kernels are 
 * readily available, with varying quality.
 * To create new kernels take a look into SSS::calculateKernel, or simply
 * push CTRL+C in the demo to copy the customized kernel into the clipboard.
 *
 * Note: these preset kernels are not used by the demo. They are calculated on
 * the fly depending on the selected values in the interface, by directly using
 * SSS::calculateKernel.
 *
 * Quality ranges from 0 to 2, being 2 the highest quality available.
 * The quality is with respect to 1080p; for 720p Quality=0 suffices.
 */
#define SSSS_QUALITY 1

#if SSSS_QUALITY == 2
#define SSSS_N_SAMPLES 25
float4 kernel[] = {
    float4(0.530605, 0.613514, 0.739601, 0),
    float4(0.000973794, 1.11862e-005, 9.43437e-007, -3),
    float4(0.00333804, 7.85443e-005, 1.2945e-005, -2.52083),
    float4(0.00500364, 0.00020094, 5.28848e-005, -2.08333),
    float4(0.00700976, 0.00049366, 0.000151938, -1.6875),
    float4(0.0094389, 0.00139119, 0.000416598, -1.33333),
    float4(0.0128496, 0.00356329, 0.00132016, -1.02083),
    float4(0.017924, 0.00711691, 0.00347194, -0.75),
    float4(0.0263642, 0.0119715, 0.00684598, -0.520833),
    float4(0.0410172, 0.0199899, 0.0118481, -0.333333),
    float4(0.0493588, 0.0367726, 0.0219485, -0.1875),
    float4(0.0402784, 0.0657244, 0.04631, -0.0833333),
    float4(0.0211412, 0.0459286, 0.0378196, -0.0208333),
    float4(0.0211412, 0.0459286, 0.0378196, 0.0208333),
    float4(0.0402784, 0.0657244, 0.04631, 0.0833333),
    float4(0.0493588, 0.0367726, 0.0219485, 0.1875),
    float4(0.0410172, 0.0199899, 0.0118481, 0.333333),
    float4(0.0263642, 0.0119715, 0.00684598, 0.520833),
    float4(0.017924, 0.00711691, 0.00347194, 0.75),
    float4(0.0128496, 0.00356329, 0.00132016, 1.02083),
    float4(0.0094389, 0.00139119, 0.000416598, 1.33333),
    float4(0.00700976, 0.00049366, 0.000151938, 1.6875),
    float4(0.00500364, 0.00020094, 5.28848e-005, 2.08333),
    float4(0.00333804, 7.85443e-005, 1.2945e-005, 2.52083),
    float4(0.000973794, 1.11862e-005, 9.43437e-007, 3),
};
#elif SSSS_QUALITY == 1
#define SSSS_N_SAMPLES 17
float4 kernel[] = {
    float4(0.536343, 0.624624, 0.748867, 0),
    float4(0.00317394, 0.000134823, 3.77269e-005, -2),
    float4(0.0100386, 0.000914679, 0.000275702, -1.53125),
    float4(0.0144609, 0.00317269, 0.00106399, -1.125),
    float4(0.0216301, 0.00794618, 0.00376991, -0.78125),
    float4(0.0347317, 0.0151085, 0.00871983, -0.5),
    float4(0.0571056, 0.0287432, 0.0172844, -0.28125),
    float4(0.0582416, 0.0659959, 0.0411329, -0.125),
    float4(0.0324462, 0.0656718, 0.0532821, -0.03125),
    float4(0.0324462, 0.0656718, 0.0532821, 0.03125),
    float4(0.0582416, 0.0659959, 0.0411329, 0.125),
    float4(0.0571056, 0.0287432, 0.0172844, 0.28125),
    float4(0.0347317, 0.0151085, 0.00871983, 0.5),
    float4(0.0216301, 0.00794618, 0.00376991, 0.78125),
    float4(0.0144609, 0.00317269, 0.00106399, 1.125),
    float4(0.0100386, 0.000914679, 0.000275702, 1.53125),
    float4(0.00317394, 0.000134823, 3.77269e-005, 2),
};
#elif SSSS_QUALITY == 0
#define SSSS_N_SAMPLES 11
float4 kernel[] = {
    float4(0.560479, 0.669086, 0.784728, 0),
    float4(0.00471691, 0.000184771, 5.07566e-005, -2),
    float4(0.0192831, 0.00282018, 0.00084214, -1.28),
    float4(0.03639, 0.0130999, 0.00643685, -0.72),
    float4(0.0821904, 0.0358608, 0.0209261, -0.32),
    float4(0.0771802, 0.113491, 0.0793803, -0.08),
    float4(0.0771802, 0.113491, 0.0793803, 0.08),
    float4(0.0821904, 0.0358608, 0.0209261, 0.32),
    float4(0.03639, 0.0130999, 0.00643685, 0.72),
    float4(0.0192831, 0.00282018, 0.00084214, 1.28),
    float4(0.00471691, 0.000184771, 5.07565e-005, 2),
};
#else
#error Quality must be one of {0,1,2}
#endif
#endif

//-----------------------------------------------------------------------------
// Porting Functions

#if SSSS_HLSL_3 == 1
#define SSSSTexture2D sampler2D
#define SSSSSampleLevelZero(tex, coord) tex2Dlod(tex, float4(coord, 0.0, 0.0))
#define SSSSSampleLevelZeroPoint(tex, coord) tex2Dlod(tex, float4(coord, 0.0, 0.0))
#define SSSSSample(tex, coord) tex2D(tex, coord)
#define SSSSSamplePoint(tex, coord) tex2D(tex, coord)
#define SSSSSampleLevelZeroOffset(tex, coord, offset) tex2Dlod(tex, float4(coord + offset * SSSS_PIXEL_SIZE, 0.0, 0.0))
#define SSSSSampleOffset(tex, coord, offset) tex2D(tex, coord + offset * SSSS_PIXEL_SIZE)
#define SSSSLerp(a, b, t) lerp(a, b, t)
#define SSSSSaturate(a) saturate(a)
#define SSSSMad(a, b, c) mad(a, b, c)
#define SSSSMul(v, m) mul(v, m)
#define SSSS_FLATTEN [flatten]
#define SSSS_BRANCH [branch]
#define SSSS_UNROLL [unroll]
#endif
#if SSSS_HLSL_4 == 1
SamplerState LinearSampler { Filter = MIN_MAG_LINEAR_MIP_POINT; AddressU = Clamp; AddressV = Clamp; };
SamplerState PointSampler { Filter = MIN_MAG_MIP_POINT; AddressU = Clamp; AddressV = Clamp; };
#define SSSSTexture2D Texture2D
#define SSSSSampleLevelZero(tex, coord) tex.SampleLevel(LinearSampler, coord, 0)
#define SSSSSampleLevelZeroPoint(tex, coord) tex.SampleLevel(PointSampler, coord, 0)
#define SSSSSample(tex, coord) SSSSSampleLevelZero(tex, coord)
#define SSSSSamplePoint(tex, coord) SSSSSampleLevelZeroPoint(tex, coord)
#define SSSSSampleLevelZeroOffset(tex, coord, offset) tex.SampleLevel(LinearSampler, coord, 0, offset)
#define SSSSSampleOffset(tex, coord, offset) SSSSSampleLevelZeroOffset(tex, coord, offset)
#define SSSSLerp(a, b, t) lerp(a, b, t)
#define SSSSSaturate(a) saturate(a)
#define SSSSMad(a, b, c) mad(a, b, c)
#define SSSSMul(v, m) mul(v, m)
#define SSSS_FLATTEN [flatten]
#define SSSS_BRANCH [branch]
#define SSSS_UNROLL [unroll]
#endif
#if SSSS_GLSL_3 == 1
#define SSSSTexture2D sampler2D
#define SSSSSampleLevelZero(tex, coord) textureLod(tex, coord, 0.0)
#define SSSSSampleLevelZeroPoint(tex, coord) textureLod(tex, coord, 0.0)
#define SSSSSample(tex, coord) texture(tex, coord)
#define SSSSSamplePoint(tex, coord) texture(tex, coord)
#define SSSSSampleLevelZeroOffset(tex, coord, offset) textureLodOffset(tex, coord, 0.0, offset)
#define SSSSSampleOffset(tex, coord, offset) texture(tex, coord, offset)
#define SSSSLerp(a, b, t) mix(a, b, t)
#define SSSSSaturate(a) clamp(a, 0.0, 1.0)
#define SSSSMad(a, b, c) (a * b + c)
#define SSSSMul(v, m) (m * v)
#define SSSS_FLATTEN
#define SSSS_BRANCH
#define SSSS_UNROLL
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define int2 ivec2
#define int3 ivec3
#define int4 ivec4
#define float4x4 mat4x4
#endif


//-----------------------------------------------------------------------------
// Separable SSS Transmittance Function

float3 SSSSTransmittance(
        /**
         * This parameter allows to control the transmittance effect. Its range
         * should be 0..1. Higher values translate to a stronger effect.
         */
        float translucency,

        /**
         * This parameter should be the same as the 'SSSSBlurPS' one. See below
         * for more details.
         */
        float sssWidth,

        /**
         * Position in world space.
         */
        float3 worldPosition,

        /**
         * Normal in world space.
         */
        float3 worldNormal,

        /**
         * Light vector: lightWorldPosition - worldPosition.
         */
        float3 light,

        /**
         * Linear 0..1 shadow map.
         */
        SSSSTexture2D shadowMap,

        /**
         * Regular world to light space matrix.
         */
        float4x4 lightViewProjection,

        /**
         * Far plane distance used in the light projection matrix.
         */
        float lightFarPlane) {
    /**
     * Calculate the scale of the effect.
     */
    float scale = 8.25 * (1.0 - translucency) / sssWidth;
       
    /**
     * First we shrink the position inwards the surface to avoid artifacts:
     * (Note that this can be done once for all the lights)
     */
    float4 shrinkedPos = float4(worldPosition - 0.005 * worldNormal, 1.0);

    /**
     * Now we calculate the thickness from the light point of view:
     */
    float4 shadowPosition = SSSSMul(shrinkedPos, lightViewProjection);
    float d1 = SSSSSample(shadowMap, shadowPosition.xy / shadowPosition.w).r; // 'd1' has a range of 0..1
    float d2 = shadowPosition.z; // 'd2' has a range of 0..'lightFarPlane'
    d1 *= lightFarPlane; // So we scale 'd1' accordingly:
    float d = scale * abs(d1 - d2);

    /**
     * Armed with the thickness, we can now calculate the color by means of the
     * precalculated transmittance profile.
     * (It can be precomputed into a texture, for maximum performance):
     */
    float dd = -d * d;
    float3 profile = float3(0.233, 0.455, 0.649) * exp(dd / 0.0064) +
                     float3(0.1,   0.336, 0.344) * exp(dd / 0.0484) +
                     float3(0.118, 0.198, 0.0)   * exp(dd / 0.187)  +
                     float3(0.113, 0.007, 0.007) * exp(dd / 0.567)  +
                     float3(0.358, 0.004, 0.0)   * exp(dd / 1.99)   +
                     float3(0.078, 0.0,   0.0)   * exp(dd / 7.41);

    /** 
     * Using the profile, we finally approximate the transmitted lighting from
     * the back of the object:
     */
    return profile * SSSSSaturate(0.3 + dot(light, -worldNormal));
}

//-----------------------------------------------------------------------------
// Separable SSS Reflectance Vertex Shader

void SSSSBlurVS(float4 position,
                out float4 svPosition,
                inout float2 texcoord) {
    svPosition = position;
}

//-----------------------------------------------------------------------------
// Separable SSS Reflectance Pixel Shader

float4 SSSSBlurPS(
        /**
         * The usual quad texture coordinates.
         */
        float2 texcoord,

        /**
         * This is a SRGB or HDR color input buffer, which should be the final
         * color frame, resolved in case of using multisampling. The desired
         * SSS strength should be stored in the alpha channel (1 for full
         * strength, 0 for disabling SSS). If this is not possible, you an
         * customize the source of this value using SSSS_STREGTH_SOURCE.
         *
         * When using non-SRGB buffers, you
         * should convert to linear before processing, and back again to gamma
         * space before storing the pixels (see Chapter 24 of GPU Gems 3 for
         * more info)
         *
         * IMPORTANT: WORKING IN A NON-LINEAR SPACE WILL TOTALLY RUIN SSS!
         */
        SSSSTexture2D colorTex,

        /**
         * The linear depth buffer of the scene, resolved in case of using
         * multisampling. The resolve should be a simple average to avoid
         * artifacts in the silhouette of objects.
         */
        SSSSTexture2D depthTex,

        /**
         * This parameter specifies the global level of subsurface scattering
         * or, in other words, the width of the filter. It's specified in
         * world space units.
         */
        float sssWidth,

        /**
         * Direction of the blur:
         *   - First pass:   float2(1.0, 0.0)
         *   - Second pass:  float2(0.0, 1.0)
         */
        float2 dir,

        /**
         * This parameter indicates whether the stencil buffer should be
         * initialized. Should be set to 'true' for the first pass if not
         * previously initialized, to enable optimization of the second
         * pass.
         */
        bool initStencil) {

    // Fetch color of current pixel:
    float4 colorM = SSSSSamplePoint(colorTex, texcoord);

    // Initialize the stencil buffer in case it was not already available:
    if (initStencil) // (Checked in compile time, it's optimized away)
        if (SSSS_STREGTH_SOURCE == 0.0) discard;

    // Fetch linear depth of current pixel:
    float depthM = SSSSSamplePoint(depthTex, texcoord).r;

    // Calculate the sssWidth scale (1.0 for a unit plane sitting on the
    // projection window):
    float distanceToProjectionWindow = 1.0 / tan(0.5 * radians(SSSS_FOVY));
    float scale = distanceToProjectionWindow / depthM;

    // Calculate the final step to fetch the surrounding pixels:
    float2 finalStep = sssWidth * scale * dir;
    finalStep *= SSSS_STREGTH_SOURCE; // Modulate it using the alpha channel.
    finalStep *= 1.0 / 3.0; // Divide by 3 as the kernels range from -3 to 3.

    // Accumulate the center sample:
    float4 colorBlurred = colorM;
    colorBlurred.rgb *= kernel[0].rgb;

    // Accumulate the other samples:
    SSSS_UNROLL
    for (int i = 1; i < SSSS_N_SAMPLES; i++) {
        // Fetch color and depth for current sample:
        float2 offset = texcoord + kernel[i].a * finalStep;
        float4 color = SSSSSample(colorTex, offset);

        #if SSSS_FOLLOW_SURFACE == 1
        // If the difference in depth is huge, we lerp color back to "colorM":
        float depth = SSSSSample(depthTex, offset).r;
        float s = SSSSSaturate(300.0f * distanceToProjectionWindow *
                               sssWidth * abs(depthM - depth));
        color.rgb = SSSSLerp(color.rgb, colorM.rgb, s);
        #endif

        // Accumulate:
        colorBlurred.rgb += kernel[i].rgb * color.rgb;
    }

    return colorBlurred;
}

//-----------------------------------------------------------------------------
