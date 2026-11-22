// Environment map filtering for image based lighting (ROADMAP M5.2), see GfxPbrMaps::PrefilterEnvMap().
// Renders one face of a cube map level from a fullscreen quad.
//  DOWNSAMPLE: 2x2 box of the next larger chain texture (bilinear sample at the texel centre)
//  PREFILTER:  GGX importance sampling (Karis 2013) - each prefiltered mip is one perceptual roughness, the way
//              RTSS's image_based_lighting reads it (lod = levels * r * (2 - r)).

OGRE_NATIVE_GLSL_VERSION_DIRECTIVE
#include <OgreUnifiedShader.h>

OGRE_UNIFORMS(
    uniform vec4 envParams; // x: face (0-5: +X -X +Y -Y +Z -Z), y: GGX alpha (roughness^2), z: sample count
)
SAMPLERCUBE(srcEnv, 0);

#define PI 3.14159265

// Direction through a texel of a cube face (OpenGL cube map convention), uv in [0,1], v = 0 at the top
vec3 FaceDirection(float face, vec2 uv)
{
    float sc = uv.x * 2.0 - 1.0;
    float tc = uv.y * 2.0 - 1.0;
    vec3 d;
    if (face < 0.5)      d = vec3( 1.0, -tc, -sc);
    else if (face < 1.5) d = vec3(-1.0, -tc,  sc);
    else if (face < 2.5) d = vec3( sc,  1.0,  tc);
    else if (face < 3.5) d = vec3( sc, -1.0, -tc);
    else if (face < 4.5) d = vec3( sc, -tc,  1.0);
    else                 d = vec3(-sc, -tc, -1.0);
    return normalize(d);
}

// Van der Corput radical inverse without bit operations (GLSL 1.20 / legacy GL)
float RadicalInverse(int i)
{
    float r = 0.0;
    float f = 0.5;
    int n = i;
    for (int k = 0; k < 12; k++)
    {
        if (n <= 0)
            break;
        int half_n = n / 2;
        r += f * float(n - 2 * half_n);
        n = half_n;
        f *= 0.5;
    }
    return r;
}

vec3 ImportanceSampleGGX(vec2 xi, float a, vec3 n)
{
    float phi = 2.0 * PI * xi.x;
    float cos_theta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
    vec3 h = vec3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
    vec3 up = (abs(n.z) < 0.999) ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tx = normalize(cross(up, n));
    vec3 ty = cross(n, tx);
    return tx * h.x + ty * h.y + n * h.z;
}

MAIN_PARAMETERS
IN(vec2 oUv, TEXCOORD0)
MAIN_DECLARATION
{
    vec3 n = FaceDirection(envParams.x, oUv);
    vec3 colour;
#ifdef PREFILTER
    if (envParams.y <= 0.0001)
    {
        colour = textureCubeLod(srcEnv, n, 0.0).rgb; // Mirror: plain copy
    }
    else
    {
        // N = V = R (Karis' split-sum assumption)
        int count = int(envParams.z);
        vec3 sum = vec3_splat(0.0);
        float weight = 0.0;
        for (int i = 0; i < 256; i++)
        {
            if (i >= count)
                break;
            vec2 xi = vec2((float(i) + 0.5) / float(count), RadicalInverse(i));
            vec3 h = ImportanceSampleGGX(xi, envParams.y, n);
            vec3 l = 2.0 * dot(n, h) * h - n;
            float n_dot_l = dot(n, l);
            if (n_dot_l > 0.0)
            {
                sum += textureCubeLod(srcEnv, l, 0.0).rgb * n_dot_l;
                weight += n_dot_l;
            }
        }
        colour = sum / max(weight, 0.0001);
    }
#else
    colour = textureCubeLod(srcEnv, n, 0.0).rgb; // Bilinear at the texel centre of the half size target = 2x2 box
#endif
    // Guard against NaN/negative values from the scene (one bad texel would smear over a whole rough level)
    colour = clamp(colour, vec3_splat(0.0), vec3_splat(64000.0));
    gl_FragColor = vec4(colour, 1.0);
}
