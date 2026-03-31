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

#include "RTSSManager.h"

#include "Actor.h"
#include "CameraManager.h"
#include "GfxScene.h"
#include "AppContext.h"

#include <Ogre.h>
#include <Terrain/OgreTerrain.h>
#include <Overlay/OgreOverlayContainer.h>
#include <OgreMaterialManager.h>
#include <RTShaderSystem/OgreRTShaderSystem.h>
#include <RTShaderSystem/OgreShaderSubRenderState.h>

using namespace Ogre;
using namespace RoR;

RTSSManager::RTSSManager()
{
}

RTSSManager::~RTSSManager()
{
}

void RTSSManager::SetupRTSS()
{
    auto* mShaderGenerator = Ogre::RTShader::ShaderGenerator::getSingletonPtr();
    auto* schemRenderState = mShaderGenerator->getRenderState(Ogre::MSN_SHADERGEN);

    // RTSS PSSM3
    if (App::gfx_shadow_type->getEnum<GfxShadowType>() == GfxShadowType::PSSM)
    {
        App::GetGfxScene()->GetSceneManager()->setShadowTechnique(SHADOWTYPE_TEXTURE_MODULATIVE_INTEGRATED);
        App::GetGfxScene()->GetSceneManager()->setShadowFarDistance(350);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureCountPerLightType(Ogre::Light::LT_DIRECTIONAL, 3);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureCountPerLightType(Ogre::Light::LT_POINT, 0);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureCountPerLightType(Ogre::Light::LT_SPOTLIGHT, 0);
        // D3D9 cannot clear a PF_DEPTH16 surface as a color render target; use a float format instead.
        const bool isD3D9 = Root::getSingleton().getRenderSystem()->getName().find("Direct3D9") != std::string::npos;
        const Ogre::PixelFormat shadowFmt = isD3D9 ? PF_FLOAT32_R : PF_DEPTH16;
        App::GetGfxScene()->GetSceneManager()->setShadowTextureSettings(2048, 3, shadowFmt);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureSelfShadow(true);

        pssmSetup = new PSSMShadowCameraSetup();
        pssmSetup->calculateSplitPoints(3, 1, 500, 1);
        pssmSetup->setSplitPadding(App::GetCameraManager()->GetCamera()->getNearClipDistance());
        pssmSetup->setOptimalAdjustFactor(0, 2);
        pssmSetup->setOptimalAdjustFactor(1, 1);
        pssmSetup->setOptimalAdjustFactor(2, 0.5);

        App::GetGfxScene()->GetSceneManager()->setShadowCameraSetup(ShadowCameraSetupPtr(pssmSetup));
        auto pssmState = mShaderGenerator->createSubRenderState(RTShader::SRS_INTEGRATED_PSSM3);
        // PSSMShadowCameraSetup::SplitPointList is vector<Real> (double on 64-bit),
        // but IntegratedPSSM3 expects vector<float>. Convert explicitly to avoid bad_any_cast crash.
        const auto& srcSplits = pssmSetup->getSplitPoints();
        std::vector<float> splits(srcSplits.begin(), srcSplits.end());
        pssmState->setParameter("split_points", Ogre::Any(splits));
        schemRenderState->addTemplateSubRenderState(pssmState);

        // Propagate the computed split points to the hand-written PSSM shadow shaders
        // (general.hlsl / general_diffuse_fp.glsl). The hardcoded values in general.program
        // are stale; the actual splits come from pssmSetup->getSplitPoints().
        Ogre::Vector4 splitPts(splits[0], splits[1], splits[2], splits[3]);
        for (const char* progName : {"diffuse_sh_ps_HLSL", "diffuse_sh_ps_GLSL",
                                     "diffuse_sh_a_ps_HLSL", "diffuse_sh_a_ps_GLSL"})
        {
            Ogre::GpuProgramPtr prog = Ogre::GpuProgramManager::getSingleton().getByName(progName);
            if (prog && prog->getDefaultParameters())
                prog->getDefaultParameters()->setNamedConstant("pssmSplitPoints", splitPts);
        }
    }

}

void RTSSManager::EnableRTSS(const MaterialPtr& mat)
{
    Ogre::RTShader::ShaderGenerator* mShaderGenerator = Ogre::RTShader::ShaderGenerator::getSingletonPtr();
    mShaderGenerator->createShaderBasedTechnique(*mat, Ogre::MaterialManager::DEFAULT_SCHEME_NAME, Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);
}