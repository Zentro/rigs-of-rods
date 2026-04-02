#version 150 core

in vec4 vertex;
in vec3 normal;
in vec2 uv0;

uniform mat4  worldViewProjMatrix;
uniform vec3  eyePosition;
uniform float fresnelBias;
uniform float fresnelScale;
uniform float fresnelPower;
uniform float timeVal;
uniform float scale;
uniform float scroll;
uniform float noise;

out float vFresnel;
out vec3  vNoiseCoord;
out vec4  vProjectionCoord;

void main()
{
    gl_Position = worldViewProjMatrix * vertex;

    // Projective texture coordinates, adjust for mapping
    // Scale matrix (row-major): maps clip [-1,1] to texture [0,1] with Y-flip
    // In GLSL mat4 is column-major, so we transpose:
    mat4 scalemat = mat4(
        0.5,  0.0,  0.0,  0.0,
        0.0, -0.5,  0.0,  0.0,
        0.0,  0.0,  0.5,  0.0,
        0.5,  0.5,  0.5,  1.0
    );
    vProjectionCoord = scalemat * gl_Position;

    // Noise map coords
    vNoiseCoord.xy = (uv0 + (timeVal * scroll)) * scale;
    vNoiseCoord.z  = noise * timeVal;

    // calc fresnel factor
    vec3 eyeDir = normalize(vertex.xyz - eyePosition);
    vFresnel = fresnelBias + fresnelScale * pow(1.0 + dot(eyeDir, normal), fresnelPower);
}
