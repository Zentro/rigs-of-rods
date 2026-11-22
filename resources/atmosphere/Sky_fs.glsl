// RoR atmosphere - sky dome, fragment stage: sky-view LUT + sun/moon discs + stars + clouds.
// Define CLOUDS_2D or CLOUDS_VOLUMETRIC for the cloud layer.

OGRE_NATIVE_GLSL_VERSION_DIRECTIVE
#include <OgreUnifiedShader.h>
#include "Atmosphere.glsl"

uniform vec4 atmoSunDir;      // xyz: towards sun, w: sun disc radiance multiplier
uniform vec4 atmoMoonDir;     // xyz: towards moon, w: moon brightness
uniform vec4 atmoSunColour;   // rgb: sun radiance reaching the camera (transmittance applied)
uniform vec4 atmoAmbient;     // rgb: sky ambient (for cloud lighting), w: night factor (0 day .. 1 night)
uniform vec4 atmoCloudParams; // x: coverage, y: density, z: wind offset (m), w: time (s)
uniform vec4 atmoCameraPos;   // xyz: camera world position, w: camera altitude (m)
// Unit 0 is a dummy cube map (OGRE sky box requirement)
SAMPLER2D(skyLut, 1);
SAMPLER2D(noise2D, 2);
#ifdef CLOUDS_VOLUMETRIC
SAMPLER3D(noise3D, 3);
#endif

#define CLOUD_BASE 1500.0
#define CLOUD_TOP  3200.0

float Hash(vec3 p)
{
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float CloudCoverage(vec2 xz)
{
    vec2 uv = (xz + vec2(atmoCloudParams.z, atmoCloudParams.z * 0.37)) / 24000.0;
    float n = texture2D(noise2D, uv).r * 0.65 + texture2D(noise2D, uv * 3.1 + 0.37).r * 0.35;
    return saturate((n - (1.0 - atmoCloudParams.x)) / max(atmoCloudParams.x, 0.05));
}

#ifdef CLOUDS_VOLUMETRIC
float CloudDensity(vec3 p)
{
    float h = (p.y - CLOUD_BASE) / (CLOUD_TOP - CLOUD_BASE);
    if (h < 0.0 || h > 1.0)
        return 0.0;
    float cov = CloudCoverage(p.xz);
    // Rounded bottoms, anvil-ish tops
    float profile = saturate(h * 4.0) * saturate((1.0 - h) * 1.8);
    float base = saturate(cov * profile * 1.6 - 0.25);
    if (base <= 0.0)
        return 0.0;
    vec3 q = (p + vec3(atmoCloudParams.z, 0.0, atmoCloudParams.z * 0.37)) / 1800.0;
    float detail = texture3D(noise3D, q).r * 0.6 + texture3D(noise3D, q * 2.7).r * 0.4;
    return saturate(base - (1.0 - detail) * 0.35 * (1.0 - base)) * atmoCloudParams.y;
}
#endif

MAIN_PARAMETERS
IN(vec3 oDir, TEXCOORD0)
MAIN_DECLARATION
{
    vec3 dir = normalize(oDir);
    vec3 sunDir = normalize(atmoSunDir.xyz);
    vec3 colour = texture2D(skyLut, AtmoDirToLutUv(dir)).rgb;

    // Visibility of space objects: roughly the transmittance towards the zenith-weighted direction
    float horizonFade = saturate(dir.y * 10.0 + 0.2);

    // Stars
    float night = atmoAmbient.w;
    if (night > 0.0 && dir.y > 0.0)
    {
        vec3 cell = floor(dir * 400.0);
        float h = Hash(cell);
        if (h > 0.9985)
        {
            float twinkle = 0.7 + 0.3 * sin(atmoCloudParams.w * (2.0 + h * 5.0) + h * 100.0);
            colour += vec3_splat((h - 0.9985) * 400.0 * twinkle * night * horizonFade * 0.05);
        }
    }

    // Moon disc
    float moonCos = dot(dir, normalize(atmoMoonDir.xyz));
    if (moonCos > 0.99996)
        colour += vec3(0.8, 0.85, 1.0) * atmoMoonDir.w * horizonFade;

    // Sun disc with limb darkening
    float sunCos = dot(dir, sunDir);
    const float SUN_COS = 0.999989; // ~0.27 degree radius
    if (sunCos > SUN_COS)
    {
        float r = saturate((1.0 - sunCos) / (1.0 - SUN_COS));
        float limb = 1.0 - 0.6 * (1.0 - sqrt(1.0 - r));
        colour += atmoSunColour.rgb * atmoSunDir.w * limb;
    }

#if defined(CLOUDS_2D) || defined(CLOUDS_VOLUMETRIC)
    if (dir.y > 0.01 && atmoCloudParams.x > 0.0)
    {
        vec3 ro = atmoCameraPos.xyz;
        float mu = dot(dir, sunDir);
        // Two-lobe phase: forward scattering silver lining + back scattering
        float phase = mix(AtmoPhaseMie(mu, 0.6), AtmoPhaseMie(mu, -0.2), 0.3) * 4.0 * ATMO_PI;
        vec3 sunLight = atmoSunColour.rgb;
        vec3 ambient = atmoAmbient.rgb;
        float distFade;
        vec3 cloudCol = vec3_splat(0.0);
        float cloudAlpha = 0.0;

#ifdef CLOUDS_VOLUMETRIC
        float tA = (CLOUD_BASE - ro.y) / dir.y;
        float tB = (CLOUD_TOP - ro.y) / dir.y;
        float t0 = max(min(tA, tB), 0.0);
        float t1 = min(max(tA, tB), 60000.0);
        distFade = saturate(1.0 - t0 / 60000.0);
        const int STEPS = 56;
        float ds = (t1 - t0) / float(STEPS);
        // Interleaved gradient noise: spatially stable jitter against banding (no temporal flicker)
        float ign = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
        float t = t0 + ds * ign;
        float trans = 1.0;
        for (int i = 0; i < STEPS; i++)
        {
            vec3 p = ro + dir * t;
            float d = CloudDensity(p);
            if (d > 0.001)
            {
                // Light march towards the sun
                float od = 0.0;
                for (int j = 1; j <= 4; j++)
                    od += CloudDensity(p + sunDir * (float(j * j) * 60.0)) * float(j) * 60.0;
                float sigma = d * 0.04;
                float lightT = exp(-od * 0.04) ;
                float powder = 1.0 - exp(-od * 0.08);
                vec3 S = (sunLight * lightT * phase * mix(1.0, powder, 0.5) + ambient * (0.6 + 0.4 * (p.y - CLOUD_BASE) / (CLOUD_TOP - CLOUD_BASE))) * sigma;
                float stepT = exp(-sigma * ds);
                cloudCol += trans * (S - S * stepT) / sigma;
                trans *= stepT;
                if (trans < 0.02)
                    break;
            }
            t += ds;
        }
        cloudAlpha = 1.0 - trans;
#else
        float t = (CLOUD_BASE + 500.0 - ro.y) / dir.y;
        vec3 p = ro + dir * t;
        distFade = saturate(1.0 - t / 60000.0);
        float cov = CloudCoverage(p.xz);
        float shade = CloudCoverage(p.xz + sunDir.xz * 400.0);
        cloudAlpha = saturate(cov * 1.5) * atmoCloudParams.y;
        float lightT = exp(-shade * 2.5);
        cloudCol = (sunLight * lightT * phase * 0.35 + ambient) * cloudAlpha;
#endif
        // Clouds fade into the haze near the horizon
        float fade = distFade * saturate(dir.y * 8.0);
        colour = colour * (1.0 - cloudAlpha * fade) + cloudCol * fade;
    }
#endif

    gl_FragColor = vec4(colour, 1.0);
}
