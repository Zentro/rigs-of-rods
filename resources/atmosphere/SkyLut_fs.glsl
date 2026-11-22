// RoR atmosphere - renders the sky-view LUT (lat-long radiance around the camera).

OGRE_NATIVE_GLSL_VERSION_DIRECTIVE
#include <OgreUnifiedShader.h>
#include "Atmosphere.glsl"

uniform vec4 atmoSunDir;   // xyz: direction towards the sun (world)
uniform vec4 atmoParams;   // x: camera altitude (m), y: haze (mie multiplier), z: sun irradiance, w: ground albedo

MAIN_PARAMETERS
IN(vec2 oUv, TEXCOORD0)
MAIN_DECLARATION
{
    vec3 dir = AtmoLutUvToDir(oUv);
    vec3 ro = vec3(0.0, PLANET_R + max(atmoParams.x, 1.0), 0.0);
    vec3 L = AtmoSkyRadiance(ro, dir, normalize(atmoSunDir.xyz), atmoParams.z, atmoParams.y, atmoParams.w);
    gl_FragColor = vec4(L, 1.0);
}
