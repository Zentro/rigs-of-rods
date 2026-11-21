// Fullscreen quad for compositor passes.

OGRE_NATIVE_GLSL_VERSION_DIRECTIVE
#include <OgreUnifiedShader.h>

uniform mat4 worldViewProj;

MAIN_PARAMETERS
IN(vec4 vertex, POSITION)
IN(vec2 uv0, TEXCOORD0)
OUT(vec2 oUv, TEXCOORD0)
MAIN_DECLARATION
{
    gl_Position = mul(worldViewProj, vertex);
    oUv = uv0;
}
