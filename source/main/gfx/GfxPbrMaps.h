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
/// @brief Textures for physically based materials made from legacy content (ROADMAP M4.3).

#pragma once

#include <OgreTexture.h>

#include <map>
#include <string>

namespace RoR {

/// @addtogroup Gfx
/// @{

/// Bakes legacy texture maps into the layouts RTSS's Cook-Torrance lighting reads, on the GPU (handles compressed
/// textures), cached for the session. HDR pipeline only.
class GfxPbrMaps
{
public:
    /// Metal/roughness texture (glTF layout: R occlusion = 1, G roughness, B metallic = 0, A gloss mask) from a
    /// legacy specular map, whose rgb is a reflectivity mask: black texels get `base_roughness`, white ones
    /// `glossy_roughness`.
    /// @return Name of the baked texture (resource group RGN_DEFAULT), or empty if the map can't be loaded.
    static std::string BakeSpecularMap(Ogre::TexturePtr const& specular_map, float base_roughness, float glossy_roughness);

private:
    static bool SetupBaker();

    static Ogre::SceneManager*                s_scene;
    static Ogre::Camera*                      s_camera;
    static Ogre::MaterialPtr                  s_material;
    static std::map<std::string, std::string> s_baked; //!< Cache key -> baked texture name
};

/// @} // addtogroup Gfx

} // namespace RoR
