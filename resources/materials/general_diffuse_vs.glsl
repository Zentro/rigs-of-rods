#version 150 core
// HAS_SHADOW=1 - output shadow map positions for PSSM

in vec4 vertex;
in vec3 normal;
in vec3 tangent;
in vec3 uv0;
in vec4 colour;

uniform mat4 wMat;
uniform mat4 wvpMat;
uniform vec4 fogParams;

#if HAS_SHADOW
uniform mat4 texWVPMat0;
uniform mat4 texWVPMat1;
uniform mat4 texWVPMat2;
#endif

out vec3 vUv;
out vec4 vWp;
out vec3 vN;
out vec3 vT;
out vec3 vB;
out vec4 vC;
#if HAS_SHADOW
out vec4 vLightPos0;
out vec4 vLightPos1;
out vec4 vLightPos2;
#endif

void main()
{
    vUv      = uv0;
    vWp      = wMat * vertex;
    gl_Position = wvpMat * vertex;
    vUv.z    = gl_Position.z;
    vN       = normal;
    vT       = tangent;
    vB       = cross(tangent, normal);
    vC       = colour;
    vWp.w    = clamp(fogParams.x * (gl_Position.z - fogParams.y) * fogParams.w, 0.0, 1.0);

#if HAS_SHADOW
    vLightPos0 = texWVPMat0 * vertex;
    vLightPos1 = texWVPMat1 * vertex;
    vLightPos2 = texWVPMat2 * vertex;
#endif
}
