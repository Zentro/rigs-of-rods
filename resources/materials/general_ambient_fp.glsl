#version 150 core

in vec2 vUv;

uniform vec3      ambient;
uniform vec4      matDif;
uniform sampler2D diffuseMap;

out vec4 fragColor;

void main()
{
    vec4 diffuseTex = texture(diffuseMap, vUv);
    fragColor = vec4(ambient * matDif.rgb * diffuseTex.rgb, diffuseTex.a);
}
