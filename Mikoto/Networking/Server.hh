//    Copyright 2026 ケイト
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

#include <functional>
#include <memory>
#include <string_view>

#include <EASTL/unique_ptr.h>
#include <ankerl/unordered_dense.h>
#include <asio.hpp>

#include <Core/Types.hh>
#include <Networking/NetworkService.hh>
#include <Networking/NetworkUtilities.hh>

namespace mikoto::network {

    /**
     * @brief Asynchronous HTTP/1.1 server. One request is served per connection. */
    class HttpServer final {
    public:
        using Handler = std::function<void( const HttpRequest&, HttpResponse& )>;

        explicit HttpServer( NetworkSystem& network );

        auto Get( std::string_view path, Handler handler ) -> void;
        auto Post( std::string_view path, Handler handler ) -> void;

        MKT_NODISCARD auto IsListening() const -> bool;
        MKT_NODISCARD auto Listen( std::string_view address, core::u16 port ) -> bool;

        auto Stop() -> void;

        ~HttpServer();

    private:
        class Session;

        auto AcceptNext() -> void;
        auto Dispatch( const HttpRequest& request ) const -> HttpResponse;

    private:
        NetworkSystem& mNetwork;

        eastl::unique_ptr<asio::ip::tcp::acceptor> mAcceptor{};

        ankerl::unordered_dense::map<eastl::string, Handler> mGetRoutes{};
        ankerl::unordered_dense::map<eastl::string, Handler> mPostRoutes{};

        bool mStopping{};
    };
}

#endif // MIKOTO_SERVER_HH
