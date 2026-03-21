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

#pragma once

#include <imgui.h>
#include <string>

namespace RoR {
namespace GUI {

class GameMainMenu
{
public:
    // Keyboard focus tracking (for main list-style pause menu)
    const float   WINDOW_WIDTH     = 200.f;
    const ImVec4  WINDOW_BG_COLOR  = ImVec4(0.1f, 0.1f, 0.1f, 0.8f);
    const ImVec4  BUTTON_BG_COLOR  = ImVec4(0.25f, 0.25f, 0.24f, 0.6f);
    const ImVec2  BUTTON_PADDING   = ImVec2(4.f, 6.f);

    // Main menu tile sizes
    const float   TILE_W           = 280.f;
    const float   TILE_H           = 230.f;
    const float   TILE_GAP         = 10.f;
    const int     TILE_COLS        = 4;

    inline bool   IsVisible() const                { return m_is_visible; }
    inline void   SetVisible(bool v)               { m_is_visible = v; m_kb_focus_index = -1; }
    void          Draw();
    void          CacheUpdatedNotice();

private:
    void          DrawMainMenuTiles();      // BeamNG-style tile grid (main menu state)
    void          DrawPauseMenuPanel();     // Compact list (simulation state)
    void          DrawVersionBox();
    void          DrawNoticeBox();

    // Tile drawing helper: returns true if the tile was clicked
    bool          DrawTile(const char* icon, const std::string& label, int index);

    // Pause menu helpers
    bool          HighlightButton(const std::string& text, ImVec2 btn_size, int index) const;
    void          HandleInputEvents(int num_buttons);

    bool          m_is_visible = false;
    int           m_kb_focus_index = -1;
    int           m_kb_enter_index = -1;
    bool          cache_updated = false;
};

} // namespace GUI
} // namespace RoR
