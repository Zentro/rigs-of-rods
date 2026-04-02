#version 150 core

in vec4 vColor;
in vec3 vUv;
in vec4 vLightPos0;
in vec4 vLightPos1;
in vec4 vLightPos2;

uniform sampler2D diffuseMap;
uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;
uniform sampler2D shadowMap2;

uniform vec4 invShadowMapSize0;
uniform vec4 invShadowMapSize1;
uniform vec4 invShadowMapSize2;
uniform vec4 pssmSplitPoints;

out vec4 fragColor;

float shadowPCF(sampler2D shadowMap, vec4 shadowMapPos, vec2 offset)
{
    shadowMapPos = shadowMapPos / shadowMapPos.w;
    vec2 uv = shadowMapPos.xy;
    vec3 o  = vec3(offset, -offset.x) * 0.3;
    float shPos = shadowMapPos.z;
    float c  = (shPos <= texture(shadowMap, uv.xy - o.xy).r) ? 1.0 : 0.0;
         c += (shPos <= texture(shadowMap, uv.xy + o.xy).r) ? 1.0 : 0.0;
         c += (shPos <= texture(shadowMap, uv.xy + o.zy).r) ? 1.0 : 0.0;
         c += (shPos <= texture(shadowMap, uv.xy - o.zy).r) ? 1.0 : 0.0;
    return c / 4.0;
}

void main()
{
    vec4 diffTex = texture(diffuseMap, vUv.xy);
    vec3 diffClr = diffTex.xyz;

    float shadowing = 1.0;
    if (vUv.z <= pssmSplitPoints.y)
        shadowing = shadowPCF(shadowMap0, vLightPos0, invShadowMapSize0.xy);
    else if (vUv.z <= pssmSplitPoints.z)
        shadowing = shadowPCF(shadowMap1, vLightPos1, invShadowMapSize1.xy);
    else
        shadowing = shadowPCF(shadowMap2, vLightPos2, invShadowMapSize2.xy);

    vec3 clr = diffClr * (0.65 + 0.35 * shadowing);
    fragColor = vec4(clr, diffTex.a) * vColor;
}
