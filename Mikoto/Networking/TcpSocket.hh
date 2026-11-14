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

#ifndef MIKOTO_NETWORKING_TCP_SOCKET_HH
#define MIKOTO_NETWORKING_TCP_SOCKET_HH

#include <Networking/Socket.hh>
#include <Networking/NetworkRequest.hh>

namespace mikoto::network {

    /**
     * @brief TCP client socket with optional TLS transport.
     *
     * TLS uses SNI and certificate verification. It never silently downgrades
     * an HTTPS connection to plaintext when OpenSSL is absent.
     */
    class TcpSocket final : public ISocket {
    public:
        /** @brief Creates a disconnected socket; call Connect or ConnectAsync to open it.
         *
         * @param context Shared networking context.
         * @param security Transport security used for subsequent connections.
         */
        explicit TcpSocket( std::shared_ptr<SocketContext> context, SecurityProtocol security = SecurityProtocol::eNone );

        auto Disconnect() -> void override;
        auto ConnectAsync( eastl::string_view host, core::u16 port, ConnectCallback callback ) -> void override;

        MKT_NODISCARD auto Connect( eastl::string_view host, core::u16 port ) -> bool override;

        MKT_NODISCARD auto SendSync( const void *data, core::usize size ) -> bool override;
        MKT_NODISCARD auto SendSync( eastl::string_view data ) -> bool override;
        MKT_NODISCARD auto ReceiveSync( void *buffer, core::usize maxSize ) -> core::usize override;

        MKT_NODISCARD auto GetHost() const -> const eastl::string & override;
        MKT_NODISCARD auto GetLastError() const -> const eastl::string & override;
        MKT_NODISCARD auto GetConnectionStatus() const -> ConnectionStatus override;

        MKT_NODISCARD auto IsConnectionStatus( ConnectionStatus status ) const -> bool override;
        MKT_NODISCARD auto IsConnected() const -> bool override;

        ~TcpSocket() override;

    private:
        auto SetFailure( const asio::error_code &error ) -> void;

    private:
        // Declared first so Asio objects are destroyed before their context.
        std::shared_ptr<SocketContext> mContextOwner;
        asio::io_context &mContext;
        asio::ip::tcp::resolver mResolver;
        asio::ip::tcp::socket mSocket;
#if defined( MIKOTO_OPENSSL_AVAILABLE )
        asio::ssl::context *mTlsContext{};
        std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket>> mTlsSocket{};
#endif

        // Endpoint and connection diagnostics, populated by Connect/ConnectAsync.
        core::u16 mPort{};
        eastl::string mHostName{};
        eastl::string mLastError{};
        SecurityProtocol mSecurity{ SecurityProtocol::eNone };
        eastl::atomic<ConnectionStatus> mConnectionStatus{ ConnectionStatus::eDisconnected };

        // Invalidates pending handlers when a connection is replaced or disconnected.
        eastl::atomic<core::u64> mConnectGeneration{};
    };

    using TcpSocketHandle = core::Ref<TcpSocket>;
}// namespace mikoto::network

#endif// MIKOTO_NETWORKING_TCP_SOCKET_HH
