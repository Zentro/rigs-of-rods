#version 420 core

// Defines:
//   USE_VERTEX_COLOR - modulates specular map by vertex colour (b=wear, a=damage)

layout(binding = 0) uniform sampler2D Specular_Map;
layout(binding = 1) uniform samplerCube envmaptex;

in vec2 vUv;
in vec3 vViewDirection;
in vec3 vNormal;
in vec4 vCols;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec3 N = normalize(vNormal);
    vec3 R = reflect(vViewDirection, N);
    R.z = -R.z;

    vec4 reflectedColor = texture(envmaptex, R);
#ifdef USE_VERTEX_COLOR
    vec4 emissiveColor = texture(Specular_Map, vUv) + vCols.b / 3.0 - vCols.a / 2.0;
#else
    vec4 emissiveColor = texture(Specular_Map, vUv);
#endif

    fragColor = reflectedColor * emissiveColor;
    fragColor.a = 1.0;
}
