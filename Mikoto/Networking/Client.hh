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

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Networking/Socket.hh>
#include <Networking/NetworkService.hh>
#include <Networking/NetworkUtilities.hh>

namespace mikoto::network {
    using namespace mikoto::core;

    class HttpClient {
    public:
        explicit HttpClient( eastl::string_view url );
        HttpClient( eastl::string_view host, u16 port, SecurityProtocol sp = SecurityProtocol::eNone );

        auto Get( eastl::string_view path, eastl::string_view contentType ) -> HttpResponse;
        auto Post( eastl::string_view path, eastl::string_view body, eastl::string_view contentType ) -> HttpResponse;

        // WIP: Unavailable for now
        // auto SetTimeout( Int32 milliseconds ) -> void;
        // auto EnableServerCertificateVerification( bool enable ) -> void;

        ~HttpClient();

    private:
        auto SendRawRequest( eastl::string_view raw ) -> HttpResponse;
        auto ParseUrl( eastl::string_view url ) -> void;

        auto InitSocket() -> void;

    private:
        u16 mPort{};

        eastl::string mHost{};
        SocketHandle mSocket{};

        SecurityProtocol mSecurity{ SecurityProtocol::eNone };
    };
}

#endif//MIKOTO_CLIENT_HH
