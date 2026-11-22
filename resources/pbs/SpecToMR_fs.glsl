// Bakes a legacy RoR specular map into a metal/roughness texture for RTSS Cook-Torrance (glTF layout):
// R = occlusion (1), G = roughness, B = metallic (the material class's, constant), A = gloss mask (gloss layer).
// In legacy RoR the specular map's rgb is a reflectivity mask (NiceMetal: lit diffuse -> highlight + cubemap
// reflection), so bright texels become smooth. See GfxPbrMaps::BakeSpecularMap().
// Ported from the gfx-rtss branch.

OGRE_NATIVE_GLSL_VERSION_DIRECTIVE
#include <OgreUnifiedShader.h>

OGRE_UNIFORMS(
    uniform vec4 bakeParams; // x: base roughness (where the specular map is black), y: roughness where it is white,
                             // z: metallic
)
SAMPLER2D(specMap, 0);

MAIN_PARAMETERS
IN(vec2 oUv, TEXCOORD0)
MAIN_DECLARATION
{
    vec4 spec = texture2D(specMap, oUv);
    float specLum = dot(spec.rgb, vec3(0.2126, 0.7152, 0.0722));
    float roughness = mix(bakeParams.x, bakeParams.y, saturate(specLum));
    float gloss = spec.a * specLum;
    gl_FragColor = vec4(1.0, clamp(roughness, 0.05, 1.0), bakeParams.z, gloss);
}
