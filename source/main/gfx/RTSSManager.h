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

/// PSSM with stable cascades. OGRE's PSSMShadowCameraSetup (LiSPSM/"focused") fits each shadow camera to the
/// bounding boxes of what's currently visible and warps it by the view direction - with few objects in view the
/// shadow cameras jump as the camera turns, and shadows get clipped away. Here each cascade is a fixed-size,
/// unwarped orthographic view around its slice of the camera frustum, snapped to whole shadow texels: turning
/// or moving the camera only shifts it smoothly. Costs some resolution compared to a tight fit.
class StableCSMShadowCameraSetup: public Ogre::PSSMShadowCameraSetup
{
public:
    void getShadowCamera(const Ogre::SceneManager* sm, const Ogre::Camera* cam, const Ogre::Viewport* vp,
                         const Ogre::Light* light, Ogre::Camera* texCam, size_t iteration) const override;
};

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

    /// Applies RescueMaterial() and SetupShadowCaster() to every material now and to every resource group parsed
    /// later (terrains, vehicles, mods). Call once, after the technique resolver is installed.
    static void InstallMaterialRules();

    /// Cg rescue: if none of a material's techniques can run (shaders in a language without a plugin, e.g. Cg),
    /// adds a fallback technique - a copy of the first one without shaders, keeping only the diffuse texture -
    /// which the technique resolver then shades like any classic material. Mod files are not changed; this only
    /// adds a technique to the loaded material.
    /// @return True if a fallback was added.
    static bool RescueMaterial(Ogre::Material& mat);

    /// Shadow caster rules: invisible materials (`colour_write off`) and see-through ones (blended, no depth
    /// write - e.g. headlight beam cones) cast no shadow. (Alpha-tested materials keep their cut-out shapes
    /// through OGRE's own caster, see the technique resolver.)

    /// Applies the shadow caster rules (see InstallShadowCasterRules()) to one material.
    /// @return True if the material was set to cast no shadow.
    static bool SetupShadowCaster(Ogre::Material& mat);

    /// Caps the number of lights RTSS generates shader code for at OGRE_MAX_SIMULTANEOUS_LIGHTS (8). Call once,
    /// after RTSS is initialized.
    static void LimitLightCount();

    /// Registers RoR's own RTSS sub render states (the 'RoR/NiceMetal' vehicle shading). Call once, after RTSS
    /// is initialized.
    static void InstallSubRenderStates();

    /// Attaches the vehicle surface shading (NiceMetal: damage/wetness blend, specular-map-masked cubemap
    /// reflection) to a spawned managed material. Must be called after the spawner has assigned the textures,
    /// because the shading depends on which texture units are present. `is_flexbody` says whether the mesh
    /// carries the per-node damage/wetness vertex colours; only flexbodies do.
    static void ApplyActorShading(const Ogre::MaterialPtr& mat, bool transparent, bool is_flexbody);

    static const int NUM_CASCADES = 4; //!< Sun shadow cascades (RTSS supports up to 4)

    ~RTSSManager();

    void setupShadows();
    void applyToTerrain(Ogre::TerrainMaterialGeneratorA::SM2Profile* matProfile);

    Ogre::PSSMShadowCameraSetup* pssmSetup = nullptr;

protected:

    void processPSSM();

    /// Shadow receiving stage in the RTSS scheme's render state - applies to every material RTSS shades
    Ogre::RTShader::SubRenderState* m_shadow_srs = nullptr;
};

/// @} // addtogroup Gfx

} // namespace RoR
