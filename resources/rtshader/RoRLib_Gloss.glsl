// Rigs of Rods: gloss layer (smooth dielectric top layer: car paint lacquer, glass, lenses) for RTSS
// metal/roughness materials. Not part of the OGRE RTSS media - called after PBR_Lights by RoR's "RoR_Gloss" sub
// render state, see source/main/gfx/RTSSGloss.cpp. Ported from the gfx-rtss branch ("clearcoat").

// surfaceSpecular: x = base roughness, y = metallic (RTSS Cook-Torrance), z = gloss strength, w = gloss roughness
// mask: per-texel reflectivity of a legacy specular map (baked into the metal/roughness texture's alpha). It was a
// direct reflection weight in legacy RoR (NiceMetal) and is faint; here it says "lacquered", the Fresnel term below
// decides how much is reflected -> scaled up, combined with z by max().
void RoR_Gloss(in vec3 vNormal, in vec3 viewPos, in mat4 invViewMat,
                   in samplerCube envTex, in vec4 envSize,
                   in vec4 lightPos, in vec4 lightColour, in float shadow,
                   in vec4 surfaceSpecular, in float mask, inout vec3 color)
{
    float coat = max(surfaceSpecular.z, saturate(mask * 4.0));
    if (coat <= 0.0)
        return;
    float rough = clamp(surfaceSpecular.w, 0.045, 1.0);
    float a2 = rough * rough * rough * rough;

    vec3 N = normalize(vNormal);
    vec3 V = -normalize(viewPos);
    float NoV = clamp(abs(dot(N, V)), 1e-4, 1.0);
    // Schlick Fresnel, IOR 1.5 (f0 = 0.04)
    float Fv = (0.04 + 0.96 * pow(1.0 - NoV, 5.0)) * coat;

    // Reflection from the prefiltered env map (same orientation and mip mapping as RTSLib_IBL)
    vec3 R = reflect(-V, N);
    R = normalize(mul(invViewMat, vec4(R, 0.0)).xyz);
    R.z *= -1.0;
    vec3 env = textureCubeLod(envTex, R, envSize.w * rough * (2.0 - rough)).rgb;

    // Sun highlight on the coat (GGX, Kelemen visibility)
    vec3 spec = vec3_splat(0.0);
    vec3 L = normalize(lightPos.xyz - viewPos * lightPos.w);
    float NoL = saturate(dot(N, L));
    if (NoL > 0.0)
    {
        vec3 H = normalize(V + L);
        float NoH = saturate(dot(N, H));
        float LoH = saturate(dot(L, H));
        float d = NoH * NoH * (a2 - 1.0) + 1.0;
        float D = a2 / (M_PI * d * d);
        float Vis = 0.25 / max(LoH * LoH, 1e-4);
        float F = 0.04 + 0.96 * pow(1.0 - LoH, 5.0);
        // x PI: legacy light units, see SGXLib_CookTorrance.glsl
        spec = (D * Vis * F * NoL * coat * shadow * M_PI) * lightColour.rgb;
    }

    // The coat reflects Fv of the light; the base layer below gets the rest
    color = color * (1.0 - Fv) + env * Fv + spec;
}
