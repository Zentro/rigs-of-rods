#version 330 core

// OGRE attribute locations: POSITION=0, NORMAL=2, COLOR=3, TEXCOORD0=8
layout(location = 0) in vec4 vertex;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec4 colour;
layout(location = 8) in vec2 uv0;

uniform mat4 worldviewproj;
uniform vec4 lightPosition;
uniform vec3 eyePosition;

out vec4 vCols;
out vec4 vPos;
out vec3 vNorm;
out vec4 vLightPos;
out vec3 vEyePos;
out vec2 vUv;

void main()
{
    gl_Position = worldviewproj * vertex;
    vPos = vertex;
    vNorm = normal;
    vLightPos = lightPosition;
    vEyePos = eyePosition;
    vCols = colour;
    vUv = uv0;
}
