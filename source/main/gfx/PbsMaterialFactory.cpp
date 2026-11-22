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
            mr_texture = GfxPbrMaps::BakeSpecularMap(spec, def.roughness, std::min(GLOSSY_ROUGHNESS, def.roughness));
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
    if (!mat)
        return nullptr;
    auto it = m_defs.find(mat->getGroup() + "|" + mat->getName());
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
    // c.metal: applied with ROADMAP M4.5, once reflections are in
}
