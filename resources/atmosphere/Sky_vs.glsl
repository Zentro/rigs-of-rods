// RoR atmosphere - sky dome (OGRE sky box centred on the camera), vertex stage.

OGRE_NATIVE_GLSL_VERSION_DIRECTIVE
#include <OgreUnifiedShader.h>

uniform mat4 worldViewProj;

MAIN_PARAMETERS
IN(vec4 vertex, POSITION)
OUT(vec3 oDir, TEXCOORD0)
MAIN_DECLARATION
{
    gl_Position = mul(worldViewProj, vertex);
    oDir = vertex.xyz; // Box is world-aligned and camera-centred: position == view direction
}
