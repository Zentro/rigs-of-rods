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

#include "HttpRemoteQuery.h"

#include "Application.h"
#include "RoRVersion.h"

#include <fmt/core.h>

#ifdef USE_CURL
#include <curl/curl.h>
#include <curl/easy.h>

using namespace RoR;

static size_t CurlWriteCallback(void* ptr, size_t size, size_t nmemb, std::string* data)
{
    data->append((char*)ptr, size * nmemb);
    return size * nmemb;
}

void HttpRemoteQuery::GetServerList()
{

}

void HttpRemoteQuery::doHttpDownloadRequest(std::string url)
{

}

void HttpRemoteQuery::HandleHttpRequestErrors(HttpResponse& response) 
{
    if (HasErrors(response.status_code)) 
    {
        return;
    }
}

bool HttpRemoteQuery::HasErrors(int status_code)
{
    return status_code >= 300 || status_code < 200;
}

HttpRemoteQuery::HttpResponse HttpRemoteQuery::doHttpRequest(
    std::string path, 
    std::string method, 
    std::string body,
    bool privilaged /*= false*/) 
{
    HttpResponse http_response;

    std::string url             = fmt::format("{}/{}", App::api_query_url->getStr(), path);
    std::string user_agent      = fmt::format("{}/{}", "Rigs of Rods Client", ROR_VERSION_STRING);

    struct curl_slist* slist;
    slist = NULL;

    slist = curl_slist_append(slist, "Accept: */*");
    slist = curl_slist_append(slist, "Content-Type: application/json");

    CURL* handle = curl_easy_init();
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(handle, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
    curl_easy_setopt(handle, CURLOPT_XOAUTH2_BEARER, App::api_login_token->getStr().c_str());
    curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "gzip");
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, user_agent.c_str());
#ifdef _WIN32
    curl_easy_setopt(handle, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif // _WIN32
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &http_response.body);
    curl_easy_setopt(handle, CURLOPT_HEADERDATA, &http_response.headers);

    curl_easy_perform(handle);
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &http_response.status_code);

    curl_easy_cleanup(handle);
    curl_slist_free_all(slist);
    handle = nullptr;

    return http_response;
}

#endif // USE_URL