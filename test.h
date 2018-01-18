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
