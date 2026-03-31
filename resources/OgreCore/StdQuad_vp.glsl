#version 150 core

in vec4 vertex;

uniform mat4 worldViewProj;

out vec2 uv0;

void main()
{
    gl_Position = worldViewProj * vertex;
    vec2 inPos = sign(vertex.xy);
    uv0 = (vec2(inPos.x, -inPos.y) + 1.0) / 2.0;
}
