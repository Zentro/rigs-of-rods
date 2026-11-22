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

/// @file
/// @brief Builds physically based materials, shaded by OGRE's RTSS (Cook-Torrance + image based lighting + shadows).
///        Ported from the gfx-rtss branch (ROADMAP M4, path A).

#pragma once

#include "Application.h"

#include <OgreColourValue.h>
#include <OgreMaterial.h>

#include <map>
#include <string>

namespace RoR {

/// @addtogroup Gfx
/// @{

/// A texture reference; the group may differ from the material's (e.g. tuneup media).
struct PbsTexture
{
    std::string name;
    std::string group; //!< Empty = material's group

    bool IsSet() const { return !name.empty(); }
};

/// Everything a RoR PBS material needs to know about a surface.
struct PbsMaterialDef
{
    std::string       name;
    std::string       group;

    Ogre::ColourValue albedo = Ogre::ColourValue::White;   //!< Multiplies the albedo map; alpha = opacity
    Ogre::ColourValue emissive = Ogre::ColourValue::Black; //!< Multiplies the emissive map (or constant)
    float             roughness = 0.7f;          //!< Perceptual; base of a baked legacy specular map
    float             metallic = 0.f;
    float             gloss = 0.f;               //!< Gloss layer strength (smooth top layer: paint, glass), M4.6
    float             gloss_roughness = 0.05f;
    float             alpha_threshold = 0.5f;    //!< For `alpha_test`

    PbsTexture        albedo_map;
    PbsTexture        orm_map;                   //!< Authored glTF layout: R occlusion, G roughness, B metallic
    PbsTexture        legacy_specular_map;       //!< RoR specular map (reflectivity mask), baked to roughness + gloss mask
    PbsTexture        emissive_map;
    PbsTexture        damage_map;                //!< Blended over albedo by vertex colour alpha (flexbodies)

    bool              alpha_test = false;
    bool              alpha_test_invert = false; //!< Keep only pixels with alpha <= threshold (cab windows)
    bool              alpha_blend = false;
    bool              two_sided = false;
    bool              vertex_tint = false;       //!< Multiply albedo by vertex colour (ignored with damage_map)
    bool              receive_shadows = true;
    bool              env_map = true;            //!< Reflections of the realtime environment map (image based lighting)
};

/// Physically based values a material's names suggest (ROADMAP M4.4); -1 = not set by the class
struct PbrMaterialClass
{
    float roughness = -1.f;
    float roughness_max = -1.f;    //!< Caps the roughness instead of setting it
    float gloss = -1.f;            //!< Gloss layer strength
    float gloss_roughness = -1.f;
    bool  metal = false;           //!< Metallic 1 on PBS materials (they have reflections); not on translated legacy ones

    bool IsSet() const { return roughness >= 0.f || roughness_max >= 0.f || metal; }
};

/// Creates RoR PBS materials: plain OGRE materials whose pass gets RTSS sub render states (metal/roughness
/// Cook-Torrance lighting, image based lighting from the prefiltered environment map); shadows come from the RTSS
/// scheme's integrated PSSM. HDR pipeline only.
class PbsMaterialFactory
{
public:
    static PbsMaterialFactory& Get();

    /// Creates (or replaces) a material. Returns null on failure (error is logged).
    Ogre::MaterialPtr CreateMaterial(PbsMaterialDef const& def);

    /// Use instead of `Material::clone()` for PBS materials (a plain clone isn't registered with RTSS).
    /// Returns null if `src` isn't a PBS material (clone it the regular way then).
    Ogre::MaterialPtr CloneMaterial(Ogre::MaterialPtr const& src, std::string const& new_name, std::string const& new_group);

    /// Converts a simple legacy (fixed-function, or dead Cg) material in place into a RoR PBS material (ROADMAP M4
    /// A.3, from the gfx-rtss branch). Recognized: diffuse texture/colour, shininess, environment map reflection,
    /// additive emissive/reflection passes, alpha test/blend, culling; Cg-rescued materials by texture unit names.
    /// Anything else (working shaders, lightmaps, detail maps, unlit...) is left as it is.
    /// @param colours_linear The material's colours were already linearized by the HDR pipeline.
    /// @return true if converted.
    bool ConvertLegacyMaterial(Ogre::MaterialPtr mat, bool colours_linear);

    /// The definition a PBS material was created from, or null if it isn't one.
    const PbsMaterialDef* FindDef(Ogre::MaterialPtr const& mat) const;
    const PbsMaterialDef* FindDef(Ogre::Material const& mat) const;

    /// Material class from names (material + diffuse texture), then from (dead, Cg) shader program names: legacy
    /// formats never stored what a surface is made of. Order matters, first match wins.
    static PbrMaterialClass ClassifyMaterial(std::string names, std::string program_names);

    /// Applies a class to a definition (roughness, gloss, metallic).
    static void ApplyMaterialClass(PbsMaterialDef& def, PbrMaterialClass const& c);

private:
    std::map<std::string, PbsMaterialDef> m_defs; //!< "group|name" -> definition
};

/// @} // addtogroup Gfx

} // namespace RoR
