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

/// @file   GUI_LoginBox.h
/// @author Rafael Galvan
/// @date   01/2022

#pragma once

#include "Application.h"
#include "OgreImGui.h"

#include <future>
#include <memory>
#include <thread>
#include <vector>

namespace RoR {
namespace GUI {

struct Token
{
    std::string     access_token; //<- bearer token
    std::string     expiry_date;
    //std::string     refresh_token; //<- remember me if token expires
};

struct UserItem
{
    std::string                                 email;    
    int                                         uid;      //<- uid for fetching purposes
    std::string                                 username; //<- this will replace MP usernames
    bool                                        use_tfa;  //<- for indication purposes
    //std::vector<UserAvatarCollection>           avatar_urls;
    //std::vector<UserProfileBannerCollection>    profile_banner_urls;
};

struct UserAvatarCollection {
    std::string o;
    std::string h;
    std::string l;
    std::string m;
    std::string s;
};

struct UserProfileBannerCollection {
    std::string l;
    std::string m;
};

class LoginBox {
public:
    LoginBox();
    ~LoginBox();

    void SetVisible(bool visible);
    bool IsVisible() const { return m_is_visible; }
    void Draw();

private:
    bool m_is_visible = false;
    Str<1000> m_login;
    Str<1000> m_passwd;
    bool m_remember = true;
};

}
}