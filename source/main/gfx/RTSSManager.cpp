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
#include "RTSSPaged.h"

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

#include <map>
#include <regex>
#include <unordered_set>

using namespace Ogre;
using namespace RoR;

namespace {

const char* CG_UNRUNNABLE_SCHEME = "RoR/OldCgDead"; //!< Scheme of the dead techniques of rescued materials

/// Applies the material rules (Cg rescue) to all materials of a group, or all materials if empty
void ApplyMaterialRules(const Ogre::String& groupName, const std::string& what)
{
    int rescued = 0;
    auto it = Ogre::MaterialManager::getSingleton().getResourceIterator();
    while (it.hasMoreElements())
    {
        Ogre::MaterialPtr mat = Ogre::static_pointer_cast<Ogre::Material>(it.getNext());
        if (!groupName.empty() && mat->getGroup() != groupName)
            continue;
        if (RTSSManager::RescueMaterial(*mat))
            rescued++;
    }
    if (rescued > 0)
        LOG(fmt::format("[RoR|OldCg] {} materials in {} have no runnable shaders, using fallbacks", rescued, what));
}

//TODO: Maybe the cg rescue to its own CgRescue.h/cpp

/// Resource group listener: applies the material rules to every material of a newly parsed group
class MaterialRulesListener: public Ogre::ResourceGroupListener
{
public:
    void resourceGroupScriptingEnded(const Ogre::String& groupName) override
    {
        ApplyMaterialRules(groupName, fmt::format("group '{}'", groupName));
    }
};

/// Whether any pass of the technique has a shader that can't run (unsupported language, e.g. Cg without plugin)
bool HasUnrunnableShaders(const Ogre::Technique* tech)
{
    const Ogre::GpuProgramType types[] = { Ogre::GPT_VERTEX_PROGRAM, Ogre::GPT_FRAGMENT_PROGRAM,
        Ogre::GPT_GEOMETRY_PROGRAM, Ogre::GPT_DOMAIN_PROGRAM, Ogre::GPT_HULL_PROGRAM };
    for (const Ogre::Pass* pass : tech->getPasses())
    {
        for (Ogre::GpuProgramType type : types)
        {
            if (!pass->hasGpuProgram(type))
                continue;
            // A program only knows it can't compile (e.g. Cg on Mesa, `vertex.weight` in arbvp1) once it was loaded
            const Ogre::GpuProgramPtr& prog = pass->getGpuProgram(type);
            if (!prog->isLoaded())
            {
                try { prog->load(); }
                catch (Ogre::Exception&) {}
            }
            // Cg compiles to an assembly program (the binding delegate); that one can fail while the Cg program is fine
            Ogre::GpuProgram* bound = prog->_getBindingDelegate();
            if (!prog->isSupported() || !bound || !bound->isSupported())
                return true;
        }
    }
    return false;
}

/// Picks the texture unit most likely to hold the color (diffuse) texture, searching all passes of the technique:
/// * the texture named in the material name by the Blender exporter (.../SOLID/TEX/<texture>);
/// * the first 2D texture whose name doesn't suggest data (specular, normal, environment, damage, masks...),
///    skipping textures used by more than one unit (placeholders, e.g. ET coverage maps);
/// * the first such unique 2D texture, then the first 2D texture at all.
Ogre::TextureUnitState* FindDiffuseUnit(const Ogre::Material& mat, Ogre::Technique* tech)
{
    std::vector<Ogre::TextureUnitState*> units;
    std::map<std::string, int> uses;
    for (Ogre::Pass* pass : tech->getPasses())
    {
        for (Ogre::TextureUnitState* tus : pass->getTextureUnitStates())
        {
            if (tus->getTextureType() != Ogre::TEX_TYPE_2D || tus->getTextureName().empty())
                continue;
            units.push_back(tus);
            uses[tus->getTextureName()]++;
        }
    }
    if (units.empty())
        return nullptr;

    const size_t tex_pos = mat.getName().rfind("/TEX/");
    if (tex_pos != std::string::npos)
    {
        const std::string tex_name = mat.getName().substr(tex_pos + 5);
        for (Ogre::TextureUnitState* tus : units)
        {
            if (Ogre::StringUtil::match(tus->getTextureName(), tex_name, /*caseSensitive=*/false))
                return tus;
        }
    }

    static const std::regex data_hint("spec|[-_]s\\.|normal|nrm|[-_]n\\.|_nm|bump|env|cube|refl|dmg|damage|mask|_ao|occl|noise",
        std::regex::icase);

    // The texture unit's own name, e.g. 'diffusetex' or 'Diffuse_Map' (but not 'diffusedmgtex')
    static const std::regex diffuse_name("diffuse|albedo|basecolou?r", std::regex::icase);
    for (Ogre::TextureUnitState* tus : units)
    {
        if (std::regex_search(tus->getName(), diffuse_name) && !std::regex_search(tus->getName(), data_hint))
            return tus;
    }

    // A texture used more than twice is a placeholder (e.g. ET coverage maps); twice is common (diffuse + damage)
    Ogre::TextureUnitState* first_unique = nullptr;
    for (Ogre::TextureUnitState* tus : units)
    {
        if (uses[tus->getTextureName()] > 2)
            continue;
        if (!first_unique)
            first_unique = tus;
        if (!std::regex_search(tus->getTextureName(), data_hint))
            return tus;
    }
    return first_unique ? first_unique : units.front();
}

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
    // clone() also copies RTSS techniques, but RTSS only removes them again if the source still carries its pass
    // tag - not when the source was itself a plain clone (e.g. PagedGeometry's '_Batched'). Such a stale technique
    // never gets its uniforms (lights, alpha test function 0 = always fail: invisible alpha-tested leaves).
    for (int i = (int)dst->getNumTechniques() - 1; i >= 0; i--)
    {
        if (dst->getTechnique(i)->getSchemeName() == RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME)
            dst->removeTechnique(i);
    }
    RTShader::ShaderGenerator::getSingleton().cloneShaderBasedTechniques(*src, *dst);
    return dst;
}

void RTSSManager::InstallMaterialRules()
{
    static MaterialRulesListener listener;
    Ogre::ResourceGroupManager::getSingleton().addResourceGroupListener(&listener);

    // Groups parsed before this point (RoR's own resource packs)
    ApplyMaterialRules("", "RoR's resource packs");
}

bool RTSSManager::RescueMaterial(Ogre::Material& mat)
{
    Ogre::Technique* src = nullptr;
    for (Ogre::Technique* tech : mat.getTechniques())
    {
        if (tech->getSchemeName() != Ogre::MaterialManager::DEFAULT_SCHEME_NAME || tech->getNumPasses() == 0)
            continue;
        if (!HasUnrunnableShaders(tech))
            return false; // Something can run, nothing to rescue
        if (!src)
            src = tech;
    }
    if (!src)
        return false;

    // The color texture may be in any pass (e.g. ET materials: pass 1 Ambient has none, it's in Splatting)
    Ogre::TextureUnitState* diffuse_src = FindDiffuseUnit(mat, src);

    // Copy of the first technique, without shaders. Extra passes (specular/reflection passes written for the
    // shaders) would only add odd layers without them - keep the first.
    Ogre::Technique* tech = mat.createTechnique();
    *tech = *src;
    tech->setName("RoR/OldCg");

    // OGRE only checks the high level program, so a Cg technique whose compiled assembly fails still counts as
    // supported and would be drawn (as nothing). Move the dead techniques out of the default scheme and kept for
    // reading their inputs later and drop RTSS techniques generated from them; the resolver shades the rescue.
    for (Ogre::Technique* dead : mat.getTechniques())
    {
        if (dead != tech && dead->getSchemeName() == Ogre::MaterialManager::DEFAULT_SCHEME_NAME)
            dead->setSchemeName(CG_UNRUNNABLE_SCHEME);
    }
    RTShader::ShaderGenerator::getSingleton().removeAllShaderBasedTechniques(mat);
    while (tech->getNumPasses() > 1)
        tech->removePass(1);
    Ogre::Pass* pass = tech->getPass(0);
    const Ogre::GpuProgramType types[] = { Ogre::GPT_VERTEX_PROGRAM, Ogre::GPT_FRAGMENT_PROGRAM,
        Ogre::GPT_GEOMETRY_PROGRAM, Ogre::GPT_DOMAIN_PROGRAM, Ogre::GPT_HULL_PROGRAM };
    for (Ogre::GpuProgramType type : types)
    {
        if (pass->hasGpuProgram(type))
            pass->setGpuProgram(type, Ogre::GpuProgramPtr());
    }

    // Keep only the color texture; the others (specular, normal, environment...) were inputs for the shaders
    pass->removeAllTextureUnitStates();
    if (diffuse_src)
    {
        Ogre::TextureUnitState* diffuse = OGRE_NEW Ogre::TextureUnitState(pass, *diffuse_src);
        pass->addTextureUnitState(diffuse);
        diffuse->setColourOperation(Ogre::LBO_MODULATE);
        diffuse->setTextureCoordSet(0);
    }

    LOG(fmt::format("[RoR|OldCg] Material '{}' (group '{}'): no runnable shaders, using its texture '{}'",
        mat.getName(), mat.getGroup(), diffuse_src ? diffuse_src->getTextureName() : "(none)"));
    return true;
}

void RTSSManager::RefreshMaterial(Ogre::Material& mat)
{
    auto& shader_gen = RTShader::ShaderGenerator::getSingleton();
    shader_gen.invalidateMaterial(RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME, mat);
    shader_gen.validateMaterial(RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME, mat);
}

void RTSSManager::LimitLightCount()
{
    // RTSS sizes its light arrays from the number of lights in the frustum, which is unbounded so one vehicle's
    // flares alone push it past 70. OGRE never binds more than OGRE_MAX_SIMULTANEOUS_LIGHTS to a pass, so
    // everything above that is dead weight and on D3D9 the oversized arrays overrun the ps_3_0 constant register
    // budget and the shader fails to assemble; elsewhere it costs shader time and causes shader rebuilds
    // whenever the number of visible lights changes.
    RTShader::RenderState* scheme_render_state = RTShader::ShaderGenerator::getSingleton().getRenderState(MSN_SHADERGEN);
    scheme_render_state->setLightCountAutoUpdate(false);
    scheme_render_state->setLightCount(OGRE_MAX_SIMULTANEOUS_LIGHTS);
}

void RTSSManager::InstallSubRenderStates()
{
    // RTSS keeps the factory for the rest of the process, and RoR never shuts
    // RTSS down. A static object would be destroyed at exit while materials still hold its
    // sub render states since OGRE asserts on that and the game aborts.
    RTShader::ShaderGenerator::getSingleton().addSubRenderStateFactory(new PagedFadeSubRenderStateFactory());
    RTShader::ShaderGenerator::getSingleton().addSubRenderStateFactory(new GrassSubRenderStateFactory());
    RTShader::ShaderGenerator::getSingleton().addSubRenderStateFactory(new SpriteSubRenderStateFactory());
}

bool RTSSManager::AddSubRenderState(const Ogre::MaterialPtr& mat, unsigned short pass_index, RTShader::SubRenderState* srs)
{
    RTShader::ShaderGenerator& shadergen = RTShader::ShaderGenerator::getSingleton();
    // RTSS only considers supported techniques such as compile() is what finds them.
    if (mat->getSupportedTechniques().empty())
    {
        mat->compile();
    }
    shadergen.createShaderBasedTechnique(*mat, MaterialManager::DEFAULT_SCHEME_NAME, MSN_SHADERGEN);
    RTShader::RenderState* render_state = shadergen.getRenderState(MSN_SHADERGEN, *mat, pass_index);
    if (!render_state)
    {
        LOG(fmt::format("[RoR|RTSS] No shader render state for material '{}' pass {}, can't add '{}'",
            mat->getName(), pass_index, srs->getType()));
        shadergen.destroySubRenderState(srs);
        return false;
    }
    render_state->addTemplateSubRenderState(srs);
    shadergen.invalidateMaterial(MSN_SHADERGEN, *mat);
    return true;
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


    // The way I understood this is that the shadow cameras only render depth, which receivers
    // compare against their own depth. With color textures the caster material writes a color,
    // which is meaningless for the receivers' comparison, so shadows pop in and out with the camera and casters
    // can't shadow themselves. PF_DEPTH16 is the only depth render target our GL render system accepts
    // OGRE silently replaces other depth formats with a color format. So we force PF_DEPTH16 here.
    const Ogre::PixelFormat SHADOW_FORMAT = PF_DEPTH16; // 16 bit depth is enough for shadows
    // might be worth investigating PF_DEPTH24 later, but our current older render systems may not support it.

    if (App::gfx_shadow_quality->getInt() == 3) // "Ultra" 
    {
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(0, 4096, 4096, SHADOW_FORMAT);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(1, 3072, 3072, SHADOW_FORMAT);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(2, 2048, 2048, SHADOW_FORMAT);
    }
    else if (App::gfx_shadow_quality->getInt() == 2) // "High"
    {
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(0, 3072, 3072, SHADOW_FORMAT);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(1, 2048, 2048, SHADOW_FORMAT);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(2, 2048, 2048, SHADOW_FORMAT);
    }
    else if (App::gfx_shadow_quality->getInt() == 1) // "Medium"
    {
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(0, 2048, 2048, SHADOW_FORMAT);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(1, 1024, 1024, SHADOW_FORMAT);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(2, 1024, 1024, SHADOW_FORMAT);
    }
    else // "Low"
    {
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(0, 1024, 1024, SHADOW_FORMAT);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(1, 1024, 1024, SHADOW_FORMAT);
        App::GetGfxScene()->GetSceneManager()->setShadowTextureConfig(2, 512, 512, SHADOW_FORMAT);
    }

    // shadow camera setup
    pssmSetup = new StableCSMShadowCameraSetup(); // our own PSSM with stable cascades, see RTSSManager.h
    pssmSetup->calculateSplitPoints(NUM_SPLITS, App::GetCameraManager()->GetCamera()->getNearClipDistance(), App::GetGfxScene()->GetSceneManager()->getShadowFarDistance());
    pssmSetup->setSplitPadding(App::GetCameraManager()->GetCamera()->getNearClipDistance() * 2);
    App::GetGfxScene()->GetSceneManager()->setShadowCameraSetup(Ogre::ShadowCameraSetupPtr(pssmSetup));

    // Log what OGRE actually created. It will fall back to other formats silently.
    for (size_t i = 0; i < NUM_SHADOW_TEXTURES; i++)
    {
        const Ogre::TexturePtr& tex = App::GetGfxScene()->GetSceneManager()->getShadowTexture(i);
        LOG(fmt::format("[RoR|RTSS] Shadow texture {}: {}x{} {}", i, tex->getWidth(), tex->getHeight(),
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
    // Taken from Ogre::PSSMShadowCameraSetup::getShadowCamera() and modified to snap the shadow camera to whole texels
    if (light->getType() != Light::LT_DIRECTIONAL)
    {
        DefaultShadowCameraSetup::getShadowCamera(sm, cam, vp, light, texCam, iteration);
        return;
    }

    // This cascade's slice of the view frustum (with padding, so nearby cascades overlap a little)
    const SplitPointList& splits = this->getSplitPoints();
    Real nearDist = splits[iteration];
    Real farDist = splits[iteration + 1];
    if (iteration > 0)
        nearDist -= this->getSplitPadding();
    if (iteration < this->getSplitCount() - 1)
        farDist += this->getSplitPadding();

    // Bounding sphere of the slice. Its radius only depends on FOV/aspect/split distances, so the shadow map
    // always covers the same world area with no resizing when the camera turns
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

    // Light space basis; the shadow camera looks along the light direction
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
    // gives it a box of twice the light's shadow far distance around the camera wih is the same for every cascade,
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
