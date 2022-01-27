/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2013-2022 Petr Ohlidal

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

/// @file   GUI_LoginBox.cpp
/// @author Rafael Galvan
/// @date   01/2022

#include "GUI_LoginBox.h"

#include "Application.h"
#include "GUIManager.h"
#include "AppContext.h"
#include "Language.h"
#include "RoRVersion.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <rapidjson/document.h>
#include <vector>
#include <fmt/core.h>
#include <stdio.h>

#ifdef USE_URL
#   include <curl/curl.h>
#   include <curl/easy.h>
#endif

using namespace RoR;
using namespace GUI;

#if defined(USE_CURL)

static size_t CurlWriteFunc(void* ptr, size_t size, size_t nmemb, std::string* data)
{
    data->append((char*)ptr, size * nmemb);
    return size * nmemb;
}

void GetUser()
{
    std::string user_agent = fmt::format("{}/{}", "Rigs of Rods Client", ROR_VERSION_STRING);
    std::string response_payload;
    std::string response_header;
    long response_code = 0;

    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, "http://dev.api.rigsofrods.org/auth");


    // Authorization: Bearer <ACCESS_TOKEN>
}

void GetUserAvatar()
{

}

void GetUserProfileBanner()
{

}

void PostAuth(std::string login, std::string passwd)
{
    // !!!! DO NOT USE QUERY PARAMS - USE REQUEST BODY INSTEAD !!!!
    std::string user_agent = fmt::format("{}/{}", "Rigs of Rods Client", ROR_VERSION_STRING);
    std::string response_payload;
    std::string response_header;
    long response_code = 0;

    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, "http://dev.api.rigsofrods.org/auth"); // todo api url + endpoint
    //curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonstr.c_str()); // post request body
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteFunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_payload);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_header);

    curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    curl_easy_cleanup(curl);
    curl = nullptr;

    // non-standard see https://developer.mozilla.org/en-US/docs/Web/HTTP/Status/402
    // we will use for it indicating that 2fa is required
    if (response_code == 402) // not a failure
    {
        return;
    }
    else if (response_code == 401) // a failure
    {
        return;
    }
    else if (response_code != 200)
    {
        return;
    }

    // fetch user after, get token now

    rapidjson::Document j_data_doc;
    j_data_doc.Parse(response_payload.c_str());
}

#endif

LoginBox::LoginBox()
{}

LoginBox::~LoginBox()
{}

void LoginBox::Draw()
{
    GUIManager::GuiTheme const& theme = App::GetGuiManager()->GetTheme();

    ImGui::SetNextWindowContentWidth(300.f);
    ImGui::SetNextWindowPosCenter(ImGuiCond_Appearing);
    ImGuiWindowFlags win_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    bool keep_open = true;
    ImGui::Begin(_LC("Login", "Login"), &keep_open, win_flags);

    ImGui::Text(_LC("Login", "Your name or email address"));
    ImGui::InputText("##login", m_login.GetBuffer(), m_login.GetCapacity());
    ImGui::Text(_LC("Login", "Password"));
    ImGui::InputText("##password", m_passwd.GetBuffer(), m_passwd.GetCapacity(), ImGuiInputTextFlags_Password | ImGuiInputTextFlags_CharsNoBlank);
    ImGui::Checkbox("Remember me", &m_remember);
    ImGui::Button("Login");

    ImGui::End();
    if (!keep_open)
    {
        this->SetVisible(false);
    }
}

void LoginBox::SetVisible(bool visible)
{
    m_is_visible = visible;
    if (!visible && (App::app_state->getEnum<AppState>() == AppState::MAIN_MENU))
    {
        App::GetGuiManager()->SetVisible_GameMainMenu(true);
    }
}