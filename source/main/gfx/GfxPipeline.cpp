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
#include "GameContext.h"
#include "GfxAtmosphere.h"
#include "GfxScene.h"
#include "Terrain.h"
#include "PlatformUtils.h"
#include "PbsMaterialFactory.h"
#include "RTSSManager.h"

#include <OgreCompositorManager.h>
#include <RTShaderSystem/OgreShaderGenerator.h>

using namespace Ogre;
using namespace RoR;

// Resource packs whose materials render after tonemapping, in display space (UI)
static const char* UI_RESOURCE_PACKS[] = { "overlays", "dashboards", "mygui", "OgreCore" };

// Material name prefixes (lowercase) that are UI or the pipeline itself - created in code, no script origin
static const char* UI_MATERIAL_PREFIXES[] = { "imgui", "mygui", "rigsofrods/wallpaper", "ror/pipeline/", "ogre/" };

// Substrings (lowercase) of texture names and texture unit names which hold data, not colour - must not be sRGB-decoded.
static const char* NON_COLOUR_TEXTURE_HINTS[] = {
    "normal", "_n.", "_nm.", "nrm", "bump", "height", "_s.", "spec", "rough", "metal", "_orm",
    "_ao.", "occlusion", "mask", "noise", "perlin", "fresnel", "reflection", "refraction",
    "environmenttexture", "lightmap", "blendmap", "densitymap"
};

namespace {

/// With ShaderGenerator::setTargetLinearColours(), OGRE converts colour constants from gamma to linear while
/// uploading them (GpuProgramParameters::setUseLinearColours) - surface ambient/diffuse/emissive, derived ambient
/// and scene colour, light colours. RoR already supplies linear colours (GfxPipeline::LinearizeMaterial, ToLinear),
/// so they would be decoded twice: ambient 0.09 -> 0.008, black shadows, too dark surfaces. Turned off here right
/// before the parameters are uploaded (render object listeners fire before SceneManager::updateGpuProgramParameters).
/// Found on the gfx-rtss branch, see its WALKTHROUGH.md, section 3.4.
class LinearColourParamsListener: public RenderObjectListener
{
public:
    void notifyRenderSingleObject(Renderable*, const Pass* pass, const AutoParamDataSource*, const LightList*, bool) override
    {
        if (!pass->isProgrammable())
            return;
        for (int i = 0; i < GPT_COMPUTE_PROGRAM; i++)
        {
            if (pass->hasGpuProgram((GpuProgramType)i))
                pass->getGpuProgramParameters((GpuProgramType)i)->setUseLinearColours(false);
        }
    }
};

LinearColourParamsListener g_linear_colour_params_listener;

} // namespace

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

    // RTSS shaders: output linear colours (no gamma encoding, the tonemapper does that) and decode 8-bit textures
    // which aren't hardware-sRGB. Both parts below go together - either one alone makes shadows black.
    RTShader::ShaderGenerator::getSingleton().setTargetLinearColours(true);
    App::GetGfxScene()->GetSceneManager()->addRenderObjectListener(&g_linear_colour_params_listener);

    // Linear content: every group parsed from now on, plus the ones already parsed (RoR's own packs)
    this->CollectUiScripts();
    ResourceGroupManager::getSingleton().addResourceGroupListener(this);
    for (const String& group: ResourceGroupManager::getSingleton().getResourceGroups())
    {
        if (ResourceGroupManager::getSingleton().isResourceGroupInitialised(group))
            this->resourceGroupScriptingEnded(group);
    }
}

bool GfxPipeline::IsRequested()
{
    return App::gfx_render_pipeline->getEnum<GfxRenderPipeline>() == GfxRenderPipeline::PBR;
}

Ogre::PixelFormat GfxPipeline::GetSceneTargetFormat(Ogre::PixelFormat legacy_format)
{
    return IsRequested() ? PF_FLOAT16_RGBA : legacy_format;
}

Ogre::uint GfxPipeline::GetSceneTargetFsaa(Ogre::uint legacy_fsaa)
{
    return IsRequested() ? 0 : legacy_fsaa;
}

void GfxPipeline::AddToViewport(Ogre::Viewport* viewport)
{
    if (!m_active || !viewport)
        return;
    CompositorInstance* hdr = CompositorManager::getSingleton().addCompositor(viewport, "RoR/HDR");
    if (!hdr)
    {
        LOG("[RoR|GfxPipeline] Could not add compositor 'RoR/HDR' to a secondary view, it will look too dark");
        return;
    }
    hdr->addListener(this); // Tonemap constants, see notifyMaterialRender()
    CompositorManager::getSingleton().setCompositorEnabled(viewport, "RoR/HDR", true);
}

void GfxPipeline::UpdateFrame()
{
    if (!m_active)
        return;

    float exposure = App::gfx_exposure->getFloat();
    GfxAtmosphere* atmosphere = App::GetGameContext()->GetTerrain() ? App::GetGameContext()->GetTerrain()->getAtmosphere() : nullptr;
    if (atmosphere && App::gfx_auto_exposure->getBool())
    {
        exposure *= atmosphere->GetAutoExposure();
    }
    m_tonemap_values.x = Math::Clamp(exposure, 0.01f, 64.f);
    m_tonemap_values.y = static_cast<float>(Math::Clamp(App::gfx_tonemap->getInt(), 0, 3));
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

Ogre::ColourValue GfxPipeline::ToLinear(Ogre::ColourValue const& c) const
{
    if (!m_active)
        return c;

    // The exact sRGB curve is overkill for authored colour values; gamma 2.2 matches what authors saw.
    return Ogre::ColourValue(
        std::pow(std::max(c.r, 0.f), 2.2f),
        std::pow(std::max(c.g, 0.f), 2.2f),
        std::pow(std::max(c.b, 0.f), 2.2f),
        c.a);
}

void GfxPipeline::EnsureTextureSRGB(Ogre::TexturePtr tex)
{
    if (!tex || tex->isHardwareGammaEnabled() || (tex->getUsage() & TU_RENDERTARGET))
        return;

    const bool reload = tex->isLoaded();
    if (reload)
        tex->unload();
    tex->setHardwareGammaEnabled(true);
    if (reload)
        tex->load();
}

static bool ContainsAnyHint(std::string text)
{
    Ogre::StringUtil::toLowerCase(text);
    for (const char* hint: NON_COLOUR_TEXTURE_HINTS)
    {
        if (text.find(hint) != std::string::npos)
            return true;
    }
    return false;
}

static bool IsColourTexture(TextureUnitState* tus)
{
    if (tus->getContentType() != TextureUnitState::CONTENT_NAMED)
        return false;
    if (tus->getTextureType() != TEX_TYPE_2D && tus->getTextureType() != TEX_TYPE_CUBE_MAP)
        return false;
    if (tus->getNumFrames() == 0 || tus->getFrameTextureName(0).empty())
        return false;
    // Data textures: by texture name (e.g. 'car_s.dds') or by texture unit name (e.g. managed 'Specular_Map')
    return !ContainsAnyHint(tus->getFrameTextureName(0)) && !ContainsAnyHint(tus->getName());
}

bool GfxPipeline::IsUiMaterial(const Ogre::Material* mat) const
{
    std::string name = mat->getName();
    Ogre::StringUtil::toLowerCase(name);
    for (const char* prefix: UI_MATERIAL_PREFIXES)
    {
        if (Ogre::StringUtil::startsWith(name, prefix, /*lowerCase:*/false))
            return true;
    }
    return mat->getGroup() == RGN_INTERNAL || m_ui_scripts.count(mat->getOrigin()) > 0;
}

void GfxPipeline::LinearizeTextures(Ogre::Material* mat)
{
    if (!m_active || !mat || this->IsUiMaterial(mat))
        return;

    for (Technique* tech: mat->getTechniques())
    {
        for (Pass* pass: tech->getPasses())
        {
            for (TextureUnitState* tus: pass->getTextureUnitStates())
            {
                if (!IsColourTexture(tus))
                    continue;
                // OGRE 14 keeps the sRGB flag on the texture itself (TextureUnitState::setHardwareGammaEnabled just
                // forwards it, which would mark already-loaded textures without reloading them). The texture unit
                // created its (unloaded) texture objects when it got the names - look them up and flag/reload them.
                // Never load anything new here: e.g. at startup the texture's pack may not be added yet, and a failed
                // load leaves the texture layer blank for good (black character).
                for (unsigned int i = 0; i < tus->getNumFrames(); i++)
                {
                    const String& name = tus->getFrameTextureName(i);
                    TexturePtr tex = TextureManager::getSingleton().getByName(name, mat->getGroup());
                    if (!tex)
                        tex = TextureManager::getSingleton().getByName(name, RGN_AUTODETECT);
                    EnsureTextureSRGB(tex);
                }
            }
        }
    }
}

void GfxPipeline::LinearizeMaterial(Ogre::Material* mat)
{
    if (!m_active || !mat || this->IsUiMaterial(mat))
        return;
    if (PbsMaterialFactory::Get().FindDef(*mat))
        return; // PBS materials are created with linear colours

    if (m_linearized.insert(mat->getGroup() + "|" + mat->getName()).second) // Colours only once!
    {
        for (Technique* tech: mat->getTechniques())
        {
            for (Pass* pass: tech->getPasses())
            {
                pass->setAmbient(this->ToLinear(pass->getAmbient()));
                pass->setDiffuse(this->ToLinear(pass->getDiffuse()));
                // Translated for physically based lighting, the specular holds roughness/metallic, not a colour
                if (!pass->getUserObjectBindings().getUserAny(RTSSManager::PBR_TRANSLATED_KEY).has_value())
                    pass->setSpecular(this->ToLinear(pass->getSpecular()));
                pass->setSelfIllumination(this->ToLinear(pass->getSelfIllumination()));

                // Materials with their own shaders: authored colour constants (e.g. the water's 'tintColour')
                for (int i = 0; i < GPT_COMPUTE_PROGRAM; i++)
                {
                    if (pass->hasGpuProgram((GpuProgramType)i))
                        this->LinearizeColourConstants(pass->getGpuProgramParameters((GpuProgramType)i));
                }
            }
        }
    }
    this->LinearizeTextures(mat);
}

void GfxPipeline::LinearizeColourConstants(Ogre::GpuProgramParametersSharedPtr params)
{
    if (!params || !params->hasNamedParameters())
        return;
    for (const auto& entry: params->getConstantDefinitions().map)
    {
        std::string name = entry.first;
        Ogre::StringUtil::toLowerCase(name);
        const GpuConstantDefinition& def = entry.second;
        if ((name.find("colour") == std::string::npos && name.find("color") == std::string::npos)
            || (def.constType != GCT_FLOAT3 && def.constType != GCT_FLOAT4)
            || def.arraySize != 1
            || params->_findRawAutoConstantEntryFloat(def.physicalIndex)) // Automatic constants are linear already
            continue;
        float* v = params->getFloatPointer(def.physicalIndex);
        const ColourValue lin = this->ToLinear(ColourValue(v[0], v[1], v[2]));
        v[0] = lin.r; v[1] = lin.g; v[2] = lin.b;
    }
}

void GfxPipeline::resourceGroupScriptingEnded(const Ogre::String& groupName)
{
    if (!m_active)
        return;

    size_t count = 0;
    auto it = MaterialManager::getSingleton().getResourceIterator();
    while (it.hasMoreElements())
    {
        MaterialPtr mat = static_pointer_cast<Material>(it.getNext());
        if (mat->getGroup() == groupName && !this->IsUiMaterial(mat.get()))
        {
            this->LinearizeMaterial(mat.get());
            count++;
        }
    }
    if (count > 0)
        LOG(fmt::format("[RoR|GfxPipeline] {} materials in group '{}' converted to linear", count, groupName));
}

void GfxPipeline::CollectUiScripts()
{
    for (const char* pack: UI_RESOURCE_PACKS)
    {
        const std::string dir_path = PathCombine(App::sys_resources_dir->getStr(), pack);
        const std::string zip_path = dir_path + ".zip";
        Archive* arch = nullptr;
        if (FileExists(zip_path))
            arch = ArchiveManager::getSingleton().load(zip_path, "Zip", /*readOnly=*/true);
        else if (FolderExists(dir_path))
            arch = ArchiveManager::getSingleton().load(dir_path, "FileSystem", /*readOnly=*/true);
        if (!arch)
            continue;

        StringVectorPtr files = arch->find("*.material", /*recursive=*/true);
        for (const String& f: *files)
        {
            m_ui_scripts.insert(f);
        }
        // Not unloaded - archives are shared by path with the resource groups that use the same pack
    }
}
