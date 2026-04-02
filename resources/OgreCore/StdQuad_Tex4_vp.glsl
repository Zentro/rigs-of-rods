#version 150 core

in vec4 vertex;

uniform mat4 worldViewProj;

out vec2 uv0;
out vec2 uv1;
out vec2 uv2;
out vec2 uv3;

void main()
{
    gl_Position = worldViewProj * vertex;
    vec2 inPos = sign(vertex.xy);
    uv0 = (vec2(inPos.x, -inPos.y) + 1.0) / 2.0;
    uv1 = uv0;
    uv2 = uv0;
    uv3 = uv0;
}
