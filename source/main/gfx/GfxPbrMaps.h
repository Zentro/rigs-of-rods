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

#include <OgrePrerequisites.h>
#include <OgreTexture.h>

#include <map>
#include <vector>
#include <string>

namespace RoR {

/// @addtogroup Gfx
/// @{

/// Bakes legacy texture maps into the layouts RTSS's Cook-Torrance lighting reads, on the GPU (handles compressed
/// textures), cached for the session. HDR pipeline only.
class GfxPbrMaps
{
public:
    /// Metal/roughness texture (glTF layout: R occlusion = 1, G roughness, B metallic, A gloss mask) from a
    /// legacy specular map, whose rgb is a reflectivity mask: black texels get `base_roughness`, white ones
    /// `glossy_roughness`; `metallic` is constant (legacy content has no metalness, it comes from the material class).
    /// @return Name of the baked texture (resource group RGN_DEFAULT), or empty if the map can't be loaded.
    static std::string BakeSpecularMap(Ogre::TexturePtr const& specular_map, float base_roughness, float glossy_roughness, float metallic = 0.f);

    /// Cube map for image based lighting (ROADMAP M5): each mip level is one perceptual roughness, the last one 1.
    static Ogre::TexturePtr CreatePrefilteredEnvMap(std::string const& name);

    /// GGX-prefilters `src` (the realtime environment map) into all levels of `dst` (see CreatePrefilteredEnvMap()).
    static void PrefilterEnvMap(Ogre::TexturePtr const& src, Ogre::TexturePtr const& dst);

private:
    static void SetupScene();
    static void PrefilterEnvMapInternal(Ogre::TexturePtr const& src, Ogre::TexturePtr const& dst);
    static Ogre::MaterialPtr GetMaterial(const char* name, const char* sampler_name);
    static void RenderQuad(Ogre::MaterialPtr const& mat, Ogre::RenderTarget* rt); //!< Fullscreen quad, private scene

    static Ogre::SceneManager*                s_scene;
    static Ogre::Camera*                      s_camera;
    static Ogre::Rectangle2D*                 s_rect;
    static std::map<std::string, std::string> s_baked;     //!< Cache key -> baked texture name
    static std::vector<Ogre::TexturePtr>      s_env_chain; //!< Environment map downsample chain, [0] = source
};

/// @} // addtogroup Gfx

} // namespace RoR
