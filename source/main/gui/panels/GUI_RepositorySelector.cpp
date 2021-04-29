/*
	This source file is part of Rigs of Rods

	Copyright 2005-2012 Pierre-Michel Ricordel
	Copyright 2007-2012 Thomas Fischer
	Copyright 2013-2021 Petr Ohlidal

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

/// @file   GUI_RepositorySelector.cpp
/// @author Rafael Galvan
/// @date   04/2021

#include "GUI_RepositorySelector.h"

#include "GUIManager.h"
#include "Utils.h"
#include "Language.h"

using namespace RoR;
using namespace GUI;

void FetchRepo() 
{

}

void RepositorySelector::Draw()
{
	GUIManager::GuiTheme const& theme = App::GetGuiManager()->GetTheme();

	ImGui::SetNextWindowSize(ImVec2((ImGui::GetIO().DisplaySize.x / 1.4), (ImGui::GetIO().DisplaySize.y / 1.2)), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPosCenter(ImGuiCond_Appearing);
	ImGuiWindowFlags win_flags = ImGuiWindowFlags_NoCollapse;
	bool keep_open = true;
	ImGui::Begin(_LC("RepositorySelector", "Rigs of Rods Repository"), &keep_open, win_flags);

	

	App::GetGuiManager()->RequestGuiCaptureKeyboard(ImGui::IsWindowHovered());
	ImGui::End();
	if (!keep_open)
	{
		this->SetVisible(false);
	}
}

void RepositorySelector::SetVisible(bool v)
{
	m_is_visible = v;
	if (!v && (App::app_state->GetEnum<AppState>() == AppState::MAIN_MENU))
	{
		App::GetGuiManager()->SetVisible_GameMainMenu(true);
	}
}
