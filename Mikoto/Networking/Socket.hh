//
// Created by kate on 10/29/25.
//

#ifndef SOCKET_HH
#define SOCKET_HH

#include <asio.hpp>
#include <string>
#include <string_view>

#if defined( MKT_ALLOW_HTTPS )
#include <asio/ssl.hpp>
#endif

#include <Library/Data/ResourcePool.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    class Socket : public IResource {
    public:
        explicit Socket() = default;

        virtual auto Disconnect() -> void = 0;

        MKT_NODISCARD virtual auto GetHost() const -> const std::string& = 0;

        MKT_NODISCARD virtual auto IsConnected() const -> bool = 0;
        MKT_NODISCARD virtual auto Connect( std::string_view address, UInt16 port ) -> bool = 0;

        MKT_NODISCARD virtual auto SendSync( std::string_view data ) -> bool = 0;
        MKT_NODISCARD virtual auto SendSync( const void* data, Size size ) -> bool = 0;

        MKT_NODISCARD virtual auto ReceiveSync( void* buffer, Size maxSize ) -> Size = 0;
    };

    using SocketHandle = Ref<Socket>;


    class TcpSocket final : public Socket {
    public:

        TcpSocket( asio::io_context& ctx, std::string_view address, UInt16 port, bool wait );

#if defined( MKT_ALLOW_HTTPS )
        TcpSocket( asio::io_context& ctx, asio::ssl::context& sslContext, std::string_view address, UInt16 port, bool wait = true );
#endif

        auto Disconnect() -> void override;

        MKT_NODISCARD auto IsConnected() const -> bool override;
        MKT_NODISCARD auto Connect( std::string_view address, UInt16 port ) -> bool override;

        MKT_NODISCARD auto SendSync( std::string_view data ) -> bool override;
        MKT_NODISCARD auto SendSync( const void* data, Size size ) -> bool override;

        MKT_NODISCARD auto ReceiveSync( void* buffer, Size maxSize ) -> Size override;

        MKT_NODISCARD auto GetHost() const -> const std::string& override;

        ~TcpSocket() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

        auto InitConnectionSync() -> void;

    private:
        // To avoid keep reading if we reach eof
        asio::error_code m_ErrorCode{};

#if defined( MKT_ALLOW_HTTPS )
        asio::ip::tcp::socket m_Socket;
        asio::ssl::stream<asio::ip::tcp::socket>* m_SslSocket{ nullptr };

#else
asio::ip::tcp::socket m_Socket;
#endif

        UInt16 m_Port{};
        std::string m_HostName{};

        bool m_IsSsl{ false };
        bool m_Connected{ false };

        bool m_InitSync{ false };
    };
}// namespace Mikoto


#endif
