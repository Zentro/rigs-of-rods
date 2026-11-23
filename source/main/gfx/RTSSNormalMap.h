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
/// @brief RTSS sub-render state: normal mapping without mesh tangents, see `RoRLib_NormalMap.glsl`. Ported from the
///        gfx-rtss branch (ROADMAP M4 A.5).

#pragma once

#include <RTShaderSystem/OgreShaderSubRenderState.h>

namespace RoR {

/// @addtogroup Gfx
/// @{

/// Perturbs the view space normal with a tangent space normal map, deriving the tangent frame per pixel.
/// OGRE's SRS_NORMALMAP needs mesh tangents, which deforming flexbodies don't have.
/// Publishes the result as the local `SPC_NORMAL_VIEW_SPACE` parameter, which Cook-Torrance, IBL,
/// RTSSGloss pick up. Must execute before the lighting stage.
class RTSSNormalMap: public Ogre::RTShader::SubRenderState
{
public:
    static const Ogre::String TYPE;

    const Ogre::String& getType() const override { return TYPE; }
    int getExecutionOrder() const override;
    bool setParameter(const Ogre::String& name, const Ogre::String& value) override; //!< "texture" = normal map
    void copyFrom(const Ogre::RTShader::SubRenderState& rhs) override;
    bool preAddToRenderState(const Ogre::RTShader::RenderState* renderState, Ogre::Pass* srcPass, Ogre::Pass* dstPass) override;
    bool createCpuSubPrograms(Ogre::RTShader::ProgramSet* programSet) override;

private:
    Ogre::String m_texture_name;
    int          m_sampler_index = 0;
};

class RTSSNormalMapFactory: public Ogre::RTShader::SubRenderStateFactory
{
public:
    const Ogre::String& getType() const override { return RTSSNormalMap::TYPE; }

protected:
    Ogre::RTShader::SubRenderState* createInstanceImpl() override { return new RTSSNormalMap(); }
};

/// @} // addtogroup Gfx

} // namespace RoR
