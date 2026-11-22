// RoR atmosphere - shared physically based sky model (single scattering + multiple scattering approximation).
// Earth-like parameters from Hillaire 2020, "A Scalable and Production Ready Sky and Atmosphere Rendering Technique".
// Keep in sync with the CPU mirror in GfxAtmosphere.cpp.
// Units: meters. World is Y-up; the planet centre is PLANET_R below the camera's sea level.

#define ATMO_PI 3.14159265359

#if !defined(OGRE_HLSL)
#define atan2 atan
#endif
#define PLANET_R 6360000.0
#define ATMOS_R  6460000.0

#define RAYLEIGH_SCAT vec3(5.802e-6, 13.558e-6, 33.1e-6)
#define RAYLEIGH_H    8000.0
#define MIE_SCAT      3.996e-6
#define MIE_EXT       8.396e-6
#define MIE_H         1200.0
#define MIE_G         0.8
#define OZONE_ABS     vec3(0.650e-6, 1.881e-6, 0.085e-6)

// Returns (near, far) distances of ray-sphere intersection, far < 0 if missed
vec2 AtmoRaySphere(vec3 ro, vec3 rd, float r)
{
    float b = dot(ro, rd);
    float c = dot(ro, ro) - r * r;
    float d = b * b - c;
    if (d < 0.0)
        return vec2(-1.0, -1.0);
    d = sqrt(d);
    return vec2(-b - d, -b + d);
}

// x: rayleigh, y: mie, z: ozone density at height h above ground
vec3 AtmoDensity(float h)
{
    return vec3(exp(-h / RAYLEIGH_H), exp(-h / MIE_H), max(0.0, 1.0 - abs(h - 25000.0) / 15000.0));
}

vec3 AtmoExtinction(vec3 dens, float haze)
{
    return RAYLEIGH_SCAT * dens.x + vec3_splat(MIE_EXT * haze) * dens.y + OZONE_ABS * dens.z;
}

float AtmoPhaseRayleigh(float mu)
{
    return 3.0 / (16.0 * ATMO_PI) * (1.0 + mu * mu);
}

// Cornette-Shanks
float AtmoPhaseMie(float mu, float g)
{
    float g2 = g * g;
    return 3.0 / (8.0 * ATMO_PI) * ((1.0 - g2) * (1.0 + mu * mu)) / ((2.0 + g2) * pow(max(1.0 + g2 - 2.0 * g * mu, 1e-4), 1.5));
}

// Transmittance from point `p` (planet-centred) towards `dir`, 0 if the planet blocks it
vec3 AtmoTransmittance(vec3 p, vec3 dir, float haze)
{
    vec2 g = AtmoRaySphere(p, dir, PLANET_R);
    if (g.x > 0.0)
        return vec3_splat(0.0);
    float t = AtmoRaySphere(p, dir, ATMOS_R).y;
    const int STEPS = 8;
    float ds = t / float(STEPS);
    vec3 od = vec3_splat(0.0);
    for (int i = 0; i < STEPS; i++)
    {
        vec3 s = p + dir * (ds * (float(i) + 0.5));
        od += AtmoExtinction(AtmoDensity(length(s) - PLANET_R), haze) * ds;
    }
    return exp(-od);
}

// Sky radiance seen from `ro` (planet-centred) along `rd`, for sun direction `sunDir` with irradiance `sunE`.
// Multiple scattering is approximated by an isotropic term proportional to single scattering.
vec3 AtmoSkyRadiance(vec3 ro, vec3 rd, vec3 sunDir, float sunE, float haze, float groundAlbedo)
{
    vec2 a = AtmoRaySphere(ro, rd, ATMOS_R);
    if (a.y < 0.0)
        return vec3_splat(0.0);
    float t0 = max(a.x, 0.0);
    float t1 = a.y;
    vec2 g = AtmoRaySphere(ro, rd, PLANET_R);
    bool hitGround = g.x > 0.0;
    if (hitGround)
        t1 = g.x;

    const int STEPS = 24;
    float mu = dot(rd, sunDir);
    float phR = AtmoPhaseRayleigh(mu);
    float phM = AtmoPhaseMie(mu, MIE_G);

    vec3 L = vec3_splat(0.0);
    vec3 T = vec3_splat(1.0);
    float prevT = t0;
    for (int i = 0; i < STEPS; i++)
    {
        // Quadratic step distribution: more samples near the viewer
        float f = (float(i) + 0.5) / float(STEPS);
        float t = t0 + (t1 - t0) * f * f;
        float ds = t - prevT;
        prevT = t;
        vec3 p = ro + rd * t;
        float h = length(p) - PLANET_R;
        vec3 dens = AtmoDensity(h);
        vec3 ext = AtmoExtinction(dens, haze);
        vec3 scatR = RAYLEIGH_SCAT * dens.x;
        vec3 scatM = vec3_splat(MIE_SCAT * haze) * dens.y;
        vec3 sunT = AtmoTransmittance(p, sunDir, haze);
        vec3 single = (scatR * phR + scatM * phM) * sunT;
        vec3 multi = (scatR + scatM) * (0.25 / ATMO_PI) * (sunT * 0.6 + 0.04) ; // cheap multiple-scattering approximation
        vec3 stepT = exp(-ext * ds);
        vec3 S = (single + multi) * sunE;
        // Energy-conserving integration (Hillaire 2015)
        L += T * (S - S * stepT) / max(ext, vec3_splat(1e-12));
        T *= stepT;
    }

    if (hitGround)
    {
        vec3 p = ro + rd * t1;
        vec3 n = normalize(p);
        vec3 sunT = AtmoTransmittance(p, sunDir, haze);
        L += T * sunT * sunE * saturate(dot(n, sunDir)) * groundAlbedo / ATMO_PI;
    }
    return L;
}

// Sky-view LUT mapping: u = azimuth [0,1) around Y, v = elevation with more resolution at the horizon
vec2 AtmoDirToLutUv(vec3 d)
{
    float az = atan2(d.z, d.x); // HLSL/GLSL compatible via macro below
    float u = az / (2.0 * ATMO_PI) + 0.5;
    float el = asin(clamp(d.y, -1.0, 1.0));
    float x = sqrt(abs(el) / (0.5 * ATMO_PI));
    float v = 0.5 + 0.5 * sign(el) * x;
    return vec2(u, v);
}

vec3 AtmoLutUvToDir(vec2 uv)
{
    float az = (uv.x - 0.5) * 2.0 * ATMO_PI;
    float x = (uv.y - 0.5) * 2.0;
    float el = sign(x) * x * x * 0.5 * ATMO_PI;
    return vec3(cos(el) * cos(az), sin(el), cos(el) * sin(az));
}
