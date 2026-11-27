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

#include <RTShaderSystem/OgreShaderSubRenderState.h>

namespace RoR {

/// @addtogroup Gfx
/// @{

class RTSSManager
{
public:

    static void InstallTechniqueResolver();

    static Ogre::MaterialPtr CloneMaterial(const Ogre::MaterialPtr& src, const std::string& new_name, const std::string& new_group = "");

    static void RefreshMaterial(Ogre::Material& mat);

    /// Caps the number of lights RTSS generates shader code for at OGRE_MAX_SIMULTANEOUS_LIGHTS (8). Call once,
    /// after RTSS is initialized.
    static void LimitLightCount();

    /// Applies RescueMaterial() to every material now and to every resource group parsed later (terrains,
    /// vehicles, mods). Call once, after the technique resolver is installed.
    static void InstallMaterialRules();

    /// Cg rescue: if none of a material's techniques can run (shaders in a language without a plugin, e.g. Cg),
    /// adds a fallback technique - a copy of the first one without shaders, keeping only the color texture
    /// (searched in all passes). Mod files are not changed; this only adds a technique to the loaded material.
    /// @return True if a fallback was added.
    static bool RescueMaterial(Ogre::Material& mat);

    /// Registers RoR's own RTSS sub render states. Call once, after RTSS is initialized.
    static void InstallSubRenderStates();

    /// Gives the material an RTSS technique now (instead of at its first draw) and adds "srs" to the render state
    /// of pass "pass_index". The material must not have its own shaders.
    /// @return False if RTSS can't shade the material and "srs" is destroyed then
    static bool AddSubRenderState(const Ogre::MaterialPtr& mat, unsigned short pass_index, Ogre::RTShader::SubRenderState* srs);

    void setupShadows();
    void applyToTerrain(Ogre::TerrainMaterialGeneratorA::SM2Profile* matProfile);

    Ogre::PSSMShadowCameraSetup* pssmSetup = nullptr;

protected:

    void processPSSM();
};

/// @} // addtogroup Gfx

} // namespace RoR
