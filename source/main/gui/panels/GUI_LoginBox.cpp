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
#include "GameContext.h"
#include "GUIManager.h"
#include "AppContext.h"
#include "Language.h"
#include "RoRVersion.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <vector>
#include <fmt/core.h>
#include <stdio.h>

#ifdef USE_URL
#   include <curl/curl.h>
#   include <curl/easy.h>
#endif

#if defined(_MSC_VER) && defined(GetObject) // This MS Windows macro from <wingdi.h> (Windows Kit 8.1) clashes with RapidJSON
#   undef GetObject
#endif

using namespace RoR;
using namespace GUI;

#if defined(USE_CURL)

static size_t CurlWriteFunc(void* ptr, size_t size, size_t nmemb, std::string* data)
{
    data->append((char*)ptr, size * nmemb);
    return size * nmemb;
}

void PostAuthWithTfa(std::string login, std::string passwd, std::string provider, std::string code)
{
    rapidjson::Document j_request_body;
    j_request_body.SetObject();
    j_request_body.AddMember("login", rapidjson::StringRef(login.c_str()), j_request_body.GetAllocator());
    j_request_body.AddMember("password", rapidjson::StringRef(passwd.c_str()), j_request_body.GetAllocator());
    j_request_body.AddMember("limit_ip", rapidjson::StringRef("1.1.1.1"), j_request_body.GetAllocator());
    j_request_body.AddMember("tfa_provider", rapidjson::StringRef(provider.c_str()), j_request_body.GetAllocator());
    j_request_body.AddMember("code", rapidjson::StringRef(code.c_str()), j_request_body.GetAllocator());
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    j_request_body.Accept(writer);
    std::string request_body = buffer.GetString();

    std::string user_agent = fmt::format("{}/{}", "Rigs of Rods Client", ROR_VERSION_STRING);
    std::string response_payload;
    std::string response_header;
    long response_code = 0;

    struct curl_slist* slist;
    slist = NULL;
    slist = curl_slist_append(slist, "Content-Type: application/json");


    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8080/auth"); // todo api url + endpoint
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str()); // post request body
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteFunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_payload);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_header);

    curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    curl_easy_cleanup(curl);
    curl = nullptr;

    if (response_code == 400) // a failure, bad tfa code
    {
        App::GetGameContext()->PushMessage(
            Message(MSG_NET_SSO_2FA_FAILURE, _LC("Login", "The two-step verification value could not be confirmed. Please retry."))
        );
        return;
    }
    else if (response_code != 200) // a net failure, restart from beginning
    {
        App::GetGameContext()->PushMessage(
            Message(MSG_NET_SSO_FAILURE, _LC("Login", "Connection error. Please check your connection and try again."))
        );
        return;
    }

    // if tfa success, then sso success
    App::GetGameContext()->PushMessage(Message(MSG_NET_SSO_SUCCESS));
}

void PostAuthTriggerTfa(std::string login, std::string passwd, std::string provider)
{
    rapidjson::Document j_request_body;
    j_request_body.SetObject();
    j_request_body.AddMember("login", rapidjson::StringRef(login.c_str()), j_request_body.GetAllocator());
    j_request_body.AddMember("password", rapidjson::StringRef(passwd.c_str()), j_request_body.GetAllocator());
    j_request_body.AddMember("tfa_provider", rapidjson::StringRef(provider.c_str()), j_request_body.GetAllocator());
    j_request_body.AddMember("limit_ip", rapidjson::StringRef("1.1.1.1"), j_request_body.GetAllocator());
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    j_request_body.Accept(writer);
    std::string request_body = buffer.GetString();

    std::string user_agent = fmt::format("{}/{}", "Rigs of Rods Client", ROR_VERSION_STRING);
    std::string response_payload;
    std::string response_header;
    long response_code = 0;

    struct curl_slist* slist;
    slist = NULL;
    slist = curl_slist_append(slist, "Content-Type: application/json");

    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8080/auth"); // todo api url + endpoint
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str()); // post request body
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteFunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_payload);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_header);

    curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    curl_easy_cleanup(curl);
    curl = nullptr;

    if (response_code != 200) // a net failure, restart from beginning
    {
        App::GetGameContext()->PushMessage(
            Message(MSG_NET_SSO_FAILURE, _LC("Login", "Connection error. Please check your connection and try again."))
        );
        return;
    }

    App::GetGameContext()->PushMessage(Message(MSG_NET_SSO_2FA_REQUESTED)); // success
}

void PostAuth(std::string login, std::string passwd)
{
    rapidjson::Document j_request_body;
    j_request_body.SetObject();
    j_request_body.AddMember("login", rapidjson::StringRef(login.c_str()), j_request_body.GetAllocator());
    j_request_body.AddMember("password", rapidjson::StringRef(passwd.c_str()), j_request_body.GetAllocator());
    j_request_body.AddMember("limit_ip", rapidjson::StringRef("1.1.1.1"), j_request_body.GetAllocator());
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    j_request_body.Accept(writer);
    std::string request_body = buffer.GetString();

    std::string user_agent = fmt::format("{}/{}", "Rigs of Rods Client", ROR_VERSION_STRING);
    std::string response_payload;
    std::string response_header;
    long response_code = 0;

    struct curl_slist* slist;
    slist = NULL;
    slist = curl_slist_append(slist, "Content-Type: application/json");

    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8080/auth"); // todo api url + endpoint
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str()); // post request body
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteFunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_payload);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_header);

    curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    curl_easy_cleanup(curl);
    curl = nullptr;

    if (response_code == 400) // a failure, user not found or bad pass combo
    {
        App::GetGameContext()->PushMessage(
            Message(MSG_NET_SSO_FAILURE, _LC("Login", "You did not sign in correctly or your account is temporarily disabled. Please retry."))
        );
        return;
    }
    else if (response_code == 401) // a failure, user is likely banned
    {
        App::GetGameContext()->PushMessage(
            Message(MSG_NET_SSO_FAILURE, _LC("Login", "Could not log you in. Your account has been suspended."))
        );
        return;
    }
    else if (response_code >= 300) // a net failure
    {
        Ogre::LogManager::getSingleton().stream()
            << "[RoR|User|SSO] Failed to sign user in; HTTP status code: " << response_code;
        App::GetGameContext()->PushMessage(
            Message(MSG_NET_SSO_FAILURE, _LC("Login", "Connection error. Please check your connection and try again."))
        );
        return;
    }

    /*rapidjson::Document j_response_body;
    j_response_body.Parse(response_payload.c_str());
    if (j_response_body.HasParseError() || !j_response_body.IsObject())
    {
        Ogre::LogManager::getSingleton().stream()
            << "[RoR|User|SSO] Error parsing JSON";
        App::GetGameContext()->PushMessage(
            Message(MSG_NET_SSO_FAILURE, _LC("Login", "Received malformed data. Please try again."))
        );
        return;
    }*/

    // non-standard see https://developer.mozilla.org/en-US/docs/Web/HTTP/Status/402
    // we will use for it indicating that 2fa is required
    if (response_code == 202)
    {
        // fetch providers
        /*std::vector<std::string> tfa_providers;
        rapidjson::Value& j_tfa_providers = j_response_body["tfa_providers"];
        for (auto&& item : j_tfa_providers.GetArray()) 
        {
            tfa_providers.push_back(item.GetString());
        }*/
        App::GetGameContext()->PushMessage(Message(MSG_NET_SSO_2FA_REQUESTED/*,tfa_providers*/));
        return;
    }

    //App::sso_access_token->setStr(j_response_body["login_token"].GetString()); // todo rename
    //App::sso_refresh_token->setStr(j_response_body["refresh_token"].GetString());
    //App::sso_expiry_date->setStr(j_response_body["expiry_date"].GetString());

    App::GetGameContext()->PushMessage(Message(MSG_NET_SSO_SUCCESS));
}

#endif

LoginBox::LoginBox()
{}

LoginBox::~LoginBox()
{}

void LoginBox::Draw()
{
    // do not load if the client is already signed in

    GUIManager::GuiTheme const& theme = App::GetGuiManager()->GetTheme();

    ImGui::SetNextWindowSize(ImVec2(400.f, 250.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPosCenter(ImGuiCond_Appearing);
    ImGuiWindowFlags win_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    bool keep_open = true;
    ImGui::Begin(_LC("Login", "Login"), &keep_open, win_flags);

    if (!m_is_processing)
    {
        // draw errors if they exist but do not block anything else from drawing, allows for retries
        if (!m_errors.empty())
        {
            ImGui::TextColored(App::GetGuiManager()->GetTheme().error_text_color, "%s", m_errors.c_str());
        }

        if (m_needs_tfa) // we got 202, 2fa time
        {
            ImGui::BeginTabBar("TfaOptTab");
            if (ImGui::BeginTabItem(_LC("Login", "Verification code via app")))
            {
                m_tfa_provider = "totp";
                ImGui::TextWrapped(_LC("Login", "Please enter the verification code generated by the app on your phone."));
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(_LC("Login", "Email confirmation")))
            {
                m_tfa_provider = "email";
                /*if (m_tfa_trigger) // do not trigger again, default true until TriggerTfa() envoked
                {
                    this->TriggerTfa();
                }*/
                m_tfa_trigger = true; // switching between tabs
                ImGui::TextWrapped(_LC("Login", "An email has been sent with a single-use code. Please enter that code to continue."));
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
            ImGui::InputText("##2fa", m_tfa_code.GetBuffer(), m_tfa_code.GetCapacity());
            if (ImGui::Button("Confirm"))
            {
                this->ConfirmTfa();
            }
            ImGui::Separator();
            ImGui::TextWrapped(_LC("Login", "A backup code can be used when you don't have access to an alternative verification method. To do so, you must login using a web browser."));
        }
        else
        {
            ImGui::Text(_LC("Login", "Your name or email address"));
            ImGui::InputText("##login", m_login.GetBuffer(), m_login.GetCapacity());
            ImGui::Text(_LC("Login", "Password"));
            ImGui::InputText("##password", m_passwd.GetBuffer(), m_passwd.GetCapacity(), ImGuiInputTextFlags_Password | ImGuiInputTextFlags_CharsNoBlank);
            ImGui::Checkbox("Remember me", &m_remember);
            if (ImGui::Button("Login"))
            {
                this->Login();
            }
        }
    }
    else
    {
        // todo make this a spinner
        ImGui::Text("Please wait...");
    }

    ImGui::End();
    if (!keep_open)
    {
        this->SetVisible(false);
    }
}

void LoginBox::Login()
{
#if defined(USE_CURL)
    m_is_processing = true;

    // check if we have any empty input from the form
    if (m_login.IsEmpty() && m_passwd.IsEmpty())
    {
        App::GetGameContext()->PushMessage(
            Message(MSG_NET_SSO_FAILURE, _LC("Login", "There must not be any empty fields."))
        );
        return;
    }

    std::string login(m_login);
    std::string passwd(m_passwd);

    std::packaged_task<void(std::string, std::string)> task(PostAuth);
    std::thread(std::move(task), login, passwd).detach();
#endif
}

void LoginBox::ShowError(std::string const& msg)
{
    m_is_processing = false;
    m_errors = msg;
    //m_needs_tfa = false; // restart from beginning
}

void LoginBox::ConfirmTfa()
{
#if defined(USE_CURL)
    m_is_processing = true;

    if (m_tfa_code.IsEmpty())
    {
        App::GetGameContext()->PushMessage(
            Message(MSG_NET_SSO_FAILURE, _LC("Login", "There must not be any empty fields."))
        );
        return;
    }

    std::string login(m_login);
    std::string passwd(m_passwd);
    std::string tfa_code(m_tfa_code);

    std::packaged_task<void(std::string, std::string, std::string, std::string)> task(PostAuthWithTfa);
    std::thread(std::move(task), login, passwd, m_tfa_provider, tfa_code).detach();
#endif
}

void LoginBox::TriggerTfa()
{
#if defined(USE_CURL)
    // check if the provider is present
    m_is_processing = true; // don't let them switch tabs
    m_tfa_trigger = false;

    std::string login(m_login);
    std::string passwd(m_passwd);

    std::packaged_task<void(std::string, std::string, std::string)> task(PostAuthTriggerTfa);
    std::thread(std::move(task), login, passwd, m_tfa_provider).detach();
#endif
}

void LoginBox::NeedsTfa(/*tfa_providers*/)
{
    // prefer auth app, alternative method is email
    m_is_processing = false;
    m_needs_tfa = true;
    //m_tfa_providers = tfa_providers;
}

//void LoginBox::

void LoginBox::SetVisible(bool visible)
{
    m_is_visible = visible;
    if (!visible && (App::app_state->getEnum<AppState>() == AppState::MAIN_MENU))
    {
        App::GetGuiManager()->SetVisible_GameMainMenu(true);
    }
}