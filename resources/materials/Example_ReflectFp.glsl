// Fresnel water: reflection only (no refraction), distorted by a 3D noise texture, alpha from the Fresnel factor.
// 1:1 GLSL/HLSL port of RoR's former Cg 'main_simplerfp' (Example_Fresnel.cg).
#include <OgreUnifiedShader.h>

SAMPLER3D(noiseMap, 0);
SAMPLER2D(reflectMap, 1);

OGRE_UNIFORMS(
uniform float distortionRange;
uniform vec4 tintColour;
)

MAIN_PARAMETERS
IN(vec3 noiseCoord, TEXCOORD0)
IN(vec4 projectionCoord, TEXCOORD1)
IN(float fresnel, TEXCOORD2)
MAIN_DECLARATION
{
	// Randomly chosen offset for y noise sample
	vec3 yoffset = vec3(0.31, 0.58, 0.23);
	vec2 distort;
	// Sample the noise texture at 2 places
	distort.x = texture3D(noiseMap, noiseCoord).x;
	distort.y = texture3D(noiseMap, noiseCoord + yoffset).x;
	// Scale the distortion from [0,1] to [-range,range]
	distort = (distort * 2.0 - 1.0) * distortionRange;

	// Do the tex projection manually so we can distort _after_
	vec2 final = projectionCoord.xy / projectionCoord.w;
	final += distort;

	vec4 reflectionColour = texture2D(reflectMap, final);
	gl_FragColor = mix(tintColour, reflectionColour, 0.33 + fresnel);
	gl_FragColor.a = 0.33 + fresnel;
}
