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

#include "RTSSNormalMap.h"

#include <OgrePass.h>
#include <OgreTextureUnitState.h>
#include <RTShaderSystem/OgreShaderFFPRenderState.h>
#include <RTShaderSystem/OgreShaderFunction.h>
#include <RTShaderSystem/OgreShaderProgram.h>
#include <RTShaderSystem/OgreShaderProgramSet.h>

using namespace Ogre;
using namespace Ogre::RTShader;
using namespace RoR;

const String RTSSNormalMap::TYPE = "RoR_NormalMapTL";

int RTSSNormalMap::getExecutionOrder() const
{
    return FFP_LIGHTING - 2; // Before lighting: it resolves the normal the lighting stages look for
}

bool RTSSNormalMap::setParameter(const String& name, const String& value)
{
    if (name == "texture" && !value.empty())
    {
        m_texture_name = value;
        return true;
    }
    return false;
}

void RTSSNormalMap::copyFrom(const SubRenderState& rhs)
{
    m_texture_name = static_cast<const RTSSNormalMap&>(rhs).m_texture_name;
}

bool RTSSNormalMap::preAddToRenderState(const RenderState* renderState, Pass* srcPass, Pass* dstPass)
{
    if (!srcPass->getLightingEnabled() || m_texture_name.empty())
        return false;

    TextureUnitState* tus = dstPass->createTextureUnitState(m_texture_name);
    tus->setHardwareGammaEnabled(false); // Data, not colour
    m_sampler_index = dstPass->getNumTextureUnitStates() - 1;
    return true;
}

bool RTSSNormalMap::createCpuSubPrograms(ProgramSet* programSet)
{
    Program* vsProgram = programSet->getCpuProgram(GPT_VERTEX_PROGRAM);
    Function* vsMain = vsProgram->getEntryPointFunction();
    Program* psProgram = programSet->getCpuProgram(GPT_FRAGMENT_PROGRAM);
    Function* psMain = psProgram->getEntryPointFunction();

    psProgram->addDependency("RoRLib_NormalMap");
    vsProgram->addDependency("FFPLib_Transform");

    // Texture coordinates (set 0), shared with other stages if they already forward them
    ParameterPtr vsOutUv = vsMain->getOutputParameter(Parameter::SPC_TEXTURE_COORDINATE0, GCT_FLOAT2);
    auto vstage = vsMain->getStage(FFP_VS_POST_PROCESS);
    if (!vsOutUv)
    {
        ParameterPtr vsInUv = vsMain->resolveInputParameter(Parameter::SPC_TEXTURE_COORDINATE0, GCT_FLOAT2);
        vsOutUv = vsMain->resolveOutputParameter(Parameter::SPC_TEXTURE_COORDINATE0, GCT_FLOAT2);
        vstage.assign(vsInUv, vsOutUv);
    }

    // View space position and vertex normal (lighting stages skip the normal once we provide a local one)
    ParameterPtr vsInPos = vsMain->resolveInputParameter(Parameter::SPC_POSITION_OBJECT_SPACE);
    ParameterPtr vsOutViewPos = vsMain->resolveOutputParameter(Parameter::SPC_POSITION_VIEW_SPACE);
    ParameterPtr vsInNormal = vsMain->resolveInputParameter(Parameter::SPC_NORMAL_OBJECT_SPACE);
    ParameterPtr vsOutNormal = vsMain->resolveOutputParameter(Parameter::SPC_NORMAL_VIEW_SPACE);
    vstage.callFunction("FFP_Transform", In(vsProgram->resolveParameter(GpuProgramParameters::ACT_WORLDVIEW_MATRIX)),
                        In(vsInPos), Out(vsOutViewPos));
    vstage.callBuiltin("mul", In(vsProgram->resolveParameter(GpuProgramParameters::ACT_NORMAL_MATRIX)),
                       In(vsInNormal), Out(vsOutNormal));

    ParameterPtr uv = psMain->resolveInputParameter(vsOutUv);
    ParameterPtr viewPos = psMain->resolveInputParameter(vsOutViewPos);
    ParameterPtr vertexNormal = psMain->resolveInputParameter(vsOutNormal);
    UniformParameterPtr sampler = psProgram->resolveParameter(GCT_SAMPLER2D, "rorNormalSampler", m_sampler_index);
    ParameterPtr mappedNormal = psMain->resolveLocalParameter(Parameter::SPC_NORMAL_VIEW_SPACE, GCT_FLOAT3);

    // Early, before any lighting stage reads the normal
    psMain->getStage(FFP_PS_COLOUR_BEGIN + 2).callFunction("RoR_NormalMapTL",
        { In(vertexNormal), In(viewPos), In(uv), In(sampler), Out(mappedNormal) });
    return true;
}
