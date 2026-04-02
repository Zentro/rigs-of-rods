#version 150 core
// HAS_REFRACT=1 - use refraction texture (full fresnel), 0 - reflection only (simpler)

in float vFresnel;
in vec3  vNoiseCoord;
in vec4  vProjectionCoord;

uniform float     distortionRange;
uniform vec4      tintColour;
uniform sampler3D noiseMap;
uniform sampler2D reflectMap;
#if HAS_REFRACT
uniform sampler2D refractMap;
#endif

out vec4 fragColor;

void main()
{
    vec3 yoffset = vec3(0.31, 0.58, 0.23);
    vec2 distort;
    distort.x = texture(noiseMap, vNoiseCoord).x;
    distort.y = texture(noiseMap, vNoiseCoord + yoffset).x;
    distort   = (distort * 2.0 - 1.0) * distortionRange;

    vec2 final = vProjectionCoord.xy / vProjectionCoord.w;
    final += distort;

    vec4 reflectionColour = texture(reflectMap, final);
#if HAS_REFRACT
    vec4 refractionColour = texture(refractMap, final) + tintColour;
    fragColor = mix(refractionColour, reflectionColour, vFresnel);
#else
    fragColor   = mix(tintColour, reflectionColour, 0.33 + vFresnel);
    fragColor.a = 0.33 + vFresnel;
#endif
}
