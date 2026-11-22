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
/// @brief RTSS sub render state adding a gloss layer (smooth top layer: paint lacquer, glass) on top of RTSS
///        Cook-Torrance + image based lighting. Ported from the gfx-rtss branch ("clearcoat"), ROADMAP M4 A.4.

#pragma once

#include <RTShaderSystem/OgreShaderSubRenderState.h>

namespace RoR {

/// @addtogroup Gfx
/// @{

/// Glossy dielectric top layer: Fresnel-weighted env map reflection + sun highlight, see `RoRLib_Gloss.glsl`.
/// Gloss strength and roughness come from the pass specular colour (z, w - RTSS Cook-Torrance only uses x, y);
/// optionally masked per texel by the alpha of the metal/roughness texture (baked legacy specular maps).
/// Must be added after SRS_COOK_TORRANCE_LIGHTING.
class RTSSGloss: public Ogre::RTShader::SubRenderState
{
public:
    static const Ogre::String TYPE;

    const Ogre::String& getType() const override { return TYPE; }
    int getExecutionOrder() const override;
    bool setParameter(const Ogre::String& name, const Ogre::String& value) override; //!< "texture" = env cube map, "mask_from_texture" = bool
    void copyFrom(const Ogre::RTShader::SubRenderState& rhs) override;
    bool preAddToRenderState(const Ogre::RTShader::RenderState* renderState, Ogre::Pass* srcPass, Ogre::Pass* dstPass) override;
    bool createCpuSubPrograms(Ogre::RTShader::ProgramSet* programSet) override;

private:
    Ogre::String m_env_map_name;
    bool         m_mask_from_texture = false;
    int          m_env_sampler_index = 0;
};

class RTSSGlossFactory: public Ogre::RTShader::SubRenderStateFactory
{
public:
    const Ogre::String& getType() const override { return RTSSGloss::TYPE; }

protected:
    Ogre::RTShader::SubRenderState* createInstanceImpl() override { return new RTSSGloss(); }
};

/// @} // addtogroup Gfx

} // namespace RoR
