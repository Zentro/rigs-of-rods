// grass.hlsl - Converted from grass.cg

void main_vp(
    in float4 iPosition : POSITION,
    in float4 iColor    : COLOR,
    in float3 iUV       : TEXCOORD0,

    uniform float     time,
    uniform float     frequency,
    uniform float4    direction,
    uniform float4x4  worldViewProj,
    uniform float3    camPos,
    uniform float     fadeRange,
    uniform float4x4  texWorldViewProjMatrix0,
    uniform float4x4  texWorldViewProjMatrix1,
    uniform float4x4  texWorldViewProjMatrix2,

    out float4 oPosition    : POSITION,
    out float4 oColor       : COLOR,
    out float3 oUV          : TEXCOORD0,
    out float4 oLightPos0   : TEXCOORD1,
    out float4 oLightPos1   : TEXCOORD2,
    out float4 oLightPos2   : TEXCOORD3)
{
    float4 position  = iPosition;
    float dist       = distance(camPos.xz, position.xz);
    oColor.rgb       = iColor.rgb;
    oColor.a         = 2.0f - (2.0f * dist / fadeRange);

    float oldposx = position.x;
    if (iUV.y == 0.0f)
    {
        float offset = sin(time + oldposx * frequency);
        position += direction * offset;
    }
    oPosition = mul(worldViewProj, position);
    oUV       = iUV;
    oUV.z     = oPosition.z;

    oLightPos0 = mul(texWorldViewProjMatrix0, position);
    oLightPos1 = mul(texWorldViewProjMatrix1, position);
    oLightPos2 = mul(texWorldViewProjMatrix2, position);
}

float shadowPCF(sampler2D shadowMap, float4 shadowMapPos, float2 offset)
{
    shadowMapPos = shadowMapPos / shadowMapPos.w;
    float2 uv = shadowMapPos.xy;
    float3 o  = float3(offset, -offset.x) * 0.3f;
    float bias = 0.0f;
    float c  = (shadowMapPos.z + bias <= tex2D(shadowMap, uv.xy - o.xy).r) ? 1 : 0;
         c += (shadowMapPos.z + bias <= tex2D(shadowMap, uv.xy + o.xy).r) ? 1 : 0;
         c += (shadowMapPos.z + bias <= tex2D(shadowMap, uv.xy + o.zy).r) ? 1 : 0;
         c += (shadowMapPos.z + bias <= tex2D(shadowMap, uv.xy - o.zy).r) ? 1 : 0;
    return c / 4;
}

void main_fp(
    in float3 uv        : TEXCOORD0,
    in float4 iColor    : COLOR,
    in float4 LightPos0 : TEXCOORD1,
    in float4 LightPos1 : TEXCOORD2,
    in float4 LightPos2 : TEXCOORD3,

    uniform sampler2D diffuseMap : register(s0),
    uniform sampler2D shadowMap0 : register(s1),
    uniform sampler2D shadowMap1 : register(s2),
    uniform sampler2D shadowMap2 : register(s3),

    uniform float4 invShadowMapSize0,
    uniform float4 invShadowMapSize1,
    uniform float4 invShadowMapSize2,
    uniform float4 pssmSplitPoints,

    out float4 oColor : COLOR)
{
    float4 diffTex = tex2D(diffuseMap, uv);
    float3 diffClr = diffTex.xyz;

    float shadowing = 1.0f;
    if (uv.z <= pssmSplitPoints.y)
        shadowing = shadowPCF(shadowMap0, LightPos0, invShadowMapSize0.xy);
    else if (uv.z <= pssmSplitPoints.z)
        shadowing = shadowPCF(shadowMap1, LightPos1, invShadowMapSize1.xy);
    else
        shadowing = shadowPCF(shadowMap2, LightPos2, invShadowMapSize2.xy);

    float3 clr = diffClr * (0.65f + 0.35f * shadowing);
    oColor = float4(clr, diffTex.a) * iColor;
}
