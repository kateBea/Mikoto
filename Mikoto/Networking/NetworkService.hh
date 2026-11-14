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

#ifndef MIKOTO_NETWORKING_NETWORK_SERVICE_HH
#define MIKOTO_NETWORKING_NETWORK_SERVICE_HH

#include <thread>

#include <EASTL/unique_ptr.h>

#include <asio.hpp>

#if defined( MIKOTO_OPENSSL_AVAILABLE )
#include <asio/ssl.hpp>

#endif

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Networking/Socket.hh>
#include <Networking/NetworkRequest.hh>

namespace mikoto::network {

    /** @brief Transport requested from the socket factories. */
    enum class SocketType : core::u8 {
        eTcp, ///< TCP transport, optionally secured with TLS.
        eUdp  ///< Reserved; the socket factories currently return an empty handle.
    };

    /** @brief Configures how the service dispatches asynchronous networking work. */
    struct NetworkServiceCreateInfo {
        /** @brief Runs the shared context on a dedicated worker when true.
         *
         * When false, the application must call NetworkService::PollTasks regularly.
         */
        bool mUseWorkerThread{ true };
    };

    /** @brief Owns the shared I/O context, TLS configuration and networking worker.
     *
     * Initialize before creating sockets or starting asynchronous HTTP requests.
     * Run lifecycle operations and request submission on the owning application
     * thread. In worker mode, asynchronous handlers execute on the networking
     * thread; publish results safely before accessing them from the UI.
     *
     * Shutdown cancels registered requests, disconnects sockets and drains pending
     * handlers. Surviving handles retain their backend context, but cannot start
     * new connections on that stopped context.
     */
    class NetworkService final : public core::IService, public core::Singleton<NetworkService> {
    public:
        /** @brief Stores the service configuration without starting networking.
         *
         * @param options Worker-thread or manual-polling configuration.
         */
        explicit NetworkService( const NetworkServiceCreateInfo &options );


        /** @brief Initializes the shared context and starts the optional worker.
         *
         * Loads the default TLS verification paths when OpenSSL is available.
         * Repeated calls while initialized have no effect. After Shutdown, creates
         * a fresh backend context; existing socket handles retain the old context.
         */
        auto Initialize() -> void override;


        /** @brief Stops request submission, cancels networking work and joins the worker.
         *
         * Disconnects tracked sockets and drains cancellation callbacks on the
         * calling thread. Call from the owning thread, never from a networking
         * callback. Has no effect when the service is not initialized.
         */
        auto Shutdown() -> void override;


        /** @brief Dispatches up to 256 ready handlers without waiting for traffic.
         *
         * Call regularly on the owning thread in manual-polling mode to advance
         * connections, transfers and deadlines. Does nothing in worker mode or
         * while the service is uninitialized, inactive or sleeping.
         */
        auto PollTasks() -> void;


        /** @brief Creates a socket and starts an asynchronous connection.
         *
         * @param type Transport type; currently only TCP is implemented.
         * @param host Server hostname or IP address, without a URL scheme.
         * @param port Nonzero destination port.
         * @param security Plain TCP or TLS; TLS requires OpenSSL support.
         * @return Socket handle, or empty for an invalid configuration, unsupported
         * transport or uninitialized service. A nonempty handle does not imply
         * connection success; inspect the socket's connection status.
         */
        MKT_NODISCARD auto CreateSocket( SocketType type, eastl::string_view host, core::u16 port, SecurityProtocol security = SecurityProtocol::eNone ) -> SocketHandle;


        /** @brief Creates a socket and waits for its connection attempt to finish.
         *
         * @param type Transport type; currently only TCP is implemented.
         * @param host Server hostname or IP address, without a URL scheme.
         * @param port Nonzero destination port.
         * @param security Plain TCP or TLS; TLS requires OpenSSL support.
         * @return Connected socket, or empty when configuration or connection fails.
         */
        MKT_NODISCARD auto CreateSocketSync( SocketType type, eastl::string_view host, core::u16 port, SecurityProtocol security = SecurityProtocol::eNone ) -> SocketHandle;


        /** @brief Creates a plain TCP connection to port 80.
         *
         * This convenience method connects the transport; it does not send an HTTP request.
         *
         * @param host Server hostname or IP address, without a URL scheme or path.
         * @param wait True to wait for connection; false to initiate it asynchronously.
         * @return Socket handle, or empty on configuration or synchronous connection
         * failure. Asynchronous connection success must be checked on the socket.
         */
        MKT_NODISCARD auto CreateSocketHttp( eastl::string_view host, bool wait = true ) -> SocketHandle;


        /** @brief Creates a TLS connection to port 443.
         *
         * Requires OpenSSL support. Connects the transport without sending an HTTP request.
         *
         * @param host Server hostname or IP address, without a URL scheme or path.
         * @param wait True to wait for connection; false to initiate it asynchronously.
         * @return Socket handle, or empty on configuration or synchronous connection
         * failure. Asynchronous connection success must be checked on the socket.
         */
        MKT_NODISCARD auto CreateSocketHttps( eastl::string_view host, bool wait = true ) -> SocketHandle;


        /** @brief Provides access to the service's shared Asio context.
         *
         * Use only while initialized; do not stop or restart the context externally.
         * Raw operations submitted here are not automatically registered for shutdown
         * cancellation. Their owner must arrange cancellation before service shutdown.
         *
         * @return Borrowed context reference; do not retain across service shutdown
         * and reinitialization or destruction.
         */
        MKT_NODISCARD auto GetContext() -> asio::io_context &;


        /** @brief Shuts down networking before releasing the service's backend ownership. */
        ~NetworkService() override;


        /** @brief Checks whether this build includes OpenSSL transport support.
         *
         * @return True when TLS support was compiled in; does not test certificate
         * availability, server reachability or successful TLS negotiation.
         */
        MKT_NODISCARD static constexpr auto HasTlsSupport() -> bool {
#if defined( MIKOTO_OPENSSL_AVAILABLE )
            return true;
#else
            return false;
#endif
        }

    private:
        /** @brief Creates the TCP transport used by the public socket factories.
         *
         * @param host Server hostname or IP address.
         * @param port Nonzero destination port.
         * @param synchronous True to connect immediately; false to queue connection.
         * @param security Plain TCP or TLS transport.
         * @return Socket handle, or empty on configuration or synchronous connection
         * failure. Asynchronous failures are reported through the socket's state.
         */
        MKT_NODISCARD auto CreateTcpSocket( eastl::string_view host, core::u16 port, bool synchronous, SecurityProtocol security ) -> SocketHandle;

    private:
        friend class HttpClient;

        NetworkServiceCreateInfo mOptions{};
        std::shared_ptr<SocketContext> mContext{ std::make_shared<SocketContext>() };
        eastl::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> mWorkGuard{};
        std::jthread mWorker{};
    };
}// namespace mikoto::network

#endif// MIKOTO_NETWORKING_NETWORK_SERVICE_HH
