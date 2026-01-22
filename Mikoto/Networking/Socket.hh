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

#include <atomic>
#include <asio.hpp>
#include <string>
#include <string_view>

#if defined( MIKOTO_OPENSSL_AVAILABLE )
#include <asio/ssl.hpp>
#endif

#include <Library/Utility/Types.hh>
#include <Library/Data/ResourcePool.hh>

namespace Mikoto {

    enum class ConnectionStatus { PENDING, CONNECTED, DISCONNECTED };

    class Socket : public IResource {
    public:
        explicit Socket() = default;

        virtual auto Disconnect() -> void = 0;

        MKT_NODISCARD virtual auto GetHost() const -> const std::string& = 0;

        MKT_NODISCARD auto GetConnectionStatus() const -> ConnectionStatus { return m_ConnectionStatus; }
        MKT_NODISCARD auto IsConnectionStatus(const ConnectionStatus status ) const -> bool { return m_ConnectionStatus == status; }

        MKT_NODISCARD virtual auto IsConnected() const -> bool = 0;
        MKT_NODISCARD virtual auto Connect( std::string_view address, UInt16 port ) -> bool = 0;

        virtual auto SendSync( std::string_view data ) -> void = 0;
        virtual auto SendSync( const void* data, Size size ) -> void = 0;

        MKT_NODISCARD virtual auto ReceiveSync( void* buffer, Size maxSize ) -> Size = 0;

        using IResource::Initialize;

    protected:
        ConnectionStatus m_ConnectionStatus{ ConnectionStatus::DISCONNECTED };
    };

    using SocketHandle = Ref<Socket>;

    class TcpSocket final : public Socket {
    public:

        TcpSocket( asio::io_context& ctx, std::string_view address, UInt16 port, bool wait = true );

#if defined( MIKOTO_OPENSSL_AVAILABLE )
        TcpSocket( asio::io_context& ctx, asio::ssl::context& sslContext, std::string_view address, UInt16 port, bool wait = true );
#endif

        auto Disconnect() -> void override;

        MKT_NODISCARD auto IsConnected() const -> bool override;
        MKT_NODISCARD auto Connect( std::string_view address, UInt16 port ) -> bool override;

        auto SendSync( std::string_view data ) -> void override;
        auto SendSync( const void* data, Size size ) -> void override;

        MKT_NODISCARD auto ReceiveSync( void* buffer, Size maxSize ) -> Size override;

        MKT_NODISCARD auto GetHost() const -> const std::string& override;

        ~TcpSocket() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

        auto InitConnection() -> void;
        auto InitConnectionSync() -> void;

    private:
        // To avoid keep reading if we reach eof
        asio::error_code m_ErrorCode{};

#if defined( MIKOTO_OPENSSL_AVAILABLE )
        asio::ip::tcp::socket m_Socket;
        asio::ip::tcp::endpoint m_TcpEndpoint{};
        asio::ssl::stream<asio::ip::tcp::socket>* m_SslSocket{ nullptr };

#else
        asio::ip::tcp::socket m_Socket;
        asio::ip::tcp::endpoint m_TcpEndpoint{};
#endif

        UInt16 m_Port{};
        std::string m_HostName{};

        bool m_IsSsl{ false };
        bool m_InitSync{ false };
    };
}// namespace Mikoto


#endif // MIKOTO_SOCKET_HH
