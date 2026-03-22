#version 330 core

// Defines:
//   HAS_DAMAGE     - enables Dmg_Diffuse_Map and vertex alpha damage blending
//   TRANSPARENT    - outputs alpha from diffuse texture instead of 1.0
//   NO_VERTEX_COLOR - disables vertex colour modulation (SimpleMetal variant)

uniform vec4 lightDiffuse;
uniform vec4 lightSpecular;
uniform float exponent;
uniform vec4 ambient;

uniform sampler2D Diffuse_Map;
uniform sampler2D Specular_Map;
#ifdef HAS_DAMAGE
uniform sampler2D Dmg_Diffuse_Map;
#endif

in vec4 vPos;
in vec4 vCols;
in vec3 vNorm;
in vec4 vLightPos;
in vec3 vEyePos;
in vec2 vUv;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec3 N = normalize(vNorm);
    vec3 EyeDir = normalize(vEyePos - vPos.xyz);
    vec3 LightDir = normalize(vLightPos.xyz - (vPos * vLightPos.w).xyz);
    vec3 HalfAngle = normalize(LightDir + EyeDir);

    float NdotL = dot(LightDir, N);
    float NdotH = dot(HalfAngle, N);
    float diffFactor = max(0.0, NdotL);
    float specFactor = (NdotL > 0.0 && NdotH > 0.0) ? pow(max(0.0, NdotH), exponent) : 0.0;

#ifdef HAS_DAMAGE
    vec4 textColour = texture(Diffuse_Map, vUv) * (1.0 - vCols.a);
    textColour += texture(Dmg_Diffuse_Map, vUv) * vCols.a;
#else
    vec4 textColour = texture(Diffuse_Map, vUv);
#endif

#ifndef NO_VERTEX_COLOR
    textColour *= (1.0 - vCols.b / 3.0);
    vec4 specColour = texture(Specular_Map, vUv) + vCols.b / 3.0 - vCols.a / 2.0;
#else
    vec4 specColour = texture(Specular_Map, vUv);
#endif

    fragColor = mix(lightDiffuse * textColour * diffFactor + textColour * ambient,
                    lightSpecular * specFactor, specColour);
#ifdef TRANSPARENT
    fragColor.a = texture(Diffuse_Map, vUv).a;
#else
    fragColor.a = 1.0;
#endif
}
