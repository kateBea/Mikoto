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

#ifndef MIKOTO_SOCKET_HH
#define MIKOTO_SOCKET_HH

#include <EASTL/atomic.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Platform/PlatformWin32.hh>
#include <asio.hpp>
#if defined( MIKOTO_OPENSSL_AVAILABLE )
#include <asio/ssl.hpp>
#endif

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/ResourcePool.hh>

namespace mikoto::network {

    using namespace mikoto::core;

    enum class ConnectionStatus { ePending, eConnected, eDisconnected };

    class ISocket : public IResource {
    public:
        explicit ISocket() = default;

        virtual auto Disconnect() -> void = 0;

        MKT_NODISCARD virtual auto GetHost() const -> const eastl::string& = 0;

        MKT_NODISCARD auto GetConnectionStatus() const -> ConnectionStatus { return mConnectionStatus; }
        MKT_NODISCARD auto IsConnectionStatus(const ConnectionStatus status ) const -> bool { return mConnectionStatus == status; }

        MKT_NODISCARD virtual auto IsConnected() const -> bool = 0;
        MKT_NODISCARD virtual auto Connect( eastl::string_view address, u16 port ) -> bool = 0;

        virtual auto SendSync( eastl::string_view data ) -> void = 0;
        virtual auto SendSync( const void* data, size_t size ) -> void = 0;

        MKT_NODISCARD virtual auto ReceiveSync( void* buffer, size_t maxSize ) -> size_t = 0;

        using IResource::Initialize;

    protected:
        ConnectionStatus mConnectionStatus{ ConnectionStatus::eDisconnected };
    };

    using SocketHandle = Ref<ISocket>;

    class TcpSocket final : public ISocket {
    public:

        TcpSocket( asio::io_context& ctx, eastl::string_view address, u16 port, bool wait = true );

#if defined( MIKOTO_OPENSSL_AVAILABLE )
        TcpSocket( asio::io_context& ctx, asio::ssl::context& sslContext, eastl::string_view address, UInt16 port, bool wait = true );
#endif

        auto Disconnect() -> void override;

        MKT_NODISCARD auto IsConnected() const -> bool override;
        MKT_NODISCARD auto Connect( eastl::string_view address, u16 port ) -> bool override;

        auto SendSync( eastl::string_view data ) -> void override;
        auto SendSync( const void* data, size_t size ) -> void override;

        MKT_NODISCARD auto ReceiveSync( void* buffer, size_t maxSize ) -> size_t override;

        MKT_NODISCARD auto GetHost() const -> const eastl::string& override;

        ~TcpSocket() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

        auto InitConnection() -> void;
        auto InitConnectionSync() -> void;

    private:
        // To avoid keep reading if we reach eof
        asio::error_code mErrorCode{};

#if defined( MIKOTO_OPENSSL_AVAILABLE )
        asio::ip::tcp::socket m_Socket;
        asio::ip::tcp::endpoint m_TcpEndpoint{};
        asio::ssl::stream<asio::ip::tcp::socket>* m_SslSocket{ nullptr };

#else
        asio::ip::tcp::socket mSocket;
        asio::ip::tcp::endpoint mTcpEndpoint{};
#endif

        u16 mPort{};
        eastl::string mHostName{};

        bool mIsSsl{ false };
        bool mInitSync{ false };
    };
}// namespace Mikoto


#endif // MIKOTO_SOCKET_HH
