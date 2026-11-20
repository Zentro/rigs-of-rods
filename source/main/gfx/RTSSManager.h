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
/// @author Thomas Fischer (thomas{AT}thomasfischer{DOT}biz)
/// @date   5th of July 2010

#pragma once

#include <Terrain/OgreTerrain.h>
#include <OgreShadowCameraSetupPSSM.h>
#include <OgreTerrainMaterialGeneratorA.h>
#include "Application.h"

namespace RoR {

/// @addtogroup Gfx
/// @{

class RTSSManager
{
public:

    /// Makes RTSS shade every classic (fixed-function style) material, not only the ones with an
    /// `rtshader_system` block in their script: whenever OGRE needs a material's technique for the
    /// RTSS scheme and there is none, one is generated. Materials with their own shaders are left alone.
    /// Call once, after RTSS is initialized.
    static void InstallTechniqueResolver();

    /// Copies a material and registers the copy with RTSS. A plain `Material::clone()` also copies the
    /// source's RTSS technique, but not RTSS's bookkeeping for it: RTSS then never generates shaders for
    /// the copy or updates their per-frame parameters (lights, shadows, view matrix). Use this for every
    /// material that gets drawn in the 3D scene.
    /// @param new_group Resource group of the copy; empty = same as the source.
    static Ogre::MaterialPtr CloneMaterial(const Ogre::MaterialPtr& src, const std::string& new_name, const std::string& new_group = "");

    /// Makes RTSS rebuild its technique from the material's current passes. RTSS copies the passes when it
    /// generates the technique (before the first draw); changes made after that are only picked up by this.
    static void RefreshMaterial(Ogre::Material& mat);

    void setupShadows();
    void applyToTerrain(Ogre::TerrainMaterialGeneratorA::SM2Profile* matProfile);

    Ogre::PSSMShadowCameraSetup* pssmSetup = nullptr;

protected:

    void processPSSM();
};

/// @} // addtogroup Gfx

} // namespace RoR
