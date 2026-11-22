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

#include "PbsMaterialFactory.h"

#include "GfxPbrMaps.h"
#include "GfxPipeline.h"
#include "GfxScene.h"
#include "RTSSGloss.h"
#include "RTSSManager.h"

#include <Ogre.h>
#include <RTShaderSystem/OgreShaderGenerator.h>
#include <RTShaderSystem/OgreShaderRenderState.h>
#include <RTShaderSystem/OgreShaderSubRenderState.h>

using namespace Ogre;
using namespace RoR;

static const float GLOSSY_ROUGHNESS = 0.2f; //!< Where a legacy specular map is white (full reflection)
static const char* const ENV_MAP_NAME = "EnvironmentTexturePrefiltered"; //!< GfxEnvmap::SetupPrefilteredEnvMap()

PbsMaterialFactory& PbsMaterialFactory::Get()
{
    static PbsMaterialFactory instance;
    return instance;
}

static TexturePtr LoadTexture(PbsTexture const& tex, std::string const& mat_group, bool srgb)
{
    const std::string group = tex.group.empty() ? mat_group : tex.group;
    try
    {
        TexturePtr t = TextureManager::getSingleton().load(tex.name, group, TEX_TYPE_2D, MIP_DEFAULT, 1.f, PF_UNKNOWN, srgb);
        if (srgb)
            GfxPipeline::EnsureTextureSRGB(t); // In case it was loaded before by a non-colour user
        return t;
    }
    catch (Ogre::Exception& e)
    {
        LOG(fmt::format("[RoR|PBS] Texture '{}' (group '{}') failed to load: {}", tex.name, group, e.getDescription()));
        return TexturePtr();
    }
}

MaterialPtr PbsMaterialFactory::CreateMaterial(PbsMaterialDef const& def)
{
    auto* shader_gen = RTShader::ShaderGenerator::getSingletonPtr();
    if (!shader_gen || !App::GetGfxScene()->GetPipeline().IsActive())
        return MaterialPtr();

    const String& scheme = RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME;
    // A previous material of the same name (e.g. a legacy material being converted) may be registered with RTSS
    shader_gen->removeAllShaderBasedTechniques(def.name, def.group);
    MaterialManager::getSingleton().remove(def.name, def.group);
    MaterialPtr mat = MaterialManager::getSingleton().create(def.name, def.group);
    mat->setReceiveShadows(def.receive_shadows);
    mat->setTransparencyCastsShadows(def.alpha_test);

    // ---- Base pass: described with regular material properties, RTSS turns them into shaders ----
    Technique* tech = mat->getTechnique(0);
    Pass* pass = tech->getPass(0);
    pass->setLightingEnabled(true);
    pass->setDiffuse(def.albedo);
    // Sky light comes from image based lighting (directional: bright sky above, ground below) - no flat scene
    // ambient on top, it would count the sky twice. Without the realtime environment map, keep the ambient.
    const bool ibl = def.env_map && App::gfx_envmap_enabled->getBool();
    pass->setAmbient(ibl ? ColourValue::Black : ColourValue::White);
    // x = roughness, y = metallic (RTSS Cook-Torrance); z = gloss strength, w = gloss roughness (gloss layer, M4.6)
    pass->setSpecular(ColourValue(def.roughness, def.metallic, Math::Clamp(def.gloss, 0.f, 1.f), def.gloss_roughness));
    // Marks the pass as already in physically based terms (RTSSManager::TranslateLegacyLighting skips it)
    pass->getUserObjectBindings().setUserAny(RTSSManager::PBR_TRANSLATED_KEY, Any(true));
    // With an emissive map, the emission comes from the map's additive pass - `emissive` is its tint
    pass->setSelfIllumination(def.emissive_map.IsSet() ? ColourValue::Black : def.emissive);

    if (def.two_sided)
    {
        pass->setCullingMode(CULL_NONE);
    }
    if (def.alpha_test)
    {
        const unsigned char thr = (unsigned char)Math::Clamp(def.alpha_threshold * 255.f, 1.f, 255.f);
        pass->setAlphaRejectSettings(def.alpha_test_invert ? CMPF_LESS_EQUAL : CMPF_GREATER_EQUAL, thr);
    }
    if (def.alpha_blend)
    {
        pass->setSceneBlending(SBT_TRANSPARENT_ALPHA);
        pass->setDepthWriteEnabled(false);
    }

    if (def.albedo_map.IsSet())
    {
        TextureUnitState* tus = pass->createTextureUnitState();
        tus->setTexture(LoadTexture(def.albedo_map, def.group, /*srgb=*/true));
        if (def.damage_map.IsSet())
        {
            // Flexbody damage: blend towards the damage texture by vertex colour alpha (classic RoR setup).
            // The vertex colour's rgb are simulation flags (blue = wet), not a colour - kept out of the albedo.
            pass->setVertexColourTracking(TVC_DIFFUSE);
            tus->setColourOperationEx(LBX_SOURCE1, LBS_TEXTURE, LBS_CURRENT);
            TextureUnitState* dmg = pass->createTextureUnitState();
            dmg->setTexture(LoadTexture(def.damage_map, def.group, /*srgb=*/true));
            dmg->setColourOperationEx(LBX_BLEND_DIFFUSE_ALPHA, LBS_TEXTURE, LBS_CURRENT);
        }
        else if (def.vertex_tint)
        {
            pass->setVertexColourTracking(TVC_DIFFUSE);
        }
    }

    // ---- Emissive map: additive unlit pass ----
    if (def.emissive_map.IsSet())
    {
        Pass* glow = tech->createPass();
        glow->setLightingEnabled(false);
        glow->setSceneBlending(SBT_ADD);
        glow->setDepthWriteEnabled(false);
        glow->setDepthFunction(CMPF_LESS_EQUAL);
        glow->setCullingMode(pass->getCullingMode());
        glow->setFog(true, FOG_NONE);
        TextureUnitState* tus = glow->createTextureUnitState();
        tus->setTexture(LoadTexture(def.emissive_map, def.group, /*srgb=*/true));
    }

    // ---- RTSS: shader based technique + physically based sub render states on the base pass ----
    mat->compile(); // RTSS only uses supported (compiled) techniques as source
    if (!shader_gen->createShaderBasedTechnique(*mat, MaterialManager::DEFAULT_SCHEME_NAME, scheme))
    {
        LOG(fmt::format("[RoR|PBS] ERROR: RTSS couldn't create a technique for '{}'", def.name));
        return MaterialPtr();
    }
    RTShader::RenderState* rs = shader_gen->getRenderState(scheme, *mat, 0);

    RTShader::SubRenderState* lighting = shader_gen->createSubRenderState(RTShader::SRS_COOK_TORRANCE_LIGHTING);
    std::string mr_texture;
    if (def.orm_map.IsSet())
    {
        if (TexturePtr orm = LoadTexture(def.orm_map, def.group, /*srgb=*/false))
            mr_texture = orm->getName();
    }
    else if (def.legacy_specular_map.IsSet())
    {
        if (TexturePtr spec = LoadTexture(def.legacy_specular_map, def.group, /*srgb=*/false))
            mr_texture = GfxPbrMaps::BakeSpecularMap(spec, def.roughness, std::min(GLOSSY_ROUGHNESS, def.roughness), def.metallic);
    }
    if (!mr_texture.empty())
    {
        lighting->setParameter("texture", mr_texture);
    }
    rs->addTemplateSubRenderState(lighting);

    if (ibl)
    {
        RTShader::SubRenderState* ibl = shader_gen->createSubRenderState(RTShader::SRS_IMAGE_BASED_LIGHTING);
        ibl->setParameter("texture", ENV_MAP_NAME);
        ibl->setParameter("luminance", "1.0");
        rs->addTemplateSubRenderState(ibl);

        // Gloss layer: smooth top layer (paint lacquer, glass, lenses); a baked legacy specular map carries the
        // per-texel mask in alpha (legacy RoR's reflectivity)
        const bool gloss_mask = def.legacy_specular_map.IsSet() && !def.orm_map.IsSet() && !mr_texture.empty();
        if (def.gloss > 0.f || gloss_mask)
        {
            RTShader::SubRenderState* gloss = shader_gen->createSubRenderState(RTSSGloss::TYPE);
            gloss->setParameter("texture", ENV_MAP_NAME);
            gloss->setParameter("mask_from_texture", gloss_mask ? "true" : "false");
            rs->addTemplateSubRenderState(gloss);
        }
    }

    shader_gen->validateMaterial(scheme, *mat);
    RTSSManager::SetupShadowCaster(*mat); // Alpha-tested: own caster; see-through: none
    mat->load();
    m_defs[def.group + "|" + def.name] = def;
    return mat;
}

MaterialPtr PbsMaterialFactory::CloneMaterial(MaterialPtr const& src, std::string const& new_name, std::string const& new_group)
{
    const PbsMaterialDef* src_def = this->FindDef(src);
    if (!src_def)
        return MaterialPtr();

    PbsMaterialDef def = *src_def;
    // Textures stay in their original group unless they already had an explicit one
    auto keep_group = [&](PbsTexture& t) { if (t.IsSet() && t.group.empty()) t.group = src_def->group; };
    keep_group(def.albedo_map);
    keep_group(def.orm_map);
    keep_group(def.legacy_specular_map);
    keep_group(def.emissive_map);
    keep_group(def.damage_map);
    def.name = new_name;
    def.group = new_group;
    return this->CreateMaterial(def);
}

const PbsMaterialDef* PbsMaterialFactory::FindDef(MaterialPtr const& mat) const
{
    return mat ? this->FindDef(*mat) : nullptr;
}

const PbsMaterialDef* PbsMaterialFactory::FindDef(Material const& mat) const
{
    auto it = m_defs.find(mat.getGroup() + "|" + mat.getName());
    return (it != m_defs.end()) ? &it->second : nullptr;
}

static bool NameHas(std::string const& lowercase_names, std::initializer_list<const char*> hints)
{
    for (const char* hint : hints)
    {
        if (lowercase_names.find(hint) != std::string::npos)
            return true;
    }
    return false;
}

PbrMaterialClass PbsMaterialFactory::ClassifyMaterial(std::string names, std::string program_names)
{
    // Deliberately not matched: "metal" (also "metallicblue" paint), "rim" (also "trim", "primer").
    StringUtil::toLowerCase(names);
    StringUtil::toLowerCase(program_names);
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

void PbsMaterialFactory::ApplyMaterialClass(PbsMaterialDef& def, PbrMaterialClass const& c)
{
    if (c.roughness >= 0.f)
        def.roughness = c.roughness;
    if (c.roughness_max >= 0.f)
        def.roughness = std::min(def.roughness, c.roughness_max);
    if (c.gloss >= 0.f)
        def.gloss = c.gloss;
    if (c.gloss_roughness >= 0.f)
        def.gloss_roughness = c.gloss_roughness;
    // Metals reflect their surroundings and have no diffuse colour: only with image based lighting (PBS materials
    // have it; translated legacy materials keep 'metal' as a pass flag only - they'd render almost black)
    if (c.metal)
        def.metallic = 1.f;
}

// ---------------------------------------------------------------------------------------------
// Legacy material conversion (ROADMAP M4 A.3, ported from the gfx-rtss branch)

static bool IsEnvMapTus(TextureUnitState* tus)
{
    for (auto& eff: tus->getEffects())
    {
        if (eff.first == TextureUnitState::ET_ENVIRONMENT_MAP)
            return true;
    }
    return tus->getTextureType() == TEX_TYPE_CUBE_MAP;
}

static bool IsAdditive(Pass* pass)
{
    return pass->getSourceBlendFactor() == SBF_ONE && pass->getDestBlendFactor() == SBF_ONE;
}

static bool IsAlphaBlend(Pass* pass)
{
    return pass->getSourceBlendFactor() == SBF_SOURCE_ALPHA && pass->getDestBlendFactor() == SBF_ONE_MINUS_SOURCE_ALPHA;
}

static std::string Lowercase(std::string s)
{
    StringUtil::toLowerCase(s);
    return s;
}

/// True if the pass has shaders which can't run (Cg is gone, or compile errors) - rebuilt from their texture units
static bool HasDeadPrograms(Pass* pass)
{
    for (GpuProgramType t: { GPT_VERTEX_PROGRAM, GPT_FRAGMENT_PROGRAM })
    {
        if (!pass->hasGpuProgram(t))
            continue;
        const GpuProgramPtr& prog = pass->getGpuProgram(t);
        if (!prog)
            return true;
        if (StringUtil::match(prog->getLanguage(), "cg", /*caseSensitive:*/false))
            return true;
        if (prog->isLoaded() && (prog->hasCompileError() || !prog->isSupported()))
            return true;
    }
    return false;
}

static std::string ProgramNames(Pass* pass)
{
    std::string names;
    for (GpuProgramType t: { GPT_VERTEX_PROGRAM, GPT_FRAGMENT_PROGRAM })
    {
        if (pass->hasGpuProgram(t))
            names += pass->getGpuProgramName(t) + " ";
    }
    return names;
}

/// Classifies a 2D texture unit of a (Cg-rescued) legacy material by its unit/texture name
enum class LegacyTexRole { ALBEDO, SPECULAR, NORMAL, IGNORE };
static LegacyTexRole ClassifyTexture(TextureUnitState* tus)
{
    const std::string unit_name = Lowercase(tus->getName());
    const std::string tex_name = Lowercase(tus->getTextureName());
    auto has = [](std::string const& s, std::initializer_list<const char*> hints) {
        for (const char* h: hints) { if (s.find(h) != std::string::npos) return true; }
        return false;
    };
    if (has(unit_name, {"spec", "gloss", "refl"}) || has(tex_name, {"_s.", "_spec", "specular", "_sm."}))
        return LegacyTexRole::SPECULAR;
    if (has(unit_name, {"normal", "bump", "nm"}) || has(tex_name, {"_n.", "_nm.", "normal", "bump", "_nrm"}))
        return LegacyTexRole::NORMAL;
    if (has(unit_name, {"light", "ao", "shadow", "dirt", "detail", "noise"}))
        return LegacyTexRole::IGNORE;
    return LegacyTexRole::ALBEDO;
}

bool PbsMaterialFactory::ConvertLegacyMaterial(MaterialPtr mat, bool colours_linear)
{
    if (!mat || mat->getNumTechniques() == 0 || !App::GetGfxScene()->GetPipeline().IsActive())
        return false;
    if (this->FindDef(mat))
        return false; // Already a PBS material

    // Materials authored for RTSS PBR ('rtshader_system { lighting_stage metal_roughness / normal_map }') are left
    // as they are - converting would throw the authored maps away.
    RTShader::ShaderGenerator* shader_gen = RTShader::ShaderGenerator::getSingletonPtr();
    if (shader_gen && mat->getTechnique(0)->getNumPasses() > 0
        && shader_gen->hasShaderBasedTechnique(*mat, MaterialManager::DEFAULT_SCHEME_NAME,
                                               RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME))
    {
        RTShader::RenderState* rs = shader_gen->getRenderState(RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME, *mat, 0);
        if (rs && (rs->getSubRenderState(RTShader::SRS_COOK_TORRANCE_LIGHTING) || rs->getSubRenderState(RTShader::SRS_NORMALMAP)))
            return false;
    }

    Technique* tech = mat->getTechnique(0);
    // Shaders that can't run (Cg): prefer the author's fixed-function fallback technique over guessing from the
    // shader's texture units (e.g. terrain splat materials - their first unit is an alpha mask)
    if (tech->getNumPasses() > 0 && tech->getPass(0)->isProgrammable() && HasDeadPrograms(tech->getPass(0)))
    {
        for (unsigned short t = 1; t < mat->getNumTechniques(); t++)
        {
            Technique* fallback = mat->getTechnique(t);
            if (fallback->getNumPasses() > 0 && !fallback->getPass(0)->isProgrammable()
                && fallback->getPass(0)->getNumTextureUnitStates() > 0)
            {
                tech = fallback;
                break;
            }
        }
    }
    if (tech->getNumPasses() == 0 || tech->getNumPasses() > 3)
        return false;

    PbsMaterialDef def;
    def.name = mat->getName();
    def.group = mat->getGroup();
    def.receive_shadows = mat->getReceiveShadows();

    // ---- Base pass ----
    Pass* base = tech->getPass(0);
    const bool rescue = base->isProgrammable() && HasDeadPrograms(base);
    if (base->isProgrammable() && !rescue)
        return false; // Working custom shaders: keep as they are
    if (!base->getLightingEnabled() && !rescue)
        return false; // Unlit (signs, sky, effects): keep legacy
    if (!base->getColourWriteEnabled())
        return false; // Invisible helpers (e.g. 'tracks/trans' wheel bands) or depth-only passes

    for (unsigned short i = 0; i < base->getNumTextureUnitStates(); i++)
    {
        TextureUnitState* tus = base->getTextureUnitState(i);
        if (tus->getContentType() != TextureUnitState::CONTENT_NAMED || tus->getNumFrames() == 0)
            return false; // Shadow/compositor textures
        if (IsEnvMapTus(tus))
        {
            def.gloss = std::max(def.gloss, 0.6f); // Legacy chrome/paint look
            continue;
        }
        if (tus->getTextureType() != TEX_TYPE_2D)
            return false;

        const LegacyTexRole role = rescue ? ClassifyTexture(tus) : LegacyTexRole::ALBEDO;
        if (role == LegacyTexRole::SPECULAR && !def.legacy_specular_map.IsSet())
        {
            def.legacy_specular_map.name = tus->getTextureName();
        }
        else if (role == LegacyTexRole::ALBEDO && !def.albedo_map.IsSet())
        {
            if (tus->getTextureCoordSet() != 0)
                return false;
            def.albedo_map.name = tus->getTextureName();
        }
        else if (role == LegacyTexRole::NORMAL || role == LegacyTexRole::IGNORE || rescue)
        {
            continue; // Normal maps: ROADMAP M4 A.5; extra maps of rescued shaders (lightmaps, noise...) are dropped
        }
        else
        {
            return false; // Second colour texture in a fixed-function pass: detail map, lightmap... not supported
        }
    }

    auto to_linear = [&](ColourValue const& c) { return colours_linear ? c : App::GetGfxScene()->GetPipeline().ToLinear(c); };

    // With shaders, 'diffuse vertexcolour' is only a hint for the shader - ignore; for fixed-function materials the
    // diffuse colour tints the texture. Legacy trick: 'ambient 0.9 / diffuse 0.1' (vegetation, signs) = lit mostly
    // by ambient, i.e. "show the texture regardless of the sun" - the intended surface colour is the brighter one.
    const ColourValue amb = base->getAmbient(), dif = base->getDiffuse();
    const ColourValue legacy_colour = (amb.r + amb.g + amb.b > dif.r + dif.g + dif.b) ? amb : dif;
    def.albedo = (rescue || (base->getVertexColourTracking() & TVC_DIFFUSE)) ? ColourValue::White : to_linear(legacy_colour);
    def.albedo.a = 1.f;
    // Vertex colour tinting isn't converted: meshes without vertex colours would render black (as on gfx-rtss)

    // Blinn-Phong exponent -> GGX roughness, only meaningful if there was a specular colour at all
    const ColourValue spec = base->getSpecular();
    if (!rescue && spec.r + spec.g + spec.b > 0.05f && base->getShininess() > 0.f)
        def.roughness = Math::Clamp(Math::Sqrt(2.f / (base->getShininess() + 2.f)), 0.1f, 1.f);
    else if (!rescue)
        def.roughness = 1.f; // No highlight was meant (also keeps grazing specular on fake-normal foliage low)

    def.emissive = rescue ? ColourValue::Black : to_linear(base->getSelfIllumination());
    def.two_sided = base->getCullingMode() == CULL_NONE;

    // Alpha: a high rejection threshold means "cutout" (render opaque, sorted with solids)
    const CompareFunction rej_func = base->getAlphaRejectFunction();
    const int rej_val = base->getAlphaRejectValue();
    const bool rejects = (rej_func == CMPF_GREATER || rej_func == CMPF_GREATER_EQUAL) && rej_val > 0;
    if (rejects && rej_val >= 64)
    {
        def.alpha_test = true;
        def.alpha_threshold = rej_val / 255.f;
    }
    else if (IsAlphaBlend(base))
    {
        def.alpha_blend = true;
        def.alpha_test = rejects;
        def.alpha_threshold = std::max(rej_val, 1) / 255.f;
    }
    else if (base->getSourceBlendFactor() != SBF_ONE || base->getDestBlendFactor() != SBF_ZERO)
    {
        return false; // Exotic blending (modulate, add on the base pass...)
    }
    else if (rejects)
    {
        // Low threshold without blending (e.g. grass 'alpha_rejection greater 30') is still a cutout
        def.alpha_test = true;
        def.alpha_threshold = std::max(rej_val, 1) / 255.f;
    }

    // ---- Extra passes ----
    for (unsigned short p = 1; p < tech->getNumPasses(); p++)
    {
        Pass* pass = tech->getPass(p);
        if (!IsAdditive(pass))
            return false;
        if (pass->isProgrammable() && !HasDeadPrograms(pass))
            return false;

        bool has_env = false;
        TextureUnitState* glow = nullptr;
        for (unsigned short i = 0; i < pass->getNumTextureUnitStates(); i++)
        {
            TextureUnitState* tus = pass->getTextureUnitState(i);
            if (tus->getContentType() != TextureUnitState::CONTENT_NAMED || tus->getNumFrames() == 0)
                return false;
            if (IsEnvMapTus(tus))
                has_env = true;
            else if (!glow && tus->getTextureType() == TEX_TYPE_2D)
                glow = tus;
        }

        if (has_env)
        {
            def.gloss = std::max(def.gloss, 0.6f); // Additive reflection pass (+ its specular mask, ignored)
        }
        else if (glow && !pass->getLightingEnabled() && !def.emissive_map.IsSet() && pass->getNumTextureUnitStates() == 1)
        {
            // Additive unlit texture = glow (lights, displays)
            def.emissive_map.name = glow->getTextureName();
            def.emissive = ColourValue::White;
        }
        else
        {
            return false;
        }
    }

    // Some legacy materials rely on a texture that doesn't exist (placeholder name): don't convert those.
    // Only the material's own group and the global pool are searched at load time.
    auto exists = [&](PbsTexture const& t) {
        return !t.IsSet() || ResourceGroupManager::getSingleton().resourceExists(def.group, t.name)
            || ResourceGroupManager::getSingleton().resourceExists(RGN_DEFAULT, t.name);
    };
    if (!exists(def.albedo_map) || !exists(def.emissive_map))
        return false;
    if (!exists(def.legacy_specular_map))
        def.legacy_specular_map = PbsTexture();

    const std::string prog_names = rescue ? ProgramNames(base) : "";
    PbsMaterialFactory::ApplyMaterialClass(def, PbsMaterialFactory::ClassifyMaterial(def.name + " " + def.albedo_map.name, prog_names));
    if (rescue)
    {
        LOG(fmt::format("[RoR|PBS] Rescued material '{}' (programs: {}) as PBS", def.name, prog_names));
    }
    return this->CreateMaterial(def) != nullptr;
}
