
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

#include "OgreImGui.h"

#include "Actor.h"
#include "AppContext.h"
#include "ContentManager.h"
#include "IconsFontAwesome6.h"
#include "OgreImGuiOverlay.h"

#include <imgui.h>
#include <Ogre.h>
#include <OgreResourceGroupManager.h>

using namespace RoR;

// --------------------------------------------------------------------------
// OIS → ImGuiKey translation
// --------------------------------------------------------------------------

/*static*/ ImGuiKey OgreImGui::OisKeyToImGuiKey(OIS::KeyCode key)
{
    switch (key)
    {
    case OIS::KC_TAB:         return ImGuiKey_Tab;
    case OIS::KC_LEFT:        return ImGuiKey_LeftArrow;
    case OIS::KC_RIGHT:       return ImGuiKey_RightArrow;
    case OIS::KC_UP:          return ImGuiKey_UpArrow;
    case OIS::KC_DOWN:        return ImGuiKey_DownArrow;
    case OIS::KC_PGUP:        return ImGuiKey_PageUp;
    case OIS::KC_PGDOWN:      return ImGuiKey_PageDown;
    case OIS::KC_HOME:        return ImGuiKey_Home;
    case OIS::KC_END:         return ImGuiKey_End;
    case OIS::KC_INSERT:      return ImGuiKey_Insert;
    case OIS::KC_DELETE:      return ImGuiKey_Delete;
    case OIS::KC_BACK:        return ImGuiKey_Backspace;
    case OIS::KC_SPACE:       return ImGuiKey_Space;
    case OIS::KC_RETURN:      return ImGuiKey_Enter;
    case OIS::KC_ESCAPE:      return ImGuiKey_Escape;
    case OIS::KC_NUMPADENTER: return ImGuiKey_KeypadEnter;
    case OIS::KC_A:           return ImGuiKey_A;
    case OIS::KC_B:           return ImGuiKey_B;
    case OIS::KC_C:           return ImGuiKey_C;
    case OIS::KC_D:           return ImGuiKey_D;
    case OIS::KC_E:           return ImGuiKey_E;
    case OIS::KC_F:           return ImGuiKey_F;
    case OIS::KC_G:           return ImGuiKey_G;
    case OIS::KC_H:           return ImGuiKey_H;
    case OIS::KC_I:           return ImGuiKey_I;
    case OIS::KC_J:           return ImGuiKey_J;
    case OIS::KC_K:           return ImGuiKey_K;
    case OIS::KC_L:           return ImGuiKey_L;
    case OIS::KC_M:           return ImGuiKey_M;
    case OIS::KC_N:           return ImGuiKey_N;
    case OIS::KC_O:           return ImGuiKey_O;
    case OIS::KC_P:           return ImGuiKey_P;
    case OIS::KC_Q:           return ImGuiKey_Q;
    case OIS::KC_R:           return ImGuiKey_R;
    case OIS::KC_S:           return ImGuiKey_S;
    case OIS::KC_T:           return ImGuiKey_T;
    case OIS::KC_U:           return ImGuiKey_U;
    case OIS::KC_V:           return ImGuiKey_V;
    case OIS::KC_W:           return ImGuiKey_W;
    case OIS::KC_X:           return ImGuiKey_X;
    case OIS::KC_Y:           return ImGuiKey_Y;
    case OIS::KC_Z:           return ImGuiKey_Z;
    case OIS::KC_1:           return ImGuiKey_1;
    case OIS::KC_2:           return ImGuiKey_2;
    case OIS::KC_3:           return ImGuiKey_3;
    case OIS::KC_4:           return ImGuiKey_4;
    case OIS::KC_5:           return ImGuiKey_5;
    case OIS::KC_6:           return ImGuiKey_6;
    case OIS::KC_7:           return ImGuiKey_7;
    case OIS::KC_8:           return ImGuiKey_8;
    case OIS::KC_9:           return ImGuiKey_9;
    case OIS::KC_0:           return ImGuiKey_0;
    case OIS::KC_MINUS:       return ImGuiKey_Minus;
    case OIS::KC_EQUALS:      return ImGuiKey_Equal;
    case OIS::KC_LBRACKET:    return ImGuiKey_LeftBracket;
    case OIS::KC_RBRACKET:    return ImGuiKey_RightBracket;
    case OIS::KC_SEMICOLON:   return ImGuiKey_Semicolon;
    case OIS::KC_APOSTROPHE:  return ImGuiKey_Apostrophe;
    case OIS::KC_GRAVE:       return ImGuiKey_GraveAccent;
    case OIS::KC_BACKSLASH:   return ImGuiKey_Backslash;
    case OIS::KC_COMMA:       return ImGuiKey_Comma;
    case OIS::KC_PERIOD:      return ImGuiKey_Period;
    case OIS::KC_SLASH:       return ImGuiKey_Slash;
    case OIS::KC_CAPITAL:     return ImGuiKey_CapsLock;
    case OIS::KC_F1:          return ImGuiKey_F1;
    case OIS::KC_F2:          return ImGuiKey_F2;
    case OIS::KC_F3:          return ImGuiKey_F3;
    case OIS::KC_F4:          return ImGuiKey_F4;
    case OIS::KC_F5:          return ImGuiKey_F5;
    case OIS::KC_F6:          return ImGuiKey_F6;
    case OIS::KC_F7:          return ImGuiKey_F7;
    case OIS::KC_F8:          return ImGuiKey_F8;
    case OIS::KC_F9:          return ImGuiKey_F9;
    case OIS::KC_F10:         return ImGuiKey_F10;
    case OIS::KC_F11:         return ImGuiKey_F11;
    case OIS::KC_F12:         return ImGuiKey_F12;
    case OIS::KC_NUMLOCK:     return ImGuiKey_NumLock;
    case OIS::KC_SCROLL:      return ImGuiKey_ScrollLock;
    case OIS::KC_NUMPAD7:     return ImGuiKey_Keypad7;
    case OIS::KC_NUMPAD8:     return ImGuiKey_Keypad8;
    case OIS::KC_NUMPAD9:     return ImGuiKey_Keypad9;
    case OIS::KC_SUBTRACT:    return ImGuiKey_KeypadSubtract;
    case OIS::KC_NUMPAD4:     return ImGuiKey_Keypad4;
    case OIS::KC_NUMPAD5:     return ImGuiKey_Keypad5;
    case OIS::KC_NUMPAD6:     return ImGuiKey_Keypad6;
    case OIS::KC_ADD:         return ImGuiKey_KeypadAdd;
    case OIS::KC_NUMPAD1:     return ImGuiKey_Keypad1;
    case OIS::KC_NUMPAD2:     return ImGuiKey_Keypad2;
    case OIS::KC_NUMPAD3:     return ImGuiKey_Keypad3;
    case OIS::KC_NUMPAD0:     return ImGuiKey_Keypad0;
    case OIS::KC_DECIMAL:     return ImGuiKey_KeypadDecimal;
    case OIS::KC_DIVIDE:      return ImGuiKey_KeypadDivide;
    case OIS::KC_MULTIPLY:    return ImGuiKey_KeypadMultiply;
    case OIS::KC_LCONTROL:    return ImGuiKey_LeftCtrl;
    case OIS::KC_RCONTROL:    return ImGuiKey_RightCtrl;
    case OIS::KC_LSHIFT:      return ImGuiKey_LeftShift;
    case OIS::KC_RSHIFT:      return ImGuiKey_RightShift;
    case OIS::KC_LMENU:       return ImGuiKey_LeftAlt;
    case OIS::KC_RMENU:       return ImGuiKey_RightAlt;
    case OIS::KC_LWIN:        return ImGuiKey_LeftSuper;
    case OIS::KC_RWIN:        return ImGuiKey_RightSuper;
    case OIS::KC_PAUSE:       return ImGuiKey_Pause;
    default:                  return ImGuiKey_None;
    }
}

// --------------------------------------------------------------------------
// Init
// --------------------------------------------------------------------------

void OgreImGui::Init()
{
    m_imgui_overlay = std::unique_ptr<Ogre::ImGuiOverlay>(new Ogre::ImGuiOverlay());

    ImGuiIO& io = ImGui::GetIO();

    // Disable 'imgui.ini' - we don't need to persist window positions.
    io.IniFilename = nullptr;

    // Load font data from OGRE resource system into persistent member buffers.
    const char* fonts_rg = ContentManager::ResourcePack::FONTS.resource_group_name;

    auto loadFontData = [&](const char* filename, std::vector<uint8_t>& out)
    {
        Ogre::DataStreamPtr stream =
            Ogre::ResourceGroupManager::getSingleton().openResource(filename, fonts_rg);
        out.resize(stream->size());
        stream->read(out.data(), out.size());
    };

    loadFontData("MavenPro-Regular.ttf", m_font_data_maven);
    loadFontData("MavenPro-Bold.ttf",    m_font_data_bold);
    loadFontData("fa-solid-900.ttf",     m_font_data_fa);

    // FontAwesome 6 icon glyph range
    static const ImWchar fa_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

    // Load Maven Pro Regular at multiple sizes and merge FontAwesome icons into each.
    // FontDataOwnedByAtlas=false keeps the data in our member vectors.
    auto addFontWithIcons = [&](float size_px, ImFont** out_font)
    {
        ImFontConfig cfg;
        cfg.FontDataOwnedByAtlas = false;
        *out_font = io.Fonts->AddFontFromMemoryTTF(
            m_font_data_maven.data(), (int)m_font_data_maven.size(), size_px, &cfg);

        ImFontConfig fa_cfg;
        fa_cfg.MergeMode             = true;
        fa_cfg.PixelSnapH            = true;
        fa_cfg.GlyphMinAdvanceX      = size_px; // monospaced icons
        fa_cfg.FontDataOwnedByAtlas  = false;
        io.Fonts->AddFontFromMemoryTTF(
            m_font_data_fa.data(), (int)m_font_data_fa.size(), size_px, &fa_cfg, fa_ranges);
    };

    // Load Maven Pro Bold with FontAwesome icons merged in
    auto addBoldFontWithIcons = [&](float size_px, ImFont** out_font)
    {
        ImFontConfig cfg;
        cfg.FontDataOwnedByAtlas = false;
        *out_font = io.Fonts->AddFontFromMemoryTTF(
            m_font_data_bold.data(), (int)m_font_data_bold.size(), size_px, &cfg);

        ImFontConfig fa_cfg;
        fa_cfg.MergeMode            = true;
        fa_cfg.PixelSnapH           = true;
        fa_cfg.GlyphMinAdvanceX     = size_px;
        fa_cfg.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF(
            m_font_data_fa.data(), (int)m_font_data_fa.size(), size_px, &fa_cfg, fa_ranges);
    };

    addFontWithIcons(13.0f, &FontSmall);
    addFontWithIcons(16.0f, &FontRegular);
    addFontWithIcons(20.0f, &FontMedium);
    addFontWithIcons(32.0f, &FontLarge);
    addFontWithIcons(52.0f, &FontIconHuge);
    addBoldFontWithIcons(20.0f, &FontBold);
    addBoldFontWithIcons(28.0f, &FontBoldLarge);

    // The first font in the list becomes the default.
    // Promote FontRegular (16px) to be the default by using io.FontDefault.
    io.FontDefault = FontRegular;

    // Start rendering
    m_imgui_overlay->setZOrder(300);
    m_imgui_overlay->initialise(); // Build font texture atlas
    m_imgui_overlay->show();
}

// --------------------------------------------------------------------------
// Mouse input
// --------------------------------------------------------------------------

void OgreImGui::InjectMouseMoved(const OIS::MouseEvent& arg)
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent((float)arg.state.X.abs, (float)arg.state.Y.abs);
    io.AddMouseWheelEvent(0.0f, Ogre::Math::Clamp((float)arg.state.Z.rel / 120.0f, -1.0f, 1.0f));
}

void OgreImGui::SetMouseButtonState(OIS::MouseButtonID id, bool down)
{
    ImGuiIO& io = ImGui::GetIO();
    if (id >= 0 && id < 5)
        io.AddMouseButtonEvent((int)id, down);
}

void OgreImGui::ResetAllMouseButtons()
{
    ImGuiIO& io = ImGui::GetIO();
    for (int i = 0; i < 5; i++)
        io.AddMouseButtonEvent(i, false);
}

// --------------------------------------------------------------------------
// Keyboard input
// --------------------------------------------------------------------------

void OgreImGui::InjectKeyPressed(const OIS::KeyEvent& arg)
{
    ImGuiIO& io = ImGui::GetIO();

    ImGuiKey key = OisKeyToImGuiKey(arg.key);
    if (key != ImGuiKey_None)
    {
        io.AddKeyEvent(key, true);
        // Mirror modifier state for Ctrl/Shift/Alt/Super
        if (key == ImGuiKey_LeftCtrl  || key == ImGuiKey_RightCtrl)  io.AddKeyEvent(ImGuiMod_Ctrl,  true);
        if (key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift) io.AddKeyEvent(ImGuiMod_Shift, true);
        if (key == ImGuiKey_LeftAlt   || key == ImGuiKey_RightAlt)   io.AddKeyEvent(ImGuiMod_Alt,   true);
        if (key == ImGuiKey_LeftSuper || key == ImGuiKey_RightSuper) io.AddKeyEvent(ImGuiMod_Super, true);
    }

    if (arg.text > 0)
        io.AddInputCharacter((unsigned int)arg.text);
}

void OgreImGui::InjectKeyReleased(const OIS::KeyEvent& arg)
{
    ImGuiIO& io = ImGui::GetIO();

    ImGuiKey key = OisKeyToImGuiKey(arg.key);
    if (key != ImGuiKey_None)
    {
        io.AddKeyEvent(key, false);
        if (key == ImGuiKey_LeftCtrl  || key == ImGuiKey_RightCtrl)  io.AddKeyEvent(ImGuiMod_Ctrl,  false);
        if (key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift) io.AddKeyEvent(ImGuiMod_Shift, false);
        if (key == ImGuiKey_LeftAlt   || key == ImGuiKey_RightAlt)   io.AddKeyEvent(ImGuiMod_Alt,   false);
        if (key == ImGuiKey_LeftSuper || key == ImGuiKey_RightSuper) io.AddKeyEvent(ImGuiMod_Super, false);
    }
}

// --------------------------------------------------------------------------
// Render queue listener
// --------------------------------------------------------------------------

void OgreImGui::renderQueueStarted(Ogre::uint8 queueGroupId,
        const Ogre::String& invocation, bool& skipThisInvocation)
{
    // Shamelessly copy-pasted from `Ogre::OverlaySystem::renderQueueStarted()`
    if(queueGroupId == Ogre::RENDER_QUEUE_OVERLAY)
    {
        Ogre::Viewport* vp = Ogre::Root::getSingletonPtr()->getRenderSystem()->_getViewport();
        if(vp != NULL)
        {
            Ogre::SceneManager* sceneMgr = vp->getCamera()->getSceneManager();
            if (vp->getOverlaysEnabled() && sceneMgr->_getCurrentRenderStage() != Ogre::SceneManager::IRS_RENDER_TO_TEXTURE)
            {
                // Checking `sceneMgr->_getCurrentRenderStage() == Ogre::SceneManager::IRS_RENDER_TO_TEXTURE`)
                // doesn't do the trick if the RTT is updated by calling `Ogre::RenderTarget::update()` by hand,
                // which we do frequently.
                // To compensate, we also check if the active viewport matches our screen viewport.
                Ogre::Viewport* vp_target = App::GetAppContext()->GetViewport();
                if (vp == vp_target)
                {
                    m_imgui_overlay->_findVisibleObjects(vp->getCamera(), sceneMgr->getRenderQueue(), vp);
                }
            }
        }
    }
}
