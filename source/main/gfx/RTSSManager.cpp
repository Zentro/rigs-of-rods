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

#include <Ogre.h>
#include <Terrain/OgreTerrain.h>
#include <Overlay/OgreOverlayManager.h>
#include <Overlay/OgreOverlayContainer.h>
#include <Overlay/OgreOverlay.h>
#include <OgreMaterialManager.h>
#include <Bites/OgreSGTechniqueResolverListener.h>
#include <RTShaderSystem/OgreRTShaderSystem.h>

#include <unordered_set>

using namespace Ogre;
using namespace RoR;

namespace {

class RoRTechniqueResolver: public OgreBites::SGTechniqueResolverListener
{
public:
    using SGTechniqueResolverListener::SGTechniqueResolverListener;

    Ogre::Technique* handleSchemeNotFound(unsigned short schemeIndex, const Ogre::String& schemeName,
        Ogre::Material* mat, unsigned short lodIndex, const Ogre::Renderable* rend) override
    {
        if (schemeName != RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME)
            return nullptr;

        if (m_busy)
            return nullptr;
        struct BusyScope { bool& b; BusyScope(bool& x): b(x) { b = true; } ~BusyScope() { b = false; } } busy(m_busy);

        if (HasOwnShaders(mat))
            return nullptr;

        const std::string key = mat->getGroup() + "/" + mat->getName();
        if (m_failed.count(key))
            return nullptr;

        if (HasVertexColors(rend))
            UseVertexColorIfUnlit(mat);

        Ogre::Technique* tech = SGTechniqueResolverListener::handleSchemeNotFound(schemeIndex, schemeName, mat, lodIndex, rend);

        if (tech && (tech->getNumPasses() == 0 || !tech->getPass(0)->isProgrammable()))
        {
            RTShader::ShaderGenerator::getSingleton().removeAllShaderBasedTechniques(*mat);
            tech = nullptr;
        }
        if (!tech)
        {
            m_failed.insert(key);
            LOG(fmt::format("[RoR|RTSS] Could not generate a technique for material '{}' (group '{}'), using its own",
                mat->getName(), mat->getGroup()));
        }
        return tech;
    }

private:
    static bool HasVertexColors(const Ogre::Renderable* rend)
    {
        if (!rend)
            return false;
        if (dynamic_cast<const Ogre::BillboardSet*>(rend) || dynamic_cast<const Ogre::BillboardChain*>(rend))
            return true;
        const Ogre::VertexData* vd = nullptr;
        if (const Ogre::SubEntity* se = dynamic_cast<const Ogre::SubEntity*>(rend))
        {
            const Ogre::SubMesh* sm = se->getSubMesh();
            vd = sm->useSharedVertices ? sm->parent->sharedVertexData : sm->vertexData;
        }
        else
        {
            Ogre::RenderOperation op;
            const_cast<Ogre::Renderable*>(rend)->getRenderOperation(op);
            vd = op.vertexData;
        }
        return vd && vd->vertexDeclaration && vd->vertexDeclaration->findElementBySemantic(Ogre::VES_DIFFUSE);
    }

    static void UseVertexColorIfUnlit(Ogre::Material* mat)
    {
        for (Ogre::Technique* tech : mat->getTechniques())
        {
            if (tech->getSchemeName() != Ogre::MaterialManager::DEFAULT_SCHEME_NAME)
                continue;
            for (Ogre::Pass* pass : tech->getPasses())
            {
                if (!pass->getLightingEnabled() && pass->getVertexColourTracking() == Ogre::TVC_NONE)
                    pass->setVertexColourTracking(Ogre::TVC_DIFFUSE);
            }
        }
    }

    static bool HasOwnShaders(const Ogre::Material* mat)
    {
        for (const Ogre::Technique* tech : mat->getTechniques())
        {
            if (tech->getSchemeName() != Ogre::MaterialManager::DEFAULT_SCHEME_NAME)
                continue;
            for (const Ogre::Pass* pass : tech->getPasses())
            {
                if (pass->isProgrammable())
                    return true;
            }
        }
        return false;
    }

    std::unordered_set<std::string> m_failed;
    bool m_busy = false;
};

}

void RTSSManager::InstallTechniqueResolver()
{
    static RoRTechniqueResolver resolver(RTShader::ShaderGenerator::getSingletonPtr());
    Ogre::MaterialManager::getSingleton().addListener(&resolver);
}

Ogre::MaterialPtr RTSSManager::CloneMaterial(const Ogre::MaterialPtr& src, const std::string& new_name, const std::string& new_group)
{
    Ogre::MaterialPtr dst = src->clone(new_name, new_group);
    RTShader::ShaderGenerator::getSingleton().cloneShaderBasedTechniques(*src, *dst);
    return dst;
}

void RTSSManager::RefreshMaterial(Ogre::Material& mat)
{
    auto& shader_gen = RTShader::ShaderGenerator::getSingleton();
    shader_gen.invalidateMaterial(RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME, mat);
    shader_gen.validateMaterial(RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME, mat);
}


void RTSSManager::setupShadows()
{
    if (App::gfx_shadow_type->getEnum<GfxShadowType>() == GfxShadowType::NONE)
        return;

    this->processPSSM();
}

void RTSSManager::processPSSM()
{
    const int NUM_SHADOW_TEXTURES = 3;
    const int NUM_SPLITS = 3;

    App::GetGfxScene()->GetSceneManager()->setShadowTechnique(Ogre::SHADOWTYPE_TEXTURE_MODULATIVE_INTEGRATED);
    App::GetGfxScene()->GetSceneManager()->setShadowDirLightTextureOffset(0);
    App::GetGfxScene()->GetSceneManager()->setShadowFarDistance(3000.0f);
    App::GetGfxScene()->GetSceneManager()->setShadowTextureCountPerLightType(Ogre::Light::LT_DIRECTIONAL, NUM_SHADOW_TEXTURES);
    App::GetGfxScene()->GetSceneManager()->setShadowTextureCountPerLightType(Ogre::Light::LT_SPOTLIGHT, NUM_SHADOW_TEXTURES);
    App::GetGfxScene()->GetSceneManager()->setShadowTextureCountPerLightType(Ogre::Light::LT_POINT, NUM_SHADOW_TEXTURES);
    App::GetGfxScene()->GetSceneManager()->setShadowTextureCount(NUM_SHADOW_TEXTURES);

    //Caster is set via materials
    MaterialPtr shadowMat = MaterialManager::getSingleton().getByName("Ogre/TextureShadowCaster");
    App::GetGfxScene()->GetSceneManager()->setShadowTextureCasterMaterial(shadowMat);

    if (App::gfx_shadow_quality->getInt() == 3) // Ultra
    {
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(0, 4096, 4096, PF_X8B8G8R8);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(1, 3072, 3072, PF_X8B8G8R8);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(2, 2048, 2048, PF_X8B8G8R8);
    }
    else if (App::gfx_shadow_quality->getInt() == 2) // HQ
    {
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(0, 3072, 3072, PF_X8B8G8R8);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(1, 2048, 2048, PF_X8B8G8R8);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(2, 2048, 2048, PF_X8B8G8R8);
    }
    else if (App::gfx_shadow_quality->getInt() == 1) // Mid
    {
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(0, 2048, 2048, PF_X8B8G8R8);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(1, 1024, 1024, PF_X8B8G8R8);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(2, 1024, 1024, PF_X8B8G8R8);
    }
    else // Low
    {
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(0, 1024, 1024, PF_X8B8G8R8);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(1, 1024, 1024, PF_X8B8G8R8);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(2, 512, 512, PF_X8B8G8R8);
    }

    // shadow camera setup
    pssmSetup = new Ogre::PSSMShadowCameraSetup();
    pssmSetup->calculateSplitPoints(NUM_SPLITS, App::GetCameraManager()->GetCamera()->getNearClipDistance(), App::GetGfxScene()->GetSceneManager()->getShadowFarDistance());
    pssmSetup->setSplitPadding(App::GetCameraManager()->GetCamera()->getNearClipDistance() * 2);
    pssmSetup->setOptimalAdjustFactor(0, 2);
    pssmSetup->setOptimalAdjustFactor(1, 1);
    pssmSetup->setOptimalAdjustFactor(2, 0.5);
    App::GetGfxScene()->GetSceneManager()->setShadowCameraSetup(Ogre::ShadowCameraSetupPtr(pssmSetup));

}

// from Paroj on Gitter chat:
// PSSM are woking w/o MSN_SHADERGEN, but only the first split is rendered.
//the issue is that the RTSS does not call updateGpuProgramsParams if it is not active on that viewport, 
// so the PSSM splits are stale
//you can override that **for testing purposes only** like:
struct RTSSParamsUpdater : public Ogre::RenderObjectListener
{
    void notifyRenderSingleObject(Ogre::Renderable* rend, const Ogre::Pass* pass,
                                  const Ogre::AutoParamDataSource* source,
                                  const Ogre::LightList* pLightList, bool suppressRenderStateChanges) override
    {
        auto userData = pass->getUserObjectBindings().getUserAny(Ogre::RTShader::TargetRenderState::UserKey);
        if (userData.has_value())
        {
            auto renderState = Ogre::any_cast<Ogre::RTShader::TargetRenderStatePtr>(userData);
            renderState->updateGpuProgramsParams(rend, pass, source, pLightList);
        }
    }
};

void RTSSManager::applyToTerrain(Ogre::TerrainMaterialGeneratorA::SM2Profile* matProfile)
{
    if (pssmSetup)
    {
        LOG("[RoR|Terrain] Enabling PSSM...");
        matProfile->setReceiveDynamicShadowsEnabled(true);
        matProfile->setReceiveDynamicShadowsPSSM(pssmSetup);
        matProfile->setLightmapEnabled(false);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureSelfShadow(false);

        App::GetGameContext()->GetTerrain()->getMainLight()->setCastShadows(true);

        App::GetGfxScene()->GetSceneManager()->addRenderObjectListener(new RTSSParamsUpdater());
    }
}


