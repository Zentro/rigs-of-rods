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

#include "RTSSPaged.h"

#include <OgrePass.h>
#include <OgreShaderFFPRenderState.h>
#include <OgreShaderGenerator.h>
#include <OgreShaderProgram.h>
#include <OgreShaderProgramSet.h>

using namespace Ogre;
using namespace Ogre::RTShader;
using namespace RoR;

/*static*/ const String PagedFadeSubRenderState::Type = "RoR/PagedFade";
/*static*/ const String GrassSubRenderState::Type = "RoR/Grass";

// Varyings private to these effects (NiceMetal uses SPC_CUSTOM_CONTENT_BEGIN + 1..3)
static const int PAGED_TREE_FADE  = Parameter::SPC_CUSTOM_CONTENT_BEGIN + 20;
static const int PAGED_GRASS_FADE = Parameter::SPC_CUSTOM_CONTENT_BEGIN + 21;

// Per-object: the values differ between materials which may share one generated program.
static UniformParameterPtr ResolveUniform(Program* program, GpuConstantType type, const char* name)
{
    return program->resolveParameter(type, -1, (uint16)GPV_PER_OBJECT, name);
}

// ----------------------------------------------------------------------------
// Trees

int PagedFadeSubRenderState::getExecutionOrder() const
{
    return FFP_TEXTURING + 10;
}

void PagedFadeSubRenderState::copyFrom(const SubRenderState& rhs)
{
    const auto& other = static_cast<const PagedFadeSubRenderState&>(rhs);
    m_invisible_dist = other.m_invisible_dist;
    m_fade_gap = other.m_fade_gap;
}

bool PagedFadeSubRenderState::setParameter(const String& name, const String& value)
{
    if (name == "invisible_dist")
        return StringConverter::parse(value, m_invisible_dist);
    if (name == "fade_gap")
        return StringConverter::parse(value, m_fade_gap);
    return false;
}

bool PagedFadeSubRenderState::createCpuSubPrograms(ProgramSet* programSet)
{
    Program* vsProgram = programSet->getCpuProgram(GPT_VERTEX_PROGRAM);
    Function* vsMain = vsProgram->getEntryPointFunction();
    Program* psProgram = programSet->getCpuProgram(GPT_FRAGMENT_PROGRAM);
    Function* psMain = psProgram->getEntryPointFunction();

    vsProgram->addDependency("SGXLib_Paged");
    psProgram->addDependency("SGXLib_Paged");

    auto position = vsMain->resolveInputParameter(Parameter::SPC_POSITION_OBJECT_SPACE);
    auto camPos = vsProgram->resolveParameter(GpuProgramParameters::ACT_CAMERA_POSITION_OBJECT_SPACE);
    m_invisible_dist_param = ResolveUniform(vsProgram, GCT_FLOAT1, "pagedInvisibleDist");
    m_fade_gap_param = ResolveUniform(vsProgram, GCT_FLOAT1, "pagedFadeGap");
    auto vsOutFade = vsMain->resolveOutputParameter(PAGED_TREE_FADE, GCT_FLOAT1);

    vsMain->getStage(FFP_VS_TEXTURING).callFunction("SGX_Paged_TreeFade",
        {In(position), In(camPos).xyz(), In(m_invisible_dist_param), In(m_fade_gap_param), Out(vsOutFade)});

    auto psInFade = psMain->resolveInputParameter(vsOutFade);
    auto outDiffuse = psMain->resolveOutputParameter(Parameter::SPC_COLOR_DIFFUSE);
    psMain->getStage(FFP_PS_TEXTURING + 10).callFunction("SGX_Paged_ApplyFade", {In(psInFade), InOut(outDiffuse)});

    return true;
}

void PagedFadeSubRenderState::updateGpuProgramsParams(Renderable*, const Pass*, const AutoParamDataSource*, const LightList*)
{
    m_invisible_dist_param->setGpuParameter(m_invisible_dist);
    m_fade_gap_param->setGpuParameter(m_fade_gap);
}

// ----------------------------------------------------------------------------
// Grass

int GrassSubRenderState::getExecutionOrder() const
{
    return FFP_TEXTURING + 10;
}

void GrassSubRenderState::copyFrom(const SubRenderState& rhs)
{
    const auto& other = static_cast<const GrassSubRenderState&>(rhs);
    m_state = other.m_state;
    m_sprite = other.m_sprite;
    m_animate = other.m_animate;
    m_blend = other.m_blend;
    m_fade_alpha = other.m_fade_alpha;
    m_fade_grow = other.m_fade_grow;
    m_caster = other.m_caster;
    m_fade_range = other.m_fade_range;
    m_grass_height = other.m_grass_height;
}

bool GrassSubRenderState::setParameter(const String&, const String&)
{
    return false; // Configured in code, see setup()
}

void GrassSubRenderState::setup(std::shared_ptr<GrassShaderState> state, bool sprite, bool animate, bool blend,
                                bool fade_alpha, bool fade_grow, float fade_range, float grass_height, bool caster)
{
    m_state = state;
    m_sprite = sprite;
    m_animate = animate;
    m_blend = blend;
    m_fade_alpha = fade_alpha;
    m_fade_grow = fade_grow;
    m_fade_range = fade_range;
    m_grass_height = grass_height;
    m_caster = caster;
}

bool GrassSubRenderState::createCpuSubPrograms(ProgramSet* programSet)
{
    Program* vsProgram = programSet->getCpuProgram(GPT_VERTEX_PROGRAM);
    Function* vsMain = vsProgram->getEntryPointFunction();
    Program* psProgram = programSet->getCpuProgram(GPT_FRAGMENT_PROGRAM);
    Function* psMain = psProgram->getEntryPointFunction();

    vsProgram->addDependency("SGXLib_Paged");
    psProgram->addDependency("SGXLib_Paged");

    auto position = vsMain->resolveInputParameter(Parameter::SPC_POSITION_OBJECT_SPACE);
    auto uv = vsMain->resolveInputParameter(Parameter::SPC_TEXTURE_COORDINATE0, GCT_FLOAT2);

    // The main camera, also when rendering the shadow caster: fading, growing and sprite facing must
    // match what the player sees, not the light's point of view.
    m_camera_pos_param = ResolveUniform(vsProgram, GCT_FLOAT3, "grassCameraPos");
    auto invWorld = vsProgram->resolveParameter(GpuProgramParameters::ACT_INVERSE_WORLD_MATRIX);
    m_fade_range_param = ResolveUniform(vsProgram, GCT_FLOAT1, "grassFadeRange");

    auto camPos = vsMain->resolveLocalParameter(GCT_FLOAT3, "grassCamPosObj");
    auto origPosition = vsMain->resolveLocalParameter(GCT_FLOAT4, "grassOrigPosition");
    auto dist = vsMain->resolveLocalParameter(GCT_FLOAT1, "grassDist");
    auto vsOutFade = vsMain->resolveOutputParameter(PAGED_GRASS_FADE, GCT_FLOAT1);

    // Before FFP_VS_TRANSFORM, so that everything else (transform, lighting, shadow receiving) uses the moved vertex
    auto stage = vsMain->getStage(FFP_VS_PRE_PROCESS + 10);
    stage.callFunction("SGX_Paged_ToObjectSpace", {In(invWorld), In(m_camera_pos_param), Out(camPos)});
    stage.assign(position, origPosition);
    stage.callFunction("SGX_Paged_GrassDistance", {In(position), In(camPos), Out(dist)});

    if (m_fade_alpha)
        stage.callFunction("SGX_Paged_GrassFadeAlpha", {In(dist), In(m_fade_range_param), Out(vsOutFade)});
    else
        stage.assign(1.0f, vsOutFade);

    if (m_sprite)
    {
        // The corners are packed in the normal (see GrassLayer)
        auto corner = vsMain->resolveInputParameter(Parameter::SPC_NORMAL_OBJECT_SPACE);
        stage.callFunction("SGX_Paged_GrassSprite", {InOut(position), In(camPos), In(corner)});
    }

    if (m_animate)
    {
        m_time_param = ResolveUniform(vsProgram, GCT_FLOAT1, "grassTime");
        m_frequency_param = ResolveUniform(vsProgram, GCT_FLOAT1, "grassFrequency");
        m_direction_param = ResolveUniform(vsProgram, GCT_FLOAT4, "grassDirection");
        stage.callFunction("SGX_Paged_GrassWave",
            {InOut(position), In(origPosition), In(uv), In(m_time_param), In(m_frequency_param), In(m_direction_param)});
    }

    if (m_blend)
        stage.callFunction("SGX_Paged_GrassBlendBase", {InOut(vsOutFade), In(uv), In(dist), In(m_fade_range_param)});

    if (m_fade_grow)
    {
        m_grass_height_param = ResolveUniform(vsProgram, GCT_FLOAT1, "grassHeight");
        stage.callFunction("SGX_Paged_GrassGrow", {InOut(position), In(dist), In(m_fade_range_param), In(m_grass_height_param)});
    }

    if (!m_caster)
    {
        // Lit like the old shader: as if the blades faced straight up (sprites carry their corners in the normal,
        // the other grass types have no normals at all). After the sprite code read it.
        auto normal = vsMain->resolveInputParameter(Parameter::SPC_NORMAL_OBJECT_SPACE);
        stage.assign(Vector3::UNIT_Y, normal);
    }

    auto psInFade = psMain->resolveInputParameter(vsOutFade);
    auto outDiffuse = psMain->resolveOutputParameter(Parameter::SPC_COLOR_DIFFUSE);
    psMain->getStage(FFP_PS_TEXTURING + 10).callFunction("SGX_Paged_ApplyFade", {In(psInFade), InOut(outDiffuse)});

    return true;
}

void GrassSubRenderState::updateGpuProgramsParams(Renderable*, const Pass*, const AutoParamDataSource*, const LightList*)
{
    m_camera_pos_param->setGpuParameter(m_state->camera_pos);
    m_fade_range_param->setGpuParameter(m_fade_range);
    if (m_animate)
    {
        m_time_param->setGpuParameter(m_state->time);
        m_frequency_param->setGpuParameter(m_state->frequency);
        m_direction_param->setGpuParameter(m_state->direction);
    }
    if (m_fade_grow)
    {
        m_grass_height_param->setGpuParameter(m_grass_height);
    }
}
