#version 150 core

in vec4 vertex;
in vec3 normal;
in vec3 uv0;
in vec4 colour;

uniform mat4 wMat;
uniform mat4 wvpMat;

out vec3 vUv;
out vec4 vWp;
out vec3 vN;
out vec4 vC;

void main()
{
    vUv = uv0;
    vWp = wMat * vertex;
    gl_Position = wvpMat * vertex;
    vN  = normal;
    vC  = colour;
}
