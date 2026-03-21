
// -------------------------------------------
// OGRE-IMGUI bindings
// See file 'README-OgreImGui.txt' for details
// -------------------------------------------

/*
    This source file is part of Rigs of Rods
    Copyright 2016-2020 Petr Ohlidal

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

#pragma once

#include "OgreImGuiOverlay.h" // RoR's ported version

#include <imgui.h>
#include <Ogre.h>
#include <OISMouse.h>
#include <OISKeyboard.h>
#include <memory>
#include <vector>

/// DearIMGUI integration.
/// Input handling is done by injecting OIS events to ImGUI
/// Rendering is done via port of Ogre::ImGuiOverlay; this is temporary until we migrate to OGRE 1.12.x
///   however, since our OGRE version doesn't have `OverlayManager::addOverlay()`,
///   we queue it for rendering ourselves via RenderQueueListener
///   (code is shamelessly copy-pasted from OGRE)
class OgreImGui: public Ogre::RenderQueueListener
{
public:
    void Init();

    // Input-injecting functions
    void InjectMouseMoved( const OIS::MouseEvent &arg );
    void SetMouseButtonState(OIS::MouseButtonID id, bool down);
    void ResetAllMouseButtons();
    void InjectKeyPressed( const OIS::KeyEvent &arg );
    void InjectKeyReleased( const OIS::KeyEvent &arg );

    // Ogre::RenderQueueListener
    virtual void renderQueueStarted(Ogre::uint8 queueGroupId,
        const Ogre::String& invocation, bool& skipThisInvocation) override;

    // Font pointers for different sizes (Maven Pro + FontAwesome merged)
    ImFont* FontRegular   = nullptr;  // 16px regular  - default UI font
    ImFont* FontSmall     = nullptr;  // 13px regular  - dense info text
    ImFont* FontMedium    = nullptr;  // 20px regular  - section headers
    ImFont* FontLarge     = nullptr;  // 32px regular  - large icons/text
    ImFont* FontBold      = nullptr;  // 20px bold     - emphasis text
    ImFont* FontBoldLarge = nullptr;  // 28px bold     - tile labels
    ImFont* FontIconHuge  = nullptr;  // 52px regular  - main menu tile icons

private:
    static ImGuiKey OisKeyToImGuiKey(OIS::KeyCode key);

    std::unique_ptr<Ogre::ImGuiOverlay> m_imgui_overlay;
    std::vector<uint8_t> m_font_data_maven;
    std::vector<uint8_t> m_font_data_bold;
    std::vector<uint8_t> m_font_data_fa;
};
