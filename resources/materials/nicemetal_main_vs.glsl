#version 150 core

in vec4 vertex;
in vec3 normal;
in vec4 colour;
in vec2 uv0;

uniform vec4 lightPosition;
uniform vec3 eyePosition;
uniform mat4 worldviewproj;

out vec4 vColour;
out vec4 vPos;
out vec3 vNorm;
out vec4 vLightPos;
out vec3 vEyePos;
out vec2 vUv;

void main()
{
    gl_Position = worldviewproj * vertex;
    vPos      = vertex;
    vNorm     = normal;
    vLightPos = lightPosition;
    vEyePos   = eyePosition;
    vColour   = colour;
    vUv       = uv0;
}
