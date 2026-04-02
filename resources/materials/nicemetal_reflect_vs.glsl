#version 150 core

in vec4 vertex;
in vec3 normal;
in vec2 uv0;
in vec4 colour;

uniform vec3 camPosition;
uniform mat4 world;
uniform mat4 worldViewProj;

out vec2 vUv;
out vec3 vViewDirection;
out vec3 vNormal;
out vec4 vColour;

void main()
{
    gl_Position    = worldViewProj * vertex;
    vUv            = uv0;
    vColour        = colour;
    vNormal        = mat3(world) * normal;
    vViewDirection = mat3(world) * (vertex.xyz - camPosition);
}
