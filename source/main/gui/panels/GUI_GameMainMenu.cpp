/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2013-2020 Petr Ohlidal

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
/// @author Petr Ohlidal
/// @date   06/2017

#include "GUI_GameMainMenu.h"
#include <fmt/format.h>

#include "Application.h"
#include "GameContext.h"
#include "GUIManager.h"
#include "GUIStyle.h"
#include "GUIUtils.h"
#include "IconsFontAwesome6.h"
#include "Language.h"
#include "OgreImGui.h"

#include "PlatformUtils.h"
#include "RoRVersion.h"
#include "RoRnet.h" // for version string

using namespace RoR;
using namespace GUI;

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void GameMainMenu::Draw()
{
    if (App::app_state->getEnum<AppState>() == AppState::MAIN_MENU)
    {
        this->DrawMainMenuTiles();
        this->DrawVersionBox();
        if (cache_updated)
            this->DrawNoticeBox();
    }
    else
    {
        this->DrawPauseMenuPanel();
    }
}

void GameMainMenu::CacheUpdatedNotice()
{
    cache_updated = true;
}

// ---------------------------------------------------------------------------
// BeamNG-style tile grid (main menu state only)
// ---------------------------------------------------------------------------

bool GameMainMenu::DrawTile(const char* icon, const std::string& label, int index)
{
    bool kb_hovered    = (m_kb_focus_index == index);
    bool enter_pressed = (m_kb_enter_index == index);

    ImGui::PushID(index);

    bool clicked    = ImGui::InvisibleButton("##tile", ImVec2(TILE_W, TILE_H));
    bool is_hovered = ImGui::IsItemHovered();

    ImVec2 tile_min = ImGui::GetItemRectMin();
    ImVec2 tile_max = ImGui::GetItemRectMax();
    ImDrawList* dl  = ImGui::GetWindowDrawList();

    // Square tile, no border - flat color, bright on hover
    if (is_hovered || kb_hovered)
    {
        dl->AddRectFilled(tile_min, tile_max,
            IM_COL32(30, 100, 200, 235));
    }
    else
    {
        dl->AddRectFilled(tile_min, tile_max,
            IM_COL32(28, 28, 28, 220));
    }

    ImFont* font_large  = App::GetGuiManager()->GetImGui().FontIconHuge;
    ImFont* font_medium = App::GetGuiManager()->GetImGui().FontBoldLarge;

    // Icon centered, vertically filling upper 60% of tile
    if (font_large)
    {
        ImGui::PushFont(font_large);
        ImVec2 icon_sz  = ImGui::CalcTextSize(icon);
        ImVec2 icon_pos = ImVec2(
            tile_min.x + (TILE_W - icon_sz.x) * 0.5f,
            tile_min.y + (TILE_H * 0.60f - icon_sz.y) * 0.5f);
        dl->AddText(icon_pos, IM_COL32(255, 255, 255, 230), icon);
        ImGui::PopFont();
    }

    // Label (bold font) centered in lower 30% of tile
    if (font_medium)
    {
        ImGui::PushFont(font_medium);
        ImVec2 lbl_sz  = ImGui::CalcTextSize(label.c_str());
        ImVec2 lbl_pos = ImVec2(
            tile_min.x + (TILE_W - lbl_sz.x) * 0.5f,
            tile_min.y + TILE_H * 0.72f);
        dl->AddText(lbl_pos, IM_COL32(255, 255, 255, 255), label.c_str());
        ImGui::PopFont();
    }

    ImGui::PopID();
    return clicked || enter_pressed;
}

void GameMainMenu::DrawMainMenuTiles()
{
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 screen_sz = io.DisplaySize;

    // Full-screen transparent window
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(screen_sz);
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (ImGui::Begin("##MainMenuBg", nullptr, flags))
    {
        this->HandleInputEvents(/* num_buttons set inside */0);

        // Build button list depending on savegame presence
        // NOTE: labels must be std::string - _LC() returns .c_str() of a temporary
        struct TileEntry { const char* icon; std::string label; };
        TileEntry tiles[10];
        int n = 0;

        bool has_autosave = FileExists(PathCombine(App::sys_savegames_dir->getStr(), "autosave.sav"));
        if (has_autosave)
            tiles[n++] = { ICON_FA_ROTATE_LEFT, _LC("MainMenu", "Resume game") };

        tiles[n++] = { ICON_FA_PLAY,        _LC("MainMenu", "Single player") };
        tiles[n++] = { ICON_FA_USERS,       _LC("MainMenu", "Multiplayer")   };
        tiles[n++] = { ICON_FA_FOLDER_OPEN, _LC("MainMenu", "Repository")    };
        tiles[n++] = { ICON_FA_GEAR,        _LC("MainMenu", "Settings")      };
        tiles[n++] = { ICON_FA_GAMEPAD,     _LC("MainMenu", "Controls")      };
        tiles[n++] = { ICON_FA_CIRCLE_INFO, _LC("MainMenu", "About")         };
        tiles[n++] = { ICON_FA_POWER_OFF,   _LC("MainMenu", "Exit game")     };

        // Center the grid on screen
        const float grid_w = TILE_COLS * TILE_W + (TILE_COLS - 1) * TILE_GAP;
        int rows = (n + TILE_COLS - 1) / TILE_COLS;
        const float grid_h = rows * TILE_H + (rows - 1) * TILE_GAP;

        float start_x = (screen_sz.x - grid_w) * 0.5f;
        float start_y = (screen_sz.y - grid_h) * 0.5f;

        ImGui::SetCursorPos(ImVec2(start_x, start_y));

        // Handle keyboard navigation (tile count known now)
        m_kb_enter_index = -1;
        {
            bool kb_ok = !App::GetGuiManager()->IsGuiCaptureKeyboardRequested()
                      || ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
            if (kb_ok)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
                    m_kb_focus_index = (m_kb_focus_index < TILE_COLS) ? (n - 1) : (m_kb_focus_index - TILE_COLS);
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
                    m_kb_focus_index = (m_kb_focus_index >= n - TILE_COLS) ? 0 : (m_kb_focus_index + TILE_COLS);
                if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
                    m_kb_focus_index = (m_kb_focus_index <= 0) ? (n - 1) : (m_kb_focus_index - 1);
                if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
                    m_kb_focus_index = (m_kb_focus_index >= n - 1) ? 0 : (m_kb_focus_index + 1);
                if (ImGui::IsKeyPressed(ImGuiKey_Enter))
                    m_kb_enter_index = m_kb_focus_index;
            }
        }

        // Draw tiles in row-major order
        for (int i = 0; i < n; i++)
        {
            int col = i % TILE_COLS;
            int row = i / TILE_COLS;
            float tx = start_x + col * (TILE_W + TILE_GAP);
            float ty = start_y + row * (TILE_H + TILE_GAP);
            ImGui::SetCursorPos(ImVec2(tx, ty));

            bool clicked = DrawTile(tiles[i].icon, tiles[i].label, i);
            if (!clicked) continue;

            // Map index back to action
            int action = i;
            int idx = 0;

            if (has_autosave && action == idx++)
            {
                App::GetGameContext()->PushMessage(Message(MSG_SIM_LOAD_SAVEGAME_REQUESTED, "autosave.sav"));
                this->SetVisible(false);
            }
            else if (action == idx++)
            {
                this->SetVisible(false);
                RoR::Message m(MSG_GUI_OPEN_SELECTOR_REQUESTED);
                m.payload = reinterpret_cast<void*>(new LoaderType(LT_Terrain));
                App::GetGameContext()->PushMessage(m);
            }
            else if (action == idx++)
            {
                App::GetGuiManager()->MultiplayerSelector.SetVisible(true);
                this->SetVisible(false);
            }
            else if (action == idx++)
            {
                App::GetGuiManager()->RepositorySelector.SetVisible(true);
                this->SetVisible(false);
            }
            else if (action == idx++)
            {
                App::GetGuiManager()->GameSettings.SetVisible(true);
                this->SetVisible(false);
            }
            else if (action == idx++)
            {
                App::GetGuiManager()->GameControls.SetVisible(true);
                this->SetVisible(false);
            }
            else if (action == idx++)
            {
                App::GetGuiManager()->GameAbout.SetVisible(true);
                this->SetVisible(false);
            }
            else // Exit game
            {
                App::GetGameContext()->PushMessage(Message(MSG_APP_SHUTDOWN_REQUESTED));
                this->SetVisible(false);
            }
        }

        App::GetGuiManager()->RequestGuiCaptureKeyboard(
            ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows));
    }

    if (!this->IsVisible())
        cache_updated = false;

    ImGui::End();
    ImGui::PopStyleVar();  // WindowPadding
    ImGui::PopStyleColor(); // WindowBg
    m_kb_enter_index = -1;
}

// ---------------------------------------------------------------------------
// Pause menu — xemu-inspired dark flat style
// ---------------------------------------------------------------------------

void GameMainMenu::DrawPauseMenuPanel()
{
    bool is_mp = (App::mp_state->getEnum<MpState>() == MpState::CONNECTED);

    struct PauseEntry { const char* icon; std::string label; int id; };
    PauseEntry entries[5];
    int n = 0;
    entries[n++] = { ICON_FA_PLAY,        _LC("MainMenu", "Resume"),     0 };
    entries[n++] = { ICON_FA_FOLDER_OPEN, _LC("MainMenu", "Repository"), 1 };
    entries[n++] = { ICON_FA_GAMEPAD,     _LC("MainMenu", "Controls"),   2 };
    entries[n++] = { ICON_FA_HOUSE,       _L("Return to menu"),          3 };
    entries[n++] = { ICON_FA_POWER_OFF,   _LC("MainMenu", "Exit game"),  4 };

    HandleInputEvents(n);

    ImFont* font = App::GetGuiManager()->GetImGui().FontBold; // 20px bold + icons
    const float BTN_H  = 42.f;
    const float W      = 300.f;
    const float total_h = (float)n * BTN_H;

    ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(
        ImVec2((display.x - W) * 0.5f, (display.y - total_h) * 0.5f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(W, total_h), ImGuiCond_Always);

    // Only override hover/active + layout — keep everything else default
    StylePush ws;
    ws.Col(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.42f, 0.82f, 1.00f))  // solid primary blue
      .Col(ImGuiCol_ButtonActive,  ImVec4(0.10f, 0.33f, 0.68f, 1.00f))  // slightly darker
      .Var(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f))            // left-align
      .Var(ImGuiStyleVar_FrameRounding,   0.f)                           // no radius
      .Var(ImGuiStyleVar_FramePadding,    ImVec2(12.f, 0.f))             // no vertical padding
      .Var(ImGuiStyleVar_ItemSpacing,     ImVec2(0.f, 0.f))              // no gap between buttons
      .Var(ImGuiStyleVar_WindowPadding,   ImVec2(0.f, 0.f));

    ImGuiWindowFlags wflags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (font) ImGui::PushFont(font);

    m_kb_enter_index = -1;
    if (ImGui::Begin("##PauseMenu", nullptr, wflags))
    {
        for (int i = 0; i < n; i++)
        {
            // Separator line before Exit (last entry)
            if (i == n - 1)
            {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
            }

            std::string lbl = fmt::format("   {}   {}", entries[i].icon, entries[i].label);
            bool clicked = ImGui::Button(lbl.c_str(), ImVec2(W, BTN_H));
            bool activated = clicked || (m_kb_enter_index == entries[i].id);
            if (!activated) continue;

            switch (entries[i].id)
            {
            case 0:
                App::GetGameContext()->PushMessage(Message(MSG_SIM_UNPAUSE_REQUESTED));
                this->SetVisible(false);
                break;
            case 1:
                App::GetGuiManager()->RepositorySelector.SetVisible(true);
                this->SetVisible(false);
                break;
            case 2:
                App::GetGuiManager()->GameControls.SetVisible(true);
                this->SetVisible(false);
                break;
            case 3:
                App::GetGameContext()->PushMessage(Message(MSG_SIM_UNLOAD_TERRN_REQUESTED));
                if (is_mp)
                    App::GetGameContext()->PushMessage(Message(MSG_NET_DISCONNECT_REQUESTED));
                App::GetGameContext()->PushMessage(Message(MSG_GUI_OPEN_MENU_REQUESTED));
                break;
            case 4:
                App::GetGameContext()->PushMessage(Message(MSG_APP_SHUTDOWN_REQUESTED));
                this->SetVisible(false);
                break;
            }
        }
    }

    if (font) ImGui::PopFont();

    if (!this->IsVisible())
        cache_updated = false;

    App::GetGuiManager()->RequestGuiCaptureKeyboard(
        ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows));

    if (App::GetGuiManager()->IsGuiCaptureKeyboardRequested() &&
        ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        this->SetVisible(false);
    }

    ImGui::End();
    m_kb_enter_index = -1;
}

// ---------------------------------------------------------------------------
// Version & notice boxes
// ---------------------------------------------------------------------------

void GameMainMenu::DrawVersionBox()
{
    const float margin = ImGui::GetIO().DisplaySize.y / 30.f;
    std::string game_ver   = fmt::format("{}: {}", _LC("MainMenu", "Game version"), ROR_VERSION_STRING);
    std::string rornet_ver = fmt::format("{}: {}", _LC("MainMenu", "Net. protocol"), RORNET_VERSION);
    float text_w = std::max(
        ImGui::CalcTextSize(game_ver.c_str()).x,
        ImGui::CalcTextSize(rornet_ver.c_str()).x);
    ImVec2 box_size(
        (2 * ImGui::GetStyle().WindowPadding.y) + text_w,
        (2 * ImGui::GetStyle().WindowPadding.y) + (2 * ImGui::GetTextLineHeight()));
    ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize - (box_size + ImVec2(margin, margin)));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, WINDOW_BG_COLOR);
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoInputs;
    if (ImGui::Begin(_LC("MainMenu", "Version box"), nullptr, flags))
    {
        ImGui::Text("%s", game_ver.c_str());
        ImGui::Text("%s", rornet_ver.c_str());
        ImGui::End();
    }
    ImGui::PopStyleColor(1);
}

void GameMainMenu::DrawNoticeBox()
{
    Ogre::TexturePtr tex = FetchIcon("accept.png");

    const float margin = ImGui::GetIO().DisplaySize.y / 30.f;
    std::string game_ver   = fmt::format("{}: {}", _LC("MainMenu", "Game version"), ROR_VERSION_STRING);
    std::string rornet_ver = fmt::format("{}: {}", _LC("MainMenu", "Net. protocol"), RORNET_VERSION);
    std::string cache_ntc  = fmt::format("{}", _LC("MainMenu", "Cache updated"));
    float text_w = std::max(
        ImGui::CalcTextSize(game_ver.c_str()).x,
        ImGui::CalcTextSize(rornet_ver.c_str()).x);
    ImVec2 box_size(
        (2 * ImGui::GetStyle().WindowPadding.y) + text_w,
        (2 * ImGui::GetStyle().WindowPadding.y) + (4.5f * ImGui::GetTextLineHeight()));
    ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize - (box_size + ImVec2(margin, margin)));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, WINDOW_BG_COLOR);
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoInputs;
    if (ImGui::Begin(_LC("MainMenu", "Notice box"), nullptr, flags))
    {
        ImGui::Image(reinterpret_cast<ImTextureID>(tex->getHandle()), ImVec2(16, 16));
        ImGui::SameLine();
        ImGui::Text("%s", cache_ntc.c_str());
        ImGui::End();
    }
    ImGui::PopStyleColor(1);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

bool GameMainMenu::HighlightButton(const std::string& txt, ImVec2 btn_size, int index) const
{
    std::string button_txt = (m_kb_focus_index == index)
        ? fmt::format("--> {} <--", txt) : txt;
    return ImGui::Button(button_txt.c_str(), btn_size) || (m_kb_enter_index == index);
}

void GameMainMenu::HandleInputEvents(int num_buttons)
{
    bool kb_ok = !App::GetGuiManager()->IsGuiCaptureKeyboardRequested()
              || ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
    if (!kb_ok) return;

    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
        m_kb_focus_index = (m_kb_focus_index <= 0) ? (num_buttons - 1) : (m_kb_focus_index - 1);
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
        m_kb_focus_index = (m_kb_focus_index < (num_buttons - 1)) ? (m_kb_focus_index + 1) : 0;
    if (ImGui::IsKeyPressed(ImGuiKey_Enter))
        m_kb_enter_index = m_kb_focus_index;
}
