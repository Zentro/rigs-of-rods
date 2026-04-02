#version 150 core
// Preprocessor defines:
//   HAS_SHADOW=1    - PSSM 3-split shadow sampling
//   HAS_ENV=1       - cubemap environment reflection
//   HAS_ALPHA_TEX=1 - separate alpha texture (diffuseAlpha at TEXUNIT1, normalMap shifts to TEXUNIT2)

in vec3 vUv;
in vec4 vWp;
in vec3 vN;
in vec3 vT;
in vec3 vB;
in vec4 vC;
#if HAS_SHADOW
in vec4 vLightPos0;
in vec4 vLightPos1;
in vec4 vLightPos2;
#endif

uniform vec3     ambient;
uniform vec3     lightDif0;
uniform vec3     lightSpec0;
uniform vec4     matDif;
uniform vec4     matSpec;
uniform float    matShininess;
uniform vec3     fogColor;
uniform vec4     lightPos0;
uniform vec3     camPos;
uniform vec4     invSMSize;
uniform mat4     iTWMat;

#if HAS_ENV
uniform vec4      envPars;
#endif

uniform sampler2D diffuseMap;
#if HAS_ALPHA_TEX
uniform sampler2D diffuseAlpha;
uniform sampler2D normalMap;
#else
uniform sampler2D normalMap;
#endif
#if HAS_ENV
uniform samplerCube envMap;
#endif
#if HAS_SHADOW
uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;
uniform sampler2D shadowMap2;
uniform vec4 invShadowMapSize0;
uniform vec4 invShadowMapSize1;
uniform vec4 invShadowMapSize2;
uniform vec4 pssmSplitPoints;
#endif

out vec4 fragColor;

#if HAS_SHADOW
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
#endif

void main()
{
    vec3 ldir = normalize(lightPos0.xyz - (lightPos0.w * vWp.xyz));

    vec4 normalTex = texture(normalMap, vUv.xy);
    mat3 tbn   = mat3(vT, vB, vN);
    vec3 normal = tbn * (normalTex.xyz * 2.0 - 1.0);
    normal = normalize(mat3(iTWMat) * normal);

    vec3 diffuse  = vec3(max(dot(ldir, normal), 0.0));
    vec3 camDir   = normalize(camPos - vWp.xyz);
    vec3 halfVec  = normalize(ldir + camDir);
    vec3 specular = vec3(pow(max(dot(normal, halfVec), 0.0), matShininess));

    vec4 diffuseTex = texture(diffuseMap, vUv.xy);
#if HAS_ALPHA_TEX
    diffuseTex.a = texture(diffuseAlpha, vUv.xy).g;
#endif

    vec3 diffC  = diffuse * lightDif0 * matDif.rgb * diffuseTex.rgb;
    vec3 specC  = specular * lightSpec0 * matSpec.rgb;
    vec3 clrSUM = diffuseTex.rgb * ambient + diffC + specC;

#if HAS_SHADOW
    float shadowing = 1.0;
    if (vUv.z <= pssmSplitPoints.y)
        shadowing = shadowPCF(shadowMap0, vLightPos0, invShadowMapSize0.xy);
    else if (vUv.z <= pssmSplitPoints.z)
        shadowing = shadowPCF(shadowMap1, vLightPos1, invShadowMapSize1.xy);
    else
        shadowing = shadowPCF(shadowMap2, vLightPos2, invShadowMapSize2.xy);

    clrSUM = diffuseTex.rgb * ambient + diffC * (0.25 + 0.75 * shadowing) + specC * shadowing;
#endif

#if HAS_ENV
    vec4 envTex  = texture(envMap, reflect(-camDir, normal));
    clrSUM = envPars.x * clrSUM + envPars.y * envTex.rgb;
#endif

    clrSUM = mix(clrSUM, fogColor, vWp.w);
    fragColor = vec4(clrSUM, diffuseTex.a);
}
