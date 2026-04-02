#version 150 core

in vec4 vertex;
in vec4 colour;
in vec3 uv0;

uniform float    time;
uniform float    frequency;
uniform vec4     direction;
uniform mat4     worldViewProj;
uniform vec3     camPos;
uniform float    fadeRange;
uniform mat4     texWorldViewProjMatrix0;
uniform mat4     texWorldViewProjMatrix1;
uniform mat4     texWorldViewProjMatrix2;

out vec4 vColor;
out vec3 vUv;
out vec4 vLightPos0;
out vec4 vLightPos1;
out vec4 vLightPos2;

void main()
{
    vec4 position = vertex;
    float dist    = distance(camPos.xz, position.xz);
    vColor.rgb    = colour.rgb;
    vColor.a      = 2.0 - (2.0 * dist / fadeRange);

    float oldposx = position.x;
    if (uv0.y == 0.0)
    {
        float offset = sin(time + oldposx * frequency);
        position += direction * offset;
    }
    gl_Position = worldViewProj * position;
    vUv         = uv0;
    vUv.z       = gl_Position.z;

    vLightPos0 = texWorldViewProjMatrix0 * position;
    vLightPos1 = texWorldViewProjMatrix1 * position;
    vLightPos2 = texWorldViewProjMatrix2 * position;
}
