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

//#ifdef USE_CURL

#include "HttpRemoteQuery.h"

#include "Application.h"
#include "RoRVersion.h"

#include <fmt/core.h>

#include <curl/curl.h>
#include <curl/easy.h>

using namespace RoR;

static size_t write_callback(void* ptr, size_t size, size_t nmemb, std::string* data)
{
    data->append((char*)ptr, size * nmemb);
    return size * nmemb;
}

HttpRemoteQuery::HttpResponse HttpRemoteQuery::doHttpRequest(
    std::string path, 
    std::string method, 
    std::string body,
    bool privilaged /*= false*/) {

    HttpResponse http_response;

    std::string url = 
        fmt::format("{}/{}", App::remote_query_url->getStr(), path);
    std::string user_agent = 
        fmt::format("{}/{}", "Rigs of Rods Client", ROR_VERSION_STRING);

    struct curl_slist* headers;
    headers = NULL;

    headers = curl_slist_append(headers, "Accept: */*");
    headers = curl_slist_append(headers, "Accept-Encoding: gzip");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (privilaged) {
        headers = curl_slist_append(headers,
            fmt::format("Authorization: Bearer {}", "something").c_str());
    }

    CURL* curl_th = curl_easy_init();
    curl_easy_setopt(curl_th, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_th, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl_th, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_th, CURLOPT_USERAGENT, user_agent.c_str());
#ifdef _WIN32
    curl_easy_setopt(curl_th, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif // _WIN32
    curl_easy_setopt(curl_th, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_th, CURLOPT_WRITEDATA, &http_response.body);
    curl_easy_setopt(curl_th, CURLOPT_HEADERDATA, &http_response.headers);

    curl_easy_perform(curl_th);
    curl_easy_getinfo(curl_th, CURLINFO_RESPONSE_CODE, &http_response.status_code);

    curl_easy_cleanup(curl_th);
    curl_slist_free_all(headers);
    curl_th = nullptr;

    return http_response;
}