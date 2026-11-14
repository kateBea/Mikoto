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

#ifndef MIKOTO_NETWORKING_NETWORK_REQUEST_HH
#define MIKOTO_NETWORKING_NETWORK_REQUEST_HH

#include <mutex>
#include <memory>

#include <EASTL/atomic.h>
#include <EASTL/vector.h>

#include <asio.hpp>

#if defined( MIKOTO_OPENSSL_AVAILABLE )
#include <asio/ssl.hpp>
#endif

namespace mikoto::network {

    class TcpSocket;

    /** @brief Cancellation interface for requests using the shared networking context. */
    class INetworkRequest {
    public:
        /** @brief Allows request destruction through the cancellation interface. */
        virtual ~INetworkRequest();

        /**
         * @brief Queues cancellation on the request executor without blocking.
         * @details NetworkService calls this during shutdown before draining pending handlers.
         */
        virtual auto CancelRequest() -> void = 0;
    };

    /**
     * @brief Shared backend lifetime retained by sockets and asynchronous requests.
     *
     * Surviving socket handles retain this context after NetworkService shuts down.
     * The active flag prevents new connections on a stopped context. Registry access
     * is protected by mutex; socket entries are borrowed and request entries are weak.
     */
    struct SocketContext {
        /** @brief Creates the shared I/O context and optional client TLS configuration. */
        SocketContext();

        asio::io_context context{ 1 };
#if defined( MIKOTO_OPENSSL_AVAILABLE )
        asio::ssl::context tls{ asio::ssl::context::tls_client };
#endif

        eastl::atomic<bool> active{ true };
        std::mutex mutex{};

        eastl::vector<TcpSocket *> sockets{};
        // Weak registration lets shutdown cancel pending work without extending
        // client lifetime. Access is protected by mutex.
        eastl::vector<std::weak_ptr<INetworkRequest>> requests{};
    };

}// namespace mikoto::network

#endif// MIKOTO_NETWORKING_NETWORK_REQUEST_HH
