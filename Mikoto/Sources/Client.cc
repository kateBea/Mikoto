//    Copyright 2025 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Logging/Logger.hh>
#include <Networking/Client.hh>
#include <Networking/NetworkService.hh>

namespace mikoto::network {
    HttpClient::HttpClient( eastl::string_view url ) {
        ParseUrl(url);
        InitSocket();
    }

    HttpClient::HttpClient( eastl::string_view host, u16 port, SecurityProtocol sp )
        : mHost{ host }, mPort{ port }, mSecurity{ sp }
    {
        InitSocket();
    }

    auto HttpClient::Get( eastl::string_view path, eastl::string_view contentType ) -> HttpResponse {
        eastl::string req{};

        req += "GET ";
        req += path.data();
        req += " HTTP/1.1\r\n";

        req += "Host: " + mHost + "\r\n";
        req += "User-Agent: Mikoto-HttpClient/1.0\r\n";
        req += "Accept: */*\r\n";

        req += "Content-Type: ";
        req += contentType.data();
        req += "\r\n";

        req += "Connection: close\r\n\r\n";

        return SendRawRequest(req);
    }

    auto HttpClient::Post( eastl::string_view path, eastl::string_view body, eastl::string_view contentType ) -> HttpResponse {
        eastl::string req{};

        req += "POST ";
        req += path.data();
        req += " HTTP/1.1\r\n";

        req += "Host: " + mHost + "\r\n";
        req += "User-Agent: Mikoto-HttpClient/1.0\r\n";
        req += "Accept: */*\r\n";
        req += "Content-Type: ";
        req += contentType.data();
        req += "\r\n";

        req += "Content-Length: ";
        req += eastl::to_string(body.size());
        req += "\r\n";

        req += "Connection: close\r\n\r\n";

        req += body.data();

        return SendRawRequest(req);
    }

    auto HttpClient::SendRawRequest( const eastl::string_view raw ) -> HttpResponse {
        if (mSocket.IsEmpty() || !mSocket->IsConnected()) {
            MKT_CORE_LOGGER_ERROR("HttpClient::SendRawRequest – socket is not connected");
            return {};
        }

        // Send request
        mSocket->SendSync(raw);

        eastl::string apiResponse{};
        eastl::array<char, 4096>buffer{};

        // Receive until EOF
        while (true) {
            const size_t n{ mSocket->ReceiveSync(buffer.data(), sizeof(buffer)) };
            if (n == 0) {
                break;
            }

            apiResponse.append(buffer.data(), n);
        }

        return GetHttpResponse(apiResponse);
    }

    auto HttpClient::ParseUrl( eastl::string_view url ) -> void {
        // Detect scheme
        if (url.starts_with("https://")) {
            mSecurity = SecurityProtocol::eTLS;
            url.remove_prefix(strlen("https://"));
            mPort = 443;
        }
        else if (url.starts_with("http://")) {
            mSecurity = SecurityProtocol::eNone;
            url.remove_prefix(strlen("http://"));
            mPort = 80;
        }
        else {
            // Default: assume plain http://
            mSecurity = SecurityProtocol::eNone;
            mPort = 80;
        }

        // Extract host and port
        auto [host, port]{ GetHost(url) };
        mHost = host;

        if (port.has_value()) {
            mPort = as<u16>(std::stoi(port->c_str()));
        }
    }

    auto HttpClient::InitSocket() -> void {
        switch (mSecurity) {
            case SecurityProtocol::eNone:
                // if host and port specified
                if (mPort != 0) {
                    mSocket = NetworkSystem::Get()->CreateSocketSync( SocketType::eTcp, mHost, mPort );
                } else {
                    mSocket = NetworkSystem::Get()->CreateSocketHttp( mHost, true );
                }
                break;
            case SecurityProtocol::eTLS:
                mSocket = NetworkSystem::Get()->CreateSocketHttps( mHost, true );
                break;
        }
    }

    HttpClient::~HttpClient() {
        mSocket.Release();
    }
}