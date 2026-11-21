/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2013-2026 Petr Ohlidal

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
/// @brief RTSS sub render states for PagedGeometry (trees, grass).
/// They replace the vertex shaders PagedGeometry used to generate itself, so that RTSS can shade
/// vegetation like the rest of the scene (per-pixel lighting, shadows, fog).

#pragma once

#include <OgreShaderSubRenderState.h>
#include <OgreVector.h>

#include <memory>

namespace RoR {

/// @addtogroup Gfx
/// @{

/// Tree batches (PagedGeometry BatchPage): fade out between the visible and invisible distance.
class PagedFadeSubRenderState : public Ogre::RTShader::SubRenderState
{
public:
    static const Ogre::String Type;

    const Ogre::String& getType() const override { return Type; }
    int getExecutionOrder() const override;
    void copyFrom(const SubRenderState& rhs) override;
    bool setParameter(const Ogre::String& name, const Ogre::String& value) override;
    bool createCpuSubPrograms(Ogre::RTShader::ProgramSet* programSet) override;
    void updateGpuProgramsParams(Ogre::Renderable* rend, const Ogre::Pass* pass, const Ogre::AutoParamDataSource* source, const Ogre::LightList* lights) override;

private:
    float m_invisible_dist = 0.f;
    float m_fade_gap = 1.f;
    Ogre::RTShader::UniformParameterPtr m_invisible_dist_param;
    Ogre::RTShader::UniformParameterPtr m_fade_gap_param;
};

/// Values shared by all materials of one grass layer, updated every frame by GrassLoader.
struct GrassShaderState
{
    float time = 0.f;                       //!< Wave phase
    float frequency = 0.f;                  //!< Wave frequency
    Ogre::Vector4 direction = Ogre::Vector4::ZERO; //!< Wave direction and length
    Ogre::Vector3 camera_pos = Ogre::Vector3::ZERO; //!< World space; the main camera's, also while rendering shadows
};

/// Grass (PagedGeometry GrassLoader): camera facing sprites, waving, distance fade/grow and base blending.
/// Grass gets a fixed up-facing normal for lighting, as the old shader had. Also used for the grass shadow caster
/// (`caster` = true: no fading - distances are measured from the main camera, not from the light).
class GrassSubRenderState : public Ogre::RTShader::SubRenderState
{
public:
    static const Ogre::String Type;

    const Ogre::String& getType() const override { return Type; }
    int getExecutionOrder() const override;
    void copyFrom(const SubRenderState& rhs) override;
    bool setParameter(const Ogre::String& name, const Ogre::String& value) override;
    bool createCpuSubPrograms(Ogre::RTShader::ProgramSet* programSet) override;
    void updateGpuProgramsParams(Ogre::Renderable* rend, const Ogre::Pass* pass, const Ogre::AutoParamDataSource* source, const Ogre::LightList* lights) override;

    /// Settings, see GrassLayer. Call before the sub render state is added to a render state.
    void setup(std::shared_ptr<GrassShaderState> state, bool sprite, bool animate, bool blend,
               bool fade_alpha, bool fade_grow, float fade_range, float grass_height, bool caster);

private:
    std::shared_ptr<GrassShaderState> m_state;
    bool m_sprite = false;
    bool m_animate = false;
    bool m_blend = false;
    bool m_fade_alpha = false;
    bool m_fade_grow = false;
    bool m_caster = false;
    float m_fade_range = 1.f;
    float m_grass_height = 0.f;

    Ogre::RTShader::UniformParameterPtr m_time_param;
    Ogre::RTShader::UniformParameterPtr m_frequency_param;
    Ogre::RTShader::UniformParameterPtr m_direction_param;
    Ogre::RTShader::UniformParameterPtr m_camera_pos_param;
    Ogre::RTShader::UniformParameterPtr m_fade_range_param;
    Ogre::RTShader::UniformParameterPtr m_grass_height_param;
};

class PagedFadeSubRenderStateFactory : public Ogre::RTShader::SubRenderStateFactory
{
public:
    const Ogre::String& getType() const override { return PagedFadeSubRenderState::Type; }
protected:
    Ogre::RTShader::SubRenderState* createInstanceImpl() override { return new PagedFadeSubRenderState; }
};

class GrassSubRenderStateFactory : public Ogre::RTShader::SubRenderStateFactory
{
public:
    const Ogre::String& getType() const override { return GrassSubRenderState::Type; }
protected:
    Ogre::RTShader::SubRenderState* createInstanceImpl() override { return new GrassSubRenderState; }
};

/// @} // addtogroup Gfx

} // namespace RoR
