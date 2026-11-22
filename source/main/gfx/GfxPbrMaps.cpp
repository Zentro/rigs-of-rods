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
MaterialPtr                        GfxPbrMaps::s_material;
std::map<std::string, std::string> GfxPbrMaps::s_baked;

static const uint32 MAX_BAKE_SIZE = 1024;

bool GfxPbrMaps::SetupBaker()
{
    if (s_scene)
        return s_material != nullptr;

    // Private scene with just a fullscreen rectangle - keeps the bake away from shadows, RTSS, fog etc.
    s_scene = Root::getSingleton().createSceneManager();
    s_camera = s_scene->createCamera("RoR/PBS/BakeCamera");
    s_scene->getRootSceneNode()->attachObject(s_camera);
    Rectangle2D* rect = s_scene->createScreenSpaceRect(/*includeTextureCoords=*/true);
    rect->setCorners(-1.f, 1.f, 1.f, -1.f);
    rect->setBoundingBox(AxisAlignedBox::BOX_INFINITE);
    s_scene->getRootSceneNode()->attachObject(rect);

    s_material = MaterialManager::getSingleton().getByName("RoR/PBS/SpecToMR", RGN_DEFAULT);
    if (!s_material)
    {
        LOG("[RoR|PBR] ERROR: material 'RoR/PBS/SpecToMR' not found, specular maps won't be used");
        return false;
    }
    s_material->load();
    if (s_material->getNumSupportedTechniques() == 0)
    {
        LOG(fmt::format("[RoR|PBR] ERROR: 'RoR/PBS/SpecToMR' unsupported: {}", s_material->getUnsupportedTechniquesExplanation()));
        s_material.reset();
        return false;
    }
    // Sampler unit for render systems without shader-side bindings (legacy GL)
    GpuProgramParametersSharedPtr fp = s_material->getTechnique(0)->getPass(0)->getFragmentProgramParameters();
    fp->setIgnoreMissingParams(true);
    fp->setNamedConstant("specMap", 0);
    rect->setMaterial(s_material);
    return true;
}

std::string GfxPbrMaps::BakeSpecularMap(TexturePtr const& specular_map, float base_roughness, float glossy_roughness)
{
    if (!specular_map)
        return "";

    const std::string key = fmt::format("{}|{}|{:.3f}|{:.3f}", specular_map->getGroup(), specular_map->getName(),
        base_roughness, glossy_roughness);
    auto found = s_baked.find(key);
    if (found != s_baked.end())
        return found->second;

    if (!GfxPbrMaps::SetupBaker())
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

    Pass* pass = s_material->getTechnique(0)->getPass(0);
    pass->getTextureUnitState("SpecMap")->setTexture(specular_map);
    GpuProgramParametersSharedPtr fp = pass->getFragmentProgramParameters();
    fp->setIgnoreMissingParams(true);
    fp->setNamedConstant("bakeParams", Vector4(base_roughness, glossy_roughness, 0.f, 0.f));

    RenderTarget* rt = rtt->getBuffer()->getRenderTarget();
    rt->setAutoUpdated(false);
    Viewport* vp = rt->addViewport(s_camera);
    vp->setClearEveryFrame(false);
    vp->setOverlaysEnabled(false);
    vp->setShadowsEnabled(false);
    vp->setSkiesEnabled(false);
    rt->update();
    rt->removeAllViewports();

    // Regular texture from the result: its mipmaps are generated on upload like for any image file
    Image img;
    rtt->convertToImage(img);
    TextureManager::getSingleton().remove(rtt);
    TextureManager::getSingleton().loadImage(name, RGN_DEFAULT, img, TEX_TYPE_2D, MIP_UNLIMITED);

    LOG(fmt::format("[RoR|PBR] Baked metal/roughness texture '{}' ({}x{}) from specular map '{}'", name, w, h, specular_map->getName()));
    s_baked[key] = name;
    return name;
}
