#version 330 core

// OGRE attribute locations: POSITION=0, NORMAL=2, COLOR=3, TEXCOORD0=8
layout(location = 0) in vec4 vertex;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec4 colour;
layout(location = 8) in vec2 uv0;

uniform mat4 worldViewProj;
uniform mat4 world;
uniform vec3 camPosition;

out vec2 vUv;
out vec3 vViewDirection;
out vec3 vNormal;
out vec4 vCols;

void main()
{
    gl_Position = worldViewProj * vertex;
    vUv = uv0;
    vCols = colour;
    vNormal = mat3(world) * normal;
    vViewDirection = mat3(world) * (vertex.xyz - camPosition);
}
