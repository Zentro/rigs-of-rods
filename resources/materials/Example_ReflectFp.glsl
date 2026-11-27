#include <OgreUnifiedShader.h>

// Reflection-only variant of OGRE's Example_FresnelFp.glsl (water mode "Reflect": no refraction texture, the water is
// see-through instead)

SAMPLER2D(noiseMap, 0);
SAMPLER2D(reflectMap, 1);

OGRE_UNIFORMS(
uniform vec4 tintColour;
uniform float noiseScale;
uniform float fresnelBias;
uniform float fresnelScale;
uniform float fresnelPower;
)

MAIN_PARAMETERS
IN(vec3 noiseCoord, TEXCOORD0)
IN(vec4 projectionCoord, TEXCOORD1)
IN(vec3 eyeDir, TEXCOORD2)
IN(vec3 oNormal, TEXCOORD3)
MAIN_DECLARATION
{
	vec2 final = projectionCoord.xy / projectionCoord.w;

	vec3 noiseNormal = (texture2D(noiseMap, (noiseCoord.xy / 5.0)).rgb - 0.5).rbg * noiseScale;
	final += noiseNormal.xz;

	float fresnel = fresnelBias + fresnelScale * pow(1.0 + dot(eyeDir, oNormal), fresnelPower);
	float opacity = clamp(0.33 + fresnel, 0.0, 1.0);

	vec4 reflectionColour = texture2D(reflectMap, final);
	gl_FragColor = mix(tintColour, reflectionColour, opacity);
	gl_FragColor.a = opacity;
}
