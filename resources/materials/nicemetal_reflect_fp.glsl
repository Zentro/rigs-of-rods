#version 150 core
// Preprocessor defines (set via OGRE preprocessor_defines):
//   HAS_VERT_COLOUR=1 - apply vertex colour adjustments to emissive

in vec2 vUv;
in vec3 vViewDirection;
in vec3 vNormal;
#if HAS_VERT_COLOUR
in vec4 vColour;
#endif

uniform sampler2D   speculartex;
uniform samplerCube cubeMap;

out vec4 fragColor;

void main()
{
    vec3 N = normalize(vNormal);
    vec3 R = reflect(vViewDirection, N);
    R.z = -R.z;

    vec4 reflectedColor = texture(cubeMap, R);
#if HAS_VERT_COLOUR
    vec4 emissiveColor  = texture(speculartex, vUv) + vColour.b / 3.0 - vColour.a / 2.0;
#else
    vec4 emissiveColor  = texture(speculartex, vUv);
#endif

    fragColor   = reflectedColor * emissiveColor;
    fragColor.a = 1.0;
}
