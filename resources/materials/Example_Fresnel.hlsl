// Example_Fresnel.hlsl - Converted from Example_Fresnel.cg

void main_vp(
    in float4 pos    : POSITION,
    in float4 normal : NORMAL,
    in float2 tex    : TEXCOORD0,

    uniform float4x4 worldViewProjMatrix,
    uniform float3   eyePosition,
    uniform float    fresnelBias,
    uniform float    fresnelScale,
    uniform float    fresnelPower,
    uniform float    timeVal,
    uniform float    scale,
    uniform float    scroll,
    uniform float    noise,

    out float4 oPos           : POSITION,
    out float  fresnel        : COLOR,
    out float3 noiseCoord     : TEXCOORD0,
    out float4 projectionCoord : TEXCOORD1)
{
    oPos = mul(worldViewProjMatrix, pos);
    // Projective texture coordinates, adjust for mapping
    float4x4 scalemat = float4x4(0.5,    0,   0, 0.5,
                                  0, -0.5,   0, 0.5,
                                  0,    0, 0.5, 0.5,
                                  0,    0,   0,   1);
    projectionCoord = mul(scalemat, oPos);
    // Noise map coords
    noiseCoord.xy = (tex + (timeVal * scroll)) * scale;
    noiseCoord.z  = noise * timeVal;

    // calc fresnel factor
    float3 eyeDir = normalize(pos.xyz - eyePosition);
    fresnel = fresnelBias + fresnelScale * pow(1 + dot(eyeDir, normal.xyz), fresnelPower);
}

void main_fp(
    in float  fresnel         : COLOR,
    in float3 noiseCoord      : TEXCOORD0,
    in float4 projectionCoord : TEXCOORD1,

    uniform float     distortionRange,
    uniform float4    tintColour,
    uniform sampler3D noiseMap   : register(s0),
    uniform sampler2D reflectMap : register(s1),
    uniform sampler2D refractMap : register(s2),

    out float4 col : COLOR)
{
    float3 yoffset = float3(0.31, 0.58, 0.23);
    float2 distort;
    distort.x = tex3D(noiseMap, noiseCoord).x;
    distort.y = tex3D(noiseMap, noiseCoord + yoffset).x;
    distort   = (distort * 2 - 1) * distortionRange;

    float2 final = projectionCoord.xy / projectionCoord.w;
    final += distort;

    float4 reflectionColour = tex2D(reflectMap, final);
    float4 refractionColour = tex2D(refractMap, final) + tintColour;
    col = lerp(refractionColour, reflectionColour, fresnel);
}

void main_simplerfp(
    in float  fresnel         : COLOR,
    in float3 noiseCoord      : TEXCOORD0,
    in float4 projectionCoord : TEXCOORD1,

    uniform float     distortionRange,
    uniform float4    tintColour,
    uniform sampler3D noiseMap   : register(s0),
    uniform sampler2D reflectMap : register(s1),

    out float4 col : COLOR)
{
    float3 yoffset = float3(0.31, 0.58, 0.23);
    float2 distort;
    distort.x = tex3D(noiseMap, noiseCoord).x;
    distort.y = tex3D(noiseMap, noiseCoord + yoffset).x;
    distort   = (distort * 2 - 1) * distortionRange;

    float2 final = projectionCoord.xy / projectionCoord.w;
    final += distort;

    float4 reflectionColour = tex2D(reflectMap, final);
    col   = lerp(tintColour, reflectionColour, 0.33 + fresnel);
    col.a = 0.33 + fresnel;
}
