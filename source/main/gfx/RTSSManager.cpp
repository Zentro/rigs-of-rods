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

/// OGRE asks this listener whenever it needs a material's technique for a scheme the material has none for.
/// For the RTSS scheme, the base class asks RTSS to generate one from the material's 'Default' technique.
class RoRTechniqueResolver: public OgreBites::SGTechniqueResolverListener
{
public:
    using SGTechniqueResolverListener::SGTechniqueResolverListener;

    Ogre::Technique* handleSchemeNotFound(unsigned short schemeIndex, const Ogre::String& schemeName,
        Ogre::Material* mat, unsigned short lodIndex, const Ogre::Renderable* rend) override
    {
        if (schemeName != RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME)
            return nullptr;

        // Re-entry guard: anything below that makes OGRE look up a technique again (e.g. querying an
        // entity's render operation) would end up here again and recurse until the stack overflows.
        if (m_busy)
            return nullptr;
        struct BusyScope { bool& b; BusyScope(bool& x): b(x) { b = true; } ~BusyScope() { b = false; } } busy(m_busy);

        // Materials with their own shaders (GLSL, HLSL, Cg) keep them: RTSS would replace or break them
        // (on gfx-rtss this broke sky boxes and shadow casting). Returning null = OGRE uses the material's
        // normal technique, exactly as before.
        if (HasOwnShaders(mat))
            return nullptr;

        const std::string key = mat->getGroup() + "/" + mat->getName();
        if (m_failed.count(key))
            return nullptr;

        if (HasVertexColours(rend))
            UseVertexColourIfUnlit(mat);

        Ogre::Technique* tech = SGTechniqueResolverListener::handleSchemeNotFound(schemeIndex, schemeName, mat, lodIndex, rend);

        // RTSS can register a technique and then fail to generate shaders for it. An empty or shader-less
        // technique would be drawn wrong or crash code that expects passes (see GfxActor::SetMaterialFlareOn),
        // so drop it and let OGRE use the normal technique. Remembered, otherwise this repeats every frame.
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
    /// Whether the geometry being drawn has per-vertex colours (particles, flares and other billboards always
    /// do; some meshes too, e.g. the grid lines of simple2).
    static bool HasVertexColours(const Ogre::Renderable* rend)
    {
        if (!rend)
            return false;
        if (dynamic_cast<const Ogre::BillboardSet*>(rend) || dynamic_cast<const Ogre::BillboardChain*>(rend))
            return true; // Billboards always have vertex colours
        const Ogre::VertexData* vd = nullptr;
        if (const Ogre::SubEntity* se = dynamic_cast<const Ogre::SubEntity*>(rend))
        {
            // Read the mesh directly: SubEntity::getRenderOperation() looks up the material's technique
            // (to choose hardware/software animation), which would call this resolver again.
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

    /// Fixed function uses the vertex colour on unlit passes by itself; RTSS only if the pass asks for it
    /// (`diffuse vertexcolour`). Without it, particles (colour and fade-out are vertex colours) are drawn
    /// full white and opaque, vertex-coloured lines white. Only called for geometry that has vertex colours.
    static void UseVertexColourIfUnlit(Ogre::Material* mat)
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
                continue; // e.g. techniques RTSS or other systems added
            for (const Ogre::Pass* pass : tech->getPasses())
            {
                if (pass->isProgrammable())
                    return true;
            }
        }
        return false;
    }

    std::unordered_set<std::string> m_failed; //!< "group/name" of materials RTSS couldn't handle
    bool m_busy = false; //!< Inside handleSchemeNotFound()
};

} // namespace

void RTSSManager::InstallTechniqueResolver()
{
    static RoRTechniqueResolver resolver(RTShader::ShaderGenerator::getSingletonPtr());
    Ogre::MaterialManager::getSingleton().addListener(&resolver);
}

Ogre::MaterialPtr RTSSManager::CloneMaterial(const Ogre::MaterialPtr& src, const std::string& new_name, const std::string& new_group)
{
    Ogre::MaterialPtr dst = src->clone(new_name, new_group);
    // Removes the RTSS technique `clone()` copied and creates a registered one with the same render state
    // (e.g. the `rtshader_system` block of managed materials). If RTSS doesn't know the source (not drawn
    // yet), nothing is copied and the technique resolver creates one when the copy is first drawn.
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

    // Casters may shadow themselves (vehicle roof onto trunk, mirrors, wheel arches ...)
    App::GetGfxScene()->GetSceneManager()->setShadowTextureSelfShadow(true);

    //Caster is set via materials
    MaterialPtr shadowMat = MaterialManager::getSingleton().getByName("Ogre/TextureShadowCaster");
    App::GetGfxScene()->GetSceneManager()->setShadowTextureCasterMaterial(shadowMat);

    // Depth textures: the shadow cameras only render depth, which receivers compare against their own depth.
    // (With colour textures the caster material writes a *colour* - the receivers' comparison was meaningless:
    // shadows popped in and out with the camera and casters couldn't shadow themselves.)
    // PF_DEPTH16 is the only depth render target our GL render system accepts ("Valid FBO targets" in RoR.log);
    // OGRE silently replaces other depth formats with a colour format.
    const Ogre::PixelFormat SHADOW_FORMAT = PF_DEPTH16;

    if (App::gfx_shadow_quality->getInt() == 3) // Ultra
    {
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(0, 4096, 4096, SHADOW_FORMAT);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(1, 3072, 3072, SHADOW_FORMAT);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(2, 2048, 2048, SHADOW_FORMAT);
    }
    else if (App::gfx_shadow_quality->getInt() == 2) // HQ
    {
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(0, 3072, 3072, SHADOW_FORMAT);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(1, 2048, 2048, SHADOW_FORMAT);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(2, 2048, 2048, SHADOW_FORMAT);
    }
    else if (App::gfx_shadow_quality->getInt() == 1) // Mid
    {
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(0, 2048, 2048, SHADOW_FORMAT);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(1, 1024, 1024, SHADOW_FORMAT);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(2, 1024, 1024, SHADOW_FORMAT);
    }
    else // Low
    {
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(0, 1024, 1024, SHADOW_FORMAT);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(1, 1024, 1024, SHADOW_FORMAT);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(2, 512, 512, SHADOW_FORMAT);
    }

    // shadow camera setup
    pssmSetup = new StableCSMShadowCameraSetup();
    pssmSetup->calculateSplitPoints(NUM_SPLITS, App::GetCameraManager()->GetCamera()->getNearClipDistance(), App::GetGfxScene()->GetSceneManager()->getShadowFarDistance());
    pssmSetup->setSplitPadding(App::GetCameraManager()->GetCamera()->getNearClipDistance() * 2);
    App::GetGfxScene()->GetSceneManager()->setShadowCameraSetup(Ogre::ShadowCameraSetupPtr(pssmSetup));

    // Log what OGRE actually created - it falls back to other formats silently
    for (size_t i = 0; i < NUM_SHADOW_TEXTURES; i++)
    {
        const Ogre::TexturePtr& tex = App::GetGfxScene()->GetSceneManager()->getShadowTexture(i);
        LOG(fmt::format("[RoR|Shadows] Shadow texture {}: {}x{} {}", i, tex->getWidth(), tex->getHeight(),
            Ogre::PixelUtil::getFormatName(tex->getFormat())));
    }
}

void RTSSManager::applyToTerrain(Ogre::TerrainMaterialGeneratorA::SM2Profile* matProfile)
{
    if (pssmSetup)
    {
        LOG("[RoR|Terrain] Enabling PSSM...");
        matProfile->setReceiveDynamicShadowsEnabled(true);
        matProfile->setReceiveDynamicShadowsPSSM(pssmSetup);
        matProfile->setLightmapEnabled(false);

        App::GetGameContext()->GetTerrain()->getMainLight()->setCastShadows(true);
    }
}

void StableCSMShadowCameraSetup::getShadowCamera(const SceneManager* sm, const Camera* cam, const Viewport* vp,
                                                 const Light* light, Camera* texCam, size_t iteration) const
{
    if (light->getType() != Light::LT_DIRECTIONAL)
    {
        DefaultShadowCameraSetup::getShadowCamera(sm, cam, vp, light, texCam, iteration);
        return;
    }

    // This cascade's slice of the view frustum (with padding, so neighbouring cascades overlap a little)
    const SplitPointList& splits = this->getSplitPoints();
    Real nearDist = splits[iteration];
    Real farDist = splits[iteration + 1];
    if (iteration > 0)
        nearDist -= this->getSplitPadding();
    if (iteration < this->getSplitCount() - 1)
        farDist += this->getSplitPadding();

    // Bounding sphere of the slice. Its radius only depends on FOV/aspect/split distances, so the shadow map
    // always covers the same world area - no resizing when the camera turns.
    const Real tanY = Math::Tan(cam->getFOVy() * 0.5f);
    const Real tanX = tanY * cam->getAspectRatio();
    const Real kNear = (tanX * tanX + tanY * tanY) * nearDist * nearDist;
    const Real kFar = (tanX * tanX + tanY * tanY) * farDist * farDist;
    // Centre on the view axis at the distance `z` that minimizes the radius
    Real z = (farDist * farDist - nearDist * nearDist + kFar - kNear) / (2.f * (farDist - nearDist));
    z = Math::Clamp(z, nearDist, farDist);
    Real radius = Math::Sqrt(std::max((farDist - z) * (farDist - z) + kFar, (z - nearDist) * (z - nearDist) + kNear));
    radius = Math::Ceil(radius * 16.f) / 16.f; // Quantize against float noise

    Vector3 center = cam->getDerivedPosition() + cam->getDerivedDirection() * z;

    // Light-space basis; the shadow camera looks along the light direction
    const Vector3 dir = light->getDerivedDirection().normalisedCopy();
    const Vector3 up = (Math::Abs(dir.dotProduct(Vector3::UNIT_Y)) > 0.99f) ? Vector3::UNIT_Z : Vector3::UNIT_Y;
    const Matrix3 rot = Math::lookRotation(-dir, up);

    // Snap the centre to whole shadow texels in light space, so texels don't crawl over the ground as the camera moves
    const auto& configs = sm->getShadowTextureConfigList();
    const Real texSize = (iteration < configs.size()) ? (Real)configs[iteration].width : 1024.f;
    const Real texel = (2.f * radius) / texSize;
    Vector3 ls = rot.transpose() * center;
    ls.x = Math::Floor(ls.x / texel) * texel;
    ls.y = Math::Floor(ls.y / texel) * texel;
    center = rot * ls;

    // Pull the camera back towards the light so casters outside the slice (hills, tall buildings) still cast into it
    const Real backoff = radius + 500.f;

    texCam->setCustomViewMatrix(false);
    texCam->setCustomProjectionMatrix(false);
    texCam->setProjectionType(PT_ORTHOGRAPHIC);
    texCam->setOrthoWindow(2.f * radius, 2.f * radius);
    texCam->setNearClipDistance(1.f);
    texCam->setFarClipDistance(backoff + radius);
    texCam->getParentNode()->setPosition(center - dir * backoff);
    texCam->getParentNode()->setOrientation(rot);

    // OGRE finds the casters with a separate culling camera that shares texCam's node. DefaultShadowCameraSetup
    // gives it a box of twice the light's shadow far distance around the camera - the same for every cascade,
    // so each cascade drew every caster in range. Give it this cascade's own projection instead.
    if (Camera* cullCam = dynamic_cast<Camera*>(texCam->getCullingFrustum()))
    {
        cullCam->setCustomViewMatrix(false);
        cullCam->setCustomProjectionMatrix(false);
        cullCam->setProjectionType(PT_ORTHOGRAPHIC);
        cullCam->setOrthoWindow(2.f * radius, 2.f * radius);
        cullCam->setNearClipDistance(1.f);
        cullCam->setFarClipDistance(backoff + radius);
    }
}
