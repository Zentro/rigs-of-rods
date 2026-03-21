/*
    This source file is part of Rigs of Rods

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

/// @file   GUIStyle.h
/// @brief  Scoped, non-global Dear ImGui style system inspired by xemu.
///
/// Usage — apply a pre-built style scope for the duration of a block:
///
///     {
///         auto s = RoR::StylePush::DarkWindow();
///         ImGui::Begin("##foo", nullptr, flags);
///         // ...
///         ImGui::End();
///     }  // s destructs here, PushStyleColor/Var are popped automatically
///
/// Or compose custom scopes with the builder:
///
///     auto s = RoR::StylePush()
///         .Col(ImGuiCol_WindowBg, RoR::UIColor::BG_PANEL)
///         .Var(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
///
/// Pop explicitly (e.g. to pop before EndTabBar) with s.Pop().

#pragma once

#include <imgui.h>

namespace RoR {

// ---------------------------------------------------------------------------
// Color palette — xemu-inspired dark theme with blue accent
// ---------------------------------------------------------------------------
namespace UIColor {

// Background tiers (darkest → lightest)
constexpr ImVec4 BG_VOID    = { 0.05f, 0.05f, 0.05f, 1.00f }; // deepest
constexpr ImVec4 BG_DARK    = { 0.08f, 0.08f, 0.08f, 1.00f }; // sidebar
constexpr ImVec4 BG_PANEL   = { 0.11f, 0.11f, 0.11f, 0.97f }; // window body
constexpr ImVec4 BG_ITEM    = { 0.16f, 0.16f, 0.16f, 1.00f }; // input bg, child bg
constexpr ImVec4 BG_ITEM_HV = { 0.20f, 0.20f, 0.20f, 1.00f }; // item hovered

// Accent — blue
constexpr ImVec4 ACCENT          = { 0.14f, 0.42f, 0.82f, 1.00f };
constexpr ImVec4 ACCENT_HOVER    = { 0.20f, 0.53f, 0.95f, 1.00f };
constexpr ImVec4 ACCENT_ACTIVE   = { 0.10f, 0.33f, 0.68f, 1.00f };
constexpr ImVec4 ACCENT_SUBTLE   = { 0.14f, 0.42f, 0.82f, 0.18f }; // tinted overlay
constexpr ImVec4 ACCENT_MID      = { 0.14f, 0.42f, 0.82f, 0.35f }; // selected

// Flat / borderless buttons (transparent base, tinted on hover)
constexpr ImVec4 BTN_BG      = { 0.00f, 0.00f, 0.00f, 0.00f };
constexpr ImVec4 BTN_HV      = { 0.14f, 0.42f, 0.82f, 0.18f };
constexpr ImVec4 BTN_ACT     = { 0.14f, 0.42f, 0.82f, 0.35f };

// Selected sidebar nav item
constexpr ImVec4 NAVBTN_BG   = { 0.14f, 0.42f, 0.82f, 0.22f };
constexpr ImVec4 NAVBTN_HV   = { 0.14f, 0.42f, 0.82f, 0.38f };
constexpr ImVec4 NAVBTN_ACT  = { 0.14f, 0.42f, 0.82f, 0.55f };

// Borders
constexpr ImVec4 BORDER       = { 0.22f, 0.22f, 0.22f, 1.00f };
constexpr ImVec4 BORDER_NONE  = { 0.00f, 0.00f, 0.00f, 0.00f };
constexpr ImVec4 SEPARATOR    = { 0.25f, 0.25f, 0.25f, 1.00f };

// Text
constexpr ImVec4 TEXT         = { 0.93f, 0.93f, 0.93f, 1.00f };
constexpr ImVec4 TEXT_DIM     = { 0.55f, 0.55f, 0.55f, 1.00f };

} // namespace UIColor


// ---------------------------------------------------------------------------
// StylePush — RAII scoped style builder
// ---------------------------------------------------------------------------
struct StylePush
{
    int num_colors = 0;
    int num_vars   = 0;

    StylePush() = default;
    StylePush(const StylePush&) = delete;
    StylePush& operator=(const StylePush&) = delete;

    // Move: zero out source so its destructor doesn't pop
    StylePush(StylePush&& o) noexcept
        : num_colors(o.num_colors), num_vars(o.num_vars)
    { o.num_colors = 0; o.num_vars = 0; }

    StylePush& Col(ImGuiCol idx, const ImVec4& col)
    { ImGui::PushStyleColor(idx, col); ++num_colors; return *this; }

    StylePush& Var(ImGuiStyleVar idx, float val)
    { ImGui::PushStyleVar(idx, val); ++num_vars; return *this; }

    StylePush& Var(ImGuiStyleVar idx, const ImVec2& val)
    { ImGui::PushStyleVar(idx, val); ++num_vars; return *this; }

    /// Explicit pop — use when you need to pop before scope exit
    /// (e.g. before ImGui::EndTabBar which must match BeginTabBar).
    void Pop()
    {
        if (num_colors > 0) { ImGui::PopStyleColor(num_colors); num_colors = 0; }
        if (num_vars   > 0) { ImGui::PopStyleVar(num_vars);     num_vars   = 0; }
    }

    ~StylePush() { Pop(); }


    // -----------------------------------------------------------------------
    // Pre-built style profiles
    // -----------------------------------------------------------------------

    /// Dark, borderless floating window (pause menu, overlays).
    static StylePush DarkWindow()
    {
        StylePush s;
        s.Col(ImGuiCol_WindowBg,       UIColor::BG_PANEL)
         .Col(ImGuiCol_Border,         UIColor::BORDER_NONE)
         .Var(ImGuiStyleVar_WindowBorderSize, 0.f)
         .Var(ImGuiStyleVar_WindowRounding,   4.f)
         .Var(ImGuiStyleVar_WindowPadding,    ImVec2(0.f, 0.f));
        return s;
    }

    /// Dark borderless inner child (sidebar panels, content areas).
    static StylePush DarkChild()
    {
        StylePush s;
        s.Col(ImGuiCol_ChildBg, UIColor::BG_DARK)
         .Col(ImGuiCol_Border,  UIColor::BORDER_NONE)
         .Var(ImGuiStyleVar_ChildBorderSize, 0.f)
         .Var(ImGuiStyleVar_ChildRounding,   0.f);
        return s;
    }

    /// Flat, borderless button — transparent bg, accent tint on hover.
    static StylePush FlatButton()
    {
        StylePush s;
        s.Col(ImGuiCol_Button,        UIColor::BTN_BG)
         .Col(ImGuiCol_ButtonHovered, UIColor::BTN_HV)
         .Col(ImGuiCol_ButtonActive,  UIColor::BTN_ACT)
         .Var(ImGuiStyleVar_FrameBorderSize, 0.f)
         .Var(ImGuiStyleVar_FrameRounding,   0.f)
         .Var(ImGuiStyleVar_FramePadding,    ImVec2(12.f, 9.f));
        return s;
    }

    /// Sidebar nav button — selected variant uses solid accent tint.
    static StylePush NavButton(bool selected)
    {
        StylePush s;
        if (selected)
            s.Col(ImGuiCol_Button,        UIColor::NAVBTN_BG)
             .Col(ImGuiCol_ButtonHovered, UIColor::NAVBTN_HV)
             .Col(ImGuiCol_ButtonActive,  UIColor::NAVBTN_ACT);
        else
            s.Col(ImGuiCol_Button,        UIColor::BTN_BG)
             .Col(ImGuiCol_ButtonHovered, UIColor::BTN_HV)
             .Col(ImGuiCol_ButtonActive,  UIColor::BTN_ACT);
        s.Var(ImGuiStyleVar_FrameBorderSize, 0.f)
         .Var(ImGuiStyleVar_FrameRounding,   0.f)
         .Var(ImGuiStyleVar_FramePadding,    ImVec2(14.f, 10.f));
        return s;
    }

    /// Frame / input fields styled to match dark theme.
    static StylePush DarkFrame()
    {
        StylePush s;
        s.Col(ImGuiCol_FrameBg,        UIColor::BG_ITEM)
         .Col(ImGuiCol_FrameBgHovered, UIColor::BG_ITEM_HV)
         .Col(ImGuiCol_FrameBgActive,  UIColor::ACCENT_MID)
         .Col(ImGuiCol_Border,         UIColor::BORDER)
         .Var(ImGuiStyleVar_FrameRounding,   3.f)
         .Var(ImGuiStyleVar_FrameBorderSize, 1.f);
        return s;
    }
};

} // namespace RoR
