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

#ifndef NETWORK_SERVICE_HH
#define NETWORK_SERVICE_HH

#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/string_view.h>

#include <Platform/PlatformWin32.hh>
#include <asio.hpp>
#if defined(MIKOTO_OPENSSL_AVAILABLE)
#include <asio/ssl.hpp>
#endif

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Singleton.hh>
#include <Core/Subsystem.hh>
#include <Core/ResourcePool.hh>

#include <Networking/Socket.hh>

namespace mikoto::network {

    using namespace mikoto::core;

    /**
     * @enum SecurityProtocol
     * @brief Specifies the security layer applied on top of a TCP connection.
     *
     * Determines whether a socket uses a plain, unencrypted connection,
     * or a secure TLS session. TLS provides encryption, integrity, and
     * authentication for data transmitted over the network.
     *
     * Usage:
     *  - SecurityProtocol::NONE -> Raw TCP socket (no encryption)
     *  - SecurityProtocol::TLS  -> Secure TLS session layered over TCP
     */
    enum class SecurityProtocol {
        eNone,
        eTLS,
    };

    /**
     * @enum SocketType
     * @brief Specifies the type of socket to create within the NetworkService.
     *
     * Currently, only TCP sockets are implemented in Mikoto.
     * UDP is reserved for future support.
     */
    enum class SocketType {
        eTcp,
        eUdp, // WIP
    };

    /**
     * @struct NetworkServiceCreateInfo
     * @brief Configuration structure for initializing the NetworkService.
     *
     * This struct exists for future extensibility.
     * Currently, it contains no fields but allows the API to evolve without
     * breaking user code (e.g., adding timeouts, threading options, or SSL settings).
     */
    struct NetworkServiceCreateInfo {};

    /**
     * @class NetworkSystem
     * @brief Provides TCP/HTTP/HTTPS networking functionality for Mikoto Engine.
     *
     * The NetworkService manages all socket creation, initialization, shutdown,
     * and communication within the engine. Internally, it uses ASIO to implement
     * asynchronous and synchronous TCP connections. If OpenSSL is available,
     * HTTPS connections are also supported.
     *
     * Sockets are allocated through a ResourcePoolTyped<TcpSocket>, which provides
     * automatic management.
     *
     * Example usage:
     * @code
     * NetworkService ns{};
     * ns.Init();
     * SocketHandle socket = ns.CreateSocket(SocketType::SOCKET_TCP, "localhost", 8080);
     * if (socket->IsConnected()) {
     *     socket->SendSync("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
     * }
     * ns.Shutdown();
     * @endcode
     */
    class NetworkSystem final : public ISubsystem, public Singleton<NetworkSystem> {
    public:
        /**
         * @brief Constructs a new NetworkService instance.
         *
         * The constructor does not initialize networking; call Init() before use.
         * @param options User-provided settings for configuring the NetworkService.
         */
        explicit NetworkSystem( const NetworkServiceCreateInfo &options );

        /**
         * @brief Initializes the networking system.
         *
         * Sets up the ASIO I/O context and the internal TcpSocket resource pool.
         * Must be called before any socket is created.
         */
        auto Initialize() -> void override;

        /**
         * @brief Shuts down the networking system.
         *
         * Closes all sockets, clears the socket pool, and stops the I/O context.
         * After calling Shutdown(), the service cannot be used.
         */
        auto Shutdown() -> void override;

        /**
         * @brief Processes any pending asynchronous network operations.
         *
         * This must be called regularly (once per frame) when using asynchronous
         * sockets or operations. It drives ASIO's internal event loop.
         *
         * @param dt Delta time since the last update.
         */
        auto Update( float dt ) -> void override;

        /**
         * @brief Creates a TCP socket.
         *
         * The socket is created asynchronously.
         *
         * @param type The type of socket to create (currently only supports TCP).
         * @param hostName The remote host to connect to.
         * @param port The port to connect to.
         * @return A socket handle. The handle may be empty if creation failed.
         */
        auto CreateSocket( SocketType type, eastl::string_view hostName, u16 port, SecurityProtocol sp = SecurityProtocol::eNone ) -> SocketHandle;

        /**
         * @brief Creates a socket and waits for a synchronous connection.
         *
         * This forces blocks the calling thread until the socket is properly constructed and initialized.
         *
         * @param type The socket type.
         * @param hostName The host.
         * @param port The port.
         * @return A socket handle. May be empty on failure.
         */
        auto CreateSocketSync( SocketType type, eastl::string_view hostName, u16 port, SecurityProtocol sp = SecurityProtocol::eNone ) -> SocketHandle;

        /**
         * @brief Creates an HTTP socket (port 80 by default).
         *
         * If @p wait is true, the socket will connect synchronously before
         * returning. Otherwise, the connection will be performed asynchronously.
         *
         * @param hostName Hostname of the remote server.
         * @param wait Whether to block the calling thread until the connection completes.
         * @return A valid socket handle on success, or an empty handle on failure.
         */
        auto CreateSocketHttp( eastl::string_view hostName, bool wait = true ) -> SocketHandle;

        /**
         * @brief Creates an HTTPS socket (port 443 by default).
         *
         * HTTPS support requires OpenSSL to be available during build and runtime.
         * If OpenSSL is not available, this function will return an empty handle.
         *
         * If @p wait is true, the socket will connect synchronously before returning.
         *
         * @param hostName Hostname of the remote HTTPS server.
         * @param wait Whether to block until the connection completes.
         * @return A socket handle, or an empty handle if HTTPS is not supported or the connection failed.
         */
        auto CreateSocketHttps( eastl::string_view hostName, bool wait ) -> SocketHandle;

        /// @brief Destructor. Automatically calls Shutdown() if the service was still running.
        ~NetworkSystem() override = default;

    private:
        // [Internal usage]

        // If wait == true creation is asynchronous, synchronous otherwise
        auto CreateSocketTcp(eastl::string_view hostName, u16 port, bool wait, SecurityProtocol sp = SecurityProtocol::eNone ) -> SocketHandle;

    private:
        /// @brief ASIO context used for all asynchronous network operations.
        eastl::unique_ptr<asio::io_context> mIoContext{};

#if defined(MIKOTO_OPENSSL_AVAILABLE)
        /// @brief SSL context used for secure HTTPS sockets.
        asio::ssl::context mSslContext{ asio::ssl::context::tls_client };
#endif

        /// @brief Pool for allocating and reusing TcpSocket objects.
        ResourcePoolTyped<TcpSocket> mTcpSockets{};
    };
}// namespace Mikoto


#endif//
