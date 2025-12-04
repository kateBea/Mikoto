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

#ifndef MIKOTO_CLIENT_HH
#define MIKOTO_CLIENT_HH

#include <string_view>

#include <Networking/Socket.hh>
#include <Networking/NetworkService.hh>
#include <Networking/NetworkUtilities.hh>

#include <Library/Utility/Types.hh>

namespace Mikoto {
    class HttpClient {
    public:
        explicit HttpClient( std::string_view url );
        HttpClient( std::string_view host, UInt16 port, SecurityProtocol sp = SecurityProtocol::NONE );

        auto Get( std::string_view path, std::string_view contentType ) -> HttpResponse;
        auto Post( std::string_view path, std::string_view body, std::string_view contentType ) -> HttpResponse;

        // WIP: Unavailable for now
        // auto SetTimeout( Int32 milliseconds ) -> void;
        // auto EnableServerCertificateVerification( bool enable ) -> void;

        ~HttpClient();

    private:
        auto SendRawRequest( std::string_view raw ) -> HttpResponse;
        auto ParseUrl( std::string_view url ) -> void;

        auto InitSocket() -> void;

    private:
        SocketHandle m_Socket{};
        std::string m_Host{};
        UInt16 m_Port{};
        SecurityProtocol m_Security{ SecurityProtocol::NONE };
    };
}


#endif//MIKOTO_CLIENT_HH
