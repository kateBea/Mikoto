//
// Created by kate on 10/29/25.
//

#ifndef SOCKET_HH
#define SOCKET_HH

#include <string>
#include <string_view>

#include <asio.hpp>

#include <Library/Utility/Types.hh>
#include <Library/Data/ResourcePool.hh>

namespace Mikoto {

    class Socket : public IResource {
    public:
        explicit Socket() = default;

        virtual auto Disconnect() -> void  = 0;

        MKT_NODISCARD virtual auto IsConnected() const -> bool = 0;
        MKT_NODISCARD virtual auto Connect(std::string_view address, UInt16 port) -> bool = 0;

        MKT_NODISCARD virtual auto Send(const void* data, Size size) -> bool = 0;
        MKT_NODISCARD virtual auto Receive(void* buffer, Size maxSize) -> Size = 0;

    };

    using SocketHandle = Ref<Socket>;



    // TCP -------------------
    class TcpSocket final : public Socket {
    public:
        TcpSocket(asio::io_context& ctx, std::string_view address, UInt16 port);


        auto Disconnect() -> void override;

        MKT_NODISCARD auto IsConnected() const -> bool override;
        MKT_NODISCARD auto Connect( std::string_view address, UInt16 port ) -> bool override;

        MKT_NODISCARD auto Send( const void* data, Size size ) -> bool override;
        MKT_NODISCARD auto Receive( void* buffer, Size maxSize ) -> Size override;

        ~TcpSocket() override;

            private :
        auto Initialize() -> void override;
        auto Release() -> void override;
    private:
        asio::ip::tcp::socket m_Socket;

        UInt16 m_Port{};
        std::string_view m_Address{};

        bool m_Connected{ false };
    };
}



#endif
