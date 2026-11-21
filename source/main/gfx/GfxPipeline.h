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

namespace RoR {

/// @addtogroup Gfx
/// @{

/// Owns the frame-level rendering setup when `gfx_render_pipeline` is PBR. See ROADMAP.md M3.
/// - 3.1 (this): HDR target and tonemapping ('RoR/HDR' compositor, resources/pipeline).
/// - Later chunks: linear content (sRGB textures, linear colours), RTSS linear output, secondary views.
class GfxPipeline: public Ogre::CompositorInstance::Listener
{
public:
    void Init(Ogre::Viewport* viewport); //!< Call once RTSS and the main viewport exist.
    void UpdateFrame();                   //!< Call once per frame before rendering.
    bool IsActive() const { return m_active; } //!< True if the HDR pipeline renders the main view.

    // Ogre::CompositorInstance::Listener
    void notifyMaterialRender(Ogre::uint32 pass_id, Ogre::MaterialPtr& mat) override;

private:
    static const Ogre::uint32 PASS_TONEMAP = 1; //!< `identifier` of the output pass in RoRPipeline.compositor

    bool           m_active = false;
    Ogre::Vector4  m_tonemap_values = Ogre::Vector4::ZERO; //!< x: exposure, y: tonemap mode
};

/// @} // addtogroup Gfx

} // namespace RoR
