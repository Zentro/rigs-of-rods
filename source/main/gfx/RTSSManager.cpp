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
#include "GfxPbrMaps.h"
#include "GfxScene.h"
#include "RTSSNiceMetal.h"
#include "RTSSPaged.h"

#include <Ogre.h>
#include <Terrain/OgreTerrain.h>
#include <Overlay/OgreOverlayManager.h>
#include <Overlay/OgreOverlayContainer.h>
#include <Overlay/OgreOverlay.h>
#include <OgreMaterialManager.h>
#include <Bites/OgreSGTechniqueResolverListener.h>
#include <RTShaderSystem/OgreRTShaderSystem.h>

#include <regex>
#include <unordered_set>

using namespace Ogre;
using namespace RoR;

namespace {

const char* NO_SHADOW_CASTER_MAT = "RoR/NoShadowCaster";
const char* SHADOW_CASTER_MAT = "RoR/ShadowCaster";
const char* ALPHA_CASTER_SUFFIX = "/RoRShadowCaster"; //!< Casters made for alpha-tested materials, see SetupShadowCaster()

/// Shadow caster that draws nothing (writes neither colour nor depth)
Ogre::MaterialPtr GetNoShadowCaster()
{
    Ogre::MaterialPtr mat = Ogre::MaterialManager::getSingleton().getByName(NO_SHADOW_CASTER_MAT, Ogre::RGN_DEFAULT);
    if (!mat)
    {
        mat = Ogre::MaterialManager::getSingleton().create(NO_SHADOW_CASTER_MAT, Ogre::RGN_DEFAULT);
        mat->setReceiveShadows(false);
        Ogre::Pass* pass = mat->getTechnique(0)->getPass(0);
        pass->setLightingEnabled(false);
        pass->setColourWriteEnabled(false);
        pass->setDepthWriteEnabled(false);
    }
    return mat;
}

/// Shadow caster materials themselves - the caster rules don't apply to them. ('/ShadowCaster' = GrassLoader's)
bool IsShadowCasterMaterial(const Ogre::Material* mat)
{
    const std::string& name = mat->getName();
    return name == SHADOW_CASTER_MAT || name == NO_SHADOW_CASTER_MAT
        || Ogre::StringUtil::endsWith(name, ALPHA_CASTER_SUFFIX, false)
        || Ogre::StringUtil::endsWith(name, "/ShadowCaster", false);
}

/// Shared caster for everything without an own caster: depth only. Its shaders are generated right away, from the
/// plain pass: OGRE copies the textures and alpha test of alpha-tested objects that have no own caster into this
/// pass per object (TextureShadowRenderer::deriveShadowCasterPass), which a generated shader never sees - and
/// GL3+/D3D11 have no fixed function to draw it with. SetupShadowCaster() gives those objects their own caster.
Ogre::MaterialPtr GetSharedShadowCaster()
{
    Ogre::MaterialPtr mat = Ogre::MaterialManager::getSingleton().getByName(SHADOW_CASTER_MAT, Ogre::RGN_DEFAULT);
    if (!mat)
    {
        mat = Ogre::MaterialManager::getSingleton().create(SHADOW_CASTER_MAT, Ogre::RGN_DEFAULT);
        mat->setReceiveShadows(false);
        mat->getTechnique(0)->getPass(0)->setLightingEnabled(false);
        mat->compile();
        RTShader::ShaderGenerator& shadergen = RTShader::ShaderGenerator::getSingleton();
        shadergen.createShaderBasedTechnique(*mat, MaterialManager::DEFAULT_SCHEME_NAME, RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);
        shadergen.validateMaterial(RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME, *mat);
    }
    return mat;
}

/// Own caster for an alpha-tested material: depth only, cut out by the alpha of the material's first texture
/// (leaves, fences, grilles). Shaded by RTSS like any other material. Kept in step with the material's texture:
/// vehicle materials are copies of a template and get their real textures only after copying.
Ogre::MaterialPtr GetAlphaTestedCaster(const Ogre::Material& mat, const Ogre::Pass& src)
{
    const std::string name = mat.getName() + ALPHA_CASTER_SUFFIX;
    const Ogre::TextureUnitState* src_tus = (src.getNumTextureUnitStates() > 0) ? src.getTextureUnitState(0) : nullptr;

    Ogre::MaterialPtr caster = Ogre::MaterialManager::getSingleton().getByName(name, mat.getGroup());
    if (caster)
    {
        Ogre::Pass* pass = caster->getTechnique(0)->getPass(0);
        const Ogre::TextureUnitState* tus = (pass->getNumTextureUnitStates() > 0) ? pass->getTextureUnitState(0) : nullptr;
        const std::string cur_tex = tus ? tus->getTextureName() : "";
        const std::string src_tex = src_tus ? src_tus->getTextureName() : "";
        if (cur_tex != src_tex)
        {
            pass->removeAllTextureUnitStates();
            if (src_tus)
                *pass->createTextureUnitState() = *src_tus;
            RTSSManager::RefreshMaterial(*caster);
        }
        return caster;
    }

    caster = Ogre::MaterialManager::getSingleton().create(name, mat.getGroup());
    caster->setReceiveShadows(false);
    Ogre::Pass* pass = caster->getTechnique(0)->getPass(0);
    pass->setLightingEnabled(false);
    pass->setAlphaRejectSettings(src.getAlphaRejectFunction(), src.getAlphaRejectValue());
    pass->setCullingMode(src.getCullingMode());
    pass->setManualCullingMode(src.getManualCullingMode());
    if (src_tus)
        *pass->createTextureUnitState() = *src_tus;
    return caster;
}

/// Applies the material rules (Cg rescue, shadow casters) to all materials of a group, or all materials if empty
bool g_pbr_lighting = false; //!< RTSSManager::EnablePbrLighting() was called

void ApplyMaterialRules(const Ogre::String& groupName, const std::string& what)
{
    int rescued = 0, no_caster = 0, translated = 0;
    auto it = Ogre::MaterialManager::getSingleton().getResourceIterator();
    while (it.hasMoreElements())
    {
        Ogre::MaterialPtr mat = Ogre::static_pointer_cast<Ogre::Material>(it.getNext());
        if (!groupName.empty() && mat->getGroup() != groupName)
            continue;
        if (RTSSManager::RescueMaterial(*mat))
            rescued++;
        if (RTSSManager::SetupShadowCaster(*mat))
            no_caster++;
        if (g_pbr_lighting && RTSSManager::TranslateLegacyLighting(*mat)) // After the rescue: its fallback too
            translated++;
    }
    if (translated > 0)
        LOG(fmt::format("[RoR|PBR] {} materials in {} translated for physically based lighting", translated, what));
    if (rescued > 0)
        LOG(fmt::format("[RoR|Cg rescue] {} materials in {} have no runnable shaders, using fallbacks", rescued, what));
    if (no_caster > 0)
        LOG(fmt::format("[RoR|Shadows] {} invisible/see-through materials in {} cast no shadow", no_caster, what));
}

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
            if (pass->hasGpuProgram(type) && !pass->getGpuProgram(type)->isSupported())
                return true;
        }
    }
    return false;
}

/// Picks the texture unit most likely to hold the colour (diffuse) texture: the first 2D texture whose name
/// doesn't suggest data (specular, normal, environment, damage, masks...); otherwise the first 2D texture.
Ogre::TextureUnitState* FindDiffuseUnit(Ogre::Pass* pass)
{
    static const std::regex data_hint("spec|_s\\.|normal|_n\\.|_nm|bump|env|cube|refl|dmg|damage|mask|_ao|occl|noise",
        std::regex::icase);
    Ogre::TextureUnitState* first_2d = nullptr;
    for (Ogre::TextureUnitState* tus : pass->getTextureUnitStates())
    {
        if (tus->getTextureType() != Ogre::TEX_TYPE_2D || tus->getTextureName().empty())
            continue;
        if (!first_2d)
            first_2d = tus;
        if (!std::regex_search(tus->getTextureName(), data_hint))
            return tus;
    }
    return first_2d;
}

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

        // HDR pipeline: textures assigned after loading (skins, props, materials made in code) - idempotent
        App::GetGfxScene()->GetPipeline().LinearizeTextures(mat);

        if (HasVertexColours(rend))
            UseVertexColourIfUnlit(mat);

        // Materials created in code after loading (RTSS copies the caster setting into its technique)
        RTSSManager::SetupShadowCaster(*mat);

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
            if (HasUnrunnableShaders(tech))
                continue; // e.g. Cg: the material's Cg rescue technique is the one drawn, and RTSS shades it
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

void RTSSManager::LimitLightCount()
{
    // RTSS sizes its light arrays from the number of lights in the frustum, which is unbounded - one vehicle's
    // flares alone push it past 70. OGRE never binds more than OGRE_MAX_SIMULTANEOUS_LIGHTS to a pass, so
    // everything above that is dead weight: on D3D9 the oversized arrays overrun the ps_3_0 constant register
    // budget and the shader fails to assemble; elsewhere it costs shader time and causes shader rebuilds
    // whenever the number of visible lights changes.
    RTShader::RenderState* scheme_render_state = RTShader::ShaderGenerator::getSingleton().getRenderState(MSN_SHADERGEN);
    scheme_render_state->setLightCountAutoUpdate(false);
    scheme_render_state->setLightCount(OGRE_MAX_SIMULTANEOUS_LIGHTS);
}

void RTSSManager::InstallSubRenderStates()
{
    // Deliberately never deleted: RTSS keeps the factory for the rest of the process, and RoR never shuts
    // RTSS down. A `static` object would be destroyed at exit while vehicle materials still hold its
    // sub render states - OGRE asserts on that ("Sub render states still exists") and the game aborts.
    RTShader::ShaderGenerator::getSingleton().addSubRenderStateFactory(new NiceMetalSubRenderStateFactory());
    RTShader::ShaderGenerator::getSingleton().addSubRenderStateFactory(new PagedFadeSubRenderStateFactory());
    RTShader::ShaderGenerator::getSingleton().addSubRenderStateFactory(new GrassSubRenderStateFactory());
}

bool RTSSManager::AddSubRenderState(const Ogre::MaterialPtr& mat, unsigned short pass_index, RTShader::SubRenderState* srs)
{
    // The technique resolver won't see this material (it gets its RTSS technique here)
    RTSSManager::SetupShadowCaster(*mat);

    RTShader::ShaderGenerator& shadergen = RTShader::ShaderGenerator::getSingleton();
    // RTSS only considers supported techniques - compile() is what finds them.
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

static const float PBR_DEFAULT_ROUGHNESS = 0.7f; //!< Until materials get their own values (ROADMAP M4.4)
static const float PBR_GLOSSY_ROUGHNESS = 0.2f;  //!< Where a legacy specular map is white (full reflection)
static const char* const PBR_METAL_KEY = "RoR/PbrMetal"; //!< Pass user any: material class is a metal (ROADMAP M4.5)

namespace {

/// Physically based values a material's names suggest (ROADMAP M4.4); -1 = not set by the class
struct PbrMaterialClass
{
    float roughness = -1.f;
    float roughness_max = -1.f;    //!< Caps the roughness instead of setting it
    float gloss = -1.f;            //!< Gloss layer strength (M4.6)
    float gloss_roughness = -1.f;
    bool  metal = false;           //!< Only applied once image based lighting exists (M4.5)
};

bool NameHas(std::string const& lowercase_names, std::initializer_list<const char*> hints)
{
    for (const char* hint : hints)
    {
        if (lowercase_names.find(hint) != std::string::npos)
            return true;
    }
    return false;
}

/// Classifies by material/texture names, then by the names of the material's (dead, Cg) shader programs.
/// Order matters: first match wins ("bodyglass" is glass, "exhaust_chrome" is chrome). Deliberately not matched:
/// "metal" (also "metallicblue" paint), "rim" (also "trim", "primer"). Ported from the gfx-rtss branch.
PbrMaterialClass ClassifyMaterial(std::string names, std::string program_names)
{
    Ogre::StringUtil::toLowerCase(names);
    Ogre::StringUtil::toLowerCase(program_names);
    PbrMaterialClass c;
    if (NameHas(names, {"glass", "window", "windshield", "windscreen"}))
    {
        c.roughness = 0.05f; c.gloss = 1.f; c.gloss_roughness = 0.03f;
    }
    else if (NameHas(names, {"chrome", "chrom"}))
    {
        c.roughness = 0.12f; c.metal = true;
    }
    else if (NameHas(names, {"exhaust", "muffler", "aluminium", "aluminum", "steel"}))
    {
        c.roughness = 0.4f; c.metal = true;
    }
    else if (NameHas(names, {"tire", "tyre", "rubber", "wheelband", "tread"}))
    {
        c.roughness = 0.9f; c.gloss = 0.f;
    }
    else if (NameHas(names, {"leather"}))
    {
        c.roughness = 0.6f;
    }
    else if (NameHas(names, {"seat", "cloth", "fabric", "carpet", "upholster"}))
    {
        c.roughness = 0.9f; c.gloss = 0.f;
    }
    else if (NameHas(names, {"lens"}))
    {
        c.roughness = 0.15f; c.gloss = 0.7f; c.gloss_roughness = 0.05f;
    }
    else if (NameHas(names, {"paint", "carbody", "body"}))
    {
        c.roughness_max = 0.45f; c.gloss = 0.8f; c.gloss_roughness = 0.05f;
    }
    // Shader names of well known community material families (Cg-rescued materials)
    else if (NameHas(program_names, {"chrome", "metal_ps", "simplemetal"}))
    {
        c.roughness = 0.15f; c.metal = true;
    }
    else if (NameHas(program_names, {"paint", "nicemetal", "carpaint"}))
    {
        c.roughness_max = 0.45f; c.gloss = 0.8f; c.gloss_roughness = 0.05f;
    }
    return c;
}

/// Names of the shader programs in the material's techniques (only Cg-rescued materials have any left)
std::string GetProgramNames(const Ogre::Material& mat)
{
    std::string names;
    for (const Ogre::Technique* tech : mat.getTechniques())
    {
        for (const Ogre::Pass* pass : tech->getPasses())
        {
            if (pass->hasVertexProgram())   names += pass->getVertexProgramName() + " ";
            if (pass->hasFragmentProgram()) names += pass->getFragmentProgramName() + " ";
        }
    }
    return names;
}

/// Material name + name of the first texture (usually the diffuse map)
std::string GetClassNames(const Ogre::Material& mat, const Ogre::Pass* pass)
{
    std::string names = mat.getName();
    if (pass->getNumTextureUnitStates() > 0)
        names += " " + pass->getTextureUnitState(0)->getTextureName();
    return names;
}

/// Writes the class into the pass: roughness/metallic (x/y, read by Cook-Torrance) and the gloss layer (z/w, M4.6)
void ApplyMaterialClass(Ogre::Pass* pass, PbrMaterialClass const& c, float roughness)
{
    if (c.roughness >= 0.f)
        roughness = c.roughness;
    if (c.roughness_max >= 0.f)
        roughness = std::min(roughness, c.roughness_max);
    const float gloss = std::max(c.gloss, 0.f);
    const float gloss_roughness = (c.gloss_roughness >= 0.f) ? c.gloss_roughness : 0.05f;
    pass->setSpecular(Ogre::ColourValue(roughness, /*metallic, M4.5:*/0.f, gloss, gloss_roughness));
    if (c.metal)
        pass->getUserObjectBindings().setUserAny(PBR_METAL_KEY, Ogre::Any(true));
}

} // namespace

void RTSSManager::ApplyActorShading(const Ogre::MaterialPtr& mat, bool transparent, bool is_flexbody)
{
    // Textures are final now. (The material is a copy, and may get its RTSS technique below - then the technique
    // resolver, which sets up casters too, never sees it.)
    RTSSManager::SetupShadowCaster(*mat);

    Pass* pass = mat->getTechnique("BaseTechnique")->getPass("BaseRender");

    // HDR pipeline: physically based lighting (Cook-Torrance, ROADMAP M4). Roughness and metalness go in the pass
    // specular colour (x, y), where RTSS reads them - set before RTSS copies the pass into its technique below.
    // No per-material values yet: dielectric, medium-rough (material classes, specular maps: M4.3, M4.4).
    const bool pbr = App::GetGfxScene()->GetPipeline().IsActive();
    if (pbr)
    {
        PbrMaterialClass mat_class = ClassifyMaterial(GetClassNames(*mat, pass), "");
        // Body panels are usually named after the vehicle ('gavril1024') - but only they have damage textures
        const bool unclassified = mat_class.roughness < 0.f && mat_class.roughness_max < 0.f && !mat_class.metal;
        if (unclassified && pass->getTextureUnitState("Dmg_Diffuse_Map"))
        {
            mat_class = ClassifyMaterial("paint", "");
        }
        ApplyMaterialClass(pass, mat_class, PBR_DEFAULT_ROUGHNESS);
    }

    // A plain diffuse-only material needs no vehicle shading - the regular RTSS shading does the right thing.
    const bool has_specular = pass->getTextureUnitState("Specular_Map") != nullptr;
    const bool nicemetal = has_specular || pass->getTextureUnitState("Dmg_Diffuse_Map");
    if (!nicemetal && !pbr)
    {
        return;
    }

    // The per-pass render state exists only once the material has an RTSS technique. The technique resolver
    // would only create it at the first draw, so create it now. (Idempotent.)
    RTShader::ShaderGenerator& shadergen = RTShader::ShaderGenerator::getSingleton();
    shadergen.createShaderBasedTechnique(*mat, MaterialManager::DEFAULT_SCHEME_NAME, MSN_SHADERGEN);
    RTShader::RenderState* render_state = shadergen.getRenderState(MSN_SHADERGEN, *mat, 0);
    if (!render_state)
    {
        LOG(fmt::format("[RoR|RTSS] No shader render state for material '{}', leaving it without vehicle shading", mat->getName()));
        return;
    }

    if (nicemetal)
    {
        // The sub render state samples these itself, so keep FFPTexturing off them. This is also what keeps the
        // vertex colour out of the lighting stage: it carries damage and wetness flags rather than a colour.
        for (TextureUnitState* tus : pass->getTextureUnitStates())
        {
            RTShader::ShaderGenerator::_markNonFFP(tus);
        }

        RTShader::SubRenderState* srs = shadergen.createSubRenderState(NiceMetalSubRenderState::Type);
        srs->setParameter("transparent", transparent ? "true" : "false");
        srs->setParameter("is_flexbody", is_flexbody ? "true" : "false");
        // HDR: only the damage/wetness surface colour, Cook-Torrance does the lighting. The legacy specular-map
        // reflection comes back physically based (M4.3 specular maps, M5 image based lighting).
        srs->setParameter("surface_only", pbr ? "true" : "false");
        render_state->addTemplateSubRenderState(srs);
    }

    if (pbr)
    {
        // Same execution order as the fixed-function lighting stage, so it replaces it
        RTShader::SubRenderState* lighting = shadergen.createSubRenderState(RTShader::SRS_COOK_TORRANCE_LIGHTING);

        // Legacy specular map (a reflectivity mask) -> per-texel roughness, baked once per map (M4.3)
        if (TextureUnitState* spec_tus = pass->getTextureUnitState("Specular_Map"))
        {
            TexturePtr spec_tex = spec_tus->_getTexturePtr();
            // The material class's roughness is the base (where the map is black); the map only makes it smoother
            const float base_roughness = pass->getSpecular().r;
            const std::string mr_name = GfxPbrMaps::BakeSpecularMap(spec_tex, base_roughness,
                std::min(PBR_GLOSSY_ROUGHNESS, base_roughness));
            if (!mr_name.empty())
            {
                lighting->setParameter("texture", mr_name);
            }
        }
        render_state->addTemplateSubRenderState(lighting);
    }

    shadergen.invalidateMaterial(MSN_SHADERGEN, *mat);
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

bool RTSSManager::SetupShadowCaster(Ogre::Material& mat)
{
    if (IsShadowCasterMaterial(&mat))
        return false;

    bool changed = false;
    for (Ogre::Technique* tech : mat.getTechniques())
    {
        if (tech->getSchemeName() != Ogre::MaterialManager::DEFAULT_SCHEME_NAME
            || tech->getNumPasses() == 0)
            continue;

        // Keep the author's `shadow_caster_material` (and the no-shadow caster set earlier). Ours are redone:
        // a copied material (vehicles) carries the caster of the material it was copied from.
        const Ogre::MaterialPtr& cur_caster = tech->getShadowCasterMaterial();
        if (cur_caster && !Ogre::StringUtil::endsWith(cur_caster->getName(), ALPHA_CASTER_SUFFIX, false))
            continue;

        // Invisible: helper geometry (e.g. 'tracks/trans' cab shells). OGRE's caster ignores colour write.
        bool invisible = true;
        for (const Ogre::Pass* pass : tech->getPasses())
            invisible = invisible && !pass->getColourWriteEnabled();

        // See-through: blended without depth write (headlight beam cones, glow planes). Nothing is cut away by
        // an alpha test, so OGRE's caster would draw their whole shape.
        const Ogre::Pass* first = tech->getPass(0);
        const bool see_through = first->isTransparent() && !first->getDepthWriteEnabled()
            && first->getAlphaRejectFunction() == Ogre::CMPF_ALWAYS_PASS;

        if (invisible || see_through)
        {
            tech->setShadowCasterMaterial(GetNoShadowCaster());
            changed = true;
        }
        else if (first->getAlphaRejectFunction() != Ogre::CMPF_ALWAYS_PASS)
        {
            tech->setShadowCasterMaterial(GetAlphaTestedCaster(mat, *first));
        }
    }
    return changed;
}

void RTSSManager::InstallMaterialRules()
{
    static MaterialRulesListener listener;
    Ogre::ResourceGroupManager::getSingleton().addResourceGroupListener(&listener);

    // Groups parsed before this point (RoR's own resource packs)
    ApplyMaterialRules("", "RoR's resource packs");
}

void RTSSManager::EnablePbrLighting()
{
    g_pbr_lighting = true;

    // Cook-Torrance replaces the fixed-function lighting stage (same execution order) for everything RTSS shades
    // through the scheme. Unlit passes stay unlit (the stage skips them). Terrain has its own generator (M4.8).
    RTShader::ShaderGenerator& shadergen = RTShader::ShaderGenerator::getSingleton();
    shadergen.getRenderState(RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME)
        ->addTemplateSubRenderState(shadergen.createSubRenderState(RTShader::SRS_COOK_TORRANCE_LIGHTING));

    ApplyMaterialRules("", "RoR's resource packs"); // Groups parsed before; later ones via MaterialRulesListener
    shadergen.invalidateScheme(RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);
}

bool RTSSManager::TranslateLegacyLighting(Ogre::Material& mat)
{
    bool changed = false;
    for (Ogre::Technique* tech : mat.getTechniques())
    {
        if (tech->getSchemeName() != Ogre::MaterialManager::DEFAULT_SCHEME_NAME)
            continue;
        for (Ogre::Pass* pass : tech->getPasses())
        {
            if (!pass->getLightingEnabled() || pass->isProgrammable()
                || pass->getUserObjectBindings().getUserAny(PBR_TRANSLATED_KEY).has_value())
                continue;

            // Blinn-Phong exponent -> perceptual GGX roughness. Without a specular colour no highlight was meant:
            // fully rough - also keeps the grazing-angle specular of foliage with fake 'up' normals (paged grass) low.
            const Ogre::ColourValue spec = pass->getSpecular();
            const float roughness = (spec.r + spec.g + spec.b > 0.05f && pass->getShininess() > 0.f)
                ? Ogre::Math::Clamp(std::sqrt(2.f / (pass->getShininess() + 2.f)), 0.1f, 1.f)
                : 1.f;
            ApplyMaterialClass(pass, ClassifyMaterial(GetClassNames(mat, pass), GetProgramNames(mat)), roughness); // M4.4

            // Ambient and emissive / diffuse, per channel. Works in gamma or linear space alike: the HDR pipeline
            // linearizes pass colours (GfxPipeline::LinearizeMaterial), and the ratio of gamma values becomes the
            // ratio of the linear ones.
            if (!(pass->getVertexColourTracking() & (Ogre::TVC_DIFFUSE | Ogre::TVC_AMBIENT | Ogre::TVC_EMISSIVE)))
            {
                const Ogre::ColourValue dif = pass->getDiffuse();
                auto divide = [&dif](Ogre::ColourValue c)
                {
                    for (int i = 0; i < 3; i++)
                        c[i] = (dif[i] > 0.01f) ? std::min(c[i] / dif[i], 16.f) : c[i];
                    return c;
                };
                pass->setAmbient(divide(pass->getAmbient()));
                pass->setSelfIllumination(divide(pass->getSelfIllumination()));
            }

            pass->getUserObjectBindings().setUserAny(PBR_TRANSLATED_KEY, Ogre::Any(true));
            changed = true;
        }
    }
    return changed;
}

bool RTSSManager::RescueMaterial(Ogre::Material& mat)
{
    const Ogre::Technique* src = nullptr;
    for (const Ogre::Technique* tech : mat.getTechniques())
    {
        if (tech->getSchemeName() != Ogre::MaterialManager::DEFAULT_SCHEME_NAME || tech->getNumPasses() == 0)
            continue;
        if (!HasUnrunnableShaders(tech))
            return false; // Something can run - nothing to rescue
        if (!src)
            src = tech;
    }
    if (!src)
        return false;

    // Copy of the first technique, without shaders. Extra passes (specular/reflection passes written for the
    // shaders) would only add odd layers without them - keep the first.
    Ogre::Technique* tech = mat.createTechnique();
    *tech = *src;
    tech->setName("RoR/CgRescue");
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

    // Keep only the colour texture; the others (specular, normal, environment...) were inputs for the shaders
    Ogre::TextureUnitState* diffuse = FindDiffuseUnit(pass);
    for (int i = (int)pass->getNumTextureUnitStates() - 1; i >= 0; i--)
    {
        if (pass->getTextureUnitState(i) != diffuse)
            pass->removeTextureUnitState(i);
    }
    if (diffuse)
    {
        diffuse->setColourOperation(Ogre::LBO_MODULATE);
        diffuse->setTextureCoordSet(0);
    }

    LOG(fmt::format("[RoR|Cg rescue] Material '{}' (group '{}'): no runnable shaders, using its texture '{}'",
        mat.getName(), mat.getGroup(), diffuse ? diffuse->getTextureName() : "(none)"));
    return true;
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

namespace {

/// Objects flagged NEAR_SHADOW_CASTER (grass) cast only into the near cascades. Small things' shadows are invisible
/// farther away, while the far cascades cover the most ground - grass there is the most expensive and useless.
class NearShadowCasterListener: public Ogre::ShadowTextureListener
{
public:
    void shadowTextureCasterPreViewProj(Ogre::Light*, Ogre::Camera* cam, size_t iteration) override
    {
        // With one sun and no other shadow-casting lights, iteration = cascade index
        if (Ogre::Viewport* vp = cam->getViewport())
            vp->setVisibilityMask((iteration < (size_t)RTSSManager::NUM_NEAR_CASCADES) ? 0xFFFFFFFF : ~(Ogre::uint32)NEAR_SHADOW_CASTER);
    }
};

NearShadowCasterListener g_near_shadow_caster_listener;

} // namespace

RTSSManager::~RTSSManager()
{
    if (App::GetGfxScene()->GetSceneManager())
        App::GetGfxScene()->GetSceneManager()->removeShadowTextureListener(&g_near_shadow_caster_listener);

    // Remove the shadow stage from the RTSS scheme again, otherwise every terrain load adds another one
    auto* shader_gen = RTShader::ShaderGenerator::getSingletonPtr();
    if (m_shadow_srs && shader_gen)
    {
        RTShader::RenderState* scheme_rs = shader_gen->getRenderState(RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);
        scheme_rs->removeSubRenderState(m_shadow_srs);
        shader_gen->invalidateScheme(RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);
        m_shadow_srs = nullptr;
    }
    if (pssmSetup && App::GetGfxScene()->GetSceneManager())
    {
        App::GetGfxScene()->GetSceneManager()->setShadowTechnique(Ogre::SHADOWTYPE_NONE);
        App::GetGfxScene()->GetSceneManager()->setShadowCameraSetup(Ogre::ShadowCameraSetupPtr()); // Deletes `pssmSetup`
        pssmSetup = nullptr;
    }
}

void RTSSManager::processPSSM()
{
    Ogre::SceneManager* sm = App::GetGfxScene()->GetSceneManager();
    // Shorter distance = the cascades cover less ground = sharper shadows
    const float shadow_distance = std::max(50.f, App::gfx_shadow_distance->getFloat());

    sm->setShadowTechnique(Ogre::SHADOWTYPE_TEXTURE_MODULATIVE_INTEGRATED);
    sm->setShadowDirLightTextureOffset(0);
    sm->setShadowFarDistance(shadow_distance);
    sm->setShadowTextureCountPerLightType(Ogre::Light::LT_DIRECTIONAL, NUM_CASCADES);
    // Only the sun casts shadows: other light types must not reserve shadow textures, so the sun's cascades
    // always stay in texture slots 0..NUM_CASCADES-1 whatever lights are around.
    sm->setShadowTextureCountPerLightType(Ogre::Light::LT_SPOTLIGHT, 0);
    sm->setShadowTextureCountPerLightType(Ogre::Light::LT_POINT, 0);
    sm->setShadowTextureCount(NUM_CASCADES);

    // Casters may shadow themselves (vehicle roof onto trunk, mirrors, wheel arches ...)
    sm->setShadowTextureSelfShadow(true);

    // Caster for everything without an own caster (see SetupShadowCaster())
    sm->setShadowTextureCasterMaterial(GetSharedShadowCaster());

    // Depth textures: the shadow cameras only render depth, which receivers compare against their own depth.
    // (With colour textures the caster material writes a *colour* - the receivers' comparison was meaningless:
    // shadows popped in and out with the camera and casters couldn't shadow themselves.)
    // PF_DEPTH16 is the only depth render target our GL render system accepts ("Valid FBO targets" in RoR.log);
    // OGRE silently replaces other depth formats with a colour format.
    // Resolution per cascade (`gfx_shadow_quality`), the nearest cascade gets the most.
    const int sizes[4][NUM_CASCADES] = {
        { 1024, 1024, 1024,  512 }, // Low
        { 2048, 1024, 1024, 1024 }, // Mid
        { 2048, 2048, 2048, 1024 }, // HQ
        { 4096, 4096, 2048, 2048 }, // Ultra
    };
    const int quality = Ogre::Math::Clamp(App::gfx_shadow_quality->getInt(), 0, 3);
    for (int i = 0; i < NUM_CASCADES; i++)
    {
        sm->setShadowTextureConfig(i, sizes[quality][i], sizes[quality][i], PF_DEPTH16);
    }

    // Split distances: mostly logarithmic (lambda 0.9 = more resolution near the camera). Starts at 1 m rather than
    // the camera's near clip (0.01 m), which would make the first cascade uselessly small.
    pssmSetup = new StableCSMShadowCameraSetup();
    pssmSetup->calculateSplitPoints(NUM_CASCADES, /*nearDist=*/1.0f, shadow_distance, /*lambda=*/0.9f);
    pssmSetup->setSplitPadding(1.0f);
    sm->setShadowCameraSetup(Ogre::ShadowCameraSetupPtr(pssmSetup));

    // Shadow receiving for every material RTSS shades: one stage in the RTSS scheme's render state, with the real
    // split distances. (Before, only materials derived from RoR's managed bases received shadows, through an
    // `rtshader_system { shadow_mapping light_count 4 }` block that treated the cascades as 4 separate lights.)
    auto* shader_gen = RTShader::ShaderGenerator::getSingletonPtr();
    RTShader::RenderState* scheme_rs = shader_gen->getRenderState(RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);
    m_shadow_srs = shader_gen->createSubRenderState(RTShader::SRS_SHADOW_MAPPING);
    m_shadow_srs->setParameter("split_points", Ogre::Any(pssmSetup->getSplitPoints()));
    m_shadow_srs->setParameter("filter", (quality >= 2) ? "pcf16" : "pcf4");
    scheme_rs->addTemplateSubRenderState(m_shadow_srs);
    shader_gen->invalidateScheme(RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);

    sm->removeShadowTextureListener(&g_near_shadow_caster_listener); // Not twice
    sm->addShadowTextureListener(&g_near_shadow_caster_listener);

    // Log what OGRE actually created - it falls back to other formats silently
    std::string splits;
    for (Ogre::Real d : pssmSetup->getSplitPoints())
        splits += fmt::format(" {:.1f}", d);
    LOG(fmt::format("[RoR|Shadows] {} cascades, distance {:.0f} m, splits (m):{}", NUM_CASCADES, shadow_distance, splits));
    for (size_t i = 0; i < NUM_CASCADES; i++)
    {
        const Ogre::TexturePtr& tex = sm->getShadowTexture(i);
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
