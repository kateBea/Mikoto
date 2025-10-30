//
// Created by kate on 10/29/25.
//

#include "Networking/Socket.hh"

namespace Mikoto {

    // TCP ---------------

    TcpSocket::TcpSocket( asio::io_context &ctx, const std::string_view address, UInt16 port )
        : m_Socket{ ctx }, m_Port{ port }, m_Address{ address } {
        Initialize();
    }


    auto TcpSocket::Disconnect() -> void {
        m_Socket.close();
        m_Connected = false;
    }

    auto TcpSocket::IsConnected() const -> bool {
        return m_Connected;
    }

    auto TcpSocket::Connect( const std::string_view address, const UInt16 port ) -> bool {
        if (m_Connected) {
            Disconnect();
        }

        m_Port = port;
        m_Address = address;

        Initialize();

        return m_Connected;
    }

    auto TcpSocket::Send( const void *data, const Size size ) -> bool {
        asio::write(m_Socket, asio::buffer(data, size));
        return true;
    }

    auto TcpSocket::Receive( void *buffer, const Size maxSize ) -> Size {
        return m_Socket.read_some(asio::buffer(buffer, maxSize));
    }

    TcpSocket::~TcpSocket() {
        if (m_IsAllocated) {
            Release();
        }
    }

    auto TcpSocket::Initialize() -> void {
        asio::ip::tcp::resolver resolver(m_Socket.get_executor());
        auto endpoints { resolver.resolve(m_Address, std::to_string(m_Port)) };

        asio::async_connect(m_Socket, endpoints,
            [this](std::error_code ec, const asio::ip::tcp::endpoint&) {
                if (!ec) {
                    m_Connected = true;
                    MKT_CORE_LOGGER_INFO("Connected successfully");
                } else {
                    MKT_CORE_LOGGER_ERROR("Connect failed: {}", ec.message());
                }
            });
    }

    auto TcpSocket::Release() -> void {
        if (m_Connected) {
            Disconnect();
        }

        m_IsAllocated = false;
    }
}// namespace Mikoto