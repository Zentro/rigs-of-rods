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

#include "GfxPipeline.h"

#include "Application.h"

#include <OgreCompositorManager.h>

using namespace Ogre;
using namespace RoR;

void GfxPipeline::Init(Ogre::Viewport* viewport)
{
    if (App::gfx_render_pipeline->getEnum<GfxRenderPipeline>() != GfxRenderPipeline::PBR)
    {
        LOG("[RoR|GfxPipeline] Legacy pipeline (gamma space, renders directly to the window)");
        return;
    }

    CompositorInstance* hdr = CompositorManager::getSingleton().addCompositor(viewport, "RoR/HDR");
    if (!hdr)
    {
        LOG("[RoR|GfxPipeline] ERROR: could not add compositor 'RoR/HDR', using the legacy pipeline");
        return;
    }
    hdr->addListener(this);
    CompositorManager::getSingleton().setCompositorEnabled(viewport, "RoR/HDR", true);
    if (!hdr->getEnabled())
    {
        LOG("[RoR|GfxPipeline] ERROR: compositor 'RoR/HDR' is not supported here, using the legacy pipeline");
        CompositorManager::getSingleton().removeCompositor(viewport, "RoR/HDR");
        return;
    }

    m_active = true;
    this->UpdateFrame();
    LOG("[RoR|GfxPipeline] HDR pipeline active (half-float scene, tonemapping)");
}

void GfxPipeline::UpdateFrame()
{
    if (!m_active)
        return;

    m_tonemap_values.x = Math::Clamp(App::gfx_exposure->getFloat(), 0.01f, 16.f);
    m_tonemap_values.y = static_cast<float>(Math::Clamp(App::gfx_tonemap->getInt(), 0, 2));
}

void GfxPipeline::notifyMaterialRender(Ogre::uint32 pass_id, Ogre::MaterialPtr& mat)
{
    // The compositor renders its own copy of the material - its constants must be set here, every frame.
    if (!mat || mat->getNumTechniques() == 0 || mat->getTechnique(0)->getNumPasses() == 0)
        return;

    if (pass_id == PASS_TONEMAP)
    {
        GpuProgramParametersSharedPtr fp = mat->getTechnique(0)->getPass(0)->getFragmentProgramParameters();
        fp->setIgnoreMissingParams(true);
        fp->setNamedConstant("exposure", m_tonemap_values.x);
        fp->setNamedConstant("tonemapMode", m_tonemap_values.y);
    }
}
