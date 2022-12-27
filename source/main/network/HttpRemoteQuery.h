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

/// @file
/// @author Rafael Galvan
/// @date   8/30

#pragma once

#include "Application.h"

#include <string>
#include <map>

namespace RoR {

class HttpRemoteQuery {
struct HttpResponse {
    int status_code;
    std::string body;
    std::string headers;
};

public:
    void GetServerList();
    void GetServerFromUuid();

    void GetResourceList();
    void GetResourceFromId();
    void GetResourceCategoryList();
    void GetResourceCategoryById();

    void GetUserFromId();

    void SignIn();
    void SignOut();

    void GetMe();

    void doHttpDownloadRequest(std::string url);
private:
    HttpResponse doHttpRequest(
        std::string path,
        std::string method,
        std::string body,
        bool privilaged);
    void HandleHttpRequestErrors(HttpResponse& response);
    bool HasErrors(int status_code);
};

}