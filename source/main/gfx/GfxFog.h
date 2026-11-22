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
/// @brief Scene fog, including fog for passes that blend onto earlier passes (multiply/additive).

#pragma once

#include <OgreColourValue.h>
#include <OgreCommon.h>
#include <OgreResourceGroupManager.h>

namespace RoR {

/// @addtogroup Gfx
/// @{

/// The one place that sets scene fog. Fog blends every pass towards the fog colour, which is only right for passes
/// that replace or alpha-blend. A pass that multiplies onto earlier passes (e.g. a lighting pass, `scene_blend zero
/// src_colour`) would make a fully fogged object fog x fog, an additive pass fog + fog. Such passes get their own
/// fog with the neutral colour of their blend (white for multiply, grey for modulate x2, black for add) and the
/// scene's mode and distances. Passes whose material sets its own `fog_override` are left alone.
class GfxFog: public Ogre::ResourceGroupListener
{
public:
    void Init(); //!< Call once the scene manager exists.

    /// Sets the scene fog; blended passes follow when the mode or distances change (not on colour changes).
    void Set(Ogre::FogMode mode, Ogre::ColourValue const& colour, float density, float start, float end);

    // Ogre::ResourceGroupListener
    void resourceGroupScriptingEnded(const Ogre::String& groupName) override;

private:
    void ApplyToMaterials(const Ogre::String* group); //!< nullptr = all groups

    Ogre::FogMode m_mode = Ogre::FOG_NONE;
    float         m_density = 0.f;
    float         m_start = 0.f;
    float         m_end = 0.f;
};

/// @} // addtogroup Gfx

} // namespace RoR
