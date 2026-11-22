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

#include "GfxFog.h"

#include "Application.h"
#include "GfxScene.h"

#include <Ogre.h>

using namespace Ogre;
using namespace RoR;

namespace {

const char* const OWN_FOG_KEY = "RoR/BlendedPassFog"; //!< Marks passes whose fog override we set

/// Colour a pass must fog towards so that fog doesn't change what it does to the passes before it.
/// Returns false for passes that replace or alpha-blend (scene fog is right for them).
bool GetNeutralFogColour(const Pass* pass, ColourValue& out)
{
    const SceneBlendFactor src = pass->getSourceBlendFactor();
    const SceneBlendFactor dst = pass->getDestBlendFactor();
    if ((src == SBF_ZERO && dst == SBF_SOURCE_COLOUR) || (src == SBF_DEST_COLOUR && dst == SBF_ZERO))
        out = ColourValue::White;                  // Multiply
    else if (src == SBF_DEST_COLOUR && dst == SBF_SOURCE_COLOUR)
        out = ColourValue(0.5f, 0.5f, 0.5f, 1.f); // Modulate x2
    else if (dst == SBF_ONE && (src == SBF_ONE || src == SBF_SOURCE_ALPHA))
        out = ColourValue::Black;                  // Add
    else
        return false;
    return true;
}

} // namespace

void GfxFog::Init()
{
    ResourceGroupManager::getSingleton().addResourceGroupListener(this);
}

void GfxFog::Set(FogMode mode, ColourValue const& colour, float density, float start, float end)
{
    App::GetGfxScene()->GetSceneManager()->setFog(mode, colour, density, start, end);

    if (mode != m_mode || density != m_density || start != m_start || end != m_end)
    {
        m_mode = mode;
        m_density = density;
        m_start = start;
        m_end = end;
        this->ApplyToMaterials(nullptr);
    }
}

void GfxFog::resourceGroupScriptingEnded(const String& groupName)
{
    if (m_mode != FOG_NONE)
        this->ApplyToMaterials(&groupName);
}

void GfxFog::ApplyToMaterials(const String* group)
{
    auto it = MaterialManager::getSingleton().getResourceIterator();
    while (it.hasMoreElements())
    {
        MaterialPtr mat = static_pointer_cast<Material>(it.getNext());
        if (group && mat->getGroup() != *group)
            continue;

        for (Technique* tech: mat->getTechniques())
        {
            for (Pass* pass: tech->getPasses())
            {
                ColourValue neutral;
                if (!GetNeutralFogColour(pass, neutral))
                    continue;
                const bool ours = pass->getUserObjectBindings().getUserAny(OWN_FOG_KEY).has_value();
                if (pass->getFogOverride() && !ours)
                    continue; // The material's own `fog_override`

                // Same mode as the scene: RTSS compiles the mode into its shaders, the rest are parameters
                pass->setFog(m_mode != FOG_NONE, m_mode, neutral, m_density, m_start, m_end);
                pass->getUserObjectBindings().setUserAny(OWN_FOG_KEY, Any(true));
            }
        }
    }
}
