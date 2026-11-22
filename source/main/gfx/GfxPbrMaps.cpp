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

#include "GfxPbrMaps.h"

#include "Application.h"

#include <Ogre.h>
#include <OgreRectangle2D.h>

using namespace Ogre;
using namespace RoR;

SceneManager*                      GfxPbrMaps::s_scene = nullptr;
Camera*                            GfxPbrMaps::s_camera = nullptr;
Rectangle2D*                       GfxPbrMaps::s_rect = nullptr;
std::map<std::string, std::string> GfxPbrMaps::s_baked;
std::vector<TexturePtr>            GfxPbrMaps::s_env_chain;

static const uint32 MAX_BAKE_SIZE = 1024;
static const uint32 ENV_PREFILTER_SIZE = 128;
static const uint32 ENV_PREFILTER_MIPS = 4;    //!< 5 levels, the last one is roughness 1 (RTSS: lod = mips * r * (2 - r))
static const uint32 ENV_CHAIN_MIN_SIZE = 8;
static const int    ENV_PREFILTER_SAMPLES = 64;
static const PixelFormat ENV_FORMAT = PF_FLOAT16_RGBA; //!< HDR; explicit - the environment map reports PF_UNKNOWN

void GfxPbrMaps::SetupScene()
{
    if (s_scene)
        return;

    // Private scene with just a fullscreen rectangle - keeps these passes away from shadows, RTSS, fog etc.
    s_scene = Root::getSingleton().createSceneManager();
    s_camera = s_scene->createCamera("RoR/PBS/BakeCamera");
    s_scene->getRootSceneNode()->attachObject(s_camera);
    s_rect = s_scene->createScreenSpaceRect(/*includeTextureCoords=*/true);
    s_rect->setCorners(-1.f, 1.f, 1.f, -1.f);
    s_rect->setBoundingBox(AxisAlignedBox::BOX_INFINITE);
    s_scene->getRootSceneNode()->attachObject(s_rect);
}

MaterialPtr GfxPbrMaps::GetMaterial(const char* name, const char* sampler_name)
{
    MaterialPtr mat = MaterialManager::getSingleton().getByName(name, RGN_DEFAULT);
    if (!mat)
    {
        LOG(fmt::format("[RoR|PBR] ERROR: material '{}' not found", name));
        return MaterialPtr();
    }
    mat->load();
    if (mat->getNumSupportedTechniques() == 0)
    {
        LOG(fmt::format("[RoR|PBR] ERROR: '{}' unsupported: {}", name, mat->getUnsupportedTechniquesExplanation()));
        return MaterialPtr();
    }
    // Sampler unit for render systems without shader-side bindings (legacy GL)
    GpuProgramParametersSharedPtr fp = mat->getTechnique(0)->getPass(0)->getFragmentProgramParameters();
    fp->setIgnoreMissingParams(true);
    fp->setNamedConstant(sampler_name, 0);
    return mat;
}

void GfxPbrMaps::RenderQuad(MaterialPtr const& mat, RenderTarget* rt)
{
    GfxPbrMaps::SetupScene();
    s_rect->setMaterial(mat);
    rt->setAutoUpdated(false);
    Viewport* vp = rt->addViewport(s_camera);
    vp->setClearEveryFrame(false);
    vp->setOverlaysEnabled(false);
    vp->setShadowsEnabled(false);
    vp->setSkiesEnabled(false);
    rt->update();
    rt->removeAllViewports();
}

std::string GfxPbrMaps::BakeSpecularMap(TexturePtr const& specular_map, float base_roughness, float glossy_roughness, float metallic)
{
    if (!specular_map)
        return "";

    const std::string key = fmt::format("{}|{}|{:.3f}|{:.3f}|{:.3f}", specular_map->getGroup(), specular_map->getName(),
        base_roughness, glossy_roughness, metallic);
    auto found = s_baked.find(key);
    if (found != s_baked.end())
        return found->second;

    static MaterialPtr s_bake_material = GfxPbrMaps::GetMaterial("RoR/PBS/SpecToMR", "specMap");
    if (!s_bake_material)
        return "";
    try
    {
        specular_map->load();
    }
    catch (Exception& e)
    {
        LOG(fmt::format("[RoR|PBR] Specular map '{}' can't be loaded: {}", specular_map->getName(), e.getDescription()));
        s_baked[key] = "";
        return "";
    }

    const uint32 w = std::min(specular_map->getWidth(), MAX_BAKE_SIZE);
    const uint32 h = std::min(specular_map->getHeight(), MAX_BAKE_SIZE);
    const std::string name = fmt::format("RoR/PBS/MetalRoughness/{}", s_baked.size());
    // Rendered into a temporary target, then read back: drivers don't reliably generate mipmaps for render
    // targets (Mesa GL3+ left them as garbage - roughness ~0 at a distance, glittering lines on the paint).
    TexturePtr rtt = TextureManager::getSingleton().createManual(name + "/Bake", RGN_DEFAULT, TEX_TYPE_2D, w, h,
        0, PF_BYTE_RGBA, TU_RENDERTARGET);

    Pass* pass = s_bake_material->getTechnique(0)->getPass(0);
    pass->getTextureUnitState("SpecMap")->setTexture(specular_map);
    GpuProgramParametersSharedPtr fp = pass->getFragmentProgramParameters();
    fp->setIgnoreMissingParams(true);
    fp->setNamedConstant("bakeParams", Vector4(base_roughness, glossy_roughness, metallic, 0.f));
    GfxPbrMaps::RenderQuad(s_bake_material, rtt->getBuffer()->getRenderTarget());

    // Regular texture from the result: its mipmaps are generated on upload like for any image file
    Image img;
    rtt->convertToImage(img);
    TextureManager::getSingleton().remove(rtt);
    TextureManager::getSingleton().loadImage(name, RGN_DEFAULT, img, TEX_TYPE_2D, MIP_UNLIMITED);

    LOG(fmt::format("[RoR|PBR] Baked metal/roughness texture '{}' ({}x{}) from specular map '{}'", name, w, h, specular_map->getName()));
    s_baked[key] = name;
    return name;
}

TexturePtr GfxPbrMaps::CreatePrefilteredEnvMap(std::string const& name)
{
    // Every level and face is rendered explicitly (no automatic mipmaps: garbage on Mesa, see BakeSpecularMap)
    TexturePtr tex = TextureManager::getSingleton().createManual(name, RGN_DEFAULT, TEX_TYPE_CUBE_MAP,
        ENV_PREFILTER_SIZE, ENV_PREFILTER_SIZE, ENV_PREFILTER_MIPS, ENV_FORMAT, TU_RENDERTARGET);
    // Black until the first prefilter (the environment map is only rendered once there's a player vehicle)
    for (uint32 level = 0; level <= ENV_PREFILTER_MIPS; level++)
    {
        for (size_t face = 0; face < 6; face++)
        {
            RenderTarget* rt = tex->getBuffer(face, level)->getRenderTarget();
            rt->setAutoUpdated(false);
            GfxPbrMaps::SetupScene();
            Viewport* vp = rt->addViewport(s_camera);
            vp->clear(FBT_COLOUR, ColourValue::Black);
            rt->removeAllViewports();
        }
    }
    LOG(fmt::format("[RoR|PBR] Created prefiltered environment map '{}' ({}, {} levels)", name, ENV_PREFILTER_SIZE, ENV_PREFILTER_MIPS + 1));
    return tex;
}

void GfxPbrMaps::PrefilterEnvMap(TexturePtr const& src, TexturePtr const& dst)
{
    static bool s_failed = false;
    if (s_failed)
        return;
    try
    {
        GfxPbrMaps::PrefilterEnvMapInternal(src, dst);
    }
    catch (Exception& e)
    {
        // Don't take the game down because of reflections; image based lighting stays dark instead
        s_failed = true;
        LOG(fmt::format("[RoR|PBR] ERROR: environment map prefiltering failed, disabled: {}", e.getDescription()));
    }
}

void GfxPbrMaps::PrefilterEnvMapInternal(TexturePtr const& src, TexturePtr const& dst)
{
    static MaterialPtr s_down_material = GfxPbrMaps::GetMaterial("RoR/PBS/EnvDownsample", "srcEnv");
    static MaterialPtr s_filter_material = GfxPbrMaps::GetMaterial("RoR/PBS/EnvPrefilter", "srcEnv");
    if (!s_down_material || !s_filter_material || !src || !dst)
        return;

    // Downsample chain: separate textures, so no pass reads the texture it renders into
    if (s_env_chain.empty() || s_env_chain[0] != src)
    {
        s_env_chain.clear();
        s_env_chain.push_back(src);
        for (uint32 size = src->getWidth() / 2; size >= ENV_CHAIN_MIN_SIZE; size /= 2)
        {
            s_env_chain.push_back(TextureManager::getSingleton().createManual(
                fmt::format("RoR/PBS/EnvChain/{}", size), RGN_DEFAULT, TEX_TYPE_CUBE_MAP, size, size, 0,
                ENV_FORMAT, TU_RENDERTARGET));
        }
    }

    auto render_face = [](MaterialPtr const& mat, TexturePtr const& source, RenderTarget* rt, size_t face, float alpha)
    {
        Pass* pass = mat->getTechnique(0)->getPass(0);
        pass->getTextureUnitState("Source")->setTexture(source);
        GpuProgramParametersSharedPtr fp = pass->getFragmentProgramParameters();
        fp->setIgnoreMissingParams(true);
        fp->setNamedConstant("envParams", Vector4((float)face, alpha, (float)ENV_PREFILTER_SAMPLES, 0.f));
        GfxPbrMaps::RenderQuad(mat, rt);
    };

    for (size_t i = 1; i < s_env_chain.size(); i++)
    {
        for (size_t face = 0; face < 6; face++)
            render_face(s_down_material, s_env_chain[i - 1], s_env_chain[i]->getBuffer(face)->getRenderTarget(), face, 0.f);
    }

    const float src_texel_solid_angle = 4.f * Math::PI / (6.f * src->getWidth() * src->getWidth());
    for (uint32 level = 0; level <= ENV_PREFILTER_MIPS; level++)
    {
        // Inverse of RTSS's lod = mips * r * (2 - r)
        const float roughness = 1.f - std::sqrt(1.f - (float)level / ENV_PREFILTER_MIPS);
        const float alpha = roughness * roughness;
        // Source level: at least as small as this level (no aliasing), and matching the sample spacing (filtered
        // importance sampling: solid angle per sample ~ 4 PI a^2 / N at the lobe's centre) - no sun fireflies
        const uint32 level_size = std::max(ENV_PREFILTER_SIZE >> level, 1u);
        float wanted = std::log2((float)src->getWidth() / level_size);
        if (alpha > 0.f)
        {
            const float sample_solid_angle = 4.f * Math::PI * alpha * alpha / ENV_PREFILTER_SAMPLES;
            wanted = std::max(wanted, 0.5f * std::log2(sample_solid_angle / src_texel_solid_angle) + 1.f);
        }
        const size_t chain_level = Math::Clamp((size_t)std::lround(std::max(wanted, 0.f)), (size_t)0, s_env_chain.size() - 1);

        for (size_t face = 0; face < 6; face++)
            render_face(s_filter_material, s_env_chain[chain_level], dst->getBuffer(face, level)->getRenderTarget(), face, alpha);
    }
}
