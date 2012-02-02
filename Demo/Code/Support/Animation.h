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


#ifndef ANIMATION_H
#define ANIMATION_H

class Animation {
    public:
        template <class T>
        static T linear(float t, float t0, float t1, T v0, T v1) {
            float p = (t - t0) / (t1 - t0);
            return interpolate(p, v0, v1);
        }

        template <class T>
        static T smooth(float t, float t0, float t1, T v0, T v1, T stepness) {
            /**
             * Catmull-Rom spline interpolation
             */
            float u = (t - t0) / (t1 - t0);
            float u2 = u * u;
            float u3 = u * u * u;
            return v0 * (2 * u3 - 3 * u2 + 1) +
                   v1 * (3 * u2 - 2 * u3) +
                   stepness * (u3 - 2 * u2 + u) +
                   stepness * (u3 - u2);
        }
        
        template <class T>
        static T smooth2(float t, float t0, float t1, T v0, T v1, T stepness) {
            if (t < t0)
                return v0;
            else if (t > t1)
                return v1;
            else
                return smooth(t, t0, t1, v0, v1, stepness);
        }

        template <class T>
        static T constant(float t, float t0, float t1, T c) {
            if (t > t0 && t < t1)
                return c;
            else
                return T(0.0);
        }

        static float linearFade(float t, float in, float out, float inLength, float outLength) {
            if (t < in + inLength)
                return Animation::linear(t, in, in + inLength, 0.0f, 1.0f);
            else
                return Animation::linear(t, out - outLength, out, 1.0f, 0.0f);
        }

        static float smoothFade(float t, float in, float out, float inLength, float outLength) {
            if (t < in + inLength)
                return Animation::smooth2(t, in, in + inLength, 0.0f, 1.0f, 0.0f);
            else
                return Animation::smooth2(t, out - outLength, out, 1.0f, 0.0f, 0.0f);
        }

        template <class T>
        static T interpolate(float p, T v0, T v1) {
            float pp = max(min(p, 1.0f), 0.0f);
            return v0 + pp * (v1 - v0);
        }

        template <class T>
        static T push(float t, float t0, T v0, T velocity) {
            return v0 + (t - t0) * velocity;
        }
};

#endif
