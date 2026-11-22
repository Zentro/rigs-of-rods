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

#include "RTSSGloss.h"

#include <OgrePass.h>
#include <OgreStringConverter.h>
#include <OgreTextureUnitState.h>
#include <RTShaderSystem/OgreShaderFFPRenderState.h>
#include <RTShaderSystem/OgreShaderFunction.h>
#include <RTShaderSystem/OgreShaderProgram.h>
#include <RTShaderSystem/OgreShaderProgramSet.h>

using namespace Ogre;
using namespace Ogre::RTShader;
using namespace RoR;

const String RTSSGloss::TYPE = "RoR_Gloss";

int RTSSGloss::getExecutionOrder() const
{
    return FFP_LIGHTING + 20; // After Cook-Torrance (FFP_LIGHTING) and image based lighting (+10)
}

bool RTSSGloss::setParameter(const String& name, const String& value)
{
    if (name == "texture" && !value.empty())
    {
        m_env_map_name = value;
        return true;
    }
    if (name == "mask_from_texture")
    {
        return StringConverter::parse(value, m_mask_from_texture);
    }
    return false;
}

void RTSSGloss::copyFrom(const SubRenderState& rhs)
{
    const RTSSGloss& other = static_cast<const RTSSGloss&>(rhs);
    m_env_map_name = other.m_env_map_name;
    m_mask_from_texture = other.m_mask_from_texture;
}

bool RTSSGloss::preAddToRenderState(const RenderState* renderState, Pass* srcPass, Pass* dstPass)
{
    if (!srcPass->getLightingEnabled() || m_env_map_name.empty())
        return false;

    TextureUnitState* tus = dstPass->createTextureUnitState();
    tus->setTextureName(m_env_map_name, TEX_TYPE_CUBE_MAP);
    tus->setTextureFiltering(TFO_TRILINEAR);
    m_env_sampler_index = dstPass->getNumTextureUnitStates() - 1;
    return true;
}

bool RTSSGloss::createCpuSubPrograms(ProgramSet* programSet)
{
    Program* vsProgram = programSet->getCpuProgram(GPT_VERTEX_PROGRAM);
    Function* vsMain = vsProgram->getEntryPointFunction();
    Program* psProgram = programSet->getCpuProgram(GPT_FRAGMENT_PROGRAM);
    Function* psMain = psProgram->getEntryPointFunction();

    psProgram->addDependency("RoRLib_Gloss");

    // View space position and normal - written by the Cook-Torrance stage
    ParameterPtr viewPos = psMain->resolveInputParameter(vsMain->resolveOutputParameter(Parameter::SPC_POSITION_VIEW_SPACE));
    ParameterPtr viewNormal = psMain->getLocalParameter(Parameter::SPC_NORMAL_VIEW_SPACE); // Normal map injection
    if (!viewNormal)
        viewNormal = psMain->resolveInputParameter(vsMain->resolveOutputParameter(Parameter::SPC_NORMAL_VIEW_SPACE));

    UniformParameterPtr invView = psProgram->resolveParameter(GpuProgramParameters::ACT_INVERSE_VIEW_MATRIX);
    UniformParameterPtr envSampler = psProgram->resolveParameter(GCT_SAMPLERCUBE, "rorGlossEnvSampler", m_env_sampler_index);
    UniformParameterPtr envSize = psProgram->resolveParameter(GpuProgramParameters::ACT_TEXTURE_SIZE, m_env_sampler_index);
    UniformParameterPtr lightPos = psProgram->resolveParameter(GpuProgramParameters::ACT_LIGHT_POSITION_VIEW_SPACE, 0);
    UniformParameterPtr lightColour = psProgram->resolveParameter(GpuProgramParameters::ACT_LIGHT_DIFFUSE_COLOUR_POWER_SCALED, 0);
    UniformParameterPtr specular = psProgram->resolveParameter(GpuProgramParameters::ACT_SURFACE_SPECULAR_COLOUR);
    ParameterPtr outDiffuse = psMain->resolveOutputParameter(Parameter::SPC_COLOR_DIFFUSE);

    std::vector<Operand> params = { In(viewNormal), In(viewPos), In(invView), In(envSampler), In(envSize),
                                    In(lightPos), In(lightColour) };
    if (ParameterPtr shadow = psMain->getLocalParameter("lShadowFactor")) // Integrated PSSM, sun is light 0
    {
        params.push_back(In(shadow));
        params.push_back(At(0));
    }
    else
    {
        params.push_back(In(1.0f));
    }
    params.push_back(In(specular));
    ParameterPtr mrSample = psMain->getLocalParameter("ormSample"); // Metal/roughness texture sample (Cook-Torrance, OGRE 14.6)
    if (m_mask_from_texture && mrSample)
        params.push_back(In(mrSample).w());
    else
        params.push_back(In(0.0f));
    params.push_back(InOut(outDiffuse).xyz());

    // After PBR_Lights (FFP_PS_PBR_LIGHTING_END), before fog
    psMain->getStage(FFP_PS_PBR_LIGHTING_END + 1).callFunction("RoR_Gloss", params);
    return true;
}
