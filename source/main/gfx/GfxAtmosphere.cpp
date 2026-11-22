/*
    This source file is part of Rigs of Rods
    Copyright 2026 The Rigs of Rods team

    For more information, see http://www.rigsofrods.org/

    Rigs of Rods is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3, as
    published by the Free Software Foundation.

    Rigs of Rods is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Rigs of Rods. If not, see <http://www.gnu.org/licenses/>.
*/

#include "GfxAtmosphere.h"

#include "CameraManager.h"
#include "GfxScene.h"
#include "Terrain.h"

#include <Ogre.h>
#include <OgreRectangle2D.h>

using namespace Ogre;
using namespace RoR;

// Keep in sync with resources/atmosphere/Atmosphere.glsl
static const float PLANET_R = 6360000.f;
static const float ATMOS_R = 6460000.f;
static const Vector3 RAYLEIGH_SCAT(5.802e-6f, 13.558e-6f, 33.1e-6f);
static const float RAYLEIGH_H = 8000.f;
static const float MIE_SCAT = 3.996e-6f;
static const float MIE_EXT = 8.396e-6f;
static const float MIE_H = 1200.f;
static const float MIE_G = 0.8f;
static const Vector3 OZONE_ABS(0.650e-6f, 1.881e-6f, 0.085e-6f);
static const float GROUND_ALBEDO = 0.25f;

// Sun irradiance in RoR light units: a light colour of 1 lights like the legacy fixed-function pipeline did,
// which corresponds to an irradiance of PI (Lambert divides by PI).
static const float SUN_E = Math::PI;
static const float SUN_DISC_RADIANCE = 40.f; // Capped (real ~46000) - still bright enough to clip under tonemapping

static float WrapHours(float h) { return std::fmod(std::fmod(h, 24.f) + 24.f, 24.f); }
static const int LUT_WIDTH = 256;
static const int LUT_HEIGHT = 128;

static Vector3 Exp(Vector3 v) { return Vector3(std::exp(v.x), std::exp(v.y), std::exp(v.z)); }

static Vector2 RaySphere(Vector3 ro, Vector3 rd, float r)
{
    float b = ro.dotProduct(rd);
    float c = ro.dotProduct(ro) - r * r;
    float d = b * b - c;
    if (d < 0.f)
        return Vector2(-1.f, -1.f);
    d = std::sqrt(d);
    return Vector2(-b - d, -b + d);
}

static Vector3 Density(float h)
{
    return Vector3(std::exp(-h / RAYLEIGH_H), std::exp(-h / MIE_H), std::max(0.f, 1.f - std::abs(h - 25000.f) / 15000.f));
}

static Vector3 Extinction(Vector3 dens, float haze)
{
    return RAYLEIGH_SCAT * dens.x + Vector3(MIE_EXT * haze * dens.y) + OZONE_ABS * dens.z;
}

static float PhaseRayleigh(float mu) { return 3.f / (16.f * Math::PI) * (1.f + mu * mu); }

static float PhaseMie(float mu, float g)
{
    float g2 = g * g;
    return 3.f / (8.f * Math::PI) * ((1.f - g2) * (1.f + mu * mu))
        / ((2.f + g2) * std::pow(std::max(1.f + g2 - 2.f * g * mu, 1e-4f), 1.5f));
}

bool GfxAtmosphere::IsWanted()
{
    return App::GetGfxScene()->GetPipeline().IsActive();
}

GfxAtmosphere::GfxAtmosphere(Ogre::Light* main_light)
    : m_main_light(main_light)
{
    m_time_of_day = WrapHours(App::gfx_sky_time->getFloat());

    GfxAtmosphere::EnsureDummyCube();
    this->CreateSkyViewLut();

    m_sky_material = MaterialManager::getSingleton().getByName("RoR/Atmosphere/Sky", RGN_DEFAULT);
    if (m_sky_material)
    {
        // Rebind: the units may still point at placeholders created before our textures existed
        Pass* pass = m_sky_material->getTechnique(0)->getPass(0);
        pass->getTextureUnitState("DummyCube")->setTexture(TextureManager::getSingleton().getByName("RoR/Atmosphere/DummyCube", RGN_DEFAULT));
        pass->getTextureUnitState("SkyLut")->setTexture(m_lut_texture);
        m_sky_material->load();
        if (m_sky_material->getNumSupportedTechniques() == 0)
        {
            LOG(fmt::format("[RoR|Atmosphere] ERROR: sky material unsupported: {}", m_sky_material->getUnsupportedTechniquesExplanation()));
        }
        else
        {
            // Sampler units for render systems without shader-side bindings (legacy GL)
            GpuProgramParametersSharedPtr fp = m_sky_material->getTechnique(0)->getPass(0)->getFragmentProgramParameters();
            fp->setIgnoreMissingParams(true);
            fp->setNamedConstant("skyLut", 1);
        }
    }
    App::GetGfxScene()->GetSceneManager()->setSkyBox(true, "RoR/Atmosphere/Sky", 5000.f, /*drawFirst=*/true,
        Quaternion::IDENTITY, RGN_DEFAULT);

    this->Update(0.f);
    LOG(fmt::format("[RoR|Atmosphere] Created, time of day {:.2f} h", m_time_of_day));
}

GfxAtmosphere::~GfxAtmosphere()
{
    App::GetGfxScene()->GetSceneManager()->setSkyBox(false, "");
    if (m_lut_scene)
    {
        Root::getSingleton().destroySceneManager(m_lut_scene);
        m_lut_scene = nullptr;
    }
    if (m_lut_texture)
    {
        TextureManager::getSingleton().remove(m_lut_texture);
        m_lut_texture.reset();
    }
}

void GfxAtmosphere::EnsureDummyCube()
{
    // A placeholder may exist (referenced by the sky material script), replace it.
    TexturePtr existing = TextureManager::getSingleton().getByName("RoR/Atmosphere/DummyCube", RGN_DEFAULT);
    if (existing && !existing->isManuallyLoaded())
    {
        TextureManager::getSingleton().remove(existing);
        existing.reset();
    }
    if (!existing)
    {
        TexturePtr tex = TextureManager::getSingleton().createManual("RoR/Atmosphere/DummyCube", RGN_DEFAULT,
            TEX_TYPE_CUBE_MAP, 1, 1, 0, PF_BYTE_RGBA, TU_STATIC_WRITE_ONLY);
        uint8_t px[4] = { 0, 0, 0, 255 };
        for (int face = 0; face < 6; face++)
            tex->getBuffer(face, 0)->blitFromMemory(PixelBox(1, 1, 1, PF_BYTE_RGBA, px));
    }
}

void GfxAtmosphere::CreateSkyViewLut()
{
    // The sky material references this texture by name, so a placeholder (not a render target) may exist.
    m_lut_texture = TextureManager::getSingleton().getByName("RoR/Atmosphere/SkyViewLut", RGN_DEFAULT);
    if (m_lut_texture && !(m_lut_texture->getUsage() & TU_RENDERTARGET))
    {
        TextureManager::getSingleton().remove(m_lut_texture);
        m_lut_texture.reset();
    }
    if (!m_lut_texture)
    {
        m_lut_texture = TextureManager::getSingleton().createManual("RoR/Atmosphere/SkyViewLut", RGN_DEFAULT,
            TEX_TYPE_2D, LUT_WIDTH, LUT_HEIGHT, 0, PF_FLOAT16_RGBA, TU_RENDERTARGET);
    }

    // Private scene with just a fullscreen rectangle - keeps the LUT pass away from shadows, RTSS etc...
    m_lut_scene = Root::getSingleton().createSceneManager();
    m_lut_camera = m_lut_scene->createCamera("RoR/Atmosphere/LutCamera");
    m_lut_scene->getRootSceneNode()->attachObject(m_lut_camera);
    Rectangle2D* rect = m_lut_scene->createScreenSpaceRect(/*includeTextureCoords=*/true);
    rect->setCorners(-1.f, 1.f, 1.f, -1.f);
    rect->setBoundingBox(AxisAlignedBox::BOX_INFINITE);
    m_lut_material = MaterialManager::getSingleton().getByName("RoR/Atmosphere/SkyLut", RGN_DEFAULT);
    rect->setMaterial(m_lut_material);
    m_lut_scene->getRootSceneNode()->attachObject(rect);

    RenderTarget* rt = m_lut_texture->getBuffer()->getRenderTarget();
    rt->removeAllViewports();
    rt->setAutoUpdated(false);
    Viewport* vp = rt->addViewport(m_lut_camera);
    vp->setClearEveryFrame(false);
    vp->setOverlaysEnabled(false);
    vp->setShadowsEnabled(false);
    vp->setSkiesEnabled(false);
}

void GfxAtmosphere::RenderSkyViewLut()
{
    if (!m_lut_material)
        return;

    m_lut_material->load();
    GpuProgramParametersSharedPtr fp = m_lut_material->getTechnique(0)->getPass(0)->getFragmentProgramParameters();
    fp->setIgnoreMissingParams(true);
    fp->setNamedConstant("atmoSunDir", Vector4(m_sun_dir.x, m_sun_dir.y, m_sun_dir.z, 0.f));
    fp->setNamedConstant("atmoParams", Vector4(m_lut_altitude, m_haze, SUN_E, GROUND_ALBEDO));
    m_lut_texture->getBuffer()->getRenderTarget()->update();
}

std::string GfxAtmosphere::GetPrettyTime(float hours)
{
    const int seconds = static_cast<int>(WrapHours(hours) * 3600.f);
    return fmt::format("{:02d}:{:02d}:{:02d}", seconds / 3600, (seconds / 60) % 60, seconds % 60);
}

void GfxAtmosphere::UpdateAstronomy()
{
    // Simple solar position: declination by day of year, hour angle by local solar time.
    // World: +Y up, north = -Z, east = +X.
    const float lat = Degree(Math::Clamp(App::gfx_sky_latitude->getFloat(), -89.f, 89.f)).valueRadians();
    const float day = static_cast<float>(Math::Clamp(App::gfx_sky_day_of_year->getInt(), 1, 365));
    const float decl = Degree(23.44f).valueRadians() * std::sin(2.f * Math::PI * (284.f + day) / 365.f);

    auto body_dir = [lat](float decl, float hour_angle) -> Vector3
    {
        float sin_alt = std::sin(lat) * std::sin(decl) + std::cos(lat) * std::cos(decl) * std::cos(hour_angle);
        float alt = std::asin(Math::Clamp(sin_alt, -1.f, 1.f));
        float cos_az = (std::sin(decl) - sin_alt * std::sin(lat)) / std::max(std::cos(alt) * std::cos(lat), 1e-5f);
        float az = std::acos(Math::Clamp(cos_az, -1.f, 1.f));
        if (std::sin(hour_angle) > 0.f)
            az = Math::TWO_PI - az; // Afternoon: west of south
        return Vector3(std::cos(alt) * std::sin(az), std::sin(alt), -std::cos(alt) * std::cos(az)).normalisedCopy();
    };

    const float hour_angle = Degree(15.f * (m_time_of_day - 12.f)).valueRadians();
    m_sun_dir = body_dir(decl, hour_angle);
    // Moon: roughly opposite the sun (near full moon), slightly offset so it isn't a mirror image.
    m_moon_dir = body_dir(-decl * 0.8f, hour_angle + Math::PI * 0.95f);
}

Vector3 GfxAtmosphere::Transmittance(Vector3 p, Vector3 dir) const
{
    if (RaySphere(p, dir, PLANET_R).x > 0.f)
        return Vector3::ZERO;
    const float t = RaySphere(p, dir, ATMOS_R).y;
    const int STEPS = 8;
    const float ds = t / STEPS;
    Vector3 od = Vector3::ZERO;
    for (int i = 0; i < STEPS; i++)
    {
        Vector3 s = p + dir * (ds * (i + 0.5f));
        od += Extinction(Density(s.length() - PLANET_R), m_haze) * ds;
    }
    return Exp(-od);
}

Vector3 GfxAtmosphere::SkyRadiance(Vector3 ro, Vector3 rd, Vector3 sun_dir, float sun_e, int steps) const
{
    Vector2 a = RaySphere(ro, rd, ATMOS_R);
    if (a.y < 0.f)
        return Vector3::ZERO;
    float t0 = std::max(a.x, 0.f), t1 = a.y;
    Vector2 g = RaySphere(ro, rd, PLANET_R);
    const bool hit_ground = g.x > 0.f;
    if (hit_ground)
        t1 = g.x;

    const float mu = rd.dotProduct(sun_dir);
    const float ph_r = PhaseRayleigh(mu), ph_m = PhaseMie(mu, MIE_G);
    Vector3 L = Vector3::ZERO, T = Vector3::UNIT_SCALE;
    float prev = t0;
    for (int i = 0; i < steps; i++)
    {
        float f = (i + 0.5f) / steps;
        float t = t0 + (t1 - t0) * f * f;
        float ds = t - prev;
        prev = t;
        Vector3 p = ro + rd * t;
        Vector3 dens = Density(p.length() - PLANET_R);
        Vector3 ext = Extinction(dens, m_haze);
        Vector3 scat_r = RAYLEIGH_SCAT * dens.x;
        Vector3 scat_m = Vector3(MIE_SCAT * m_haze * dens.y);
        Vector3 sun_t = this->Transmittance(p, sun_dir);
        Vector3 single = (scat_r * ph_r + scat_m * ph_m) * sun_t;
        Vector3 multi = (scat_r + scat_m) * (0.25f / Math::PI) * (sun_t * 0.6f + Vector3(0.04f));
        Vector3 step_t = Exp(-ext * ds);
        Vector3 S = (single + multi) * sun_e;
        L += T * (S - S * step_t) / Vector3(std::max(ext.x, 1e-12f), std::max(ext.y, 1e-12f), std::max(ext.z, 1e-12f));
        T *= step_t;
    }
    if (hit_ground)
    {
        Vector3 p = ro + rd * t1;
        Vector3 n = p.normalisedCopy();
        L += T * this->Transmittance(p, sun_dir) * sun_e * std::max(n.dotProduct(sun_dir), 0.f) * GROUND_ALBEDO / Math::PI;
    }
    return L;
}

void GfxAtmosphere::UpdateLighting()
{
    const Vector3 ro(0.f, PLANET_R + std::max(m_lut_altitude, 1.f), 0.f);

    // Sun as seen from the camera
    const Vector3 sun_t = this->Transmittance(ro, m_sun_dir);
    m_sun_colour = ColourValue(sun_t.x, sun_t.y, sun_t.z) * (SUN_E / Math::PI);

    // Sky ambient: cosine-weighted irradiance of the upper hemisphere, converted to RoR light units (/PI)
    Vector3 irradiance = Vector3::ZERO;
    const int RINGS = 4, SEGMENTS = 8;
    for (int r = 0; r < RINGS; r++)
    {
        const float theta = (r + 0.5f) / RINGS * Math::HALF_PI; // from zenith
        const float d_omega = std::sin(theta) * (Math::HALF_PI / RINGS) * (Math::TWO_PI / SEGMENTS);
        for (int s = 0; s < SEGMENTS; s++)
        {
            const float phi = (s + 0.5f) / SEGMENTS * Math::TWO_PI;
            Vector3 dir(std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi));
            irradiance += this->SkyRadiance(ro, dir, m_sun_dir, SUN_E, 8) * std::cos(theta) * d_omega;
        }
    }
    // Ground bounce: the ground (GROUND_ALBEDO) lit by sun + sky; a vertical surface sees half of it.
    // Upward facing surfaces get it too (single ambient colour) - still far closer to reality than none.
    const Vector3 ground_in = sun_t * (SUN_E * std::max(m_sun_dir.y, 0.f)) + irradiance;
    Vector3 ambient = (irradiance + ground_in * (GROUND_ALBEDO * 0.5f)) / Math::PI;

    // Night: moon light and a starlight floor
    const float sun_elev = Radian(std::asin(Math::Clamp(m_sun_dir.y, -1.f, 1.f))).valueDegrees();
    m_night = Math::Clamp((-sun_elev - 2.f) / 10.f, 0.f, 1.f);
    const ColourValue moon_colour = ColourValue(0.55f, 0.65f, 0.9f) * 0.04f * std::max(m_moon_dir.y, 0.f) * m_night;
    ambient += Vector3(0.006f, 0.008f, 0.014f) * m_night + Vector3(moon_colour.r, moon_colour.g, moon_colour.b) * 0.3f;
    m_ambient = ColourValue(ambient.x, ambient.y, ambient.z);

    // Main light follows the sun, or the moon at night
    if (m_main_light && m_main_light->getParentSceneNode())
    {
        const bool use_moon = sun_elev < -3.f;
        const Vector3 dir = use_moon ? m_moon_dir : m_sun_dir;
        ColourValue colour = use_moon ? moon_colour : m_sun_colour;
        // Fade out below the horizon (terrain would block it anyway, avoids light from below)
        colour *= Math::Clamp(dir.y * 20.f + 0.2f, 0.f, 1.f);
        m_main_light->setDiffuseColour(colour);
        m_main_light->setSpecularColour(colour);
        m_main_light->getParentSceneNode()->setDirection(-dir, Node::TS_WORLD);
    }

    // Haze: colour of the sky at the horizon (1 deg - what is seen behind distant hills), averaged around it, so
    // distant terrain fades into the sky. Long near-horizontal path: 16 steps.
    Vector3 fog = Vector3::ZERO;
    const float el = Degree(1.f).valueRadians();
    for (int s = 0; s < SEGMENTS; s++)
    {
        const float phi = (s + 0.5f) / SEGMENTS * Math::TWO_PI;
        fog += this->SkyRadiance(ro, Vector3(std::cos(el) * std::cos(phi), std::sin(el), std::cos(el) * std::sin(phi)), m_sun_dir, SUN_E, 16);
    }
    fog /= (float)SEGMENTS;
    // At night the scattering model goes to black; keep haze slightly visible against the dark
    fog += Vector3(0.004f, 0.005f, 0.008f) * m_night;
    m_fog_colour = ColourValue(fog.x, fog.y, fog.z);
}

void GfxAtmosphere::UpdateFog()
{
    // Visibility from sight range and haze. Exp2 fog: 5% of the original colour is left at `visibility`.
    // The mode stays FOG_EXP2 (RTSS compiles the mode into its shaders); only colour and density change.
    const int sight_range = App::gfx_sight_range->getInt();
    float visibility = (sight_range >= Terrain::UNLIMITED_SIGHTRANGE) ? 20000.f : std::max(sight_range * 1.5f, 500.f);
    visibility /= std::max(m_haze, 0.1f);
    const float density = std::sqrt(3.f) / visibility;
    // Start/end are for shaders that only do linear fog (PagedGeometry)
    App::GetGfxScene()->GetFog().Set(FOG_EXP2, m_fog_colour, density, 0.f, visibility);
}

void GfxAtmosphere::UpdateSkyMaterial()
{
    if (!m_sky_material)
        return;

    const Vector3 cam_pos = App::GetCameraManager()->GetCamera()->getDerivedPosition();
    GpuProgramParametersSharedPtr fp = m_sky_material->getTechnique(0)->getPass(0)->getFragmentProgramParameters();
    fp->setIgnoreMissingParams(true);
    fp->setNamedConstant("atmoSunDir", Vector4(m_sun_dir.x, m_sun_dir.y, m_sun_dir.z, SUN_DISC_RADIANCE));
    fp->setNamedConstant("atmoMoonDir", Vector4(m_moon_dir.x, m_moon_dir.y, m_moon_dir.z, 0.15f * m_night));
    fp->setNamedConstant("atmoSunColour", m_sun_colour);
    fp->setNamedConstant("atmoAmbient", Vector4(m_ambient.r, m_ambient.g, m_ambient.b, m_night));
    fp->setNamedConstant("atmoCloudParams", Vector4::ZERO); // No clouds yet (ROADMAP M6.6)
    fp->setNamedConstant("atmoCameraPos", Vector4(cam_pos.x, cam_pos.y, cam_pos.z, m_lut_altitude));
    fp->setNamedConstant("atmoFogColour", Vector4(m_fog_colour.r, m_fog_colour.g, m_fog_colour.b, 1.f));
}

void GfxAtmosphere::Update(float dt_sec)
{
    // Time of day lives in `gfx_sky_time` (top menubar, keys, console, savegame and scripts all set it)
    float time = App::gfx_sky_time->getFloat();
    if (App::gfx_sky_time_cycle->getBool())
    {
        time += dt_sec * App::gfx_sky_time_speed->getInt() / 3600.f; // Speed: game seconds per real second
    }
    m_time_of_day = WrapHours(time);
    if (m_time_of_day != App::gfx_sky_time->getFloat())
    {
        App::gfx_sky_time->setVal(m_time_of_day);
    }

    this->UpdateAstronomy();

    // Re-render the LUT and re-compute lighting only when something relevant changed
    const float altitude = std::max(App::GetCameraManager()->GetCamera()->getDerivedPosition().y, 1.f);
    if (m_sun_dir.dotProduct(m_lut_sun_dir) < 0.999999f
        || std::abs(altitude - m_lut_altitude) > 100.f)
    {
        m_lut_sun_dir = m_sun_dir;
        m_lut_altitude = altitude;
        this->UpdateLighting();
        this->RenderSkyViewLut();
    }

    // Every frame: the sim start sets its own ambient light after the atmosphere was created (main.cpp)
    App::GetGfxScene()->GetSceneManager()->setAmbientLight(m_ambient);
    this->UpdateFog(); // Every frame: follows the sight range setting
    this->UpdateSkyMaterial();
}
