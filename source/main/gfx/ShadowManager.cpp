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

#include "ShadowManager.h"

#include "CameraManager.h"
#include "GfxScene.h"

#include <Ogre.h>
#include <Terrain/OgreTerrain.h>
#include <Overlay/OgreOverlayManager.h>
#include <Overlay/OgreOverlayContainer.h>
#include <Overlay/OgreOverlay.h>
#include <OgreMaterialManager.h>

using namespace Ogre;
using namespace RoR;

ShadowManager::ShadowManager()
{
}

ShadowManager::~ShadowManager()
{
}

void ShadowManager::loadConfiguration()
{
    this->updateShadowTechnique(); // Config handled by RoR::App
}

int ShadowManager::updateShadowTechnique()
{
    float scoef = 0.5;
    App::GetGfxScene()->GetSceneManager()->setShadowColour(Ogre::ColourValue(0.563 + scoef, 0.578 + scoef, 0.625 + scoef));
    App::GetGfxScene()->GetSceneManager()->setShowDebugShadows(false);
    return 0;
}

void ShadowManager::processPSSM()
{
    App::GetGfxScene()->GetSceneManager()->setShadowTechnique(Ogre::SHADOWTYPE_TEXTURE_ADDITIVE_INTEGRATED);

    App::GetGfxScene()->GetSceneManager()->setShadowDirectionalLightExtrusionDistance(299.0f);
    App::GetGfxScene()->GetSceneManager()->setShadowFarDistance(350.0f);
    App::GetGfxScene()->GetSceneManager()->setShadowTextureCountPerLightType(Ogre::Light::LT_DIRECTIONAL, PSSM_Shadows.ShadowsTextureNum);
    App::GetGfxScene()->GetSceneManager()->setShadowTextureCount(PSSM_Shadows.ShadowsTextureNum);

    App::GetGfxScene()->GetSceneManager()->setShadowTextureSelfShadow(true);
    App::GetGfxScene()->GetSceneManager()->setShadowCasterRenderBackFaces(true);

    //Caster is set via materials
    MaterialPtr shadowMat = MaterialManager::getSingleton().getByName("Ogre/shadow/depth/caster");
    App::GetGfxScene()->GetSceneManager()->setShadowTextureCasterMaterial(shadowMat);

    if (PSSM_Shadows.Quality == 3)
    {
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(0, 4096, 4096, PF_FLOAT32_R);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(1, 3072, 3072, PF_FLOAT32_R);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(2, 2048, 2048, PF_FLOAT32_R);
        PSSM_Shadows.lambda = 0.965f;
    }
    else if (PSSM_Shadows.Quality == 2)
    {
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(0, 3072, 3072, PF_FLOAT32_R);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(1, 2048, 2048, PF_FLOAT32_R);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(2, 2048, 2048, PF_FLOAT32_R);
        PSSM_Shadows.lambda = 0.97f;
    }
    else if (PSSM_Shadows.Quality == 1)
    {
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(0, 2048, 2048, PF_FLOAT32_R);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(1, 1024, 1024, PF_FLOAT32_R);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(2, 1024, 1024, PF_FLOAT32_R);
        PSSM_Shadows.lambda = 0.975f;
    }
    else
    {
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(0, 1024, 1024, PF_FLOAT32_R);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(1, 1024, 1024, PF_FLOAT32_R);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(2, 512, 512, PF_FLOAT32_R);
        PSSM_Shadows.lambda = 0.98f;
    }

    if (PSSM_Shadows.mPSSMSetup.isNull())
    {
        // shadow camera setup
        Ogre::PSSMShadowCameraSetup* pssmSetup = new Ogre::PSSMShadowCameraSetup();

        pssmSetup->calculateSplitPoints(3, App::GetCameraManager()->GetCamera()->getNearClipDistance(), App::GetGfxScene()->GetSceneManager()->getShadowFarDistance(), PSSM_Shadows.lambda);
        pssmSetup->setSplitPadding(App::GetCameraManager()->GetCamera()->getNearClipDistance());

        pssmSetup->setOptimalAdjustFactor(0, -1);
        pssmSetup->setOptimalAdjustFactor(1, -1);
        pssmSetup->setOptimalAdjustFactor(2, -1);

        PSSM_Shadows.mPSSMSetup.bind(pssmSetup);

        //Send split info to managed materials
        setManagedMaterialSplitPoints(pssmSetup->getSplitPoints());
    }
    App::GetGfxScene()->GetSceneManager()->setShadowCameraSetup(PSSM_Shadows.mPSSMSetup);
}

void ShadowManager::updatePSSM()
{
    if (!PSSM_Shadows.mPSSMSetup.get())
        return;
    //Ugh what here?
}

void ShadowManager::updateTerrainMaterial(Ogre::TerrainPSSMMaterialGenerator::SM2Profile* matProfile)
{
    // to be removed
}

void ShadowManager::setManagedMaterialSplitPoints(Ogre::PSSMShadowCameraSetup::SplitPointList splitPointList)
{
    Ogre::Vector4 splitPoints;

    for (int i = 0; i < 3; ++i)
        splitPoints[i] = splitPointList[i];

    GpuSharedParametersPtr p = GpuProgramManager::getSingleton().getSharedParameters("pssm_params");
    p->setNamedConstant("pssmSplitPoints", splitPoints);
}
