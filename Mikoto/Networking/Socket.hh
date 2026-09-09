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

#ifndef MIKOTO_SOCKET_HH
#define MIKOTO_SOCKET_HH

#include <atomic>

#include <EASTL/functional.h>
#include <EASTL/memory.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <asio.hpp>
#if defined( MIKOTO_OPENSSL_AVAILABLE )
    #include <asio/ssl.hpp>
#endif

#include <Core/Core.hh>
#include <Core/ResourcePool.hh>
#include <Core/Types.hh>

namespace mikoto::network {

    enum class SecurityProtocol : core::u8 { eNone, eTLS };
    enum class ConnectionStatus : core::u8 { ePending, eConnected, eDisconnected, eFailed };

    /**
     * @brief Common interface for a connected byte-stream socket. */
    class ISocket : public core::IResource {
    public:
        using IResource::Initialize;
        using ConnectCallback = eastl::function<void( bool connected, eastl::string_view error )>;

        virtual auto Disconnect() -> void = 0;
        MKT_NODISCARD virtual auto Connect( eastl::string_view host, core::u16 port ) -> bool = 0;
        virtual auto ConnectAsync( eastl::string_view host, core::u16 port, ConnectCallback callback = {} ) -> void = 0;
        MKT_NODISCARD virtual auto SendSync( const void* data, core::usize size ) -> bool = 0;
        MKT_NODISCARD virtual auto ReceiveSync( void* buffer, core::usize maxSize ) -> core::usize = 0;
        MKT_NODISCARD virtual auto GetHost() const -> const eastl::string& = 0;
        MKT_NODISCARD virtual auto GetLastError() const -> const eastl::string& = 0;

        MKT_NODISCARD auto SendSync( const eastl::string_view data ) -> bool { return SendSync( data.data(), data.size() ); }
        MKT_NODISCARD auto GetConnectionStatus() const -> ConnectionStatus { return mConnectionStatus.load(); }
        MKT_NODISCARD auto IsConnectionStatus( const ConnectionStatus status ) const -> bool { return GetConnectionStatus() == status; }
        MKT_NODISCARD auto IsConnected() const -> bool { return IsConnectionStatus( ConnectionStatus::eConnected ); }

    protected:
        std::atomic<ConnectionStatus> mConnectionStatus{ ConnectionStatus::eDisconnected };
    };

    using SocketHandle = core::Ref<ISocket>;

    /**
     * @brief TCP client socket with optional TLS transport.
     *
     * TLS uses SNI and certificate verification. It never silently downgrades
     * an HTTPS connection to plaintext when OpenSSL is absent.
     */
    class TcpSocket final : public ISocket {
    public:
        TcpSocket( asio::io_context& context, eastl::string_view host, core::u16 port, bool connectSynchronously = true );
#if defined( MIKOTO_OPENSSL_AVAILABLE )
        TcpSocket( asio::io_context& context, asio::ssl::context& tlsContext, eastl::string_view host, core::u16 port, bool connectSynchronously = true );
#endif
        auto Disconnect() -> void override;
        MKT_NODISCARD auto Connect( eastl::string_view host, core::u16 port ) -> bool override;
        auto ConnectAsync( eastl::string_view host, core::u16 port, ConnectCallback callback = {} ) -> void override;
        MKT_NODISCARD auto SendSync( const void* data, core::usize size ) -> bool override;
        MKT_NODISCARD auto ReceiveSync( void* buffer, core::usize maxSize ) -> core::usize override;
        MKT_NODISCARD auto GetHost() const -> const eastl::string& override;
        MKT_NODISCARD auto GetLastError() const -> const eastl::string& override;
        ~TcpSocket() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;
        auto SetFailure( const asio::error_code& error ) -> void;

    private:
        asio::io_context& mContext;
        asio::ip::tcp::resolver mResolver;
        asio::ip::tcp::socket mSocket;
#if defined( MIKOTO_OPENSSL_AVAILABLE )
        asio::ssl::context* mTlsContext{};
        eastl::unique_ptr<asio::ssl::stream<asio::ip::tcp::socket>> mTlsSocket{};
#endif
        core::u16 mPort{};
        eastl::string mHostName{};
        eastl::string mLastError{};
        SecurityProtocol mSecurity{ SecurityProtocol::eNone };
        bool mConnectSynchronously{ true };
    };
}

#endif // MIKOTO_SOCKET_HH
