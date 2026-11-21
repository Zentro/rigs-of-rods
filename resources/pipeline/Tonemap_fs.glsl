// HDR resolve: exposure, filmic tonemapping, sRGB encode, dithering.
// Input is the linear HDR scene; output goes to a non-sRGB 8-bit backbuffer.
// Ported from the gfx-rtss branch (trimmed: bloom, AO, GI, sun shafts and lens flare come with M8).

OGRE_NATIVE_GLSL_VERSION_DIRECTIVE
#include <OgreUnifiedShader.h>

OGRE_UNIFORMS(
    uniform float exposure;
    uniform float tonemapMode; // 0 = none (clamp), 1 = ACES fitted, 2 = AgX
)
SAMPLER2D(sceneTex, 0);

// ACES fitted curve, Stephen Hill (BakingLab)
vec3 RRTAndODTFit(vec3 v)
{
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

vec3 TonemapACES(vec3 c)
{
    // sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
    mat3 inMat = mtxFromRows(
        vec3(0.59719, 0.35458, 0.04823),
        vec3(0.07600, 0.90834, 0.01566),
        vec3(0.02840, 0.13383, 0.83777));
    // ODT_SAT => XYZ => D60_2_D65 => sRGB
    mat3 outMat = mtxFromRows(
        vec3( 1.60475, -0.53108, -0.07367),
        vec3(-0.10208,  1.10813, -0.00605),
        vec3(-0.00327, -0.07276,  1.07602));
    c = mul(inMat, c);
    c = RRTAndODTFit(c);
    c = mul(outMat, c);
    return saturate(c);
}

// AgX (Troy Sobotka), minimal fit by bwrensch
vec3 AgXDefaultContrastApprox(vec3 x)
{
    vec3 x2 = x * x;
    vec3 x4 = x2 * x2;
    return + 15.5     * x4 * x2
           - 40.14    * x4 * x
           + 31.96    * x4
           - 6.868    * x2 * x
           + 0.4298   * x2
           + 0.1191   * x
           - 0.00232;
}

vec3 TonemapAgX(vec3 c)
{
    mat3 agxMat = mtxFromRows(
        vec3(0.842479062253094, 0.0784335999999992, 0.0792237451477643),
        vec3(0.0423282422610123, 0.878468636469772, 0.0791661274605434),
        vec3(0.0423756549057051, 0.0784336, 0.879142973793104));
    mat3 agxMatInv = mtxFromRows(
        vec3(1.19687900512017, -0.0980208811401368, -0.0990297440797205),
        vec3(-0.0528968517574562, 1.15190312990417, -0.0989611768448433),
        vec3(-0.0529716355144438, -0.0980434501171241, 1.15107367264116));
    const float minEv = -12.47393;
    const float maxEv = 4.026069;

    c = mul(agxMat, c);
    c = clamp(log2(max(c, vec3_splat(1e-10))), minEv, maxEv);
    c = (c - minEv) / (maxEv - minEv);
    c = AgXDefaultContrastApprox(c);
    c = mul(agxMatInv, c);
    // AgX output is display-encoded already, linearize so that the sRGB encode below is uniform
    return pow(max(c, vec3_splat(0.0)), vec3_splat(2.2));
}

vec3 LinearToSRGB(vec3 c)
{
    vec3 lo = c * 12.92;
    vec3 hi = 1.055 * pow(max(c, vec3_splat(0.0)), vec3_splat(1.0 / 2.4)) - 0.055;
    return mix(lo, hi, step(vec3_splat(0.0031308), c));
}

MAIN_PARAMETERS
IN(vec2 oUv, TEXCOORD0)
MAIN_DECLARATION
{
    vec3 hdr = texture2D(sceneTex, oUv).rgb * exposure;

    vec3 ldr;
    if (tonemapMode > 1.5)
        ldr = TonemapAgX(hdr);
    else if (tonemapMode > 0.5)
        ldr = TonemapACES(hdr);
    else
        ldr = saturate(hdr);

    vec3 srgb = LinearToSRGB(ldr);

    // Triangular dither against banding in 8-bit output
    float n = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
    float n2 = fract(sin(dot(gl_FragCoord.xy + 0.37, vec2(39.3468, 11.1353))) * 24634.6345);
    srgb += (n + n2 - 1.0) / 255.0;

    gl_FragColor = vec4(srgb, 1.0);
}
