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

#include "Application.h"
#include "GameContext.h"
#include "GUIManager.h"
#include "GUIUtils.h"
#include "Language.h"

#include <imgui.h>
#include <rapidjson/document.h>
#include <vector>
#include <fmt/core.h>

#ifdef USE_CURL
#   include <curl/curl.h>
#   include <curl/easy.h>
#endif //USE_CURL

#if defined(_MSC_VER) && defined(GetObject) // This MS Windows macro from <wingdi.h> (Windows Kit 8.1) clashes with RapidJSON
#   undef GetObject
#endif

using namespace RoR;
using namespace GUI;

#if defined(USE_CURL)

static size_t CurlWriteFunc(void *ptr, size_t size, size_t nmemb, std::string* data)
{
	data->append((char*)ptr, size * nmemb);
	return size * nmemb;
}

void GetResources(std::string portal_url)
{
	std::string repolist_url = portal_url + "/resources";
	std::string response_payload;
	std::string response_header;
	long        response_code = 0;

	CURL *curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL,				repolist_url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,	CurlWriteFunc);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA,		&response_payload);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA,		&response_header);

	curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

	curl_easy_cleanup(curl);
	curl = nullptr;

	if (response_code != 200)
	{
		Ogre::LogManager::getSingleton().stream()
			<< "[RoR|Repository] Failed to retrieve repolist; HTTP status code: " << response_code;
		App::GetGameContext()->PushMessage(
			Message(MSG_NET_REFRESH_REPOLIST_FAILURE, _LC("RepositorySelector", "Connection error. Please check your connection.")));
		return;
	}

	rapidjson::Document j_data_doc;
	j_data_doc.Parse(response_payload.c_str());
	if (j_data_doc.HasParseError() || !j_data_doc.IsObject())
	{
		Ogre::LogManager::getSingleton().stream()
			<< "[RoR|Repository] Error parsing repolist JSON";
		App::GetGameContext()->PushMessage(
			Message(MSG_NET_REFRESH_REPOLIST_FAILURE, _LC("RepositorySelector", "Received malformed data. Please try again.")));
		return;
	}

	GUI::ResourcesCollection* cdata_ptr = new GUI::ResourcesCollection();
	GUI::ResourcesCollection& cdata = *cdata_ptr;

	std::vector<GUI::ResourceItem> resc;
	rapidjson::Value& j_resp_body = j_data_doc["data"];
	size_t num_rows = j_resp_body.GetArray().Size();
	resc.resize(num_rows);
	for (size_t i = 0; i < num_rows; i++)
	{
		rapidjson::Value& j_row = j_resp_body[static_cast<rapidjson::SizeType>(i)];

		resc[i].title = j_row["title"].GetString();
		resc[i].tag_line = j_row["tag_line"].GetString();
		resc[i].resource_id = j_row["resource_id"].GetInt();
		resc[i].resource_id = j_row["user_id"].GetInt();
		resc[i].resource_id = j_row["download_count"].GetInt();
		resc[i].resource_id = j_row["pub_date"].GetInt();
		resc[i].resource_id = j_row["last_update"].GetInt();
		resc[i].resource_id = j_row["resource_category_id"].GetInt();
	}

	cdata.items = resc;

	App::GetGameContext()->PushMessage(
		Message(MSG_NET_REFRESH_REPOLIST_SUCCESS, (void*)cdata_ptr));
}

GUI::ResourcesCategories GetResourceCategories(std::string portal_url)
{
	std::string repolist_url = portal_url + "/resource-categories";
	std::string response_payload;
	std::string response_header;
	long        response_code = 0;

	CURL *curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, repolist_url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteFunc);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_payload);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_header);

	curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

	curl_easy_cleanup(curl);
	curl = nullptr;

	// todo: finish later..
}
#endif // defined(USE_CURL)

RepositorySelector::RepositorySelector()
{}

RepositorySelector::~RepositorySelector()
{}

void RepositorySelector::RepositorySelector::Draw()
{
	const float TABS_BOTTOM_PADDING = 4.f;
	const float CONTENT_TOP_PADDING = 4.f;
	const float BUTTONS_EXTRA_SPACE = 6.f;
	const float TABLE_PADDING_LEFT = 4.f;

	GUIManager::GuiTheme const& theme = App::GetGuiManager()->GetTheme();

	ImGui::SetNextWindowSize(ImVec2((ImGui::GetIO().DisplaySize.x / 1.4), (ImGui::GetIO().DisplaySize.y / 1.2)), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPosCenter(ImGuiCond_Appearing);
	ImGuiWindowFlags win_flags = ImGuiWindowFlags_NoCollapse;
	bool keep_open = true;
	ImGui::Begin(_LC("RepositorySelector", "Rigs of Rods Repository"), &keep_open, win_flags);

	const char* draw_label_text = nullptr;
	ImVec4      draw_label_color;

	if (!m_data.items.empty())
	{
		draw_label_text = m_repolist_msg.c_str();
		draw_label_color = m_repolist_msg_color;
	}

	if (m_draw)
	{
		// Category dropdown

		// Space
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + TABS_BOTTOM_PADDING);
		ImGui::Separator();
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + CONTENT_TOP_PADDING);

		// Scroll area
		const float table_height = ImGui::GetWindowHeight()
			- ((2.f * ImGui::GetStyle().WindowPadding.y) + (3.f * ImGui::GetItemsLineHeightWithSpacing())
				+ TABS_BOTTOM_PADDING + CONTENT_TOP_PADDING - ImGui::GetStyle().ItemSpacing.y);
		ImGui::BeginChild("scrolling", ImVec2(0.f, table_height), false);

		const float table_width = ImGui::GetWindowContentRegionWidth();
		ImGui::Columns(3, "repo-selector-columns");       // Col #0: Icon
		ImGui::SetColumnOffset(1, 0.09f * table_width);   // Col #1: Description
		ImGui::SetColumnOffset(2, 0.90f * table_width);   // Col #2: Info
		ImGui::SetColumnOffset(3, 0.09f * table_width);   // Col #3: Download button
		// Draw table body
		for (int i = 0; i < m_data.items.size(); i++)
		{
			ImGui::PushID(i);

			ImGui::Text(" ");   // image, todo later 
			ImGui::NextColumn();

			ImGui::TextColored(theme.value_blue_text_color, "%s", m_data.items[i].title.c_str());
			// todo for loop authors
			ImGui::TextDisabled("%s: ", _LC("RepositorySelector", "Author(s)"));
			ImGui::Text("%s", m_data.items[i].tag_line.c_str());

			ImGui::NextColumn();
			ImGui::Text(" ");   // download/update button, todo later

			ImGui::NextColumn();

			ImGui::Separator();

			ImGui::PopID();
		}

		ImGui::EndChild();
	}

	if (draw_label_text != nullptr)
	{
		const ImVec2 label_size = ImGui::CalcTextSize(draw_label_text);
		ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.f) - (label_size.x / 2.f));
		ImGui::SetCursorPosY((ImGui::GetWindowSize().y / 2.f) - (label_size.y / 2.f));
		ImGui::TextColored(draw_label_color, "%s", draw_label_text);
	}

	App::GetGuiManager()->RequestGuiCaptureKeyboard(ImGui::IsWindowHovered());
	ImGui::End();
	if (!keep_open)
	{
		this->SetVisible(false);
	}
}

void RepositorySelector::Refresh()
{
#if defined(USE_CURL)
	m_draw = false;
	m_data.items.clear();
	//m_data.categories.clear();
	m_repolist_msg = _LC("RepositorySelector", "Please wait...");
	m_repolist_msg_color = App::GetGuiManager()->GetTheme().in_progress_text_color;
	std::packaged_task<void(std::string)> task(GetResources);
	// PASS PARAMS
	std::thread(std::move(task), App::mp_api_url->GetStr()).detach();
#endif // defined(USE_CURL)
}

void RepositorySelector::Update(ResourcesCollection* data)
{
	m_data = *data;
	m_draw = true;
	if (m_data.items.empty())
	{
		m_repolist_msg = _LC("RepositorySelector", "Sorry, the repository isn't available. Try again later.");
		m_repolist_msg_color = App::GetGuiManager()->GetTheme().no_entries_text_color;
	}
	else
	{
		m_repolist_msg = "";
	}
}

void RepositorySelector::ShowError(std::string const& msg)
{
	m_repolist_msg = msg;
	m_repolist_msg_color = App::GetGuiManager()->GetTheme().error_text_color;
	m_draw = false;
}

void RepositorySelector::SetVisible(bool v)
{
	m_is_visible = v;
	if (v && m_data.items.size() == 0)
	{
		this->Refresh();
	}
	else if (!v && (App::app_state->GetEnum<AppState>() == AppState::MAIN_MENU))
	{
		App::GetGuiManager()->SetVisible_GameMainMenu(true);
	}
}
