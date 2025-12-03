//
// Created by kate on 10/29/25.
//

#ifndef NETWORK_SERVICE_HH
#define NETWORK_SERVICE_HH

#include <asio.hpp>

#if defined(MIKOTO_OPENSSL_AVAILABLE)
#include <asio/ssl.hpp>
#endif

#include <Common/Service.hh>
#include <Common/Singleton.hh>
#include <Library/Utility/Types.hh>

#include <Networking/Socket.hh>

namespace Mikoto {

    /**
     * @enum SocketType
     * @brief Specifies the type of socket to create within the NetworkService.
     *
     * Currently, only TCP sockets are implemented in Mikoto.
     * UDP is reserved for future support.
     */
    enum class SocketType {
        SOCKET_TCP,
        SOCKET_UDP, // WIP
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
     * @class NetworkService
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
    class NetworkService final : public IService, public Singleton<NetworkService> {
    public:
        /**
         * @brief Constructs a new NetworkService instance.
         *
         * The constructor does not initialize networking; call Init() before use.
         * @param options User-provided settings for configuring the NetworkService.
         */
        explicit NetworkService( const NetworkServiceCreateInfo &options );

        /**
         * @brief Initializes the networking system.
         *
         * Sets up the ASIO I/O context and the internal TcpSocket resource pool.
         * Must be called before any socket is created.
         */
        auto Init() -> void override;

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
         * The socket is created synchronously unless.
         *
         * @param type The type of socket to create (currently only supports TCP).
         * @param hostName The remote host to connect to.
         * @param port The port to connect to.
         * @return A socket handle. The handle may be empty if creation failed.
         */
        auto CreateSocket( SocketType type, std::string_view hostName, UInt16 port ) -> SocketHandle;

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
        auto CreateSocketSync( SocketType type, std::string_view hostName, UInt16 port ) -> SocketHandle;

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
        auto CreateSocketHttp( std::string_view hostName, bool wait = true ) -> SocketHandle;

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
        auto CreateSocketHttps( std::string_view hostName, bool wait ) -> SocketHandle;

        /// @brief Destructor. Automatically calls Shutdown() if the service was still running.
        ~NetworkService() override = default;

    private:
        /// @brief ASIO context used for all asynchronous network operations.
        asio::io_context m_IoContext{};

#if defined(MIKOTO_OPENSSL_AVAILABLE)
        /// @brief SSL context used for secure HTTPS sockets.
        asio::ssl::context m_SslContext{ asio::ssl::context::tls_client };
#endif

        /// @brief Pool for allocating and reusing TcpSocket objects.
        ResourcePoolTyped<TcpSocket> m_TcpSockets{};
    };
}// namespace Mikoto


#endif//
