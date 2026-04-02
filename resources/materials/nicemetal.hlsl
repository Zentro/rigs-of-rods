// nicemetal.hlsl - Converted from nicemetal.cg
// Supports: main metallic shading (with/without damage, with/without transparency),
//           cubemap reflection shading, simple metallic shading.

// Expand Cg lit() intrinsic:
//   LitY = max(NdotL, 0)
//   LitZ = NdotL > 0 ? pow(max(NdotH, 0), exp) : 0

// ---- Vertex shaders ----

void main_nicemetal_vp(
    in float2 uv        : TEXCOORD0,
    in float4 position  : POSITION,
    in float3 normal    : NORMAL,
    in float4 cols      : COLOR,

    uniform float4    lightPosition,
    uniform float3    eyePosition,
    uniform float4x4  worldviewproj,

    out float4 oClipPos  : POSITION,
    out float4 oCols     : COLOR,
    out float4 oPos      : TEXCOORD0,
    out float3 oNorm     : TEXCOORD1,
    out float4 oLightPos : TEXCOORD2,
    out float3 oEyePos   : TEXCOORD3,
    out float2 oUv       : TEXCOORD4
)
{
    oClipPos  = mul(worldviewproj, position);
    oPos      = position;
    oNorm     = normal;
    oLightPos = lightPosition;
    oEyePos   = eyePosition;
    oCols     = cols;
    oUv       = uv;
}

void reflect_nicemetal_vp(
    in float4 position  : POSITION,
    in float3 normal    : NORMAL,
    in float2 uv        : TEXCOORD0,
    in float4 cols      : COLOR,

    uniform float3   camPosition,
    uniform float4x4 world,
    uniform float4x4 worldViewProj,

    out float4 oPosition      : POSITION,
    out float2 oUv            : TEXCOORD0,
    out float3 oViewDirection : TEXCOORD1,
    out float3 oNormal        : TEXCOORD2,
    out float4 ocols          : COLOR
)
{
    oUv           = uv;
    ocols         = cols;
    oPosition     = mul(worldViewProj, position);
    oNormal       = mul((float3x3)world, normal);
    oViewDirection = mul((float3x3)world, position.xyz - camPosition);
}

// ---- Fragment shaders ----

void main_nicemetal_fp(
    in float4 pos      : TEXCOORD0,
    in float4 incol    : COLOR,
    in float3 normal   : TEXCOORD1,
    in float4 lightpos : TEXCOORD2,
    in float3 eyepos   : TEXCOORD3,
    in float2 uv       : TEXCOORD4,

    uniform float4 lightDiffuse,
    uniform float4 lightSpecular,
    uniform float  exponent,
    uniform float4 ambient,

    uniform sampler2D diffusetex    : register(s0),
    uniform sampler2D speculartex   : register(s1),
    uniform sampler2D diffusedmgtex : register(s2),

    out float4 oColor : COLOR
)
{
    float3 N        = normalize(normal);
    float3 EyeDir   = normalize(eyepos - pos.xyz);
    float3 LightDir = normalize(lightpos.xyz - (pos * lightpos.w).xyz);
    float3 HalfAngle = normalize(LightDir + EyeDir);

    float NdotL = dot(LightDir, N);
    float NdotH = dot(HalfAngle, N);
    float LitY  = max(NdotL, 0.0f);
    float LitZ  = (NdotL > 0.0f) ? pow(max(NdotH, 0.0f), exponent) : 0.0f;

    float4 textColour = tex2D(diffusetex, uv) * (1.0f - incol.a);
    textColour = textColour + tex2D(diffusedmgtex, uv) * incol.a;
    textColour = textColour * (1.0f - incol.b / 3.0f);
    float4 specColour = tex2D(speculartex, uv) + incol.b / 3.0f - incol.a / 2.0f;

    oColor   = lerp(lightDiffuse * textColour * LitY + textColour * ambient, lightSpecular * LitZ, specColour);
    oColor.a = 1.0f;
}

void main_nicemetal_transp_fp(
    in float4 pos      : TEXCOORD0,
    in float4 incol    : COLOR,
    in float3 normal   : TEXCOORD1,
    in float4 lightpos : TEXCOORD2,
    in float3 eyepos   : TEXCOORD3,
    in float2 uv       : TEXCOORD4,

    uniform float4 lightDiffuse,
    uniform float4 lightSpecular,
    uniform float  exponent,
    uniform float4 ambient,

    uniform sampler2D diffusetex    : register(s0),
    uniform sampler2D speculartex   : register(s1),
    uniform sampler2D diffusedmgtex : register(s2),

    out float4 oColor : COLOR
)
{
    float3 N        = normalize(normal);
    float3 EyeDir   = normalize(eyepos - pos.xyz);
    float3 LightDir = normalize(lightpos.xyz - (pos * lightpos.w).xyz);
    float3 HalfAngle = normalize(LightDir + EyeDir);

    float NdotL = dot(LightDir, N);
    float NdotH = dot(HalfAngle, N);
    float LitY  = max(NdotL, 0.0f);
    float LitZ  = (NdotL > 0.0f) ? pow(max(NdotH, 0.0f), exponent) : 0.0f;

    float4 textColour = tex2D(diffusetex, uv) * (1.0f - incol.a);
    textColour = textColour + tex2D(diffusedmgtex, uv) * incol.a;
    textColour = textColour * (1.0f - incol.b / 3.0f);
    float4 specColour = tex2D(speculartex, uv) + incol.b / 3.0f - incol.a / 2.0f;

    oColor   = lerp(lightDiffuse * textColour * LitY + textColour * ambient, lightSpecular * LitZ, specColour);
    oColor.a = tex2D(diffusetex, uv).a;
}

void main_nicemetal_fp_nodmg(
    in float4 pos      : TEXCOORD0,
    in float4 incol    : COLOR,
    in float3 normal   : TEXCOORD1,
    in float4 lightpos : TEXCOORD2,
    in float3 eyepos   : TEXCOORD3,
    in float2 uv       : TEXCOORD4,

    uniform float4 lightDiffuse,
    uniform float4 lightSpecular,
    uniform float  exponent,
    uniform float4 ambient,

    uniform sampler2D diffusetex  : register(s0),
    uniform sampler2D speculartex : register(s1),

    out float4 oColor : COLOR
)
{
    float3 N        = normalize(normal);
    float3 EyeDir   = normalize(eyepos - pos.xyz);
    float3 LightDir = normalize(lightpos.xyz - (pos * lightpos.w).xyz);
    float3 HalfAngle = normalize(LightDir + EyeDir);

    float NdotL = dot(LightDir, N);
    float NdotH = dot(HalfAngle, N);
    float LitY  = max(NdotL, 0.0f);
    float LitZ  = (NdotL > 0.0f) ? pow(max(NdotH, 0.0f), exponent) : 0.0f;

    float4 textColour = tex2D(diffusetex, uv);
    textColour = textColour * (1.0f - incol.b / 3.0f);
    float4 specColour = tex2D(speculartex, uv) + incol.b / 3.0f - incol.a / 2.0f;

    oColor   = lerp(lightDiffuse * textColour * LitY + textColour * ambient, lightSpecular * LitZ, specColour);
    oColor.a = 1.0f;
}

void main_nicemetal_transp_fp_nodmg(
    in float4 pos      : TEXCOORD0,
    in float4 incol    : COLOR,
    in float3 normal   : TEXCOORD1,
    in float4 lightpos : TEXCOORD2,
    in float3 eyepos   : TEXCOORD3,
    in float2 uv       : TEXCOORD4,

    uniform float4 lightDiffuse,
    uniform float4 lightSpecular,
    uniform float  exponent,
    uniform float4 ambient,

    uniform sampler2D diffusetex  : register(s0),
    uniform sampler2D speculartex : register(s1),

    out float4 oColor : COLOR
)
{
    float3 N        = normalize(normal);
    float3 EyeDir   = normalize(eyepos - pos.xyz);
    float3 LightDir = normalize(lightpos.xyz - (pos * lightpos.w).xyz);
    float3 HalfAngle = normalize(LightDir + EyeDir);

    float NdotL = dot(LightDir, N);
    float NdotH = dot(HalfAngle, N);
    float LitY  = max(NdotL, 0.0f);
    float LitZ  = (NdotL > 0.0f) ? pow(max(NdotH, 0.0f), exponent) : 0.0f;

    float4 textColour = tex2D(diffusetex, uv);
    textColour = textColour * (1.0f - incol.b / 3.0f);
    float4 specColour = tex2D(speculartex, uv) + incol.b / 3.0f - incol.a / 2.0f;

    oColor   = lerp(lightDiffuse * textColour * LitY + textColour * ambient, lightSpecular * LitZ, specColour);
    oColor.a = tex2D(diffusetex, uv).a;
}

void reflect_nicemetal_fp(
    in float2 uv            : TEXCOORD0,
    in float3 viewDirection : TEXCOORD1,
    in float3 normal        : TEXCOORD2,
    in float4 incol         : COLOR,

    uniform sampler2D   speculartex : register(s0),
    uniform samplerCUBE cubeMap     : register(s1),

    out float4 oColor : COLOR
)
{
    float3 N = normalize(normal);
    float3 R = reflect(viewDirection, N);
    R.z = -R.z;

    float4 reflectedColor = texCUBE(cubeMap, R);
    float4 emissiveColor  = tex2D(speculartex, uv) + incol.b / 3.0f - incol.a / 2.0f;

    oColor   = reflectedColor * emissiveColor;
    oColor.a = 1.0f;
}

void reflect_nicemetal_nocolor_fp(
    in float2 uv            : TEXCOORD0,
    in float3 viewDirection : TEXCOORD1,
    in float3 normal        : TEXCOORD2,

    uniform sampler2D   speculartex : register(s0),
    uniform samplerCUBE cubeMap     : register(s1),

    out float4 oColor : COLOR
)
{
    float3 N = normalize(normal);
    float3 R = reflect(viewDirection, N);
    R.z = -R.z;

    float4 reflectedColor = texCUBE(cubeMap, R);
    float4 emissiveColor  = tex2D(speculartex, uv);

    oColor   = reflectedColor * emissiveColor;
    oColor.a = 1.0f;
}

void main_simplemetal_fp(
    in float4 pos      : TEXCOORD0,
    in float3 normal   : TEXCOORD1,
    in float4 lightpos : TEXCOORD2,
    in float3 eyepos   : TEXCOORD3,
    in float2 uv       : TEXCOORD4,

    uniform float4 lightDiffuse,
    uniform float4 lightSpecular,
    uniform float  exponent,
    uniform float4 ambient,

    uniform sampler2D diffusetex  : register(s0),
    uniform sampler2D speculartex : register(s1),

    out float4 oColor : COLOR
)
{
    float3 N        = normalize(normal);
    float3 EyeDir   = normalize(eyepos - pos.xyz);
    float3 LightDir = normalize(lightpos.xyz - (pos * lightpos.w).xyz);
    float3 HalfAngle = normalize(LightDir + EyeDir);

    float NdotL = dot(LightDir, N);
    float NdotH = dot(HalfAngle, N);
    float LitY  = max(NdotL, 0.0f);
    float LitZ  = (NdotL > 0.0f) ? pow(max(NdotH, 0.0f), exponent) : 0.0f;

    float4 textColour = tex2D(diffusetex, uv);
    float4 specColour = tex2D(speculartex, uv);

    oColor   = lerp(lightDiffuse * textColour * LitY + textColour * ambient, lightSpecular * LitZ, specColour);
    oColor.a = 1.0f;
}

void main_simplemetal_transp_fp(
    in float4 pos      : TEXCOORD0,
    in float3 normal   : TEXCOORD1,
    in float4 lightpos : TEXCOORD2,
    in float3 eyepos   : TEXCOORD3,
    in float2 uv       : TEXCOORD4,

    uniform float4 lightDiffuse,
    uniform float4 lightSpecular,
    uniform float  exponent,
    uniform float4 ambient,

    uniform sampler2D diffusetex  : register(s0),
    uniform sampler2D speculartex : register(s1),

    out float4 oColor : COLOR
)
{
    float3 N        = normalize(normal);
    float3 EyeDir   = normalize(eyepos - pos.xyz);
    float3 LightDir = normalize(lightpos.xyz - (pos * lightpos.w).xyz);
    float3 HalfAngle = normalize(LightDir + EyeDir);

    float NdotL = dot(LightDir, N);
    float NdotH = dot(HalfAngle, N);
    float LitY  = max(NdotL, 0.0f);
    float LitZ  = (NdotL > 0.0f) ? pow(max(NdotH, 0.0f), exponent) : 0.0f;

    float4 textColour = tex2D(diffusetex, uv);
    float4 specColour = tex2D(speculartex, uv);

    oColor   = lerp(lightDiffuse * textColour * LitY + textColour * ambient, lightSpecular * LitZ, specColour);
    oColor.a = tex2D(diffusetex, uv).a;
}
