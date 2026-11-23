// Rigs of Rods: normal mapping without tangents (vehicle flexbodies deform on the CPU and carry no tangents).
// Cotangent frame from screen-space derivatives: Christian Schüler, "Followup: Normal Mapping Without
// Precomputed Tangents" (2013). Not part of the OGRE RTSS media - see source/main/gfx/RTSSNormalMap.cpp.

// normalTex: tangent space, OpenGL convention (+Y), linear (not sRGB)
void RoR_NormalMapTL(in vec3 vNormal, in vec3 viewPos, in vec2 uv, in sampler2D normalTex, out vec3 outNormal)
{
    vec3 N = normalize(vNormal);
    vec3 dp1 = dFdx(viewPos);
    vec3 dp2 = dFdy(viewPos);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    float len2 = max(dot(T, T), dot(B, B));
    if (len2 <= 1e-20)
    {
        outNormal = N; // No uv gradient (degenerate mapping)
        return;
    }
    // Sign of the determinant: -1 when rendering upside down into a texture (OGRE flips the projection then, dFdy
    // runs the other way) or on back faces - without it bumps turn into dents
    float invmax = inversesqrt(len2) * (dot(N, cross(dp1, dp2)) < 0.0 ? -1.0 : 1.0);

    vec3 nm = texture2D(normalTex, uv).xyz * 2.0 - 1.0;
    // B points along +v, which is down the image in OGRE; green (+Y) points up the image
    outNormal = normalize(T * (invmax * nm.x) - B * (invmax * nm.y) + N * nm.z);
}
