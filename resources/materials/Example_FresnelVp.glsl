// Fresnel water: vertex program. 1:1 GLSL/HLSL port of RoR's former Cg 'main_vp' (Example_Fresnel.cg,
// originally from OGRE's Fresnel demo) - same parameters, same look.
#include <OgreUnifiedShader.h>

OGRE_UNIFORMS(
uniform mat4 worldViewProjMatrix;
uniform vec3 eyePosition; // object space
uniform float fresnelBias;
uniform float fresnelScale;
uniform float fresnelPower;
uniform float timeVal;
uniform float scale;  // the amount to scale the noise texture by
uniform float scroll; // the amount by which to scroll the noise
uniform float noise;  // the noise perturb as a factor of the time
)

MAIN_PARAMETERS
IN(vec4 vertex, POSITION)
IN(vec3 normal, NORMAL)
IN(vec4 uv0, TEXCOORD0)

OUT(vec3 noiseCoord, TEXCOORD0)
OUT(vec4 projectionCoord, TEXCOORD1)
OUT(float fresnel, TEXCOORD2)
MAIN_DECLARATION
{
	gl_Position = mul(worldViewProjMatrix, vertex);

	// Projective texture coordinates, adjust for mapping
	mat4 scalemat = mtxFromRows(
		vec4(0.5,  0.0, 0.0, 0.5),
		vec4(0.0, -0.5, 0.0, 0.5),
		vec4(0.0,  0.0, 0.5, 0.5),
		vec4(0.0,  0.0, 0.0, 1.0));
	projectionCoord = mul(scalemat, gl_Position);

	// Noise map coords
	noiseCoord.xy = (uv0.xy + (timeVal * scroll)) * scale;
	noiseCoord.z = noise * timeVal;

	// Fresnel factor (reflection coefficient). Clamped like the former Cg 'COLOR' output was.
	vec3 eyeDir = normalize(vertex.xyz - eyePosition);
	fresnel = saturate(fresnelBias + fresnelScale * pow(1.0 + dot(eyeDir, normal), fresnelPower));
}
