#version 150 core
// Preprocessor defines (set via OGRE preprocessor_defines):
//   HAS_DMG=1        - use damage texture blend (3 textures)
//   HAS_VERT_COLOUR=1 - apply vertex colour adjustments
//   TRANSPARENT=1    - output alpha from diffuse texture

in vec4 vPos;
in vec4 vColour;
in vec3 vNorm;
in vec4 vLightPos;
in vec3 vEyePos;
in vec2 vUv;

uniform vec4 lightDiffuse;
uniform vec4 lightSpecular;
uniform float exponent;
uniform vec4 ambient;

uniform sampler2D diffusetex;
uniform sampler2D speculartex;
#if HAS_DMG
uniform sampler2D diffusedmgtex;
#endif

out vec4 fragColor;

void main()
{
    vec3 N         = normalize(vNorm);
    vec3 EyeDir    = normalize(vEyePos - vPos.xyz);
    vec3 LightDir  = normalize(vLightPos.xyz - (vPos * vLightPos.w).xyz);
    vec3 HalfAngle = normalize(LightDir + EyeDir);

    float NdotL = dot(LightDir, N);
    float NdotH = dot(HalfAngle, N);
    float LitY  = max(NdotL, 0.0);
    float LitZ  = (NdotL > 0.0) ? pow(max(NdotH, 0.0), exponent) : 0.0;

#if HAS_DMG && HAS_VERT_COLOUR
    vec4 textColour = texture(diffusetex, vUv) * (1.0 - vColour.a);
    textColour      = textColour + texture(diffusedmgtex, vUv) * vColour.a;
    textColour      = textColour * (1.0 - vColour.b / 3.0);
    vec4 specColour = texture(speculartex, vUv) + vColour.b / 3.0 - vColour.a / 2.0;
#elif HAS_VERT_COLOUR
    vec4 textColour = texture(diffusetex, vUv);
    textColour      = textColour * (1.0 - vColour.b / 3.0);
    vec4 specColour = texture(speculartex, vUv) + vColour.b / 3.0 - vColour.a / 2.0;
#else
    vec4 textColour = texture(diffusetex, vUv);
    vec4 specColour = texture(speculartex, vUv);
#endif

    fragColor = mix(lightDiffuse * textColour * LitY + textColour * ambient,
                    lightSpecular * LitZ, specColour);
#if TRANSPARENT
    fragColor.a = texture(diffusetex, vUv).a;
#else
    fragColor.a = 1.0;
#endif
}
