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

#include <vector>

namespace RoR {
namespace GUI {

struct RepositoryCat
{
	int				cid;
	std::string		title;
	std::string		description;
	int				resource_count;
	int				dorder;
};

struct RepositoryItem
{
	int				rid;
	std::string		title;
	std::string		tag_line;
	int				uid;
	int				download_count;
	int				last_update;
};

typedef std::vector<RepositoryCat> RepoCatVec;
typedef std::vector<RepositoryItem> RepoItemVec;

class RepositorySelector
{
public:
	void SetVisible(bool v);
	bool IsVisible() const { return m_is_visible; }

	void Draw();

private:
	bool m_is_visible = false;

	RepoCatVec		m_repocat;
	RepoItemVec		m_repolist;

};

}// namespace GUI
}// namespace RoR

