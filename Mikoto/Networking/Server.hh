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

#ifndef MIKOTO_SERVER_HH
#define MIKOTO_SERVER_HH

#include <string_view>
#include <functional>

#include <ankerl/unordered_dense.h>

#include <Networking/Socket.hh>
#include <Networking/NetworkUtilities.hh>

#include <Library/Utility/Types.hh>

namespace Mikoto {

    class HttpServer {
    public:
        using Handler = std::function<void( const HttpRequest &, HttpResponse & )>;

        HttpServer() = default;

        auto Get( std::string_view path, Handler handler ) -> void;
        auto Post( std::string_view path, Handler handler ) -> void;

        auto Listen( std::string_view address, UInt16 port ) -> void;

    private:
        auto AcceptLoop() -> void;
        auto HandleClient( SocketHandle clientSocket ) -> void;

    private:
        ankerl::unordered_dense::map<std::string, Handler> m_GetRoutes{};
        ankerl::unordered_dense::map<std::string, Handler> m_PostRoutes{};

        SocketHandle m_ServerSocket;
    };
}


#endif//MIKOTO_SERVER_HH
