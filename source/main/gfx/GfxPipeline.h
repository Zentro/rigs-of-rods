/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer

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
/// @brief HDR rendering pipeline (`gfx_render_pipeline`): the scene renders into a linear half-float target,
///        an output pass applies exposure and tonemapping and encodes for the display.

#pragma once

#include <Ogre.h>
#include <OgreCompositorInstance.h>

#include <set>
#include <string>

namespace RoR {

/// @addtogroup Gfx
/// @{

/// Owns the frame-level rendering setup when `gfx_render_pipeline` is PBR. See ROADMAP.md M3.
/// - HDR target and tonemapping ('RoR/HDR' compositor, resources/pipeline).
/// - Linear content: lighting must happen in linear space, but RoR's content is authored in display (gamma) space.
///   Colour textures are decoded from sRGB by the hardware; material and light colours are converted once.
///   UI renders after tonemapping, in display space, and is left alone.
class GfxPipeline: public Ogre::CompositorInstance::Listener, public Ogre::ResourceGroupListener
{
public:
    void Init(Ogre::Viewport* viewport); //!< Call once RTSS and the main viewport exist.
    void UpdateFrame();                   //!< Call once per frame before rendering.
    bool IsActive() const { return m_active; } //!< True if the HDR pipeline renders the main view (linear lighting).

    /// Converts an authored (gamma space) colour to linear if the HDR pipeline is active, otherwise returns it as is.
    Ogre::ColourValue ToLinear(Ogre::ColourValue const& c) const;

    /// Material colours to linear (once per material) and colour textures to sRGB decoding. No-op if not active.
    /// Runs for every script material when its resource group is parsed.
    void LinearizeMaterial(Ogre::Material* mat);
    bool IsLinearized(const Ogre::Material& mat) const { return m_linearized.count(mat.getGroup() + "|" + mat.getName()) > 0; } //!< Colours already converted

    /// Only the textures part of LinearizeMaterial() - safe to call any number of times. For textures assigned
    /// later (vehicle managed materials at spawn, skins, materials created in code).
    void LinearizeTextures(Ogre::Material* mat);

    /// Forces hardware sRGB decoding on a texture, reloading it if it is loaded already.
    static void EnsureTextureSRGB(Ogre::TexturePtr tex);

    /// True if `gfx_render_pipeline` asks for the HDR pipeline - for things created before Init().
    static bool IsRequested();

    /// Format for render targets which hold scene colours that are used in the 3D scene again (environment map,
    /// water reflection/refraction, mirrors, video cameras): half-float with the HDR pipeline (linear values keep
    /// their precision and aren't clipped at 1), otherwise `legacy_format`.
    static Ogre::PixelFormat GetSceneTargetFormat(Ogre::PixelFormat legacy_format);

    /// FSAA for such render targets: none for half-float targets (not reliably supported), otherwise `legacy_fsaa`.
    static Ogre::uint GetSceneTargetFsaa(Ogre::uint legacy_fsaa);

    /// For views whose image is shown directly (the map texture in the UI, video camera windows): adds the HDR
    /// compositor, so they're tonemapped and display-encoded like the main view. No-op if not active.
    void AddToViewport(Ogre::Viewport* viewport);

    // Ogre::CompositorInstance::Listener
    void notifyMaterialRender(Ogre::uint32 pass_id, Ogre::MaterialPtr& mat) override;

    // Ogre::ResourceGroupListener
    void resourceGroupScriptingEnded(const Ogre::String& groupName) override;

private:
    static const Ogre::uint32 PASS_TONEMAP = 1; //!< `identifier` of the output pass in RoRPipeline.compositor

    bool IsUiMaterial(const Ogre::Material* mat) const; //!< Renders after tonemapping, in display space
    void LinearizeColourConstants(Ogre::GpuProgramParametersSharedPtr params); //!< Named 'colour' float3/4 constants
    void CollectUiScripts();

    bool                   m_active = false;
    Ogre::Vector4          m_tonemap_values = Ogre::Vector4::ZERO; //!< x: exposure, y: tonemap mode
    std::set<std::string>  m_linearized; //!< "group|name" of materials whose colours were converted
    std::set<std::string>  m_ui_scripts; //!< .material files of the UI resource packs
};

/// @} // addtogroup Gfx

} // namespace RoR
