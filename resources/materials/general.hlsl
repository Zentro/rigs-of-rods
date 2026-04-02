// general.hlsl - Converted from general.cg

void ambient_vs(
    in float4 p  : POSITION,
    in float3 uv : TEXCOORD0,

    uniform float4x4 wvpMat,

    out float4 oPos : POSITION,
    out float2 oUV  : TEXCOORD0)
{
    oPos = mul(wvpMat, p);
    oUV  = uv.xy;
}

float4 ambient_ps(
    in float2 uv : TEXCOORD0,
    uniform float3    ambient,
    uniform float4    matDif,
    uniform sampler2D diffuseMap : register(s0)) : COLOR0
{
    float4 diffuseTex = tex2D(diffuseMap, uv);
    return float4(ambient * matDif.rgb * diffuseTex.rgb, diffuseTex.a);
}

// ---- render (ProceduralRoad) ----

struct VIn  { float4 p : POSITION; float3 n : NORMAL; float3 t : TANGENT; float3 uv : TEXCOORD0; float4 c : COLOR; };
struct VOut { float4 p : POSITION; float3 uv : TEXCOORD0; float4 wp : TEXCOORD1; float3 n : TEXCOORD2; float3 t : TEXCOORD3; float3 b : TEXCOORD4; float4 c : COLOR; };
struct PIn  {                       float3 uv : TEXCOORD0; float4 wp : TEXCOORD1; float3 n : TEXCOORD2; float3 t : TEXCOORD3; float3 b : TEXCOORD4; float4 c : COLOR; };

VOut render_vs(VIn IN,
    uniform float4x4 wMat,
    uniform float4x4 wvpMat)
{
    VOut OUT;
    OUT.uv = IN.uv;
    OUT.wp = mul(wMat, IN.p);
    OUT.p  = mul(wvpMat, IN.p);
    OUT.n  = IN.n;
    OUT.c  = IN.c;
    OUT.t  = 0;
    OUT.b  = 0;
    return OUT;
}

float4 render_ps(PIn IN,
    uniform float3 ambient,
    uniform float4 matDif) : COLOR0
{
    float bridge = IN.c.x, pipe = IN.c.y;
    float border  = lerp(IN.c.z, 1, pipe);
    float norm    = abs(IN.n.y);

    float pwr     = lerp(lerp(8, 8, bridge), 4, pipe);
    float ter     = lerp(lerp(1, 0, bridge), 0, pipe);
    float diffuse = 1 - lerp(1 - lerp(pow(norm, pwr), pow(norm, pwr), pipe),
                             pow(1.f - 2.f * acos(norm) / 3.141592654f, pwr), ter);
    float3 clrLi  = ambient + diffuse * matDif.rgb;

    float3 clr = lerp(lerp(
        float3(1, 1, 1)    * clrLi,
        float3(0, 0.8, 1)  * (0.4 + 0.7 * clrLi), bridge),
        float3(1, 0.8, 0)  * (0.2 + 1.0 * clrLi), pipe);
    return float4(clr, 1);
}

float4 render_gr_ps(PIn IN) : COLOR0
{
    float bridge = IN.c.w;
    return float4(bridge * float3(1, 1, 1), bridge);
}

// ---- diffuse (no shadows) ----

VOut diffuse_vs(VIn IN,
    uniform float4x4 wMat,
    uniform float4x4 wvpMat,
    uniform float4   fogParams)
{
    VOut OUT;
    OUT.uv = IN.uv;
    OUT.wp = mul(wMat, IN.p);
    OUT.p  = mul(wvpMat, IN.p);
    OUT.n  = IN.n;
    OUT.t  = IN.t;
    OUT.b  = cross(IN.t, IN.n);
    OUT.c  = IN.c;
    OUT.wp.w = saturate(fogParams.x * (OUT.p.z - fogParams.y) * fogParams.w);
    return OUT;
}

float4 diffuse_ps(PIn IN,
    uniform float3   ambient,
    uniform float3   lightDif0,
    uniform float3   lightSpec0,
    uniform float4   matDif,
    uniform float4   matSpec,
    uniform float    matShininess,
    uniform float3   fogColor,
    uniform float4   lightPos0,
    uniform float3   camPos,
    uniform float4   invSMSize,
    uniform float4x4 iTWMat,

    uniform sampler2D diffuseMap : register(s0),
    uniform sampler2D normalMap  : register(s1)) : COLOR0
{
    float3 ldir = normalize(lightPos0.xyz - (lightPos0.w * IN.wp.xyz));

    float4 normalTex = tex2D(normalMap, IN.uv.xy);
    float3x3 tbn  = float3x3(IN.t, IN.b, IN.n);
    float3 normal = mul(transpose(tbn), normalTex.xyz * 2.f - 1.f);
    normal = normalize(mul((float3x3)iTWMat, normal));

    float3 diffuse  = max(dot(ldir, normal), 0);
    float3 camDir   = normalize(camPos - IN.wp.xyz);
    float3 halfVec  = normalize(ldir + camDir);
    float3 specular = pow(max(dot(normal, halfVec), 0), matShininess);

    float4 diffuseTex = tex2D(diffuseMap, IN.uv.xy);

    float3 diffC   = diffuse * lightDif0 * matDif.rgb * diffuseTex.rgb;
    float3 specC   = specular * lightSpec0 * matSpec.rgb;
    float3 clrSUM  = diffuseTex.rgb * ambient + diffC + specC;

    clrSUM = lerp(clrSUM, fogColor, IN.wp.w);
    return float4(clrSUM, diffuseTex.a);
}

float4 diffuse_ps_env(PIn IN,
    uniform float4   envPars,
    uniform float3   ambient,
    uniform float3   lightDif0,
    uniform float3   lightSpec0,
    uniform float4   matDif,
    uniform float4   matSpec,
    uniform float    matShininess,
    uniform float3   fogColor,
    uniform float4   lightPos0,
    uniform float3   camPos,
    uniform float4   invSMSize,
    uniform float4x4 iTWMat,

    uniform sampler2D   diffuseMap : register(s0),
    uniform sampler2D   normalMap  : register(s1),
    uniform samplerCUBE envMap     : register(s2)) : COLOR0
{
    float3 ldir = normalize(lightPos0.xyz - (lightPos0.w * IN.wp.xyz));

    float4 normalTex = tex2D(normalMap, IN.uv.xy);
    float3x3 tbn  = float3x3(IN.t, IN.b, IN.n);
    float3 normal = mul(transpose(tbn), normalTex.xyz * 2.f - 1.f);
    normal = normalize(mul((float3x3)iTWMat, normal));

    float3 diffuse  = max(dot(ldir, normal), 0);
    float3 camDir   = normalize(camPos - IN.wp.xyz);
    float3 halfVec  = normalize(ldir + camDir);
    float3 specular = pow(max(dot(normal, halfVec), 0), matShininess);

    float4 diffuseTex = tex2D(diffuseMap, IN.uv.xy);
    float4 envTex     = texCUBE(envMap, reflect(-camDir, normal));

    float3 diffC   = diffuse * lightDif0 * matDif.rgb * diffuseTex.rgb;
    float3 specC   = specular * lightSpec0 * matSpec.rgb;
    float3 clrSUM  = diffuseTex.rgb * ambient + diffC + specC;

    float3 clrE = envPars.x * clrSUM + envPars.y * envTex.rgb;
    clrE = lerp(clrE, fogColor, IN.wp.w);
    return float4(clrE, diffuseTex.a);
}

// ---- diffuse with PSSM shadows ----

void diffuse_sh_vs(VIn IN,
    uniform float4x4 wMat,
    uniform float4x4 wvpMat,
    uniform float4   fogParams,
    uniform float4x4 texWVPMat0,
    uniform float4x4 texWVPMat1,
    uniform float4x4 texWVPMat2,

    out float4 oPos          : POSITION,
    out float3 oUv           : TEXCOORD0,
    out float4 oWp           : TEXCOORD1,
    out float3 oN            : TEXCOORD2,
    out float3 oT            : TEXCOORD3,
    out float3 oB            : TEXCOORD4,
    out float4 oC            : COLOR,
    out float4 oLightPos0    : TEXCOORD5,
    out float4 oLightPos1    : TEXCOORD6,
    out float4 oLightPos2    : TEXCOORD7)
{
    oUv  = IN.uv;
    oWp  = mul(wMat, IN.p);
    oPos = mul(wvpMat, IN.p);
    oUv.z = oPos.z;
    oN   = IN.n;
    oT   = IN.t;
    oB   = cross(IN.t, IN.n);
    oC   = IN.c;
    oWp.w = saturate(fogParams.x * (oPos.z - fogParams.y) * fogParams.w);

    oLightPos0 = mul(texWVPMat0, IN.p);
    oLightPos1 = mul(texWVPMat1, IN.p);
    oLightPos2 = mul(texWVPMat2, IN.p);
}

float shadowPCF(sampler2D shadowMap, float4 shadowMapPos, float2 offset)
{
    shadowMapPos = shadowMapPos / shadowMapPos.w;
    float2 uv = shadowMapPos.xy;
    float3 o  = float3(offset, -offset.x) * 0.3f;
    float shPos = shadowMapPos.z;
    float c  = (shPos <= tex2D(shadowMap, uv.xy - o.xy).r) ? 1 : 0;
         c += (shPos <= tex2D(shadowMap, uv.xy + o.xy).r) ? 1 : 0;
         c += (shPos <= tex2D(shadowMap, uv.xy + o.zy).r) ? 1 : 0;
         c += (shPos <= tex2D(shadowMap, uv.xy - o.zy).r) ? 1 : 0;
    return c / 4;
}

float4 diffuse_sh_ps(
    in float3 uv         : TEXCOORD0,
    in float4 wp         : TEXCOORD1,
    in float3 n          : TEXCOORD2,
    in float3 t          : TEXCOORD3,
    in float3 b          : TEXCOORD4,
    in float4 c          : COLOR,
    in float4 LightPos0  : TEXCOORD5,
    in float4 LightPos1  : TEXCOORD6,
    in float4 LightPos2  : TEXCOORD7,

    uniform float3   ambient,
    uniform float3   lightDif0,
    uniform float3   lightSpec0,
    uniform float4   matDif,
    uniform float4   matSpec,
    uniform float    matShininess,
    uniform float3   fogColor,
    uniform float4   lightPos0,
    uniform float3   camPos,
    uniform float4   invSMSize,
    uniform float4x4 iTWMat,

    uniform sampler2D diffuseMap  : register(s0),
    uniform sampler2D normalMap   : register(s1),
    uniform sampler2D shadowMap0  : register(s2),
    uniform sampler2D shadowMap1  : register(s3),
    uniform sampler2D shadowMap2  : register(s4),

    uniform float4 invShadowMapSize0,
    uniform float4 invShadowMapSize1,
    uniform float4 invShadowMapSize2,
    uniform float4 pssmSplitPoints) : COLOR0
{
    float3 ldir = normalize(lightPos0.xyz - (lightPos0.w * wp.xyz));

    float4 normalTex = tex2D(normalMap, uv.xy);
    float3x3 tbn  = float3x3(t, b, n);
    float3 normal = mul(transpose(tbn), normalTex.xyz * 2.f - 1.f);
    normal = normalize(mul((float3x3)iTWMat, normal));

    float3 diffuse  = max(dot(ldir, normal), 0);
    float3 camDir   = normalize(camPos - wp.xyz);
    float3 halfVec  = normalize(ldir + camDir);
    float3 specular = pow(max(dot(normal, halfVec), 0), matShininess);

    float4 diffuseTex = tex2D(diffuseMap, uv.xy);
    float3 diffC = diffuse * lightDif0 * matDif.rgb * diffuseTex.rgb;
    float3 specC = specular * lightSpec0 * matSpec.rgb;

    float shadowing = 1.0f;
    if (uv.z <= pssmSplitPoints.y)
        shadowing = shadowPCF(shadowMap0, LightPos0, invShadowMapSize0.xy);
    else if (uv.z <= pssmSplitPoints.z)
        shadowing = shadowPCF(shadowMap1, LightPos1, invShadowMapSize1.xy);
    else
        shadowing = shadowPCF(shadowMap2, LightPos2, invShadowMapSize2.xy);

    float3 clrSUM = diffuseTex.rgb * ambient + diffC * (0.25f + 0.75f * shadowing) + specC * shadowing;
    clrSUM = lerp(clrSUM, fogColor, wp.w);
    return float4(clrSUM, diffuseTex.a);
}

float4 diffuse_sh_a_ps(
    in float3 uv         : TEXCOORD0,
    in float4 wp         : TEXCOORD1,
    in float3 n          : TEXCOORD2,
    in float3 t          : TEXCOORD3,
    in float3 b          : TEXCOORD4,
    in float4 c          : COLOR,
    in float4 LightPos0  : TEXCOORD5,
    in float4 LightPos1  : TEXCOORD6,
    in float4 LightPos2  : TEXCOORD7,

    uniform float3   ambient,
    uniform float3   lightDif0,
    uniform float3   lightSpec0,
    uniform float4   matDif,
    uniform float4   matSpec,
    uniform float    matShininess,
    uniform float3   fogColor,
    uniform float4   lightPos0,
    uniform float3   camPos,
    uniform float4   invSMSize,
    uniform float4x4 iTWMat,

    uniform sampler2D diffuseMap   : register(s0),
    uniform sampler2D diffuseAlpha : register(s1),
    uniform sampler2D normalMap    : register(s2),
    uniform sampler2D shadowMap0   : register(s3),
    uniform sampler2D shadowMap1   : register(s4),
    uniform sampler2D shadowMap2   : register(s5),

    uniform float4 invShadowMapSize0,
    uniform float4 invShadowMapSize1,
    uniform float4 invShadowMapSize2,
    uniform float4 pssmSplitPoints) : COLOR0
{
    float3 ldir = normalize(lightPos0.xyz - (lightPos0.w * wp.xyz));

    float4 normalTex = tex2D(normalMap, uv.xy);
    float3x3 tbn  = float3x3(t, b, n);
    float3 normal = mul(transpose(tbn), normalTex.xyz * 2.f - 1.f);
    normal = normalize(mul((float3x3)iTWMat, normal));

    float3 diffuse  = max(dot(ldir, normal), 0);
    float3 camDir   = normalize(camPos - wp.xyz);
    float3 halfVec  = normalize(ldir + camDir);
    float3 specular = pow(max(dot(normal, halfVec), 0), matShininess);

    float4 diffuseTex = tex2D(diffuseMap, uv.xy);
    diffuseTex.a = tex2D(diffuseAlpha, uv.xy).g;

    float3 diffC = diffuse * lightDif0 * matDif.rgb * diffuseTex.rgb;
    float3 specC = specular * lightSpec0 * matSpec.rgb;

    float shadowing = 1.0f;
    if (uv.z <= pssmSplitPoints.y)
        shadowing = shadowPCF(shadowMap0, LightPos0, invShadowMapSize0.xy);
    else if (uv.z <= pssmSplitPoints.z)
        shadowing = shadowPCF(shadowMap1, LightPos1, invShadowMapSize1.xy);
    else
        shadowing = shadowPCF(shadowMap2, LightPos2, invShadowMapSize2.xy);

    float3 clrSUM = diffuseTex.rgb * ambient + diffC * (0.25f + 0.75f * shadowing) + specC * shadowing;
    clrSUM = lerp(clrSUM, fogColor, wp.w);
    return float4(clrSUM, diffuseTex.a);
}
