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

/// @file
/// @brief Physically based sky with sun and sky ambient light (ported from the gfx-rtss branch, ROADMAP M6).

#pragma once

#include "Application.h"

#include <OgreColourValue.h>
#include <OgreTexture.h>
#include <OgreVector.h>

namespace RoR {

/// @addtogroup Gfx
/// @{

/// Owns the sky dome, the sky-view LUT and the main light's colour and direction with the HDR pipeline.
/// Sun colour and sky ambient are computed on the CPU from the same model the shaders use
/// (resources/atmosphere/Atmosphere.glsl), so the lighting matches the visible sky.
/// Outputs linear radiance - only used with the HDR pipeline (see ROADMAP M6).
class GfxAtmosphere
{
public:
    GfxAtmosphere(Ogre::Light* main_light);
    ~GfxAtmosphere();

    void Update(float dt_sec); //!< `dt_sec` advances the time of day while `gfx_sky_time_cycle` is on (0 when paused)

    float GetTimeOfDay() const { return m_time_of_day; } //!< Local solar time, hours [0-24)
    static std::string GetPrettyTime(float hours);         //!< "HH:MM:SS"
    Ogre::Vector3 GetSunDirection() const { return m_sun_dir; } //!< Towards the sun

    /// The atmosphere is the sky of the HDR pipeline; the terrain's sky box (SDR content) stays with the legacy pipeline.
    static bool IsWanted();

private:
    static void EnsureDummyCube();
    void CreateSkyViewLut();
    void RenderSkyViewLut();
    void UpdateAstronomy();
    void UpdateLighting();
    void UpdateFog();
    void UpdateSkyMaterial();

    // CPU mirror of the shader model
    Ogre::Vector3 Transmittance(Ogre::Vector3 p, Ogre::Vector3 dir) const;
    Ogre::Vector3 SkyRadiance(Ogre::Vector3 ro, Ogre::Vector3 rd, Ogre::Vector3 sun_dir, float sun_e, int steps) const;

    Ogre::Light*       m_main_light = nullptr;
    float              m_time_of_day = 14.f;
    float              m_haze = 1.f;
    Ogre::Vector3      m_sun_dir = Ogre::Vector3::UNIT_Y;
    Ogre::Vector3      m_moon_dir = Ogre::Vector3::NEGATIVE_UNIT_Y;
    Ogre::Vector3      m_lut_sun_dir = Ogre::Vector3::ZERO;  //!< Sun direction the LUT was last rendered with
    float              m_lut_altitude = -1.f;
    Ogre::ColourValue  m_sun_colour;             //!< Sun radiance at the camera (for the disc)
    Ogre::ColourValue  m_ambient;
    Ogre::ColourValue  m_fog_colour;             //!< Average horizon haze colour (linear)
    float              m_night = 0.f;
    Ogre::TexturePtr   m_lut_texture;
    Ogre::SceneManager* m_lut_scene = nullptr;   //!< Private scene with a fullscreen rectangle, renders the LUT
    Ogre::Camera*      m_lut_camera = nullptr;
    Ogre::MaterialPtr  m_lut_material;
    Ogre::MaterialPtr  m_sky_material;
};

/// @} // addtogroup Gfx

} // namespace RoR
