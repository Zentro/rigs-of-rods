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

/// @file   GUI_RepositorySelector.h
/// @author Rafael Galvan
/// @date   04/2021

#pragma once

#include "Application.h"
#include "OgreImGui.h" // ImVec4

#include <future>
#include <memory>
#include <thread>
#include <vector>

namespace RoR {
namespace GUI {

struct ResourceCategories
{
	int				resource_category_id;
	std::string		title;
	std::string		description;
	int				resource_count;
	int				display_order;
};

struct ResourceItem
{
	int				resource_id;
	std::string		title;
	std::string		tag_line;
	int				user_id;
	int				download_count;
	int				pub_date;
	int				last_update;
	int				resource_category_id;
};

struct ResourcePagination {
	int				current_page;
	int				last_page;
	int				per_page;
	int				shown;
	int				total;
};

struct ResourcesCollection {
	std::vector<ResourceItem>			items;
	std::vector<ResourceCategories>		categories;
	//struct ResourcePagination			pagination;
};

class RepositorySelector
{
public:
	RepositorySelector();
	~RepositorySelector();

	void					SetVisible(bool v);
	bool					IsVisible() const { return m_is_visible; }
	void					Draw();
	void					Refresh();
	void					Update(ResourcesCollection* data);
	void					ShowError(std::string const& msg);
private:
	bool					m_is_visible = false;
	bool					m_draw = false;
	std::string				m_repolist_msg;
	ImVec4					m_repolist_msg_color;
	ResourcesCollection		m_data;

};

}// namespace GUI
}// namespace RoR

